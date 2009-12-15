/*   MIR-OS : Tasklet routines or Bottom Half handling
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
;    Tasklets are called currently from ret_from_intr path:
;    The interrupt handler needs to be fast in processing the interrupt
;    coz it runs  with the interrupt disabled.Hence one should do minimum requi;    red processing in the interrupt handler and postpone the other half
;    into bottom halves which can run with interrupts enabled
;   
*/

#include <mir/kernel.h>
#include <mir/sched.h>
#include <mir/init.h>
#include <mir/tasklet.h>

#define SET_TASKLET(bit) do  { \
set_bit(bit,(void*)&tasklet_ctrl_block.mask);\
}while(0)

#define SET_TYPE0_TASKLET   SET_TASKLET(TASKLET_TYPE0)
#define SET_TYPE1_TASKLET   SET_TASKLET(TASKLET_TYPE1)

struct tasklet_ctrl_block {   
  int mask; /*contains the bitmasks of pending tasklets to delivered:
	      Each points to a list head index:
  	    */
};

/*pre-defined list of tasklets*/
struct tasklet_struct tasklet_structs[TASKLET_STRUCTS];
static struct list_head list_head[TASKLET_MAXTYPES]; /*list heads of TASKLETS*/

struct global_tasklet_stats global_tasklet_stats;
static struct tasklet_ctrl_block tasklet_ctrl_block;

static tasklet_function_t tasklet_execute_type0, tasklet_execute_type1;
static tasklet_function_t *tasklets[TASKLET_MAXTYPES] = {
  tasklet_execute_type0 , tasklet_execute_type1
};

/*Execute the tasklets based on the mask:
  Each bit in the mask will call a corresponding action routine
*/
int do_tasklets(void) { 
  int enabled,remaining ;
  unsigned long flags;
  if(in_interrupt() ) 
    return 0; /*tasklets are serialized and shouldnt be called from irq context*/
  save_flags_irq(flags);
  enabled = tasklet_ctrl_block.mask;
  tasklet_ctrl_block.mask = 0; /*clear off the mask*/
  if ( enabled ) { 
    int i;
    remaining = ~enabled; 
    TASKLET_DISABLE; /*disable the calls to tasklets*/
  restart:   
    restore_flags_irq(flags); /*re-enable interrupts*/
    for(i = 0;i < TASKLET_MAXTYPES;++i) {
      if( enabled & ( 1 << i ) ) {
	tasklets[i]( (void*) i ) ;
      }
    }
    save_flags_irq(flags);
    enabled = tasklet_ctrl_block.mask;
    if(remaining & enabled) { 
      /*someone has queued up a tasklet midway,start execution again*/
      remaining &= ~enabled; 
      tasklet_ctrl_block.mask = 0;
      goto restart;
    }
    TASKLET_ENABLE; /*re-enable tasklets*/
  }
  restore_flags_irq(flags);
  return 0;
}

int tasklet_schedule0(struct tasklet_struct *tasklet) {  
  struct list_head *head ;
  unsigned long flags;
  tasklet->active = 1;
  save_flags_irq(flags);
  head = &list_head[TASKLET_TYPE0];
  list_add_tail(&tasklet->list_head,head);
  SET_TYPE0_TASKLET; 
  restore_flags_irq(flags);
  return 0;
}
int tasklet_schedule1(struct tasklet_struct *tasklet) { 
  struct list_head *head ;
  unsigned long flags;
  tasklet->active = 1;
  save_flags_irq(flags);
  head = &list_head[TASKLET_TYPE1];
  list_add_tail(&tasklet->list_head,head);
  SET_TYPE1_TASKLET;
  restore_flags_irq(flags);
  return 0;
}

static __inline__ void set_tasklet(int type) { 
  unsigned long flags;
  save_flags_irq(flags);
  switch(type) { 
  case TASKLET_TYPE0:
    SET_TYPE0_TASKLET;
    break;
  case TASKLET_TYPE1:
    SET_TYPE1_TASKLET;
    break;
  default:;
  }
  restore_flags_irq(flags);
}

/*Execute the tasklets*/
static void execute_tasklets(void *data,int type) {
  struct list_head *head ;
  struct list_head list,*list_ptr = &list;
  struct list_head *first ,*traverse,*next;
  unsigned long flags;
  save_flags_irq(flags);
  head = &list_head[type];
  first = head->next;
  list_del_init(head);
  list_add_tail(list_ptr,first);
  restore_flags_irq(flags);
  /*remove all the elements from the tasklet and reinit*/
  for( traverse = list_ptr->next; traverse != list_ptr; traverse = next) {
    struct tasklet_struct *tasklet = list_entry(traverse,struct tasklet_struct,list_head);
    next = traverse->next;
    /*see if the tasklet is active*/
    if(tasklet->active) {
      tasklet->active = 0;
      /*run that tasklet*/
      tasklet->routine(tasklet->data); 
    }
    else { 
      /*reschedule*/
      list_add_tail(&tasklet->list_head,head);
      set_tasklet(type);
    }
  }
}

static void tasklet_execute_type0(void *data) {
  execute_tasklets(data,TASKLET_TYPE0); 
}
static void tasklet_execute_type1(void *data) {
  execute_tasklets(data,TASKLET_TYPE1);
}

int disable_tasklet(struct tasklet_struct *tasklet) {
  tasklet->active = 0;
  return 0;
}

int enable_tasklet(struct tasklet_struct *tasklet) {
  tasklet->active = 1;
  return 0;
}

/*Initialise the predefined set of tasklets*/
static tasklet_function_t tasklet_action;

static void setup_tasklets(void) {
  int i;
  for(i = 0; i < TASKLET_STRUCTS; ++i ) {
    struct tasklet_struct *tasklet = tasklet_structs + i;
    tasklet->routine = &tasklet_action;
    tasklet->data = (void *)i;
    tasklet->active  = 0;
  }
}

tasklet_function_t *tasklet_actions[TASKLET_STRUCTS];

/*This should run the tasklet or BH when marked:
  Used for predefined set of BHs or bottom halves or tasklets
*/

static void tasklet_action ( void *data ) { 
  int nr = (int ) data;
  if(tasklet_actions[nr] ) { 
    /*If there is a routine marked: run it*/
    tasklet_actions[nr] ( data );
    tasklet_actions[nr] = 0;
  }
}

int __mark_tasklet(int nr) { 
  return tasklet_schedule1(tasklet_structs + nr );
}

int queue_tasklet(int nr, tasklet_function_t *routine) { 
  int err = -1;
  if( nr < 0 || nr >= TASKLET_STRUCTS)
    goto out;
  err = 0;
  tasklet_actions[nr] = routine;
 out:
  return err;
}

int __init tasklets_initialise(void) { 
  int i;
  for(i = 0; i < TASKLET_MAXTYPES;++i)
    list_head_init(&list_head[i]);
  setup_tasklets();
  return 0;
}
