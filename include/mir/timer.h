/*A.R.Karthick for MIR:
  The definition of the timer list:
  We have a list of timers that can be added.
  A timer handler is called when the timer expires, 
  and the timer is deleted from the list.
  A process can go on a timeout schedule with this usage,
  apart from alarm expiry:
  The timers are sorted before adding:
*/

#ifdef __KERNEL__
#ifndef _TIMER_H
#define _TIMER_H

#define MAX_TIMEOUT      ( (~0UL) >> 1)

#include <mir/data_str/dlist.h>
#include <asm/string.h>
#include <asm/irq.h>

typedef unsigned long ( *timer_handler ) (void *arg) ;

struct timer_list {
  struct list_head timer_head;
  unsigned long timeout;/*This expiry is in ticks(jiffies + timeout)*/
  timer_handler handler; /*this is the handler called on timeout*/
  void    *arg; /*argument to the handler*/  
  /*the compare routine for sorted adds to list_heads*/
  int (*compare) (struct timer_list * ,struct timer_list*); 
};    

/*INitialise a timer*/
extern void __init_timer(struct timer_list *);

static __inline__ void init_timer(struct timer_list *timer) { 
  memset(timer,0x0,sizeof(*timer) );
  __init_timer(timer);
}

extern int __add_timer(struct timer_list *);
extern void __del_timer(struct timer_list *);
extern int setup_timer(struct timer_list *,unsigned long,timer_handler,void*);
extern int setup_timers(void);

/*Delete a timer*/
static __inline__ void del_timer(struct timer_list *timer) { 
  unsigned long flags;
  save_flags_irq(flags);
  __del_timer(timer);
  restore_flags_irq(flags);
}
/*Add a timer*/
static __inline__ int add_timer(struct timer_list *timer) { 
  unsigned long flags;
  int status;
  save_flags_irq(flags);
  status =  __add_timer(timer);
  restore_flags_irq(flags);
  return status;
}

#endif
#endif
