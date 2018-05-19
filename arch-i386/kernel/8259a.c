/*   MIR-OS : 8259A controller functions
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
#include <mir/floppy.h>
#include <asm/segment.h>
#include <asm/irq.h>
#include <asm/irq_stat.h>
#include <asm/irq_handlers.h>
#include <asm/io.h>

#define NR_IRQS  0x16
#define BASE_IRQ 0x20

/*define the interrupt controller callbacks*/

/*start the intr. controller*/

/*this mask contains both the mask of the master and the slave*/
int interrupt_mask = 0xFFFF;

/*master is in the first 8 and slave in the next 8 bits*/
#define master_mask  ( (unsigned char *)&interrupt_mask )[0]
#define slave_mask   ( (unsigned char *)&interrupt_mask )[1]

/*Define the common interrupt handler that invokes the do_irq handler:
  This is called by each of the interrupt handlers
*/
DEFINE_COMMON_INTR;
#define MAKE_IRQ(x,y) \
   MAKE_INTR(x##y)

#define MAKE_16_IRQS(x) \
 MAKE_IRQ(x,0) ; MAKE_IRQ(x,1); MAKE_IRQ(x,2);MAKE_IRQ(x,3);\
 MAKE_IRQ(x,4);  MAKE_IRQ(x,5); MAKE_IRQ(x,6);MAKE_IRQ(x,7);\
 MAKE_IRQ(x,8);  MAKE_IRQ(x,9); MAKE_IRQ(x,A);MAKE_IRQ(x,B);\
 MAKE_IRQ(x,C);  MAKE_IRQ(x,D); MAKE_IRQ(x,E);MAKE_IRQ(x,F)

MAKE_16_IRQS(0x0);

#undef MAKE_INTR
#undef DEFINE_COMMON_INTR

#define MAKE_IRQ_ROUTINE(x)  MAKE_INTR_ROUTINE(0x0##x)

intr_handler_t *interrupt_handlers[IDT_ENTRIES] = {
  MAKE_IRQ_ROUTINE(0), MAKE_IRQ_ROUTINE(1),MAKE_IRQ_ROUTINE(2),MAKE_IRQ_ROUTINE(3) ,
  MAKE_IRQ_ROUTINE(4),MAKE_IRQ_ROUTINE(5),MAKE_IRQ_ROUTINE(6),MAKE_IRQ_ROUTINE(7),
  MAKE_IRQ_ROUTINE(8),MAKE_IRQ_ROUTINE(9),MAKE_IRQ_ROUTINE(A),MAKE_IRQ_ROUTINE(B),
  MAKE_IRQ_ROUTINE(C),MAKE_IRQ_ROUTINE(D),MAKE_IRQ_ROUTINE(E),MAKE_IRQ_ROUTINE(F),
  [0x10 ... IDT_ENTRIES-1] = NULL,
};
  
static void enable_8259A(int irq);
static void disable_8259A(int irq);

static void start_8259A(int irq) { 
  enable_8259A(irq); /*enable the irq line*/
}

static void shutdown_8259A(int irq) { 
  disable_8259A(irq);
}

/*enable the irq line*/
static void enable_8259A(int irq) { 
  int mask = ~(1 << irq);
  interrupt_mask &= mask; 
  if( irq < 8 ) { 
    /*enable the line of the master*/
    outb(0x21, master_mask );
  } else { 
    outb(0xA1, slave_mask);
  }
}

void enable_interrupt(int irq) { 
  enable_8259A(irq);
}

static void disable_8259A(int irq) { 
  int mask = ( 1 << irq );
  interrupt_mask |= mask;
  if( irq < 8 ) {
    outb(0x21, master_mask);
  } else {
    outb(0xA1, slave_mask );
  }
}

/*Check for spurious interrupts that can happen in IRQ 7 and IRQ 15 in master
  and slave respectively.
  Check for the ISR, to see if its serviced.
*/
static int verify_spurious(int irq) { 
  int mask = ( 1 << irq );
  int val;
  if( irq < 8 ) { 
    outb(0x20,0x0B); /*fetch ISR*/
    val = inb(0x20);
    outb(0x20,0x0A); /*reset the R pulse to return IRR*/
    return val & mask;
  }
  outb(0xA0,0x0B);
  val = inb(0xA0);
  outb(0xA0,0x0A);
  return val & (mask >> 8);
}

/*First mask and ACK the 8259A controller:
  Note the order of EOIs to the controller.
  In the case of slave, also send an EOI to the masters irq 2 
*/
static void ack_8259A(int irq ) { 
  int mask = 1 << irq;
  if( interrupt_mask & mask ) {
    /*the interrupt is blocked,but even then its delivered.
      check for spurious interrupts*/
    goto check_spurious;
  }
  interrupt_mask |= mask; /*mask this irq*/
  /*Now ack the interrupt to the controller*/
 handle_irq:
  if( irq < 8 ) { 
    inb(0x21);/*this should delay:*/
    outb(0x21, master_mask);
    outb(0x20, 0x60 + irq ); /*ack the irq */
    goto out;
  }
  inb(0xA1); 
  outb(0xA1,slave_mask); 
  outb(0xA0,0x60 + (irq & 7 ) );
  outb(0x20,0x60 + 2); /*ACK masters irq line 2*/
  goto out;

 check_spurious:
  if( verify_spurious(irq) ) { 
    /*Boss!! this guy is currently being processed*/
    goto handle_irq;
  } else { 
    /*caught a spurious interrupt: Just let the user know about it:
      CRY loudly for each spurious interrupt caught
    */
    static int spurious_mask;
    if(! (spurious_mask & mask ) ) {
      printf("Caught Spurious interrupt (%d)\n", irq );
      spurious_mask |= mask;
    }
    /*we can ack this to the controller:Doesnt harm in anyway*/
    IRQ_ERR_INC; /*increment the IRQ error count*/
    goto handle_irq;
  }
 out:
  return ;
}  

static void end_8259A(int irq) { 
  if ( ! ( irq_descriptor[irq].flags & (IRQ_DISABLED | IRQ_IN_PROGRESS ) ) ) 
    enable_8259A(irq);
}

/*Define the interrupt controller abstraction interface for 8259A*/

struct interrupt_controller controller_8259A = { 
  start : start_8259A ,
  shutdown : shutdown_8259A,
  enable   : enable_8259A,
  disable  : disable_8259A,
  ack      : ack_8259A,
  end      : end_8259A,
};

/*Initialise the 8259A interrupt controller
*/

void init_8259A(void) { 
  /*Mask all interrupts*/
  outb(0x21,0xff);
  outb(0xA1,0xff);
 
  /*ICW1 for master and slave of 8259A*/
  outb_p(0x20,0x11);
  /*ICW2 remap irq line 0 - 7 to 0x20 to 0x27 in the master*/
  outb_p(0x21,0x20 );
  outb_p(0x21,0x4) ; /*tell master that it has a slave on IRQ 2*/
  /*we dont support auto_eoi*/
  outb_p(0x21,0x1); /*normal EOI for master*/
  
  /*Program the slave*/
  outb_p(0xA0,0x11); /*ICW1 INIT for slave*/
  outb_p(0xA1,0x20 + 8); /*remap 8-15 to 0x28-0x2f*/
  outb_p(0xA1,0x2); /*its a slave on masters IRQ 2*/
  outb_p(0xA1,0x1); /*no auto-eoi*/

  /*restore the master and the slave mask*/
  outb(0x21,master_mask);
  outb(0xA1,slave_mask);
}  

static void start_default(int irq) { }
static void shutdown_default(int irq) { }
static void enable_default(int irq) { }
static void disable_default(int irq) { }
static void ack_default (int irq) { }
static void end_default (int irq) { }

struct interrupt_controller default_controller = {
  start : start_default,
  shutdown : shutdown_default,
  enable : enable_default,
  disable : disable_default,
  ack : ack_default,
  end : end_default,
};

/*Initialise the IRQ descriptors for the 8259A line*/

void init_irq_8259A(void) { 
  int i;
  for( i = 0 ; i < IDT_ENTRIES; ++i ) { 
    struct irq_descriptor *irq  = irq_descriptor + i;
    irq->irq = i;
    irq->flags = IRQ_DISABLED; 
    list_head_init(&irq->action_head); /*initialise action list*/
    if(i < NR_IRQS) {
      irq->controller = &controller_8259A;
#if 1
      if(i != FDC_IRQ) 
#endif
       setidt(i+BASE_IRQ,(unsigned long)interrupt_handlers[i],INTR_TYPE,INTR_DPL);
    } else 
      irq->controller = &default_controller;
    irq->depth = 1; /*default for SHARED_IRQ*/
  }
}
    
