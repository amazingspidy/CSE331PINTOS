#include "threads/thread.h"
#include <debug.h>
#include <stddef.h>
#include <random.h>
#include <stdio.h>
#include <string.h>
#include "threads/flags.h"
#include "threads/interrupt.h"
#include "threads/intr-stubs.h"
#include "threads/palloc.h"
#include "threads/switch.h"
#include "threads/synch.h"
#include "threads/vaddr.h"
#include "threads/fixed_calculation.h"
#ifdef USERPROG
#include "userprog/process.h"
#endif

/* Random value for struct thread's `magic' member.
   Used to detect stack overflow.  See the big comment at the top
   of thread.h for details. */
#define THREAD_MAGIC 0xcd6abf4b

/* List of processes in THREAD_READY state, that is, processes
   that are ready to run but not actually running. */
static struct list ready_list;

/* List of processes in Sleeping state*/
static struct list sleep_list;
static int64_t nearest_wakeup_ticks = -1;
/* List of all processes.  Processes are added to this list
   when they are first scheduled and removed when they exit. */
static struct list all_list;

int load_avg;

/* Idle thread. */
static struct thread *idle_thread;

/* Initial thread, the thread running init.c:main(). */
static struct thread *initial_thread;

/* Lock used by allocate_tid(). */
static struct lock tid_lock;

/* Stack frame for kernel_thread(). */
struct kernel_thread_frame 
  {
    void *eip;                  /* Return address. */
    thread_func *function;      /* Function to call. */
    void *aux;                  /* Auxiliary data for function. */
  };

/* Statistics. */
static long long idle_ticks;    /* # of timer ticks spent idle. */
static long long kernel_ticks;  /* # of timer ticks in kernel threads. */
static long long user_ticks;    /* # of timer ticks in user programs. */

/* Scheduling. */
#define TIME_SLICE 4            /* # of timer ticks to give each thread. */
static unsigned thread_ticks;   /* # of timer ticks since last yield. */

/* If false (default), use round-robin scheduler.
   If true, use multi-level feedback queue scheduler.
   Controlled by kernel command-line option "-o mlfqs". */
bool thread_mlfqs;

static void kernel_thread (thread_func *, void *aux);

static void idle (void *aux UNUSED);
static struct thread *running_thread (void);
static struct thread *next_thread_to_run (void);
static void init_thread (struct thread *, const char *name, int priority);
static bool is_thread (struct thread *) UNUSED;
static void *alloc_frame (struct thread *, size_t size);
static void schedule (void);
void thread_schedule_tail (struct thread *prev);
static tid_t allocate_tid (void);

/* Initializes the threading system by transforming the code
   that's currently running into a thread.  This can't work in
   general and it is possible in this case only because loader.S
   was careful to put the bottom of the stack at a page boundary.

   Also initializes the run queue and the tid lock.

   After calling this function, be sure to initialize the page
   allocator before trying to create any threads with
   thread_create().

   It is not safe to call thread_current() until this function
   finishes. */
void
thread_init (void) 
{
  ASSERT (intr_get_level () == INTR_OFF);

  lock_init (&tid_lock);
  list_init (&ready_list);
  list_init (&sleep_list);
  list_init (&all_list);

  /* Set up a thread structure for the running thread. */
  initial_thread = running_thread ();
  init_thread (initial_thread, "main", PRI_DEFAULT);
  initial_thread->status = THREAD_RUNNING;
  initial_thread->tid = allocate_tid ();
  //initial_thread->parent = NULL;
}

/* Starts preemptive thread scheduling by enabling interrupts.
   Also creates the idle thread. */
void
thread_start (void) 
{
  /* Create the idle thread. */
  struct semaphore idle_started;
  sema_init (&idle_started, 0);
  thread_create ("idle", PRI_MIN, idle, &idle_started);
  load_avg = 0;
  /* Start preemptive thread scheduling. */
  intr_enable ();

  /* Wait for the idle thread to initialize idle_thread. */
  sema_down (&idle_started);
}

/* Called by the timer interrupt handler at each timer tick.
   Thus, this function runs in an external interrupt context. */
void
thread_tick (void) 
{
  struct thread *t = thread_current ();

  /* Update statistics. */
  if (t == idle_thread)
    idle_ticks++;
#ifdef USERPROG
  else if (t->pagedir != NULL)
    user_ticks++;
#endif
  else
    kernel_ticks++;

  /* Enforce preemption. */
  if (++thread_ticks >= TIME_SLICE)
    intr_yield_on_return ();
}

