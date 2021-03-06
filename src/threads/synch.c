/* This file is derived from source code for the Nachos
   instructional operating system.  The Nachos copyright notice
   is reproduced in full below. */

/* Copyright (c) 1992-1996 The Regents of the University of California.
   All rights reserved.

   Permission to use, copy, modify, and distribute this software
   and its documentation for any purpose, without fee, and
   without written agreement is hereby granted, provided that the
   above copyright notice and the following two paragraphs appear
   in all copies of this software.

   IN NO EVENT SHALL THE UNIVERSITY OF CALIFORNIA BE LIABLE TO
   ANY PARTY FOR DIRECT, INDIRECT, SPECIAL, INCIDENTAL, OR
   CONSEQUENTIAL DAMAGES ARISING OUT OF THE USE OF THIS SOFTWARE
   AND ITS DOCUMENTATION, EVEN IF THE UNIVERSITY OF CALIFORNIA
   HAS BEEN ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

   THE UNIVERSITY OF CALIFORNIA SPECIFICALLY DISCLAIMS ANY
   WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
   WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
   PURPOSE.  THE SOFTWARE PROVIDED HEREUNDER IS ON AN "AS IS"
   BASIS, AND THE UNIVERSITY OF CALIFORNIA HAS NO OBLIGATION TO
   PROVIDE MAINTENANCE, SUPPORT, UPDATES, ENHANCEMENTS, OR
   MODIFICATIONS.
*/

#include "threads/synch.h"
#include <stdio.h>
#include <string.h>
#include "threads/interrupt.h"
#include "threads/thread.h"

#include "threads/malloc.h"

/*user added*/
/*for donation*/
struct donor_lock_element{
  struct lock * don_lock;       /*the lock corresponding to this donation*/
  struct list_elem elem;        /*list element to save donation for every thread*/
};

void donate(struct lock * lk,int cur_priority);
void undo_donate(struct lock *lk);
void nested_donate(struct thread *t, struct lock * lk, int cur_priority);
void remove_and_refresh_priority (struct thread * t, struct lock * lk);
struct donor_lock_element * get_donor_from_list(struct list * l, struct lock * lk);

/*for semaphore condition variable comparison*/
bool is_greater_sema (const struct list_elem *a,
                             const struct list_elem *b,
                             void *aux); /* function for ordered insertion (makes higher priority come first)*/

/* Initializes semaphore SEMA to VALUE.  A semaphore is a
   nonnegative integer along with two atomic operators for
   manipulating it:

   - down or "P": wait for the value to become positive, then
     decrement it.

   - up or "V": increment the value (and wake up one waiting
     thread, if any). */
void
sema_init (struct semaphore *sema, unsigned value) 
{
  ASSERT (sema != NULL);

  sema->value = value;
  list_init (&sema->waiters);
}

/* Down or "P" operation on a semaphore.  Waits for SEMA's value
   to become positive and then atomically decrements it.

   This function may sleep, so it must not be called within an
   interrupt handler.  This function may be called with
   interrupts disabled, but if it sleeps then the next scheduled
   thread will probably turn interrupts back on. */
void
sema_down (struct semaphore *sema) 
{
  enum intr_level old_level;

  ASSERT (sema != NULL);
  ASSERT (!intr_context ());

  old_level = intr_disable ();
  while (sema->value == 0) 
    {
      list_insert_ordered (&sema->waiters, &thread_current ()->elem,&is_greater,NULL);
      /*insert ordered so that threads takes semaphores according to its priorities*/
      thread_block ();
    }
  sema->value--;
  intr_set_level (old_level);
}

/* Down or "P" operation on a semaphore, but only if the
   semaphore is not already 0.  Returns true if the semaphore is
   decremented, false otherwise.

   This function may be called from an interrupt handler. */
bool
sema_try_down (struct semaphore *sema) 
{
  enum intr_level old_level;
  bool success;

  ASSERT (sema != NULL);

  old_level = intr_disable ();
  if (sema->value > 0) 
    {
      sema->value--;
      success = true; 
    }
  else
    success = false;
  intr_set_level (old_level);

  return success;
}

