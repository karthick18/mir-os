/*Author:A.R.Karthick for MIR:
  Contains the irq handlers macros created
  for IRQS 0-15:
*/
#ifdef __KERNEL__
#ifndef _IRQ_HANDLERS_H
#define _IRQ_HANDLERS_H

typedef void (intr_handler_t)(void);

#define STR(A) #A
#define XSTR(A)  STR(A)

#define SAVE_ALL \
"pushl %es\n"\
"pushl %ds\n"\
"pushl %eax\n"\
"pushl %ebp\n"\
"pushl %edi\n"\
"pushl %esi\n"\
"pushl %edx\n"\
"pushl %ecx\n"\
"pushl %ebx\n"

#define DEFINE_COMMON_INTR \
__asm__(".align 2\n" \
".text\n"\
"common_intr:\n"\
SAVE_ALL\
"movl $" XSTR(__KERNEL_DS) ",%eax\n"\
"movl %eax,%ds\n"\
"movl %eax,%es\n"\
"movl %eax,%ss\n"\
"movl %eax,%fs\n"\
"movl %eax,%gs\n"\
"movl %esp,%eax\n"\
"pushl %eax\n"\
"call do_irq\n"\
"addl $4,%esp\n"\
"jmp ret_from_intr\n"\
)

#define MAKE_INTR_STR(nr) "INTR_"#nr"_handler"
#define MAKE_INTR_ROUTINE(nr) INTR_##nr##_handler

#define MAKE_INTR(nr) \
intr_handler_t MAKE_INTR_ROUTINE(nr);\
__asm__ (".align 2\n"\
".text\n"\
MAKE_INTR_STR(nr) ":\n"\
"pushl $"#nr"\n"\
"pushl %eax\n"\
"jmp common_intr\n"\
)

#endif
#endif





