/* Prints thread statistics. */
void
thread_print_stats (void) 
{
  printf ("Thread: %lld idle ticks, %lld kernel ticks, %lld user ticks\n",
          idle_ticks, kernel_ticks, user_ticks);
}

/* Creates a new kernel thread named NAME with the given initial
   PRIORITY, which executes FUNCTION passing AUX as the argument,
   and adds it to the ready queue.  Returns the thread identifier
   for the new thread, or TID_ERROR if creation fails.

   If thread_start() has been called, then the new thread may be
   scheduled before thread_create() returns.  It could even exit
   before thread_create() returns.  Contrariwise, the original
   thread may run for any amount of time before the new thread is
   scheduled.  Use a semaphore or some other form of
   synchronization if you need to ensure ordering.

   The code provided sets the new thread's `priority' member to
   PRIORITY, but no actual priority scheduling is implemented.
   Priority scheduling is the goal of Problem 1-3. */
tid_t
thread_create (const char *name, int priority,
               thread_func *function, void *aux) 
{
  struct thread *t;
  struct kernel_thread_frame *kf;
  struct switch_entry_frame *ef;
  struct switch_threads_frame *sf;
  tid_t tid;

  ASSERT (function != NULL);

  /* Allocate thread. */
  t = palloc_get_page (PAL_ZERO);
  if (t == NULL)
    return TID_ERROR;

  /* Initialize thread. */
  init_thread (t, name, priority);
  tid = t->tid = allocate_tid ();
  /* Stack frame for kernel_thread(). */
  kf = alloc_frame (t, sizeof *kf);
  kf->eip = NULL;
  kf->function = function;
  kf->aux = aux;

  /* Stack frame for switch_entry(). */
  ef = alloc_frame (t, sizeof *ef);
  ef->eip = (void (*) (void)) kernel_thread;

  /* Stack frame for switch_threads(). */
  sf = alloc_frame (t, sizeof *sf);
  sf->eip = switch_entry;
  sf->ebp = 0;

  /*for Hierarchy*/
  
  list_push_back(&thread_current()->child_list, &t->child_elem);
  t->parent = thread_current();
  
  /* Add to run queue. */
  /* go to ready list*/
  thread_unblock (t);
  thread_try_preemption();
  

  return tid;
}

/* Puts the current thread to sleep.  It will not be scheduled
   again until awoken by thread_unblock().

   This function must be called with interrupts turned off.  It
   is usually a better idea to use one of the synchronization
   primitives in synch.h. */

bool compare_by_priority(const struct list_elem *a, const struct list_elem *b, void *aux UNUSED) {
  if (a == NULL) {
    return true;
  }
  if (b == NULL) {
    return false;
  }

  struct thread *thread_a = list_entry(a, struct thread, elem);
  struct thread *thread_b = list_entry(b, struct thread, elem);
  return thread_a->priority > thread_b->priority;
}

bool compare_by_priority_donator(const struct list_elem *a, const struct list_elem *b, void *aux UNUSED) {
  if (a == NULL) {
    return true;
  }
  if (b == NULL) {
    return false;
  }

  struct thread *thread_a = list_entry(a, struct thread, donation_elem);
  struct thread *thread_b = list_entry(b, struct thread, donation_elem);
  return thread_a->priority > thread_b->priority;
}

void
thread_block (void) 
{
  ASSERT (!intr_context ());
  ASSERT (intr_get_level () == INTR_OFF);

  thread_current ()->status = THREAD_BLOCKED;
  schedule ();
}

/* Transitions a blocked thread T to the ready-to-run state.
   This is an error if T is not blocked.  (Use thread_yield() to
   make the running thread ready.)

   This function does not preempt the running thread.  This can
   be important: if the caller had disabled interrupts itself,
   it may expect that it can atomically unblock a thread and
   update other data. */
void
thread_unblock (struct thread *t) 
{
  enum intr_level old_level;

  ASSERT (is_thread (t));

  old_level = intr_disable ();
  ASSERT (t->status == THREAD_BLOCKED);
  //기존//list_push_back (&ready_list, &t->elem);
  list_insert_ordered(&ready_list, &t->elem, compare_by_priority, NULL);
  t->status = THREAD_READY;
  intr_set_level (old_level);
}

