#include "userprog/syscall.h"
#include <stdio.h>
#include <syscall-nr.h>
#include "threads/interrupt.h"
#include "threads/thread.h"
#include "filesys/filesys.h"

static void syscall_handler (struct intr_frame *);

void
syscall_init (void) 
{
  intr_register_int (0x30, 3, INTR_ON, syscall_handler, "syscall");
}

void syscall_halt(void) {
  shutdown_power_off();
}

void syscall_exit(int status) {
  printf("%s: exit(%d)\n", thread_current()->name, status);
  thread_exit();
}

pid_t syscall_exec(const char *cmd_line) {
  if (cmd_line == NULL) {
    syscall_exit(-1);
  }
  return process_execute(cmd_line);
}

int syscall_wait(pid_t pid) {
  return process_wait(pid);
}

bool syscall_create(const char *file, unsigned initial_size) {
  if (file == NULL) {
    syscall_exit(-1);
  }
  bool success = filesys_create(file, initial_size);
  return success;
}

bool syscall_remove(const char *file) {
  if (file == NULL) {
    syscall_exit(-1);
  }
  bool success = filesys_remove(file);
  return success;
}

int
syscall_write(int fd, const void *buffer, unsigned size) {
  if (fd == 1) {
    putbuf(buffer, size);
    return size;
  }
  return -1;
}

static void
syscall_handler (struct intr_frame *f UNUSED) 
{ uint32_t *esp = f->esp;
  
  switch (*(uint32_t *)(f->esp)) {
    case SYS_HALT:
      syscall_halt();
      break;
    case SYS_EXIT:
      syscall_exit(*(esp + 1));
      break;
    case SYS_EXEC:
      break;
    case SYS_WAIT:
      break;
    case SYS_CREATE:
      break;
    case SYS_REMOVE:
      break;
    case SYS_OPEN:
      break;
    case SYS_FILESIZE:
      break;
    case SYS_READ:
      break;
    case SYS_WRITE:
      f->eax = syscall_write (*(esp + 1), (void *) *(esp + 2), *(esp + 3));
      break;
    case SYS_SEEK:
      break;
    case SYS_TELL:
      break;
    case SYS_CLOSE:
      break;

  // printf ("system call!\n");
  // thread_exit ();
  }
}
