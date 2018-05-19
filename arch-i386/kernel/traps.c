/*   MIR-OS : Exception Handling
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
;    This file provides the C interface for the exceptions.
*/

#include <mir/kernel.h>
#include <mir/sched.h>
#include <asm/traps.h>
#include <asm/irq.h>
#include <asm/string.h>
#include <mir/init.h>

#define DOUBLE_FAULT_EXCEPTION "double_fault_exception"

/*Exceptions with IRQ: 3-5 _can_ be invoked by any of the ones below:
*/

static struct exception_table exception_table[MAX_EXCEPTIONS+1] = 
{
  {"divide",ASM_EXCEPTION_NAME(divide),0 },
  {"debug", ASM_EXCEPTION_NAME(debug) ,1 },
  {"nmi",   ASM_EXCEPTION_NAME(nmi) ,  2},
  {"int3",  ASM_EXCEPTION_NAME(int3),  3 },
  {"overflow",ASM_EXCEPTION_NAME(overflow), 4 },
  {"bounds",  ASM_EXCEPTION_NAME(bounds),5   },
  { "invalid_op",ASM_EXCEPTION_NAME(invalid_op),6 },
  {"device_not_available",ASM_EXCEPTION_NAME(device_not_available),7 },
  {"double_fault",ASM_EXCEPTION_NAME(double_fault),8 },
  {"coprocessor_segment_overrun",ASM_EXCEPTION_NAME(coprocessor_segment_overrun) ,9 },
  {"invalid_tss",ASM_EXCEPTION_NAME(invalid_tss),10 },
  {"segment_not_present",ASM_EXCEPTION_NAME(segment_not_present),11 },
  {"stack_segment",ASM_EXCEPTION_NAME(stack_segment) ,12 },
  {"general_protection",ASM_EXCEPTION_NAME(general_protection),13 },
  {"page_fault",  ASM_EXCEPTION_NAME(page_fault),14 },
  {"reserved",    ASM_EXCEPTION_NAME(reserved) , 15 },
  {"coprocessor", ASM_EXCEPTION_NAME(coprocessor),16 },
  {"alignment_check",ASM_EXCEPTION_NAME(alignment_check),17},
  {"machine_check",  ASM_EXCEPTION_NAME(machine_check),18 },
  {"simd_coprocessor",ASM_EXCEPTION_NAME(simd_coprocessor),19},
  { NULL,         NULL , 0 },
};

#ifdef DEBUG
void dump_regs(struct thread_regs *regs) { 
  printf("ebx=0x%08lx,ecx=0x%08lx,edx=0x%08lx,esi=0x%08lx,edi=0x%08lx,ebp=0x%08lx,eax=0x%08lx,ds=0x%08lx,es=0x%08lx,error_code=0x%08lx,eip=0x%08lx,cs=0x%08lx,eflags=0x%08lx\n",regs->ebx,regs->ecx,regs->edx,regs->esi,regs->edi,regs->ebp,regs->eax,regs->ds,regs->es,regs->error_code,regs->eip,regs->cs,regs->eflags);
}
#else
void dump_regs(struct thread_regs *regs) { }
#endif

static void tackle_exception(struct thread_regs *regs,const char *exception) {
#ifdef DEBUG
  printf("Caught %s for task (%s,%d).Exiting task..\n",exception,current->name,current->pid);
#endif
  dump_regs(regs);

  /*we kill guys without a double fault,coz these exceptions can come
    from certain classes of exceptions,coz those guys are already killed*/

  if(strcmp(exception,DOUBLE_FAULT_EXCEPTION) )
    current->exit_status = REAP_ME;
}

/*Math-EMULATOR: Not handled presently:*/
void handle_math_emulate(struct thread_regs *regs) {
  printf("Math emulator expected.Not supported currently..\n");
  printf("Killing process(%s,%d)\n",current->name,current->pid);
  current->exit_status = REAP_ME;
 }

/*Device not available exception: Restore the math state:*/
void handle_device_not_available(struct thread_regs *regs) { 
    __asm__ __volatile__("clts"); /*clear task switch */

#ifdef DEBUG
    printf("Handling Exception: Device not available:\n");
#endif

    if(process_used_math && process_used_math == current )
       goto out;

    if(process_used_math){
      save_fpu(process_used_math); /*save its fpu state:*/
     }
    if(current->used_math) { 
    /*restore from current processes saved fpu state*/
      restore_fpu(current);
    }
    else {
    current->used_math = 1;
    init_fpu();
   }
  process_used_math = current;
out:;
}

/*define the functions dynamically*/

EXCEPTION_HANDLER(divide)
EXCEPTION_HANDLER(debug)
EXCEPTION_HANDLER(nmi)
EXCEPTION_HANDLER(int3)
EXCEPTION_HANDLER(overflow)
EXCEPTION_HANDLER(bounds)
EXCEPTION_HANDLER(invalid_op)
/*This is handled in a special way:See above.
*/
/*EXCEPTION_HANDLER(device_not_available)*/

EXCEPTION_HANDLER(double_fault)
EXCEPTION_HANDLER(coprocessor_segment_overrun)
EXCEPTION_HANDLER(invalid_tss)
EXCEPTION_HANDLER(segment_not_present)
EXCEPTION_HANDLER(stack_segment)
EXCEPTION_HANDLER(general_protection)
/*page fault would be handled in a different way:-in future
*/
EXCEPTION_HANDLER(page_fault);
EXCEPTION_HANDLER(coprocessor)
EXCEPTION_HANDLER(reserved)
EXCEPTION_HANDLER(alignment_check)
EXCEPTION_HANDLER(machine_check)
EXCEPTION_HANDLER(simd_coprocessor)

/*Initialise the exception gates
*/
int __init traps_init(void) {
  struct exception_table *ptr = NULL; 
  int i;
  for(ptr = exception_table; ptr->name; ++ptr) {
   setidt(ptr->irq,(unsigned long)ptr->handler,TRAP_TYPE,TRAP_DPL);
  }
  for(i = MAX_EXCEPTIONS; i < EXCEPTIONS_RESERVE ; ++i) {
   setidt(i,(unsigned long)&ASM_EXCEPTION_NAME(reserved),TRAP_TYPE,TRAP_DPL);
  }
 printf("EXCEPTION Gate Initialised..\n");
 return 0;
}
  
