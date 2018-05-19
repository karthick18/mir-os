/*   MIR-OS : IRQ handling routines
;    Copyright (C) 2003 A.R.Karthick 
;    <a.r.karthick@gmail.com,a_r_karthic@rediffmail.com>
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
;  Rewrote the irq handling interface to make it controller independent.
;
;   Generic Interrupt Controller function started:
;   Each IRQ line is now wired to an interrupt source delivered to the CPU.
;   The interrupt can be delivered by a standard 8259A controller,
;   or through an APIC. So we dont  let the interrupt line bother about the
;   interrupt source,and let it perform its work in an interrupt controller.
;   independent fashion
;
*/
#include <mir/kernel.h>
#include <mir/sched.h>
#include <asm/irq.h>
#include <asm/irq_stat.h>
#include <asm/page.h>
#include <asm/io.h>
#include <asm/segment.h>
#include <mir/tasklet.h>
#include <mir/init.h>

struct idt_entries _idt_entries[IDT_ENTRIES];
/*The IRQ descriptor*/
struct irq_descriptor irq_descriptor[IDT_ENTRIES];
struct irq_stat irq_stat[IDT_ENTRIES];
struct global_irq_stats global_irq_stats;

/*setup an irq line with an action*/

static int setup_irq(struct irq_action *irq_action) { 
  int irq = irq_action->irq;
  int err = -1;
  int shared = 0;
  if(! LIST_EMPTY ( &irq_descriptor[irq].action_head ) ) {
    struct irq_action *next = list_entry(irq_descriptor[irq].action_head.next,struct irq_action,action_head);
    /*check for SHARED_IRQ: both these guys will have to agree*/
    if(! ( irq_action->flags & next->flags & SHARED_IRQ) ) {
      goto out;
    }
    shared = 1;
  }
  list_add_tail( &irq_action->action_head,&irq_descriptor[irq].action_head);
  if(! shared ) 
    irq_descriptor[irq].depth = 0;
  err = 0;
 out:
  return err;
}

/*request an irq line: This would enable the interrupts on the irq line*/

int request_irq (int irq, irq_action_t *handler,const char *name,
		 int flags, void *device) 
{
  int err =-1;
  struct irq_action *irq_action;
  if(!handler || irq < 0 || irq >= IDT_ENTRIES ) {
    goto out;
  }
  if(!device && (flags & SHARED_IRQ) ) {
    /*warn */
    printf("SHARED IRQS with null device id\n");
  }
 
  if(! (irq_action = slab_alloc(sizeof(struct irq_action),0 ) ) ) {
    printf("Unable to allocate IRQ action for irq (%d)\n",irq);
    goto out;
  }
  irq_action->irq = irq;
  irq_action->action = handler;
  irq_action->flags  = flags;
  irq_action->name = name;
  irq_action->device = device;
  if(setup_irq(irq_action) < 0 ) {
    slab_free((void*)irq_action);
    goto out;
  }
  /*the irq line has been setup successfully:
    Now start or enable the controller for this line*/
  irq_descriptor[irq].flags &= ~(IRQ_PENDING | IRQ_DISABLED | IRQ_IN_PROGRESS);
  irq_descriptor[irq].controller->start(irq); /*start the controller*/
  err = 0;
 out:
  return err;
}

/*Handle the irq actions:
  This would loop through all the actions
  and call all the handlers
*/

static int handle_irq(int irq,struct thread_regs *thread_regs) { 
  struct list_head *traverse;
  struct list_head *head = &irq_descriptor[irq].action_head;
  int flags = 0;
  /*Mark that we are calling the irq handlers*/
  IRQ_ENTER;
  list_for_each(traverse,head ) { 
    struct irq_action *irq_action = list_entry(traverse,struct irq_action,action_head);
    flags |= irq_action->action(irq, irq_action->device,thread_regs);
  }
  IRQ_LEAVE;
  return flags;
}

/*Disable an irq line*/
void disable_irq(int irq) { 
  struct irq_descriptor *irq_desc = &irq_descriptor[irq];
  if(!LIST_EMPTY(&irq_desc->action_head) ) {
  irq_desc->flags = IRQ_DISABLED; /*flag as disabled*/
  irq_desc->controller->disable(irq); /*mask the irq*/
  ++irq_desc->depth;
  }
}