/* Up or "V" operation on a semaphore.  Increments SEMA's value
   and wakes up one thread of those waiting for SEMA, if any.

   This function may be called from an interrupt handler. */
void
sema_up (struct semaphore *sema) 
{
  enum intr_level old_level;

  ASSERT (sema != NULL);

  list_sort(&sema->waiters,&is_greater,NULL); /*sort here to check donations*/

  old_level = intr_disable ();
  if (!list_empty (&sema->waiters)) 
    thread_unblock (list_entry (list_pop_front (&sema->waiters),
                                struct thread, elem));
  sema->value++;
  intr_set_level (old_level);
  thread_yield(); /*yield first to run the unblocked thread if it has bigger priority*/
}

static void sema_test_helper (void *sema_);

/* Self-test for semaphores that makes control "ping-pong"
   between a pair of threads.  Insert calls to printf() to see
   what's going on. */
void
sema_self_test (void) 
{
  struct semaphore sema[2];
  int i;

  printf ("Testing semaphores...");
  sema_init (&sema[0], 0);
  sema_init (&sema[1], 0);
  thread_create ("sema-test", PRI_DEFAULT, sema_test_helper, &sema);
  for (i = 0; i < 10; i++) 
    {
      sema_up (&sema[0]);
      sema_down (&sema[1]);
    }
  printf ("done.\n");
}

/* Thread function used by sema_self_test(). */
static void
sema_test_helper (void *sema_) 
{
  struct semaphore *sema = sema_;
  int i;

  for (i = 0; i < 10; i++) 
    {
      sema_down (&sema[0]);
      sema_up (&sema[1]);
    }
}

/* Initializes LOCK.  A lock can be held by at most a single
   thread at any given time.  Our locks are not "recursive", that
   is, it is an error for the thread currently holding a lock to
   try to acquire that lock.

   A lock is a specialization of a semaphore with an initial
   value of 1.  The difference between a lock and such a
   semaphore is twofold.  First, a semaphore can have a value
   greater than 1, but a lock can only be owned by a single
   thread at a time.  Second, a semaphore does not have an owner,
   meaning that one thread can "down" the semaphore and then
   another one "up" it, but with a lock the same thread must both
   acquire and release it.  When these restrictions prove
   onerous, it's a good sign that a semaphore should be used,
   instead of a lock. */
void
lock_init (struct lock *lock)
{
  ASSERT (lock != NULL);

  lock->holder = NULL;
  sema_init (&lock->semaphore, 1);
  lock->don_priority = -1;
}

/* Acquires LOCK, sleeping until it becomes available if
   necessary.  The lock must not already be held by the current
   thread.

   This function may sleep, so it must not be called within an
   interrupt handler.  This function may be called with
   interrupts disabled, but interrupts will be turned back on if
   we need to sleep. */
void
lock_acquire (struct lock *lock)
{
  ASSERT (lock != NULL);
  ASSERT (!intr_context ());
  ASSERT (!lock_held_by_current_thread (lock));
  if(lock->semaphore.value <= 0){
    /*if another thread is holding the lock*/
    thread_current()->pending_lock = lock;
    donate(lock, thread_current()->donated_priority);
  }
  sema_down (&lock->semaphore);   /*take the lock*/
  lock->holder = thread_current ();
}

/* Tries to acquires LOCK and returns true if successful or false
   on failure.  The lock must not already be held by the current
   thread.

   This function will not sleep, so it may be called within an
   interrupt handler. */
bool
lock_try_acquire (struct lock *lock)
{
  bool success;

  ASSERT (lock != NULL);
  ASSERT (!lock_held_by_current_thread (lock));

  success = sema_try_down (&lock->semaphore);
  if (success)
    lock->holder = thread_current ();
  return success;
}

/* Releases LOCK, which must be owned by the current thread.

   An interrupt handler cannot acquire a lock, so it does not
   make sense to try to release a lock within an interrupt
   handler. */
void
lock_release (struct lock *lock) 
{
  ASSERT (lock != NULL);
  ASSERT (lock_held_by_current_thread (lock));

  lock->holder = NULL;
  sema_up (&lock->semaphore);
  /*after unblocking we need to decrease our priority again*/
  undo_donate(lock);    /*remove donations if any*/
  /*here I have a question if we made the undo before the sema_up
    many errors happens. maybe if the thread is lowered it won't up the sema.
  */
  thread_yield();             /*yield to check higher priority unlocked threas*/
}

