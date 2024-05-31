#ifndef USERPROG_SYSCALL_H
#define USERPROG_SYSCALL_H
#include "userprog/process.h"

void syscall_init (void);
struct lock filesys_lock;
void filesys_lock_init();
void filesys_lock_acquire();
void filesys_lock_release();
typedef int pid_t;
void syscall_halt(void);
void syscall_exit(int);
pid_t syscall_exec(const char *);
int syscall_wait(pid_t);
int syscall_read(int, void *, unsigned);
int syscall_write(int , const void *, unsigned);
bool syscall_remove(const char *);
bool syscall_create(const char *, unsigned);
void syscall_seek(int, unsigned);
unsigned syscall_tell(int);
void syscall_close(int);
bool is_buffer_valid(const char*);
bool is_pointer_valid(const char*);
#endif /* userprog/syscall.h */
