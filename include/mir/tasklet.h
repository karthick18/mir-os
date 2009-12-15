/*Author: A.R.Karthick for MIR*/
#ifdef __KERNEL__
#ifndef _TASKLET_H
#define _TASKLET_H

/*Define the different types of tasklets
*/
#define TASKLET_TYPE0 0 /*high priority,executed first*/
#define TASKLET_TYPE1 1 /*low ones,executed after the above*/
#define TASKLET_MAXTYPES 0x2

/*32 pre-defined tasklets can be possible*/
#define TASKLET_STRUCTS 0x20

#include <asm/bitops.h>
#include <asm/irq.h>
#include <asm/irq_stat.h>

typedef void (tasklet_function_t) ( void * );
/*define the tasklet structure*/
struct tasklet_struct { 
  struct list_head list_head;
  int active; /*1 is active and 0 is inactive*/
  tasklet_function_t *routine;
  void *data;
};

/*pre-defined list of tasklets*/
extern struct tasklet_struct tasklet_structs[];

extern int tasklet_schedule0(struct tasklet_struct *tasklet);
extern int tasklet_schedule1(struct tasklet_struct *tasklet);
/*schedule a tasklet of a particular type*/
static __inline__ int tasklet_schedule(struct tasklet_struct *tasklet,int type) {
  int err = -1;
  if( type < 0 || type >= TASKLET_MAXTYPES)
    goto out;
  switch(type) { 
  case 0 : 
    err = tasklet_schedule0(tasklet);
    break;
  case 1:
    err = tasklet_schedule1(tasklet);
    break;
  default:;
  }
 out:
  return err;
}

/*predefined types of tasklets*/
#define TIMER_TASKLET 0x1

extern int queue_tasklet(int nr, tasklet_function_t *routine);
extern int __mark_tasklet(int nr);
/*schedule a predefined tasklet*/
static __inline__ int mark_tasklet(int nr) {
  int err = -1;
  if( nr < 0 || nr >= TASKLET_STRUCTS) 
    goto out;
  err = __mark_tasklet(nr);
 out:
  return err;
}
/*unmark a given tasklet:
  just disable the tasklet
*/
extern int disable_tasklet(struct tasklet_struct *);
static __inline__ int unmark_tasklet(int nr) { 
  int err = -1;
  if( nr < 0 || nr >= TASKLET_STRUCTS) 
    goto out;
  err = disable_tasklet(tasklet_structs + nr);
 out:
  return err;
}
extern int enable_tasklet(struct tasklet_struct *);
static __inline__ int remark_tasklet(int nr) {
  int err =-1;
  if( nr < 0 || nr >= TASKLET_STRUCTS) 
    goto out;
  err = enable_tasklet(tasklet_structs + nr);
 out:
  return err;
}
extern int do_tasklets(void);
extern int tasklets_initialise(void);

#endif
#endif
