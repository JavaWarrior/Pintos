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
#include "threads/synch.h"
#include "userprog/process.h"
#include "userprog/pagedir.h"

/*userprog defines*/
#define __one_arg_param		*(esp+1)
#define __two_arg_param		*(esp+1),*(esp+2)
#define __three_arg_param	*(esp+1),*(esp+2),*(esp+3)

static void syscall_handler (struct intr_frame *);
static bool test(uint32_t* esp);
static bool check_pntr(void * pntr);





struct lock file_lock;						/*lock for file handling as filesystem code isn't concurrent*/
/* end of definitions*/
/*-----------------------------------------------------------------------*/


/*system call initialization*/
void
syscall_init (void) 
{
  intr_register_int (0x30, 3, INTR_ON, syscall_handler, "syscall");

 
  lock_init(&file_lock);
}

static void
syscall_handler (struct intr_frame *f) 
{
	//printf ("system call!\n");
	uint32_t * esp = f->esp;
	if(esp < 0x08048000  || !test(esp)){	/*stack pointer is invalid*/
		sys_exit(-1);
	}
	uint32_t num = *esp;

	// hex_dump(esp,esp,20,true);

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

		case SYS_CREATE:							/*create a file*/
		f->eax = sys_create(__two_arg_param);
		break;



		case SYS_WRITE:
		f->eax = sys_write(__three_arg_param);
		break;
		
		case SYS_READ:
		f->eax = sys_read(__three_arg_param);
		break;
		

	}
	//thread_exit ();
}


/*turns off pintOS*/
void
sys_halt (void) 
{
	/*shutdown pintos*/
	shutdown_power_off();
}


/*
exits the current process and prints exit message
Terminates the current user program, returning status to the kernel. 
If the process's parent waits for it (see below), this is the status 
that will be returned. Conventionally, a status of 0 indicates success
 and nonzero values indicate errors.
*/
void
sys_exit (int status)
{
	/*childs part*/
	lock_acquire(&wait_lock);
	thread_current()->child_elem_pntr->status = status;
	lock_release(&wait_lock);
	/*exit the thread and free its resources*/
	thread_exit ();
}


/*
executes command line and return the process id
Runs the executable whose name is given in cmd_line, passing any given arguments,
and returns the new process's program id (pid). Must return pid -1, which otherwise
should not be a valid pid, if the program cannot load or run for any reason. Thus, 
the parent process cannot return from the exec until it knows whether the child 
process successfully loaded its executable. You must use appropriate synchronization
 to ensure this.
*/
pid_t
sys_exec (const char *file)
{
	/*nothing to be done here*/
	return process_execute(file);
}


/*waits for process with pid.
note that process referenced by pid must be a child process
If pid is still alive, waits until it terminates. Then, returns the status that pid 
passed to exit. If pid did not call exit(), but was terminated by the kernel 
(e.g. killed due to an exception), wait(pid) must return -1. 
It is perfectly legal for a parent process to wait for child processes that have 
already terminated by the time the parent calls wait, but the kernel must still allow
 the parent to retrieve its child's exit status, or learn that the child was terminated by the kernel.
*/
int
sys_wait (pid_t pid)
{
	return process_wait(pid);
}


/*
Creates a new file called file initially initial_size bytes in size. 
Returns true if successful, false otherwise. Creating a new file does not open it: 
opening the new file is a separate operation which would require a open system call.
*/
bool
sys_create (const char *file, unsigned initial_size)
{
	if(!check_pntr(file)){
		/*do all the necessary checks for this pointer*/
		sys_exit(-1);
	}
	/*filesystem isn't concurrent*/
	lock_acquire(&file_lock);
	/*we should do nothing here just return this value*/
	bool ret_value = filesys_create(file,initial_size);
	lock_release(&file_lock);
	return ret_value;
}


/*
Deletes the file called file. Returns true if successful, false otherwise. 
A file may be removed regardless of whether it is open or closed,
 and removing an open file does not close it.
*/
bool
sys_remove (const char *file)
{
	if(!check_pntr(file)){
		/*do all the necessary checks for this pointer*/
		sys_exit(-1);
	}
	lock_acquire(&file_lock);
	bool ret_value = filesys_remove(file);
	lock_release(&file_lock);

	return ret_value;
}


/*
Opens the file called file. Returns a nonnegative integer handle called a 
"file descriptor" (fd), or -1 if the file could not be opened.
File descriptors numbered 0 and 1 are reserved for the console: fd 0 
(STDIN_FILENO) is standard input, fd 1 (STDOUT_FILENO) is standard output. 
The open system call will never return either of these file descriptors, 
which are valid as system call arguments only as explicitly described below.

Each process has an independent set of file descriptors. File descriptors 
are not inherited by child processes.

When a single file is opened more than once, whether by a single process 
or different processes, each open returns a new file descriptor. Different 
file descriptors for a single file are closed independently in separate calls
to close and they do not share a file position.
*/
int
sys_open (const char *file)
{
  	if(!check_pntr(file)){
		/*do all the necessary checks for this pointer*/
		sys_exit(-1);
	}
	struct file * fp;
	lock_acquire(&file_lock);
	fp = file_open(file);							/*open the file*/
	lock_release(&file_lock);
	if(fp != NULL){
		return get_new_fd(fp);						/*if file exists store it in the running thread*/
	}
	return -1;
}

 /*
 Returns the size, in bytes, of the file open as fd.
 */
int
sys_filesize (int fd) 
{
	struct file * fp = get_file_fd(fd);				/*get corresponding file pointer for this file*/
	if(fp != NULL){
		return file_length(fp);						/*return file size if it corresponds to a valid fd*/
	}
	return 0;
}


/*
reads from file to buffer
Reads size bytes from the file open as fd into buffer. 
Returns the number of bytes actually read (0 at end of file),
or -1 if the file could not be read (due to a condition other than end of file). 
Fd 0 reads from the keyboard using input_getc().
 */
int
sys_read (int fd, void *buffer, unsigned size)
{	
	if(!check_pntr(buffer)){
		/*do all necessary stuff to check the pointer*/
		sys_exit(-1);
	}
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
  return ;
  // syscall2 (SYS_SEEK, fd, position);
}

unsigned
sys_tell (int fd) 
{
return 0;	
  // return syscall1 (SYS_TELL, fd);
}

void
sys_close (int fd)
{
	return ;
  // syscall1 (SYS_CLOSE, fd);
}

bool
test(uint32_t * esp)
{
	return is_user_vaddr(*esp) && is_user_vaddr(*(esp+1)) && is_user_vaddr(*(esp+3));
}

bool
check_pntr(void * pntr)
{
	return pntr != 0 && is_user_vaddr(pntr) && pntr > 0x08048000 && is_valid_pointer(thread_current()->pagedir,pntr);
}
