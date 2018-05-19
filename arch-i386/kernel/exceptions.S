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
;    This file provides the asm interface for the exceptions.
;    The C interface is in traps.c
*/

.text

#include <asm/thread.h>
#include <asm/segment.h>
#include <asm/traps.h>

/*Save all the registers into the stack:
  The stack state is the same as defined in
  asm/thread.h
*/
.extern test_reschedule,schedule,floppy_interrupt_handler;

#define SAVE_ALL \
pushl %es;\
pushl %ds;\
pushl %eax;\
pushl %ebp;\
pushl %edi;\
pushl %esi;\
pushl %edx;\
pushl %ecx;\
pushl %ebx

#define RESTORE_ALL \
popl %ebx;\
popl %ecx;\
popl %edx;\
popl %esi;\
popl %edi;\
popl %ebp;\
popl %eax;\
popl %ds;\
popl %es;\
addl $0x8,%esp;\
iret


.globl math_emulate,EXCEPTION_NAME(math_emulate),\
       handle_exception,ASM_EXCEPTION_NAME(divide),EXCEPTION_NAME(divide),\
       ASM_EXCEPTION_NAME(debug),EXCEPTION_NAME(debug),\
       ASM_EXCEPTION_NAME(nmi)  ,EXCEPTION_NAME(nmi)  ,\
       ASM_EXCEPTION_NAME(int3), EXCEPTION_NAME(int3) ,\
       ASM_EXCEPTION_NAME(overflow),EXCEPTION_NAME(overflow),\
       ASM_EXCEPTION_NAME(bounds), EXCEPTION_NAME(bounds),\
       ASM_EXCEPTION_NAME(invalid_op),EXCEPTION_NAME(invalid_op),\
       ASM_EXCEPTION_NAME(device_not_available), EXCEPTION_NAME(device_not_available),\
       ASM_EXCEPTION_NAME(double_fault),EXCEPTION_NAME(double_fault),\
       ASM_EXCEPTION_NAME(coprocessor_segment_overrun),EXCEPTION_NAME(coprocessor_segment_overrun),\
       ASM_EXCEPTION_NAME(invalid_tss),EXCEPTION_NAME(invalid_tss),\
       ASM_EXCEPTION_NAME(segment_not_present),EXCEPTION_NAME(segment_not_present),\
       ASM_EXCEPTION_NAME(stack_segment),EXCEPTION_NAME(stack_segment),\
       ASM_EXCEPTION_NAME(general_protection),EXCEPTION_NAME(general_protection),\
       ASM_EXCEPTION_NAME(page_fault),EXCEPTION_NAME(page_fault),\
       ASM_EXCEPTION_NAME(coprocessor),EXCEPTION_NAME(coprocessor),\
       ASM_EXCEPTION_NAME(reserved)  ,EXCEPTION_NAME(reserved),\
       ASM_EXCEPTION_NAME(alignment_check),EXCEPTION_NAME(alignment_check),\
       ASM_EXCEPTION_NAME(machine_check),  EXCEPTION_NAME(machine_check),\
       ASM_EXCEPTION_NAME(simd_coprocessor),EXCEPTION_NAME(simd_coprocessor),\
       ret_from_intr,ret_from_exception,ret_from_schedule,floppy_interrupt,__schedule

#define RESCHED 173

.align 2;
	
restore_all:
	RESTORE_ALL;

ret_from_intr:
ret_from_exception:
ret_from_schedule:
    cli				; 
    call test_reschedule;
    cmpl $0,%eax;
    je restore_all;
    call schedule;
    jmp ret_from_schedule;

.align 2;

handle_exception:
 SAVE_ALL;
 movl $__KERNEL_DS,%edx;
 movl %edx,%ds; 
 movl %edx,%es;
 movl %edx,%fs;
 movl %edx,%gs;
 movl %edx,%ss;
 movl ORIG_EAX(%esp),%eax;  /*load the function pointer*/
 lea (%esp),%edx; /*load the stack pointer*/
 pushl %edx;
 call *%eax; /*call the exception routine*/  
 addl $4,%esp;
 jmp ret_from_exception



/*The exceptions below dont have error codes associated with them.
  Hence use 0:
*/

.align 2

ASM_EXCEPTION_NAME(divide):
 pushl $0x0; 
 pushl $EXCEPTION_NAME(divide); /*the fun. pointer*/
 jmp handle_exception;

.align 2

ASM_EXCEPTION_NAME(debug):
  pushl $0x0;
  pushl $EXCEPTION_NAME(debug);
  jmp handle_exception;

.align 2

ASM_EXCEPTION_NAME(nmi) :
  pushl $0x0;
  pushl $EXCEPTION_NAME(nmi);
  jmp handle_exception;


.align 2

ASM_EXCEPTION_NAME(int3) :
  pushl $0x0;
  pushl $EXCEPTION_NAME(int3);
  jmp handle_exception;


.align 2

ASM_EXCEPTION_NAME(overflow) :
  pushl $0x0;
  pushl $EXCEPTION_NAME(overflow);
  jmp handle_exception;

