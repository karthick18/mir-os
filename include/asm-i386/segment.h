/*Author: A.R.Karthick for MIR
 Contains gdt,tss_state definitions primarily used
 for protection mechanisms and context switches.
 Please refer Intel Docs whenever in doubt: 
 Intel Manual : System Programming Guide - Volume 3.
 Refer Chapter 6 for Task Management and Chapter 3 for segment
 descriptors.
*/
#ifdef __KERNEL__
#ifndef _SEGMENT_H
#define _SEGMENT_H

#define IDT_ENTRIES 0x100
#define GDT_ENTRIES 0x6
#define RING0       0x0
#define RING3       0x3
#define SYSTEM_DESC_TYPE    0x0
#define CODE_DATA_DESC_TYPE 0x1
#define TSS_TYPE 0x9 /*1001 binary according to intel specs with busy flag cleared*/
#define TSS_MIN_SIZE  0x67

#define USER_CS_TYPE   0xA /*code segment r/x */
#define USER_DS_TYPE   0x2 /*data segment r/w */

/* A segment selector is a 16 bit identifier:
   The last 13 bits gives the index into a segment descriptor
   table that can be either a GDT or an LDT table.
   The table is decided by bit 2 which is GDT when cleared
   and LDT when set.The first 2 bits are for the RPL (requested privilege
   level) which should be zero for RING0 access.
*/

#define GDT_SELECTOR 0x0
#define LDT_SELECTOR 0x1

#define SEGMENT_SELECTOR(index,table,pl) \
( ( (index) << 3 ) | ( (table) << 2 ) | ( (pl) & 3 ) )

#define __KERNEL_CS_INDEX 0x1
#define __KERNEL_DS_INDEX 0x2
#define __USER_CS_INDEX   0x3
#define __USER_DS_INDEX   0x4
#define __TSS_INDEX       0x5

#define __KERNEL_CS    SEGMENT_SELECTOR(__KERNEL_CS_INDEX,GDT_SELECTOR,RING0)
//#define __KERNEL_DS    0x10
#define __KERNEL_DS    SEGMENT_SELECTOR(__KERNEL_DS_INDEX,GDT_SELECTOR,RING0)
#define __USER_CS      SEGMENT_SELECTOR(__USER_CS_INDEX,GDT_SELECTOR,RING3)
#define __USER_DS      SEGMENT_SELECTOR(__USER_DS_INDEX,GDT_SELECTOR,RING3)
#define __TSS_SELECTOR SEGMENT_SELECTOR(__TSS_INDEX,GDT_SELECTOR,RING0)

#define NOT_ALIGNED    __attribute__((packed))

#ifndef __ASSEMBLY__

/*Define the gdt,idt layouts + tss struct*/

struct gdt_desc {
  unsigned short size;
  unsigned long address;
  unsigned short padding;
}NOT_ALIGNED;

extern struct gdt_desc _gdt_desc;

/*
 Defines the layout of the GDT descriptor:
 The layout is explained by the structure below.
 Please also refer to the Intel manuals to see
 the logic behind the initialisation of these components/descriptors.
*/

struct gdt_entries {
  unsigned short limit0_15;
  unsigned short base0_15;
  unsigned char  base16_23;
  unsigned int type : 4;
  unsigned int desc_type : 1;
  unsigned int dpl : 2;
  unsigned int present: 1;
  unsigned int limit16_19 : 4;
  unsigned int available : 1;
  unsigned int reserved_zero: 1;
  unsigned int d_or_b : 1; /*32 bit or 16 bit*/
  unsigned int granularity : 1;
  unsigned char base24_31;
} NOT_ALIGNED;

extern struct gdt_entries _gdt_entries[GDT_ENTRIES];

struct idt_desc { 
  unsigned short size;
  unsigned long address;
  unsigned short padding;
}NOT_ALIGNED;

extern struct idt_desc _idt_desc;

/*The structure below defines the layout the interrupt descriptor actually:
 */

struct idt_entries { 
  unsigned short offset0_15;
  unsigned short segment_selector;
  unsigned char reserved_zero;
  unsigned int  type : 5;
  unsigned int dpl : 2;
  unsigned int present : 1;
  unsigned short offset16_31;
} NOT_ALIGNED;


extern struct idt_entries _idt_entries[IDT_ENTRIES];

/*
  The definition of tss or task state segment
  used by the processor on context switches,
  is according to the Intel Specs:
 NOTE!! We dont use the IO bitmap coz of the EFLAGS that
 we maintain now. The EFLAGS now contains the IOPL for RING3
 (io privilege level is set to 3 at the EFLAGS thereby allowing IO from
  user space without any hassles-albeit not a recommended one:)
  
*/

