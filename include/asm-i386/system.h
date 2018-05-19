/*Author: A.R.Karthick for MIR:
  We dont use pushad and popa any more.
  Thats because in future,we would like to 
  directly modify the values in the stack.
  Hence we define a prescribed stack state.
  The stack state at the time of interrupts 
  is the one that is reflected below by the structure,
  thread_regs.
  thread_struct contains some context switch specifics:
  fs,gs,es,ds remain the defaults- __KERNEL_DS a.k.a 0x10:
*/

#ifdef __KERNEL__
#ifndef _SYSTEM_H_
#define _SYSTEM_H_


#define FASTCALL(fun)    fun __attribute__ ( (regparm(3)) )
#define FASTCALL_DEFINE(fun) __attribute__( (regparm(3)) ) fun

#include <asm/thread.h>

#define load_debugreg(thread,reg) do {\
__asm__ __volatile__ ("movl %0,%%db" #reg "\n\t" \
                      : :"r"(thread->debugreg[reg]) \
                     );\
}while(0)

struct task_struct ;

extern void FASTCALL(__switch_to(struct task_struct *next,struct task_struct *prev) );

/*This does the actual context switching work:
  We modify the EIP to point to the call after the scheduler for the previous task and save it. The next tasks EIP is the one that is contained in its context and would point to ret_from_trap for first time calls to processes created,and for others- label 1: see-below
 __switch_to is a fastcall. Hence load ax,dx 

*/

#define switch_to(next,prev) do { \
  __asm__ __volatile__("pushl %%esi\n\t" \
                       "push %%edi\n\t" \
                       "push %%ebp\n\t" \
                       "movl %%esp,%0\n\t" \
                       "movl $1f,%1\n\t"   \
                       "movl %2,%%esp\n\t" \
                       "pushl %3\n\t" \
                       "jmp __switch_to\n\t" \
                       "1:\n\t" \
                       "popl %%ebp\n\t" \
                       "popl %%edi\n\t" \
                       "popl %%esi\n\t" \
                       :"=m"((prev)->thread_struct.esp),\
                        "=m" ((prev)->thread_struct.eip)\
                       :"m"((next)->thread_struct.esp),\
                        "m"((next)->thread_struct.eip),\
                        "a"((next)),"d"((prev)) \
                        );\
 }while(0)

#endif
#endif
