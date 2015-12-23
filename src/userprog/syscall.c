#include "userprog/syscall.h"
#include <stdio.h>
#include <syscall-nr.h>
#include "threads/thread.h"
#include <debug.h>
#include "threads/vaddr.h"
#include <string.h>
#include <list.h>
static void syscall_handler (struct intr_frame *);
static int get_user (const uint8_t *uaddr);

typedef struct file_descriptor{
	file * file_pointer;
	list_elem
}fd_t;

void
syscall_init (void) 
{
  intr_register_int (0x30, 3, INTR_ON, syscall_handler, "syscall");
}

static void
syscall_handler (struct intr_frame *f) 
{
	//printf ("system call!\n");
	uint32_t * esp = f->esp;
	uint32_t num = *esp;
	switch (num){
		case SYS_WRITE:
		syscall_write(esp+5,f);
		break;
		case SYS_READ:
		syscall_read(esp+5,f);
		break;
		case SYS_EXIT:
		syscall_exit(esp+5,f);
		break;
	}
	//thread_exit ();
}
/*prints buffer to file*/
void
syscall_write(uint32_t *esp, struct intr_frame *f)
{
	uint32_t fd = *esp;
	char * buffer = *(char **)(esp+1);
	//buffer = get_user(buffer);
	uint32_t sz = *(esp+2);
	if(fd == 1){
		putbuf(buffer,sz);
	}else if(fd == 0){
		/*do nothing we should not print in stin*/
	}else{
		/*write to file with descriptor fd*/

	}
	f->eax =  sz;
}

/*reads buffer from file*/
void
syscall_read(uint32_t *esp, struct intr_frame *f)
{
	uint32_t fd = *esp;
	char * buffer = *(char **)(esp+1);
	//buffer = get_user(buffer);
	uint32_t sz = *(esp+2);
	int i =0;
	if(fd == 0){
		for(;i<sz;i++){
			buffer[i] = input_getc();
		}
	}else if (fd == 1){
		/*we should not take anything from stdout*/
	}else{
		/*read from file with descriptor fd*/
	}
	f->eax =  sz;
}

/*exit thread*/
void
syscall_exit(uint32_t *esp, struct intr_frame *f)
{
	int status = *esp;
	char * line ;
	printf ("%s: exit(%d)\n", strtok_r(thread_current ()->name," \t",&line),status);
	thread_exit ();
}

static int
get_user (const uint8_t *uaddr)
{
  int result;
  asm ("movl $1f, %0; movzbl %1, %0; 1:"
       : "=&a" (result) : "m" (*uaddr));
  return result;
}