/* Returns the name of the running thread. */
const char *
thread_name (void) 
{
  return thread_current ()->name;
}

/* Returns the running thread.
   This is running_thread() plus a couple of sanity checks.
   See the big comment at the top of thread.h for details. */
struct thread *
thread_current (void) 
{
  struct thread *t = running_thread ();
  
  /* Make sure T is really a thread.
     If either of these assertions fire, then your thread may
     have overflowed its stack.  Each thread has less than 4 kB
     of stack, so a few big automatic arrays or moderate
     recursion can cause stack overflow. */
  ASSERT (is_thread (t));
  ASSERT (t->status == THREAD_RUNNING);

  return t;
}

/* Returns the running thread's tid. */
tid_t
thread_tid (void) 
{
  return thread_current ()->tid;
}

/* Deschedules the current thread and destroys it.  Never
   returns to the caller. */
void
thread_exit (void) 
{
  ASSERT (!intr_context ());

#ifdef USERPROG
  process_exit ();
#endif

  /* Remove thread from all threads list, set our status to dying,
     and schedule another process.  That process will destroy us
     when it calls thread_schedule_tail(). */
  intr_disable ();
  list_remove (&thread_current()->allelem);
  thread_current ()->status = THREAD_DYING;
  schedule ();
  NOT_REACHED ();
}

/* Yields the CPU.  The current thread is not put to sleep and
   may be scheduled again immediately at the scheduler's whim. */
void
thread_yield (void) 
{
  struct thread *cur = thread_current ();
  enum intr_level old_level;
  
  ASSERT (!intr_context ()); 

  old_level = intr_disable ();
  if (cur != idle_thread) {
    //list_push_back (&ready_list, &cur->elem);
    list_insert_ordered(&ready_list, &cur->elem, compare_by_priority, NULL);
  }
  cur->status = THREAD_READY;
  schedule ();
  intr_set_level (old_level);
}

void
thread_sleep(int64_t ticks) 
{
  struct thread *cur = thread_current ();
  enum intr_level old_level;

  ASSERT (!intr_context ());

  if (nearest_wakeup_ticks == -1) {
    nearest_wakeup_ticks = ticks; //initialize
  }
  else {
    if (nearest_wakeup_ticks > ticks) { // choose nearest_wakeup_ticks smaller
      nearest_wakeup_ticks = ticks;
    }
  }

  old_level = intr_disable ();
  if (cur != idle_thread) {
    list_push_back (&sleep_list, &cur->elem);
    cur->status = THREAD_BLOCKED;
    cur->wakeup_tick = ticks;
  }

  schedule ();
  intr_set_level (old_level);
}

void
thread_wakeup(int64_t ticks)
{
  if (nearest_wakeup_ticks > ticks) {
    return;
  }
  struct list_elem *sleep_iter;
  
  for (sleep_iter=list_begin(&sleep_list); sleep_iter != list_end(&sleep_list);) {
    struct thread *sleeping_thread = list_entry(sleep_iter, struct thread, elem);
    if (sleeping_thread -> wakeup_tick <= ticks) {
      sleep_iter = list_remove(sleep_iter); //여기서 remove시, 알아서 다음 요소를 준다. next 사용 안해도됨.
      thread_unblock(sleeping_thread);
    }
    else {
      sleep_iter = list_next(sleep_iter);
    }
    
  }
}

/* Invoke function 'func' on all threads, passing along 'aux'.
   This function must be called with interrupts off. */
void
thread_foreach (thread_action_func *func, void *aux)
{
  struct list_elem *e;

  ASSERT (intr_get_level () == INTR_OFF);

  for (e = list_begin (&all_list); e != list_end (&all_list);
       e = list_next (e))
    {
      struct thread *t = list_entry (e, struct thread, allelem);
      func (t, aux);
    }
}

/* Sets the current thread's priority to NEW_PRIORITY. */
 void
 thread_set_priority (int new_priority) 
 {
   if (thread_mlfqs) {
     return;
   }
   //If not turn off interrupts here, the changing in priority will not be immediate and may cause scheduling problems.
   enum intr_level old_level = intr_disable ();
   thread_current ()->original_priority = new_priority;
   thread_rollback_priority();
   thread_try_preemption ();
   intr_set_level (old_level);
 }