.align 2

ASM_EXCEPTION_NAME(bounds) :
  pushl $0x0;
  pushl $EXCEPTION_NAME(bounds);
  jmp handle_exception;

.align 2

ASM_EXCEPTION_NAME(invalid_op) :
  pushl $0x0;
  pushl $EXCEPTION_NAME(invalid_op);
  jmp handle_exception;


.align 2

/*If we have to emulate math. coprocessor,we leave it unhandled,so
  that it kills the process using it.
*/

ASM_EXCEPTION_NAME(device_not_available) :
  pushl $0x0;
  pushl $EXCEPTION_NAME(device_not_available);
  SAVE_ALL;
  movl $__KERNEL_DS,%edx;
  movl %edx,%ds;
  movl %edx,%es; 
  movl %edx,%fs;
  movl %edx,%gs;
  movl %edx,%ss;
  lea (%esp),%edx; /*load the stack pointer*/
  pushl %edx;
  xorl %eax,%eax;
  movl %cr0,%eax;
  testl $0x4,%eax; /*test for the MATH Emulator BIT*/
  jne math_emulate;
  /*Restore the math state if present*/
  movl EIP(%esp),%eax; /*function pointer*/
  call *%eax; 
  addl $0x4,%esp;
  jmp ret_from_exception;

/*Not yet handled*/
math_emulate:
  call EXCEPTION_NAME(math_emulate);
  addl $0x4,%esp;
  jmp ret_from_exception;

  
.align 2

ASM_EXCEPTION_NAME(coprocessor_segment_overrun):
  pushl $0x0;
  pushl $EXCEPTION_NAME(coprocessor_segment_overrun);
  jmp handle_exception;

.align 2

 ASM_EXCEPTION_NAME(coprocessor) :
  pushl $0x0;
  pushl $EXCEPTION_NAME(coprocessor);
  jmp handle_exception;

.align 2

ASM_EXCEPTION_NAME(reserved) :
  pushl $0x0;
  pushl $EXCEPTION_NAME(reserved);
  jmp handle_exception;

.align 2

ASM_EXCEPTION_NAME(alignment_check) :
   pushl $0x0;
   pushl $EXCEPTION_NAME(alignment_check);
   jmp handle_exception;

.align 2

ASM_EXCEPTION_NAME(machine_check) :
  pushl $0x0;
  pushl $EXCEPTION_NAME(machine_check);
  jmp handle_exception;

.align 2

ASM_EXCEPTION_NAME(simd_coprocessor) :
   pushl $0x0;
   pushl $EXCEPTION_NAME(simd_coprocessor);
   jmp handle_exception;


/*The exceptions below have error codes associated with them:
  Hence we dont pushl $0x0:
*/

.align 2

ASM_EXCEPTION_NAME(double_fault) :
    pushl $EXCEPTION_NAME(double_fault);
    jmp handle_exception;

.align 2

ASM_EXCEPTION_NAME(invalid_tss) :
  pushl $EXCEPTION_NAME(invalid_tss);
  jmp handle_exception;

.align 2

ASM_EXCEPTION_NAME(segment_not_present):
  pushl $EXCEPTION_NAME(segment_not_present);
  jmp handle_exception;

.align 2

ASM_EXCEPTION_NAME(stack_segment):
  pushl $EXCEPTION_NAME(stack_segment);
  jmp handle_exception; 

.align 2

ASM_EXCEPTION_NAME(general_protection):
  pushl $EXCEPTION_NAME(general_protection);
  jmp handle_exception;

.align 2

ASM_EXCEPTION_NAME(page_fault) :
  pushl $EXCEPTION_NAME(page_fault);
  jmp handle_exception;

.align 2;
floppy_interrupt:	
  pushl $0x6;
  pushl %eax;
  SAVE_ALL;
  movl $__KERNEL_DS,%eax;
  movl %eax,%ds;
  movl %eax,%es;	
  movl %eax,%ss;
  movl %eax,%fs;
  movl %eax,%gs;
  movl %esp,%eax;
  pushl %eax;
  call floppy_interrupt_handler;
  addl $4,%esp;
  movb $0x20,%al;
  outb %al,$0x20;
  RESTORE_ALL;                

/*Does a virtual context switch as we wont entering the above
  ret_from_schedule path for schedules called from kernel space.
  This would make the scheduler to be invoked with interrupts ON.
  So we are just trying to save the SCHEDULER semantics of context switching,
  by playing some minor games by saving and restoring stack contexts
*/		 	
__schedule: /*we already have saved EIP through a call from the caller*/
	pushfl			;
	pushl $__KERNEL_CS	;
	pushl $1f		;
	SAVE_ALL		;
	cli			;
	call schedule 		;
	popl %ebx		;
	popl %ecx 		;
	popl %edx		;
	popl %esi		;
	popl %edi		;
	popl %ebp		;
	popl %eax		;
	popl %ds		;
	popl %es		;
	iret			;
1:	
	ret			; /*this would make a return to the caller*/
			