/* Returns true if the current thread holds LOCK, false
   otherwise.  (Note that testing whether some other thread holds
   a lock would be racy.) */
bool
lock_held_by_current_thread (const struct lock *lock) 
{
  ASSERT (lock != NULL);

  return lock->holder == thread_current ();
}

/* One semaphore in a list. */
struct semaphore_elem 
  {
    struct list_elem elem;              /* List element. */
    struct semaphore semaphore;         /* This semaphore. */
    struct thread * t;                  /* the locked thread*/
  };

/* Initializes condition variable COND.  A condition variable
   allows one piece of code to signal a condition and cooperating
   code to receive the signal and act upon it. */
void
cond_init (struct condition *cond)
{
  ASSERT (cond != NULL);

  list_init (&cond->waiters);
}

/* Atomically releases LOCK and waits for COND to be signaled by
   some other piece of code.  After COND is signaled, LOCK is
   reacquired before returning.  LOCK must be held before calling
   this function.

   The monitor implemented by this function is "Mesa" style, not
   "Hoare" style, that is, sending and receiving a signal are not
   an atomic operation.  Thus, typically the caller must recheck
   the condition after the wait completes and, if necessary, wait
   again.

   A given condition variable is associated with only a single
   lock, but one lock may be associated with any number of
   condition variables.  That is, there is a one-to-many mapping
   from locks to condition variables.

   This function may sleep, so it must not be called within an
   interrupt handler.  This function may be called with
   interrupts disabled, but interrupts will be turned back on if
   we need to sleep. */
void
cond_wait (struct condition *cond, struct lock *lock) 
{
  struct semaphore_elem waiter;

  ASSERT (cond != NULL);
  ASSERT (lock != NULL);
  ASSERT (!intr_context ());
  ASSERT (lock_held_by_current_thread (lock));
  
  sema_init (&waiter.semaphore, 0);
  waiter.t = thread_current();
  /*make semaphore_elem related to thread to take priority in consideration*/
  list_insert_ordered (&cond->waiters, &waiter.elem,&is_greater_sema,NULL);
  /*sort here to check donations and wake up higher priority thread first*/
  lock_release (lock);
  sema_down (&waiter.semaphore);
  lock_acquire (lock);
}

/* If any threads are waiting on COND (protected by LOCK), then
   this function signals one of them to wake up from its wait.
   LOCK must be held before calling this function.

   An interrupt handler cannot acquire a lock, so it does not
   make sense to try to signal a condition variable within an
   interrupt handler. */
void
cond_signal (struct condition *cond, struct lock *lock UNUSED) 
{
  ASSERT (cond != NULL);
  ASSERT (lock != NULL);
  ASSERT (!intr_context ());
  ASSERT (lock_held_by_current_thread (lock));

  if (!list_empty (&cond->waiters)){
    sema_up (&list_entry (list_pop_front (&cond->waiters),
                          struct semaphore_elem, elem)->semaphore);
  }
}

/* Wakes up all threads, if any, waiting on COND (protected by
   LOCK).  LOCK must be held before calling this function.

   An interrupt handler cannot acquire a lock, so it does not
   make sense to try to signal a condition variable within an
   interrupt handler. */
void
cond_broadcast (struct condition *cond, struct lock *lock) 
{
  ASSERT (cond != NULL);
  ASSERT (lock != NULL);

  while (!list_empty (&cond->waiters))
    cond_signal (cond, lock);
}
/*make donation if this thread needs donation to release the lock*/
void 
donate(struct lock * lk,int cur_priority)
{
  struct thread * t = lk->holder;

  ASSERT(t!=NULL);      /*make sure that t isn't NULL*/

  struct donor_lock_element * donor = get_donor_from_list(&t->donors_list, lk);

  if(donor == NULL){
    /* this lock have not donated before*/
    if(cur_priority > t->priority){
      /*this thread can make donation*/
      donor = (struct donor_lock_element *) malloc(sizeof (struct donor_lock_element));
      ASSERT(donor != NULL);
      lk->don_priority = cur_priority;  /*save donation in lock*/
      donor->don_lock = lk;               /*save lock in thread donors list*/
      list_push_back(&t->donors_list, &donor->elem);
    }
  }else if(cur_priority > lk->don_priority ){
    lk->don_priority = cur_priority;      /*modify lock donation only*/
  }
  if(cur_priority > t->donated_priority){
      t->donated_priority = cur_priority;
  }
  nested_donate(t, lk, cur_priority);  /*go to thread that is holding necessary lock*/
}