/* Returns the current thread's priority. */
int
thread_get_priority (void) 
{
  return thread_current ()->priority;
}

void 
thread_set_lock_waiting_for(struct lock *lock) {
  thread_current ()->lock_waiting_for = lock;
}

void 
thread_try_preemption(void){
  //enum intr_level old_level = intr_disable ();

  if (!list_empty(&ready_list) && !intr_context()) {
    //Before checking the above if statement, turn off interrupt.  
    //Because when it switches,there may be nothing in the ready_list when it goes down.
    struct thread *ready_high_thread = list_entry(list_front(&ready_list), struct thread, elem);
    if (thread_current()->priority < ready_high_thread->priority) {
      thread_yield();
    }
  }
  //intr_set_level (old_level);
}



void
thread_add_donator_by_priority(struct thread *pred, struct thread *cur) {
  list_insert_ordered(&pred->donator_list, &cur->donation_elem, compare_by_priority_donator, NULL);
}

void
thread_priority_donation(void) {
  struct thread *cur = thread_current();
  
  for (int i = 0; i < 8; i++) { //fully nested max depth = 8
    if (cur->lock_waiting_for) {
      struct thread *cur_lock_predecessor = cur->lock_waiting_for->holder;
      cur_lock_predecessor->priority = cur->priority;
      cur = cur_lock_predecessor;
    }
    else {
      break;
    }
  }
}

void
thread_delete_some_donator(struct lock *lock) {
  struct thread *lock_holder = thread_current();
  struct list_elem *iter;
  
  for (iter = list_begin(&lock_holder->donator_list); iter != list_end(&lock_holder->donator_list);) {
    struct thread *donator_thread = list_entry(iter, struct thread, donation_elem);
    if (donator_thread->lock_waiting_for == lock) {
      iter = list_remove(&donator_thread->donation_elem);
    }
    else {
      iter = list_next(iter);
    }
  }
  
}

void
thread_rollback_priority(void) {
  struct thread *cur = thread_current();
  cur->priority = cur->original_priority;

  if (list_empty(&cur->donator_list)) {
    return;
  }

  list_sort(&cur->donator_list, compare_by_priority_donator, NULL);
  struct list_elem *front = list_front(&cur->donator_list);
  struct thread *thread_high_priority = list_entry(front, struct thread, donation_elem);
  if (cur->priority < thread_high_priority->priority) {
    cur->priority = thread_high_priority->priority;
  }
  return;
}

void calculate_mlfq_priority(struct thread* t) {
  if (t != idle_thread) {
    int temp1 = div_fixed_integer(t->recent_cpu, 4);
    int temp2 = sub_integer_fixed(PRI_MAX, temp1);
    int temp3 = sub_fixed_integer(temp2, 2*(t->nice));
    t->priority = fixed_to_integer(temp3);
  }
}

void calculate_mlfq_load_avg(void) {
  int ready_threads = (int) list_size(&ready_list);
  if (thread_current() != idle_thread) {
    ready_threads++;
  }

  int factor1 = div_fixed_integer(integer_to_fixed(59), 60);
  int factor2 = div_fixed_integer(integer_to_fixed(1), 60);
  int front = mul_fixed_fixed(factor1, load_avg);
  int back = mul_fixed_integer(factor2, ready_threads);
  load_avg = add_fixed_fixed(front, back);

}

void calculate_mlfq_recent_cpu(struct thread *t) {
  if (t == idle_thread) {
    return;
  }
  int decay_denom = mul_fixed_integer(load_avg, 2);
  int decay_numer = add_fixed_integer(decay_denom, 1);
  int decay = div_fixed_fixed(decay_denom, decay_numer);

  int old_recent_cpu = t->recent_cpu; //real-num
  int temp = mul_fixed_fixed(decay, old_recent_cpu);
  t->recent_cpu = add_fixed_integer(temp, t->nice);

  
}

//for every 1 tick
void increase_mlfq_recent_cpu(void) {
  if (thread_current() != idle_thread) {
    int cur_recent_cpu = thread_current()->recent_cpu;
    int new_recent_cpu = add_fixed_integer(cur_recent_cpu, 1);
    thread_current()->recent_cpu = new_recent_cpu;
  }
}

