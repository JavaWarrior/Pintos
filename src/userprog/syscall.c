#include "userprog/syscall.h"

/*user prog*/
#include <stdio.h>
#include <syscall-nr.h>
#include "threads/thread.h"
#include <debug.h>
#include "threads/vaddr.h"
#include <string.h>
#include <list.h>
#include "threads/init.h"
#include "threads/malloc.h"
#include "devices/input.h"

/*userprog defines*/
#define __one_arg_param		*(esp+5)
#define __two_arg_param		*(esp+5),*(esp+6)
#define __three_arg_param	*(esp+5),*(esp+6),*(esp+7)

static void syscall_handler (struct intr_frame *);

/* list that stores exit status for every process that exits*/
static struct list status_list;


struct file_descriptor{
	struct file * 		file_pointer;		/*file which this element represent*/
	int 				fd;					/*file descriptor for this element*/
	struct list_elem  	thread_elem;		/*list element to be stored in thread*/
};

struct status_elem{
	pid_t pid;					/*pid of the exiting thread*/
	struct list_elem elem;		/*element for status list*/
}
/* end of definitions*/
/*-----------------------------------------------------------------------*/


/*system call initialization*/
void
syscall_init (void) 
{
  intr_register_int (0x30, 3, INTR_ON, syscall_handler, "syscall");
  list_init(&status_list);
}

static void
syscall_handler (struct intr_frame *f) 
{
	//printf ("system call!\n");
	uint32_t * esp = f->esp;
	uint32_t num = *esp;
	switch (num){
		
		case SYS_EXIT:								/*exit this process*/
		sys_exit(__one_arg_param);
		break;

		case SYS_HALT:								/*shutdown the system*/
		sys_halt();
		break;

		case SYS_EXEC:								/*execute another process*/
		f->eax = sys_exec(__one_arg_param);
		break;

		case SYS_WAIT:								/*wait for child to finish*/
		f->eax = sys_wait(__one_arg_param);

		case SYS_WRITE:
		f->eax = sys_write(__three_arg_param);
		break;
		
		case SYS_READ:
		f->eax = syscall_read(__three_arg_param);
		break;
		

	}
	//thread_exit ();
}


/*turns of pintOS*/
void
sys_halt (void) 
{
	/*shutdown pintos*/
	shutdown_power_off();
}

/*exits the current process and prints exit message*/
void
sys_exit (int status)
{
	struct thread * t = thread_current();
	char * temp_name = (char *) malloc(strlen(t-> name));
	int i=0;
	while(t -> name[i] != ' ' || t -> name[i] != '\t'){
		/*get program name only without arguments*/
		temp_name[i] = t->name [i];
		temp_name[++i] = 0;
	}

	/*print termination message*/
	printf ("%s: exit(%d)\n", temp_name,status);
	
	/*free the holding pointer as it'll not be used anymore*/
	free(temp_name);

	/*childs part*/
	struct status_elem stat;
	stat.pid = t->tid;							/*save exiting thread tid*/
	list_push_back(&status_list,&stat.elem);	/*push the element in the status list*/

	if(t->parent_thread != NULL){
		/* a thread is waiting for us to exit*/
		thread_unblock(t->parent_thread);
	}
	/*exit the thread and free its resources*/
	thread_exit ();
}

/*executes command line and return the process id*/
pid_t
sys_exec (const char *file)
{
	tid_t ret_value;
	ret_value =  process_execute(file);
  	/*check return value*/
  	if(ret_value == TID_ERROR){
  		/*if thread was not created return -1*/
  		return -1;
  	}else{
  		/*return thread tid as process pid - one to one mapping*/
  		struct process_child_elem child_elem ;			/* make an element to represent this child*/
  		child_elem.pid = ret_value;						/*set pid of the element of the new thread*/
  			/*push back the new thread to children list*/
  		list_push_back(&thread_current() -> children,&child_elem.child_elem);
  		return ret_value;
  	}
}

int
sys_wait (pid_t pid)
{
	if(!is_pid_child(pid)){
		/*thread holding pid isn't a child thread of this thread*/
		return -1;
	}else{
		/*first we find this thread*/
		struct thread * t_chld = get_thread_by_tid(pid_t);
		if(t_chld == NULL){
			/*child thread already exited*/
			/*return find_thread_from_status_list*/
		}
		if(t_chld->is_waited){
			/*wait have been called once on this tid*/
			return -1;
		}
		t_chld->is_waited = true;
		t_chld->parent_thread = thread_current();	/*set waiting thread*/
		
		/*block the current thread*/
		enum intr_level old_level;
  		old_level = intr_disable ();
		thread_block();/*block the current thread*/
		intr_set_level(old_level);
		/*unblock the current thread*/
 	}
 return syscall1 (SYS_WAIT, pid);
}

bool
sys_create (const char *file, unsigned initial_size)
{
  return syscall2 (SYS_CREATE, file, initial_size);
}

bool
sys_remove (const char *file)
{
  return syscall1 (SYS_REMOVE, file);
}

int
sys_open (const char *file)
{
  return syscall1 (SYS_OPEN, file);
}

int
sys_filesize (int fd) 
{
  return syscall1 (SYS_FILESIZE, fd);
}


/*reads from file to buffer */
int
sys_read (int fd, void *buffer, unsigned size)
{	
	char * str = buffer;
	int i =0;
	if(fd == 0){
		for(;i<size;i++){
			str[i] = input_getc();
		}
	}else if (fd == 1){
		/*we should not take anything from stdout*/
	}else{
		/*read from file with descriptor fd*/
	}
  	return size;
}

int
sys_write (int fd, const void *buffer, unsigned size)
{
  	if(fd == 1){
		putbuf(buffer,size);
	}else if(fd == 0){
		/*do nothing we should not print in stin*/
	}else{
		/*write to file with descriptor fd*/

	}
	return  size;
}

void
sys_seek (int fd, unsigned position) 
{
  syscall2 (SYS_SEEK, fd, position);
}

unsigned
sys_tell (int fd) 
{
  return syscall1 (SYS_TELL, fd);
}

void
sys_close (int fd)
{
  syscall1 (SYS_CLOSE, fd);
}

