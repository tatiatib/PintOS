#ifndef USERPROG_SYSCALL_H
#define USERPROG_SYSCALL_H

#include <stdio.h>


void syscall_init (void);
void force_exit(uint32_t * eax);
#endif /* userprog/syscall.h */