struct tss_state { 
  unsigned short bh_low; /*previous task back link set by the processor*/
  unsigned short bh_high; /*unused*/
  unsigned long esp0; /*esp0 or the esp limit*/
  unsigned short ss0_low; /*ss0 or the DATA SEGMENT for us*/
  unsigned short ss0_high; /*unused*/
  unsigned long esp1; 
  unsigned short ss1_low;
  unsigned short ss1_high;
  unsigned long esp2;
  unsigned short ss2_low;
  unsigned short ss2_high;
  /*These are dynamic fields*/
  unsigned long cr3;
  unsigned long eip;
  unsigned long eflags;
  unsigned long eax;
  unsigned long ecx; 
  unsigned long edx;
  unsigned long ebx;
  unsigned long esp;
  unsigned long ebp;
  unsigned long esi; 
  unsigned long edi;
  unsigned short es_low;
  unsigned short es_high;
  unsigned short cs_low;
  unsigned short cs_high; 
  unsigned short ss_low;
  unsigned short ss_high;
  unsigned short ds_low;
  unsigned short ds_high;
  unsigned short fs_low;
  unsigned short fs_high;
  unsigned short gs_low;
  unsigned short gs_high;
  unsigned short ldt_selector_low;
  unsigned short ldt_selector_high;
  unsigned int trap_debug : 1;
  unsigned int padding : 15;
  unsigned short io_map_base_address;
}NOT_ALIGNED;  
  
extern struct tss_state init_tss_state;


/*Fill up the gdt entry for the USER SEGMENT selector part:
  Let us be more verbose and descriptive instead of being implicit
*/
static __inline__ void set_gdt_user(int index,unsigned int type) { 
  _gdt_entries[index].limit0_15 = 0xffff;
  _gdt_entries[index].base0_15 = 0x0000;
  _gdt_entries[index].base16_23 =0x00;
  _gdt_entries[index].type = type & 0xf;
  _gdt_entries[index].desc_type = CODE_DATA_DESC_TYPE;
  _gdt_entries[index].dpl = RING3;
  _gdt_entries[index].present = 1;
  _gdt_entries[index].limit16_19 = 0xf;
  _gdt_entries[index].available = 0;
  _gdt_entries[index].reserved_zero = 0;
  _gdt_entries[index].d_or_b = 1;
  _gdt_entries[index].granularity = 1;
  _gdt_entries[index].base24_31 = 0x00;
}

/*Now fill up the GDT entry for the TSS part:
  The base address points to the TSS segment used by the processor
  on context switches through the task register LTR
*/

static __inline__ void set_gdt_tss(int index,unsigned long base,unsigned long size) { 
  /*first sanity checks for size:*/
  if(size < TSS_MIN_SIZE)
    size = TSS_MIN_SIZE;
  _gdt_entries[index].limit0_15 = size & 0xffff;
  _gdt_entries[index].base0_15  = base & 0xffff;
  _gdt_entries[index].base16_23 = (base >> 16) & 0xff;
  _gdt_entries[index].type      =  TSS_TYPE;
  _gdt_entries[index].desc_type =  SYSTEM_DESC_TYPE;
  _gdt_entries[index].dpl       =  RING0;
  _gdt_entries[index].present   = 1;
  _gdt_entries[index].limit16_19 = (size >> 16) & 0xf;
  _gdt_entries[index].available = 0;
  _gdt_entries[index].reserved_zero = 0;
  _gdt_entries[index].d_or_b = 0;
  _gdt_entries[index].granularity =  0;
  _gdt_entries[index].base24_31   = (base >> 24) & 0xff;
}

/*Okay now let us do some real work in setting up TSS:
  Let us load the task register
*/

#define load_ltr(selector) do { \
__asm__ __volatile__("ltr %%ax\n" : :"a"(selector) );\
}while(0)

/*
  dpl or descriptor privilege level: is << 13
  type: is << 8
  High order bit in short(16 bits) is ON:
  So it should be 0x8000 | (type << 8 ) | (dpl << 13) 
  So make this guy accept dpl and load a correct type
  argument instead of zero.
  Type: 15- TRAP GATES DPL:0 

  Type: 14- Interrupt GATES DPL:0- General Interrupts-use this now.
  The DPL for above are 0.
  Type :15 - DPL: 3 System Call trap gates:
*/

static __inline__ void setidt(int index,unsigned long addr,unsigned int type,unsigned int dpl) { 
  _idt_entries[index].offset0_15 = addr & 0xffff;
  _idt_entries[index].segment_selector = __KERNEL_CS & 0xffff;
  _idt_entries[index].reserved_zero = 0x00;
  _idt_entries[index].type = type & 0x1f;
  _idt_entries[index].dpl = dpl & 0x3;
  _idt_entries[index].present = 1;
  _idt_entries[index].offset16_31 = (addr >> 16) & 0xffff;
}

   
#endif /*ENDIF __ASSEMBLY__*/

#endif /*ENDIF _SEGMENT_H*/

#endif /*ENDIF __KERNEL__*/
