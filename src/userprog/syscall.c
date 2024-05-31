#include "userprog/syscall.h"
#include <stdio.h>
#include <syscall-nr.h>
#include "threads/interrupt.h"
#include "threads/thread.h"
#include "filesys/filesys.h"
#include "filesys/file.h"
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
  if(!is_pointer_valid(add)){
		syscall_exit(-1);
	}
}

bool
is_buffer_valid(const char* ptr){
	if (ptr == NULL | !is_user_vaddr(ptr)) {
    return false;
  }
  return true;
}

bool
is_pointer_valid(const char* ptr){
  if (pagedir_get_page(thread_current()->pagedir, ptr)) {
    return true;
  }
  return false;
}

void
filesys_lock_init() {
  lock_init(&filesys_lock);
}

void
filesys_lock_acquire() {
  lock_acquire(&filesys_lock);
}

void
filesys_lock_release() {
  lock_release(&filesys_lock);
}


void
syscall_init (void) 
{
  intr_register_int (0x30, 3, INTR_ON, syscall_handler, "syscall");
  filesys_lock_init();
}

void 
syscall_halt(void) {
  shutdown_power_off();
}

void 
syscall_exit(int status) {
  thread_current()->exit_status = status;
  printf("%s: exit(%d)\n", thread_current()->name, status);
  thread_exit();
}

pid_t 
syscall_exec(const char *cmd_line) {

  if (cmd_line == NULL) {
    syscall_exit(-1);
  }
  return process_execute(cmd_line);
}

int 
syscall_wait(pid_t pid) {
  return process_wait(pid);
}

bool 
syscall_create(const char *file, unsigned initial_size) {
  if (file == NULL) {
    syscall_exit(-1);
  }
  bool success = filesys_create(file, initial_size);
  return success;
}

bool 
syscall_remove(const char *file) {
  if (file == NULL) {
    syscall_exit(-1);
  }
  bool success = filesys_remove(file);
  return success;
}

int 
syscall_open(const char *file) {
  if (file == NULL) {
    return -1;
  }
  if (!(is_pointer_valid(file)) | strlen(file)>14)
    return -1;
  
  struct file* open_file;
  struct thread *cur;
  int next_fd;
  filesys_lock_acquire();
  open_file = filesys_open(file);
  if (open_file == NULL) {
    filesys_lock_release();
    return -1;
  }

  cur = thread_current();
  next_fd = cur->next_fd; 
  cur->next_fd = next_fd + 1;

  cur->fdt[next_fd] = open_file;

  // If the file trying to open is itsself, do file_deny
  if (!strcmp(cur->name, file))  {
    file_deny_write(open_file);
  }
  filesys_lock_release();
  return next_fd;
}

int
syscall_filesize(int fd) {
  if (fd < 0 || fd >= thread_current()->next_fd) {
    return -1;
  }
  struct file *cur_file = thread_current()->fdt[fd];
  if (cur_file) {
    return file_length(cur_file);
  }
  return -1;
}

int
syscall_read(int fd, void *buffer, unsigned size) {
  struct thread *cur;
  struct file *read_file;
  int read_until;

  if (!is_buffer_valid(buffer)) {
    syscall_exit(-1);
    return -1;
  }
  if (!is_pointer_valid(buffer+size)) {
    syscall_exit(-1);
    return -1;
  }


  if (fd == 0) {
    int count = 0;
    while (1) {
      if (count >= size) {
        break;
      }
      count++;

      char key = input_getc();
      if (key == '\n') {
        char *buffer = key;
        break;
      }

      char *buffer = key;
      buffer++;
    }
    return count;
  }
  
  else if (2<=fd && fd < 64) {
    cur = thread_current();
    read_file = cur->fdt[fd];
    if (read_file == NULL) {
      return -1;
    }
    else {
      filesys_lock_acquire();
      read_until = file_read(read_file, buffer, size);
      filesys_lock_release();
    }
  }
  return read_until;
}

int
syscall_write(int fd, const void *buffer, unsigned size) {
  struct thread *cur;
  struct file *write_file;

  if (!is_buffer_valid(buffer)) {
    syscall_exit(-1);
    return -1;
  }

  if (!is_pointer_valid(buffer+size)) {
    syscall_exit(-1);
    return -1;
  }

  if (fd == 0) {
    syscall_exit(-1);
    return -1;
  }

  if (fd == 1) {
    putbuf(buffer, size);
    return size;
  }

  else if (2 <= fd && fd < 64) {
    cur = thread_current();
    write_file = cur->fdt[fd];
    if (write_file == NULL) {
      return -1;
    }
    filesys_lock_acquire();
    off_t write_until = file_write(write_file, buffer, size);
    filesys_lock_release();
    return (int)write_until;
  }
  return -1;
}

void
syscall_seek(int fd, unsigned position) {
  struct file* seek_file = thread_current()->fdt[fd];
  file_seek(seek_file, position);
}

unsigned
syscall_tell(int fd) {
  struct file* tell_file = thread_current()->fdt[fd];
  return (unsigned)file_tell(tell_file);
}

void
syscall_close(int fd) {
  if (!(0<=fd && fd<64)) {
    syscall_exit(-1);
    return;
  }

  filesys_lock_acquire();
  struct file* close_file = thread_current()->fdt[fd];
  thread_current()->fdt[fd] = NULL;
  file_close(close_file);
  filesys_lock_release();
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
      address_safe_check(esp + 4);
      address_safe_check(esp + 8);
      arg_0 = (*(uint32_t *)(esp + 4));
      arg_1 = (*(uint32_t *)(esp + 8));
      f->eax = syscall_create((char *)arg_0, (unsigned)arg_1);
      break;
    case SYS_REMOVE:
      address_safe_check(esp + 4);
      arg_0 = (*(uint32_t *)(esp + 4));
      f->eax = syscall_remove((char*)arg_0);
      break;
    case SYS_OPEN:
      address_safe_check(esp + 4);
      arg_0 = (*(uint32_t *)(esp + 4));
      f->eax = syscall_open((char *)arg_0);
      break;
    case SYS_FILESIZE:
      address_safe_check(esp + 4);
      arg_0 = (*(uint32_t *)(esp + 4));
      f->eax = syscall_filesize((int)arg_0);
      break;
    case SYS_READ:
      address_safe_check(esp + 4);
      address_safe_check(esp + 8);
      address_safe_check(esp + 12);
      arg_0 = (*(uint32_t *)(esp + 4));
      arg_1 = (*(uint32_t *)(esp + 8));
      arg_2 = (*(uint32_t *)(esp + 12));
      f->eax = syscall_read((int)arg_0, (void *)arg_1, (unsigned)arg_2);
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
      address_safe_check(esp + 4);
      address_safe_check(esp + 8);
      arg_0 = (*(uint32_t *)(esp + 4));
      arg_1 = (*(uint32_t *)(esp + 8));
      syscall_seek((int)arg_0, (unsigned)arg_1);
      break;
    case SYS_TELL:
      address_safe_check(esp + 4);
      arg_0 = (*(uint32_t *)(esp + 4));
      f->eax = syscall_tell((int)arg_0);
      break;
    case SYS_CLOSE:
      address_safe_check(esp + 4);
      arg_0 = (*(uint32_t *)(esp + 4));
      syscall_close((int)arg_0);
      break;

  }
}
