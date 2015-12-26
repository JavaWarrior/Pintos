#ifndef USERPROG_PROCESS_H
#define USERPROG_PROCESS_H

#include "threads/thread.h"

tid_t process_execute (const char *file_name);
int process_wait (tid_t);
void process_exit (void);
void process_activate (void);

/*user added*/

/*added declarations*/
struct lock wait_lock;
struct semaphore creat_sema;
struct lock file_lock;            /*lock for file handling as filesystem code isn't concurrent*/

/*macro for finding general element in general list*/
#define find_in_list(LIST_PNT, STRUCT, ELEM, VALUENAME, VALUE)			              \
({																		                                            \
	struct list_elem *e;												                                    \
	void * x_ret_value = NULL;													                            \
  for (e = list_begin (LIST_PNT); e != list_end (LIST_PNT); e = list_next (e))		\
  {																	                                              \
    STRUCT *____t = list_entry (e, STRUCT, ELEM);							                    \
    if(____t->VALUENAME == VALUE){										                            \
    	x_ret_value = ____t;															                          \
    	break;															                                        \
    }																	                                            \
  }																	                                              \
    x_ret_value;																	                                \
}) 



void process_init(void);
void thread_init_wait_DS(struct thread * t);
struct process_child_elem * find_child(int child_tid);
#endif /* userprog/process.h */