/*recursively donate priority to threads related to this thread*/
void
nested_donate(struct thread *t, struct lock * lk, int cur_priority)
{
  lk = t->pending_lock;         /*go to the lock stoping us*/
  if(lk != NULL){               /*if there's one */
    t = lk->holder;             /*see what thread is controlling it*/
    while(t != NULL){
        /* donate to this thread if possible*/
      if(t->priority < cur_priority && cur_priority > t->donated_priority){
        t->donated_priority = cur_priority;   /*donate to the thread*/
        lk->don_priority = cur_priority;      /*save donation in lock*/
      }else{
        break;
      }
      lk = t->pending_lock;                   /*go to next level*/
      if(lk == NULL){                         /*-----\O---------*/
        break;                                /*------|\--------*/
      }else{                                  /*------|---------*/
        t = lk->holder;                       /*-----/-\--------*/
      }
    }
  }
}

/*return thread priority to its original state*/
void 
undo_donate(struct lock *lk)
{
  struct thread * t = thread_current(); /*get the thread that took the donation*/

  ASSERT(t!=NULL);
  struct donor_lock_element * donor = get_donor_from_list(&t->donors_list, lk);
  if(donor != NULL){
    /* this lock donated to this thread*/
    remove_and_refresh_priority(t,lk);
    lk->don_priority = -1;
  }
}

/* comparison function for semaphore list*/
bool
is_greater_sema (const struct list_elem *a, const struct list_elem *b, void *aux UNUSED)
{
  struct thread *t_a =  list_entry (a,struct semaphore_elem, elem)->t,*t_b = list_entry (b,struct semaphore_elem, elem)->t;
  
  if(!thread_mlfqs){
    return (t_a->donated_priority > t_b->donated_priority);
  }else{
    /*in advanced scheduling mode donated priority isn't taken into consideration*/
    return (t_a->priority > t_b->priority);    
  }
}

/*search for donor_loc_elem with lk in list l*/
struct donor_lock_element *
get_donor_from_list(struct list * l, struct lock * lk)
{
  
  ASSERT (lk != NULL);
  ASSERT (l != NULL);

  struct list_elem * e ;
  struct donor_lock_element * donor;
  for(e = list_begin(l); e != list_end(l); e = list_next(e)){
    donor = list_entry(e ,struct donor_lock_element, elem);
    if(donor->don_lock == lk){
      /*if the two pointers have the same address*/
      return donor;
    }
  }
  return NULL;
}

/*search for lock lk in t donors list and remove it and refreshes priority*/
void
remove_and_refresh_priority (struct thread * t, struct lock * lk)
{
  ASSERT (lk != NULL);
  ASSERT (t != NULL);

  struct list_elem * e = list_begin(&t->donors_list);
  struct donor_lock_element * donor;
  int max_prio = 0;
  while( e != list_end(&t->donors_list)){
    donor = list_entry(e ,struct donor_lock_element, elem);
    if(donor->don_lock == lk){
      /*if the two pointers have the same address*/
      e = list_remove(&donor->elem);
      /*remove the lock for the donors list*/
      continue;
      /*no need to advance pointer*/
    }else if(donor->don_lock->don_priority > max_prio){
      max_prio = donor->don_lock->don_priority;
      /*compute max priority from donors*/
    }
    e = list_next(e);
    /*advance pointer*/
  }
  if(max_prio == 0){
    /*no remaining donors in the list*/
    t->donated_priority = t->priority;
  }else{
    /*set the  priority to the second maximum*/
    t->donated_priority = max_prio;
  }
}