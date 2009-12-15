/*Author: Pratap:
  A.R.Karthick :Reorganised this file.
  Added some setidt specific macros:
 */
#ifndef _IRQ_H
#ifdef __KERNEL__
#define _IRQ_H

#include <mir/types.h>
#include <asm/segment.h>
#include <mir/data_str/dlist.h>
#include <asm/thread.h>

/*generic interrupt controller routines*/
struct interrupt_controller { 
  void (*start) (int irq) ; /*enable the irq line,basically*/
  void (*shutdown)(int irq); /*basically disable the irq line*/
  void (*enable) (int irq);
  void (*disable) (int irq);
  void (*ack)( int irq); /*ack is mask and ack*/
  void (*end) (int irq ); 
};

/*Default controller takes over for IRQS above 15 for 8259A especially*/

extern struct interrupt_controller default_controller;

/*There is a per IRQ action structure,coz IRQs can be shared.
  Hence each irq descriptor has an action associated with it
*/

typedef int (irq_action_t)(int irq, void *device,struct thread_regs *thread_regs);
typedef enum {
  SHARED_IRQ = 0x0,
  INTERRUPT_IRQ = 0x1,
} irq_type;

struct irq_action {  
  struct list_head action_head;
  irq_action_t *action; /*callback*/
  irq_type flags; /*action flags*/
  const char *name; /*description of the irq handler*/
  void *device; /*the device it belongs to*/
  int irq; /*the irq line it belongs to : */
};

typedef enum {
  IRQ_IN_PROGRESS = 0x1,
  IRQ_PENDING = 0x2 ,
  IRQ_DISABLED = 0x4,
} irq_flags;

/*The irq descriptor struct per irq */
struct irq_descriptor { 
  struct list_head action_head ; /*head of actions*/
  int irq;
  irq_flags flags; /*irq flags*/
  int depth; /*depth for disable and enable*/
  struct interrupt_controller *controller; /*the controller*/
};

extern struct irq_descriptor irq_descriptor[];

#define iret()         __asm__ __volatile__ ("iret"::)
#define cli()          __asm__ __volatile__ ("cli"::)
#define sti()          __asm__ __volatile__ ("sti"::)

#define save_flags(flags) do { \
__asm__ __volatile__("pushfl;popl %0\n\t" :"=g"(flags) : );\
}while(0)

#define restore_flags(flags) do {\
__asm__ __volatile__("pushl %0; popfl\n\t" : :"g"(flags) : "memory" );\
}while(0)

#define save_flags_irq(flags) do { \
__asm__ __volatile__("pushfl; popl %0;cli\n\t" :"=g"(flags) );\
}while(0)

#define restore_flags_irq(flags) restore_flags(flags)

/*Define the interfaces for setidt*/
#define INTR_TYPE       0xE
#define TRAP_TYPE       0xE
#define SYS_TYPE        0xE

#define INTR_DPL        0x0
#define TRAP_DPL        0x0
#define SYS_DPL         0x3

extern void enable_irq(int irq);
extern void disable_irq(int irq);
extern void kbd_init();
extern void timer_init();
extern int request_irq(int ,irq_action_t *,const char *,int ,void *);
extern int free_irq(int irq,void *device);
extern void ret_from_intr(void);
extern void do_irq(struct thread_regs *);
extern void init_irq(void);
extern void init_irq_8259A(void);
extern void init_8259A(void);
extern void enable_interrupt(int irq);
extern void display_irq_stat(int irq);
#endif
#endif
