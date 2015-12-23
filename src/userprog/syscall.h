#ifndef USERPROG_SYSCALL_H
#define USERPROG_SYSCALL_H

#include <stdint.h>
#include "threads/interrupt.h"


void syscall_init (void);

/*user added*/
void syscall_write(uint32_t * esp, struct intr_frame *);
void syscall_read(uint32_t * esp, struct intr_frame *);
void syscall_exit(uint32_t * esp, struct intr_frame *);
#endif /* userprog/syscall.h */
