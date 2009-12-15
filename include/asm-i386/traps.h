/*Author:A.R.Karthick for MIR:
 */
#ifdef __KERNEL__
#ifndef _TRAPS_H
#define _TRAPS_H

/*This is the default exception handler generated dynamically
*/

#define MAX_EXCEPTIONS        0x14
#define EXCEPTIONS_RESERVE    0x20
#define ASM_EXCEPTION_NAME(name) name##_error
#define EXCEPTION_NAME(name)  handle_##name

#ifndef __ASSEMBLY__

struct thread_regs;

struct exception_table { 
  const char *name;
  void (*handler)(void);
  int irq;
};

#define DECLARE_EXCEPTION_HANDLER(name) \
extern void EXCEPTION_NAME(name)(struct thread_regs *regs);\
extern void ASM_EXCEPTION_NAME(name) (void)

#define EXCEPTION_HANDLER(name) \
void EXCEPTION_NAME(name)(struct thread_regs *regs) { \
  tackle_exception(regs,#name "_exception" );\
}

DECLARE_EXCEPTION_HANDLER(divide);
DECLARE_EXCEPTION_HANDLER(debug);
DECLARE_EXCEPTION_HANDLER(nmi);
DECLARE_EXCEPTION_HANDLER(int3);
DECLARE_EXCEPTION_HANDLER(overflow);
DECLARE_EXCEPTION_HANDLER(bounds);
DECLARE_EXCEPTION_HANDLER(invalid_op);
DECLARE_EXCEPTION_HANDLER(device_not_available);
DECLARE_EXCEPTION_HANDLER(double_fault);
DECLARE_EXCEPTION_HANDLER(coprocessor_segment_overrun);
DECLARE_EXCEPTION_HANDLER(invalid_tss);
DECLARE_EXCEPTION_HANDLER(segment_not_present);
DECLARE_EXCEPTION_HANDLER(stack_segment);
DECLARE_EXCEPTION_HANDLER(general_protection);
DECLARE_EXCEPTION_HANDLER(page_fault);
DECLARE_EXCEPTION_HANDLER(coprocessor);
DECLARE_EXCEPTION_HANDLER(reserved);
DECLARE_EXCEPTION_HANDLER(alignment_check);
DECLARE_EXCEPTION_HANDLER(machine_check);
DECLARE_EXCEPTION_HANDLER(simd_coprocessor);

extern void handle_math_emulate(struct thread_regs *regs);

extern int traps_init(void);
#endif

#endif
#endif
