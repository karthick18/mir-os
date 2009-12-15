/*Author: A.R.Karthick for MIR*/
#ifdef __KERNEL__
#ifndef _THREAD_H
#define _THREAD_H

#define __KERNEL_EFLAGS ( (1 << 1) + (1 << 9) + (3 << 12) )

/*These are offsets into the stack at trap time */

#define EBX       0x0
#define ECX       0x4
#define EDX       0x8
#define ESI       0xC
#define EDI       0x10
#define EBP       0x14
#define EAX       0x18
#define DS        0x1C
#define ES        0x20
#define ORIG_EAX  0x24
#define EIP       0x28
#define CS        0x2C
#define EFLAGS    0x30
#define ESP       0x34
#define SS        0x38

/*This describes the stack state at time of intr/sys_call/exception:
  Same as Linux Kernel*/
#ifndef __ASSEMBLY__

struct thread_regs { 
  long ebx;
  long ecx;
  long edx;
  long esi;
  long edi; 
  long ebp;
  long eax;
  long ds;
  long es; 
  long orig_eax;
  long error_code; /*used for traps with error codes*/
  /*these are stacked by the CPU at the time of traps*/
  long eip;
  long cs;
  long eflags;
  long esp;
  long ss;
};

/*Floating point save structure: Taken from linux 2.4
*/

struct i387_fsave_struct {
	long	cwd;
	long	swd;
	long	twd;
	long	fip;
	long	fcs;
	long	foo;
	long	fos;
	long	st_space[20];	/* 8*10 bytes for each FP-reg = 80 bytes */
	long	status;		/* software status information */
};

struct thread_struct { 
  unsigned long esp;
  unsigned long esp0;
  unsigned long eip;
  unsigned long debugreg[8];
  struct i387_fsave_struct fsave;  
  /*should contain other i386 specific stuff:
   Romit should deliver the coup-de-grace in this context
  */
};
#endif
#endif

#endif