//for every 4 ticks
void recompute_mlfq_priority(void) {
  struct list_elem *iter;
  for (iter = list_begin(&all_list); iter != list_end(&all_list);) {
    struct thread *now = list_entry(iter, struct thread, allelem);
    calculate_mlfq_priority(now);
    iter = list_next(iter);
  }
}

//for every 100 ticks
void update_mlfq_recent_cpu(void) {
  struct list_elem *iter;
  for (iter = list_begin(&all_list); iter != list_end(&all_list);) {
    struct thread *now = list_entry(iter, struct thread, allelem);
    calculate_mlfq_recent_cpu(now);
    iter = list_next(iter);
  }
}

/* Sets the current thread's nice value to NICE. */
void
thread_set_nice (int nice UNUSED) 
{
  enum intr_level old_level = intr_disable ();
  thread_current ()->nice = nice;
  calculate_mlfq_priority(thread_current());
  thread_try_preemption();
  intr_set_level (old_level);
}

/* Returns the current thread's nice value. */
int
thread_get_nice (void) 
{
  int return_nice;
  enum intr_level old_level = intr_disable ();
  return_nice = thread_current()->nice;
  intr_set_level (old_level);
  return return_nice;
}

/* Returns 100 times the system load average. */
int
thread_get_load_avg (void) 
{
  int return_load_avg;
  enum intr_level old_level = intr_disable ();
  return_load_avg = fixed_to_integer(mul_fixed_integer(load_avg, 100));
  intr_set_level (old_level);
  return return_load_avg;
}

/* Returns 100 times the current thread's recent_cpu value. */
int
thread_get_recent_cpu (void) 
{
  int return_recent_cpu;
  enum intr_level old_level = intr_disable ();
  return_recent_cpu = round_to_integer(mul_fixed_integer(thread_current()->recent_cpu, 100));
  intr_set_level (old_level);
  return return_recent_cpu;
}

/* Idle thread.  Executes when no other thread is ready to run.

   The idle thread is initially put on the ready list by
   thread_start().  It will be scheduled once initially, at which
   point it initializes idle_thread, "up"s the semaphore passed
   to it to enable thread_start() to continue, and immediately
   blocks.  After that, the idle thread never appears in the
   ready list.  It is returned by next_thread_to_run() as a
   special case when the ready list is empty. */
static void
idle (void *idle_started_ UNUSED) 
{
  struct semaphore *idle_started = idle_started_;
  idle_thread = thread_current ();
  sema_up (idle_started);

  for (;;) 
    {
      /* Let someone else run. */
      intr_disable ();
      thread_block ();

      /* Re-enable interrupts and wait for the next one.

         The `sti' instruction disables interrupts until the
         completion of the next instruction, so these two
         instructions are executed atomically.  This atomicity is
         important; otherwise, an interrupt could be handled
         between re-enabling interrupts and waiting for the next
         one to occur, wasting as much as one clock tick worth of
         time.

         See [IA32-v2a] "HLT", [IA32-v2b] "STI", and [IA32-v3a]
         7.11.1 "HLT Instruction". */
      asm volatile ("sti; hlt" : : : "memory");
    }
}

/* Function used as the basis for a kernel thread. */
static void
kernel_thread (thread_func *function, void *aux) 
{
  ASSERT (function != NULL);

  intr_enable ();       /* The scheduler runs with interrupts off. */
  function (aux);       /* Execute the thread function. */
  thread_exit ();       /* If function() returns, kill the thread. */
}

/* Returns the running thread. */
struct thread *
running_thread (void) 
{
  uint32_t *esp;

  /* Copy the CPU's stack pointer into `esp', and then round that
     down to the start of a page.  Because `struct thread' is
     always at the beginning of a page and the stack pointer is
     somewhere in the middle, this locates the curent thread. */
  asm ("mov %%esp, %0" : "=g" (esp));
  return pg_round_down (esp);
}

/* Returns true if T appears to point to a valid thread. */
static bool
is_thread (struct thread *t)
{
  return t != NULL && t->magic == THREAD_MAGIC;
}

/* Does basic initialization of T as a blocked thread named
   NAME. */
