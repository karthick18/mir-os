/*Author:
  A.R.Karthick for MIR:
  Wait queue specifics:
*/
#ifdef __KERNEL__
#ifndef _WAIT_H
#define _WAIT_H

#include <mir/data_str/dlist.h>


struct task_struct; /*forward declaration which is a strange thing in 'C'*/

struct wait_queue_head { 
  struct list_head wait_queue_head;
};

struct wait_queue {
  struct list_head wait_queue_head;
  struct task_struct *task;
};

typedef struct wait_queue_head wait_queue_head_t;
typedef struct wait_queue      wait_queue_t;

#define __INIT_WAIT_QUEUE_HEAD(name) { \
    wait_queue_head: INIT_LIST_HEAD(name.wait_queue_head),\
 }

#define __INIT_WAIT_QUEUE(name,process) { \
wait_queue_head: { NULL,NULL} ,\
task     : process,\
}

#define WAIT_QUEUE_ACTIVE(head) (!LIST_EMPTY(&head->wait_queue_head) )

#define DECLARE_WAIT_QUEUE_HEAD(name) \
struct wait_queue_head name = __INIT_WAIT_QUEUE_HEAD(name)

#define DECLARE_WAIT_QUEUE(name,process) \
struct wait_queue name = __INIT_WAIT_QUEUE(name,process)

static __inline__ void init_waitqueue_head(struct wait_queue_head *head) { 
  list_head_init(&head->wait_queue_head);
}

static __inline__ void init_waitqueue_entry(struct wait_queue *wait_queue,struct task_struct *task) { 
  wait_queue->wait_queue_head.next = wait_queue->wait_queue_head.prev= NULL;
  wait_queue->task = task;
}

extern void __add_waitqueue(struct wait_queue *,struct wait_queue_head *);
static __inline__  void add_waitqueue(struct wait_queue *element,
                                      struct wait_queue_head *head) {
  list_add_tail(&element->wait_queue_head,&head->wait_queue_head);
  __add_waitqueue(element,head); 
}

static __inline__ void remove_waitqueue(struct wait_queue *element) { 
  list_del(&element->wait_queue_head);
}

/*Interruptible sleep and interruptible sleep ons:
  The former would make it TASK_UNINTERRUPTIBLE
  and the latter TASK_INTERRUPTIBLE
*/

extern void __interruptible_sleep(struct wait_queue_head *,byte);

static  __inline__ void interruptible_sleep_on(struct wait_queue_head *head) {
  __interruptible_sleep(head,(byte)TASK_INTERRUPTIBLE);
  return ;
}

static __inline__ void interruptible_sleep(struct wait_queue_head *head) {
  __interruptible_sleep(head,(byte)TASK_UNINTERRUPTIBLE);
  return ;
}

extern void __wake_up(struct wait_queue_head *head,byte status);

static __inline__ void wake_up_interruptible(struct wait_queue_head *head) { 
  __wake_up(head,TASK_INTERRUPTIBLE);
}

static __inline__ void wake_up(struct wait_queue_head *head) {
  __wake_up(head,TASK_UNINTERRUPTIBLE); /*also wakes up INTERRUPTIBLE*/
}

#endif
#endif