void enable_irq(int irq) { 
  struct irq_descriptor *irq_desc = &irq_descriptor[irq];
  switch(irq_desc->depth) { 

  case 1 :
    /*enable the irq line*/
    irq_desc->flags &= ~IRQ_DISABLED;
    irq_desc->controller->enable(irq); 
    /*fall through*/
  default:
    --irq_desc->depth;
    break;

  case 0 :
    printf("Invalid usage of enable irq without disable irq\n");
    break;

  }
}

/*release the irq line for the device*/
int free_irq(int irq,void *device) { 
  int err =-1;
  struct irq_descriptor *irq_desc = &irq_descriptor[irq];
  struct list_head *traverse,*next;
  struct list_head *head = &irq_desc->action_head;
  if(LIST_EMPTY(head) ) {
    /*seems to be already free:*/
    printf("irq slot %d already free\n",irq);
    goto out;
  }
  for(traverse = head->next; traverse != head ; traverse = next) { 
    struct irq_action *irq_action = list_entry(traverse,struct irq_action, action_head);
    next = traverse->next;
    if( (irq_action->flags & SHARED_IRQ) && !device ) {
      printf("No device ID to identify shared irqs\n");
      goto out;
    }
    if( ( !(irq_action->flags & SHARED_IRQ) ) 
        || 
     ( (irq_action->flags & SHARED_IRQ) && (irq_action->device == device) ) 
     ) {
      /*found the slot to free*/
      list_del(&irq_action->action_head);
      slab_free((void*)device);
      slab_free((void*)irq_action);
      break;
    }
 
  }
  if(LIST_EMPTY(head) ) { 
    irq_desc->flags = IRQ_DISABLED;
    /*disable the irq*/
    irq_desc->controller->disable(irq); 
  }
  err =0;
 out:
  return err;
}
 
/*generic irq handler that calls the various actions
  associated with IRQS:
  We will have the irq number in error_code
  setup by the common irq arch/ specific routine.
*/

void do_irq(struct thread_regs *thread_regs) { 
  int irq = thread_regs->error_code;
  struct irq_descriptor *irq_desc;
  int flags;
  IRQ_INC(irq); /*increment the irq stats*/
  irq_desc = &irq_descriptor[irq];
  flags = irq_desc->flags;
  /*mask the irq and immediately ack the controller*/
  irq_desc->controller->ack(irq);

  flags |= IRQ_PENDING; /*mark it as a pending IRQ to process*/
  
  if( ! (flags & ( IRQ_IN_PROGRESS | IRQ_DISABLED) ) ) { 
    flags &= ~IRQ_PENDING;
    flags |= IRQ_IN_PROGRESS;
  } 
  /*we process pending irqs with 1 level deep:
    Hence a miss can be processed an extra time when enabled
  */
  irq_desc->flags |= flags; 
  if(flags & IRQ_PENDING )
    {
      goto out;
    }

  while(1) {     
    handle_irq(irq, thread_regs);
    if( !( irq_desc->flags & IRQ_PENDING) ) 
      break;
    irq_desc->flags &=~IRQ_PENDING; /*remove the pending IRQ flag*/
  }
  irq_desc->flags &=~IRQ_IN_PROGRESS; /*remove the IRQ in progress flag*/
 out:
  irq_desc->controller->end(irq); /*enable the irq line*/
  /*bottom half or tasklet processing: check for tasklets
   */
  do_tasklets();
}    

void __init init_irq(void) { 
  /*We now use the 8259A controller */
  init_8259A();
  init_irq_8259A();
  tasklets_initialise();
}


static char *irq_names[] = { "PIT" ,"KBD","CASCADE",NULL,NULL,"SOUND",
			     "FDC",NULL,NULL,NULL,NULL,NULL,"PS/2",NULL,
			     "ide0","ide1",
};

#define IRQ_NAMES(irq)  (sizeof(irq)/sizeof(irq[0]) )
static inline int do_irq_stat(int irq) { 
  if(irq < IRQ_NAMES(irq_names) && irq_names[irq] )
    printf("%-8d%-8d%-8s\n",irq,IRQ_COUNT(irq),irq_names[irq] );
  return 0;
}

#define NR_IRQS 0x10
void display_irq_stat(int irq) { 
  printf("%-8s%-8s%-8s\n","IRQ","COUNT","DEVICE");
  if(irq >= 0) 
    do_irq_stat(irq);
  else { 
    int i;
    for(i =0 ; i < NR_IRQS; ++i ) 
      do_irq_stat(i);
  }
}

 
  
