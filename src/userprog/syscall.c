#include "userprog/syscall.h"
#include <stdio.h>
#include <syscall-nr.h>
#include "threads/interrupt.h"
#include "threads/thread.h"
#include "filesys/filesys.h"
#include "threads/vaddr.h"
#include "userprog/pagedir.h"

static void syscall_handler (struct intr_frame *);
void address_safe_check(void* add);

void
address_safe_check(void* add) {
  if (add == NULL) {
    syscall_exit(-1);
  }
  if (!is_user_vaddr(add)) {
    syscall_exit(-1);
  }
}

void
syscall_init (void) 
{
  intr_register_int (0x30, 3, INTR_ON, syscall_handler, "syscall");
}

void syscall_halt(void) {
  shutdown_power_off();
}

void syscall_exit(int status) {
  thread_current()->exit_status = status;
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
{ void *esp = f->esp;
  uint32_t syscall_num = *(uint32_t *)(esp);
  uint32_t arg_0, arg_1, arg_2;
  
  switch (syscall_num) {
    case SYS_HALT:
      syscall_halt();
      break;
    case SYS_EXIT:
      address_safe_check(esp + 4);
      arg_0 = (*(uint32_t *)(esp + 4));
      syscall_exit((int)arg_0);
      break;
    case SYS_EXEC:
      address_safe_check(esp + 4);
      arg_0 = (*(uint32_t *)(esp + 4));
      f->eax = (uint32_t) syscall_exec((char *)arg_0);
      break;
    case SYS_WAIT:
      address_safe_check(esp + 4);
      arg_0 = (*(uint32_t *)(esp + 4));
      f->eax = syscall_wait((pid_t)arg_0);
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
      address_safe_check(esp + 4);
      address_safe_check(esp + 8);
      address_safe_check(esp + 12);
      arg_0 = (*(uint32_t *)(esp + 4));
      arg_1 = (*(uint32_t *)(esp + 8));
      arg_2 = (*(uint32_t *)(esp + 12));
      f->eax = syscall_write ((int)arg_0, (void *)arg_1, (unsigned)arg_2);
      break;
    case SYS_SEEK:
      break;
    case SYS_TELL:
      break;
    case SYS_CLOSE:
      break;

  }
}
