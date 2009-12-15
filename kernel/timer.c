/*   MIR-OS : Timer functions
;    Copyright (C) 2003 A.R.Karthick 
;    <a_r_karthic@rediffmail.com,karthick_r@infosys.com>
;
;    This program is free software; you can redistribute it and/or modify
;    it under the terms of the GNU General Public License as published by
;    the Free Software Foundation; either version 2 of the License, or
;    (at your option) any later version.
;
;    This program is distributed in the hope that it will be useful,
;    but WITHOUT ANY WARRANTY; without even the implied warranty of
;    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
;    GNU General Public License for more details.
;
;    You should have received a copy of the GNU General Public License
;    along with this program; if not, write to the Free Software
;    Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
;
;
;    Timer utility functions: add,del,etc.
*/

#include <mir/sched.h>
#include <mir/timer.h>
#include <mir/tasklet.h>

static DECLARE_LIST_HEAD(timer_head);

static int timer_compare(struct timer_list *a,struct timer_list *b) { 
  return a->timeout - b->timeout;
}

void __init_timer(struct timer_list *timer) { 
  timer->compare = timer_compare;
}

/*Sorted additions into the timer*/
int __add_timer(struct timer_list *timer) { 
  struct list_head *element = &timer->timer_head;
  struct list_head *head    = &timer_head;
  int err = -1;
  if(timer->timeout > MAX_TIMEOUT) { 
   #ifdef DEBUG
    printf("Invalid timeout for timer:\n");
   #endif
    goto out;
  }
 
  if(timer->compare)
   list_sort_add(element,head,struct timer_list,timer_head);
  else {
 #ifdef DEBUG
    printf("The compare routine for the timer isnt set:\n");
 #endif
    goto out;
  }
  err =  0;
 out:
  return err ;
}  

void __del_timer(struct timer_list *timer) { 
  list_del(&timer->timer_head); 
}

int setup_timer(struct timer_list *timer,unsigned long timeout,timer_handler handler,void *arg) { 
  int status;
  unsigned long flags;
  save_flags_irq(flags);
  timer->timeout = timeout;
  timer->handler = handler;
  timer->arg     = arg;
  status = __add_timer(timer);
  restore_flags(flags);
  return status;
} 
 
/*Run through the timer list and call the handlers:
  Called now from Tasklets or Bottom half context.
*/

static void run_timer(void *arg) { 
  struct list_head *head = &timer_head;
  struct list_head *traverse = NULL,*next = NULL;
  unsigned long flags;
  save_flags_irq(flags);
  for(traverse = head->next ; traverse != head ; traverse = next) { 
    struct timer_list *timer = list_entry(traverse,struct timer_list,timer_head);
    next = traverse->next;
    /*See if the timer has expired*/
    if(jiffies >= timer->timeout) { 
      /*This guy has expired:Call the handler for this guy*/
      __del_timer(timer);
      /*Call the handler*/
      if(timer->handler) {
	timer->handler(timer->arg);
      }
    }
  }
  restore_flags(flags);
}

/*Do some process accounting*/
void do_timer(struct thread_regs *thread_regs) {
  if(USER_MODE(thread_regs) ) {
    ++current->utime;
  }
  else ++current->stime; /*interrupting in sys mode*/
  if(--current->counter <= 0 ) {
    current->counter = 0; 
    current->need_resched = 1; /*reschedule this guy*/
  }
  /*mark the tasklet to run*/
  queue_tasklet(TIMER_TASKLET,(tasklet_function_t *)&run_timer);
  mark_tasklet(TIMER_TASKLET);
}
    
     
    
     
