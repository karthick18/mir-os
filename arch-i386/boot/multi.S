/*   MIR-OS : Boot loader interface with GRUB
;    Copyright (C) 2003 A.R.Karthick  
;   <a.r.karthick@gmail.com,a_r_karthic@rediffmail.com>
; This program is free software; you can redistribute it and/or
; modify it under the terms of the GNU General Public License
;as published by the Free Software Foundation; either version 2
;of the License, or (at your option) any later version.
;This program is distributed in the hope that it will be useful,
;but WITHOUT ANY WARRANTY; without even the implied warranty of
;MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
;GNU General Public License for more details.
;You should have received a copy of the GNU General Public License
;along with this program; if not, write to the Free Software
;Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
;
; Rewrote this file in GAS. Modified the bootstack.
; Introduced a GDT table with space for TSS descriptor and
; and user segment descriptors to be filled at run time.
; Introduced an unknown interrupt catch handler.
;
*/

.org 0x0

#include <asm/segment.h>
	
#define MULTIBOOT_HEADER_MAGIC 0x1BADB002
#define MULTIBOOT_HEADER_FLAGS 0x3
#define CHECKSUM -(MULTIBOOT_HEADER_MAGIC + MULTIBOOT_HEADER_FLAGS)
.global _start , _gdt_desc,_idt_desc,_gdt_entries
.extern kmain ,start_paging , mir_printf, _idt_entries

_start:
cli
jmp mb_entry

/*The Multiboot header*/
    .align 4
    mboot:
        .long MULTIBOOT_HEADER_MAGIC
        .long MULTIBOOT_HEADER_FLAGS
        .long CHECKSUM
        .long      mboot
        .long      _start
        .long      mb_entry

mb_entry:

/*Clear EFlags*/
pushl $0x0
popfl

/*setup the boot stack:
*/
movl $0x90000,%esp
movl %esp,%ebp

/*Now setup the gdt before we load segment selectors in start_paging.
  Also initialise the idt before we _actually_ load them up.	
*/

call start_paging

/*Setup the unknown interrupt handler carefully:
*/
call catch_unknown_interrupts

lgdt _gdt_desc
lidt _idt_desc

/*Now setup the segment descriptors*/
mov $__KERNEL_DS,%ax
mov %ax,%ss
mov %ax,%ds
mov %ax,%es
mov %ax,%fs
mov %ax,%gs

ljmp $__KERNEL_CS,$1f

/*We start here now with the long jmp above:*/
1:
pushl %ebx
call kmain

/* kmain should never return.If it indeed returns,we catch and hang
*/
	
L1:
 jmp L1

/*Catch unknown interrupts baby*/

catch_unknown_interrupts:
  lea unknown_interrupt_handler,%edx
  movl $(__KERNEL_CS << 16), %eax /*load the segment selector*/
  movw %dx,%ax /*load the bottom 16 bits of the handler*/
  movw $0x8E00,%dx /*load the type + dpl fields for the interrupt gate*/

/*Now that we have setup the basic stuffs in eax and edx,
  we can copy these to the interrupt table to catch unknown interrupts:
*/
  
  lea _idt_entries,%edi /*load the destination to populate*/
  mov $IDT_ENTRIES,%cx  /*load the count in cx*/

repeat_again:

  movl %eax,0(%edi) /*load the first half- dword*/
  movl %edx,4(%edi) /*load the second half-dword*/
  addl $0x8,%edi
  dec %cx
  jne repeat_again
  ret

/*gdt definitions*/
		
.align 4
_gdt_desc:
	.word GDT_ENTRIES*8 - 1
	.long _gdt_entries

/*idt definitions*/
.align 4
_idt_desc:
	.word IDT_ENTRIES*8 - 1
	.long _idt_entries

/*The gdt entries:
  The detailed description of the gdt format is in segment.h
*/
.align 4
_gdt_entries:
	.quad 0x0000000000000000 /*null descriptor*/
	.quad 0x00cf9a000000ffff /*code segment:	read/exec*/
	.quad 0x00cf92000000ffff /*data segment:	read/write*/
	/*These 3 entries are filled up at run time*/		
	.fill GDT_ENTRIES-3,8,0
	/*A brief look ahead of things to fill up*/
        /*user code segment descriptor*/
	/*.quad 0x0000000000000000 */
	/*user data segment descriptor*/
	/*.quad 0x0000000000000000 */
	/*TSS segment descriptor*/
	/*.quad 0x0000000000000000*/

  
    
error_msg:
	.asciz  "Unknown interrupt caught\n"

/*The unknown interrupt handler or a catch all handler */
.align 2
unknown_interrupt_handler:
	pushl %ebx
	pushl %ecx
	pushl %edx
	pushl %esi
	pushl %edi
	pushl %ebp
	pushl %eax
	push %ds
	push %es
	mov $(__KERNEL_DS) ,%ax
	mov %ax,%ds
	mov %ax,%es
	mov %ax,%ss
	pushl $error_msg
	call mir_printf
	addl $4,%esp
	pop %es
	pop %ds
	popl %eax
	popl %ebp
	popl %edi
	popl %esi
	popl %edx
	popl %ecx
	popl %ebx
#ifdef DEBUG
/*crash while debugging*/
	cli
2:	
	jmp 2b
#endif
	iret


	
