#ifndef USERPROG_SYSCALL_H
#define USERPROG_SYSCALL_H
#include "userprog/process.h"

void syscall_init (void);
typedef int pid_t;
void syscall_halt(void);
void syscall_exit(int);
pid_t syscall_exec(const char *);
int syscall_wait(pid_t);
int syscall_write(int , const void *, unsigned);
bool syscall_remove(const char *);
bool syscall_create(const char *, unsigned);
#endif /* userprog/syscall.h */