static void
init_thread (struct thread *t, const char *name, int priority)
{
  enum intr_level old_level;

  ASSERT (t != NULL);
  ASSERT (PRI_MIN <= priority && priority <= PRI_MAX);
  ASSERT (name != NULL);

  memset (t, 0, sizeof *t);
  t->status = THREAD_BLOCKED;
  strlcpy (t->name, name, sizeof t->name);
  t->stack = (uint8_t *) t + PGSIZE;
  t->priority = priority;
  t->original_priority = priority;
  list_init(&t->donator_list);
  t->lock_waiting_for = NULL;
  t->nice = 0;
  t->recent_cpu = 0;
  t->magic = THREAD_MAGIC;

/* ------------------------- */
  list_init(&t->child_list);
  sema_init(&t->sema_for_exit, 0);
  sema_init(&t->sema_for_wait, 0);
  sema_init(&t->sema_for_load, 0);
  t->load_success = false;
  t->exit_success = false;
  t->exit_status = 0;

  for (int i = 0; i++; i<64) {
    t->fdt[i] = NULL;
  }
  t->next_fd = 3;

/*-------------------------*/
  old_level = intr_disable ();
  list_push_back (&all_list, &t->allelem);
  intr_set_level (old_level);
}

/* Allocates a SIZE-byte frame at the top of thread T's stack and
   returns a pointer to the frame's base. */
static void *
alloc_frame (struct thread *t, size_t size) 
{
  /* Stack data is always allocated in word-size units. */
  ASSERT (is_thread (t));
  ASSERT (size % sizeof (uint32_t) == 0);

  t->stack -= size;
  return t->stack;
}

/* Chooses and returns the next thread to be scheduled.  Should
   return a thread from the run queue, unless the run queue is
   empty.  (If the running thread can continue running, then it
   will be in the run queue.)  If the run queue is empty, return
   idle_thread. */
static struct thread *
next_thread_to_run (void) 
{
  if (list_empty (&ready_list))
    return idle_thread;
  else
    return list_entry (list_pop_front (&ready_list), struct thread, elem);
}

/* Completes a thread switch by activating the new thread's page
   tables, and, if the previous thread is dying, destroying it.

   At this function's invocation, we just switched from thread
   PREV, the new thread is already running, and interrupts are
   still disabled.  This function is normally invoked by
   thread_schedule() as its final action before returning, but
   the first time a thread is scheduled it is called by
   switch_entry() (see switch.S).

   It's not safe to call printf() until the thread switch is
   complete.  In practice that means that printf()s should be
   added at the end of the function.

   After this function and its caller returns, the thread switch
   is complete. */
void
thread_schedule_tail (struct thread *prev)
{
  struct thread *cur = running_thread ();
  
  ASSERT (intr_get_level () == INTR_OFF);

  /* Mark us as running. */
  cur->status = THREAD_RUNNING;

  /* Start new time slice. */
  thread_ticks = 0;

#ifdef USERPROG
  /* Activate the new address space. */
  process_activate ();
#endif

  /* If the thread we switched from is dying, destroy its struct
     thread.  This must happen late so that thread_exit() doesn't
     pull out the rug under itself.  (We don't free
     initial_thread because its memory was not obtained via
     palloc().) */
  if (prev != NULL && prev->status == THREAD_DYING && prev != initial_thread) 
    {
      ASSERT (prev != cur);
      palloc_free_page (prev);
    }
}

/* Schedules a new process.  At entry, interrupts must be off and
   the running process's state must have been changed from
   running to some other state.  This function finds another
   thread to run and switches to it.

   It's not safe to call printf() until thread_schedule_tail()
   has completed. */
static void
schedule (void) 
{
  struct thread *cur = running_thread ();
  struct thread *next = next_thread_to_run ();
  struct thread *prev = NULL;

  ASSERT (intr_get_level () == INTR_OFF);
  ASSERT (cur->status != THREAD_RUNNING);
  ASSERT (is_thread (next));

  if (cur != next)
    prev = switch_threads (cur, next);
  thread_schedule_tail (prev);
}

/* Returns a tid to use for a new thread. */
static tid_t
allocate_tid (void) 
{
  static tid_t next_tid = 1;
  tid_t tid;

  lock_acquire (&tid_lock);
  tid = next_tid++;
  lock_release (&tid_lock);

  return tid;
}

/* Offset of `stack' member within `struct thread'.
   Used by switch.S, which can't figure it out on its own. */
uint32_t thread_stack_ofs = offsetof (struct thread, stack);
