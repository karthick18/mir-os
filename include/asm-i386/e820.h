/*A.R.Karthick for MIR:
  Taken from the linux source and modified:
*/
#ifdef __KERNEL__
#ifndef __E820_H
#define __E820_H
#include <asm/multiboot.h>

#define E820_MAP	0x2d0		/* our map */
#define E820_MAX	32		/* number of entries in E820MAP */
#define E820_NR	        0x1e8		/* # entries in E820MAP */
#define E820_SEGMENT    0x200

#define E820_RAM	1
#define E820_RESERVED	2
#define E820_ACPI	3 /* usable as RAM once ACPI tables have been read */
#define E820_NVS	4

struct e820map {
    int nr_map;
    struct e820entry {
	unsigned long long addr;	/* start of memory segment */
	unsigned long long size;	/* size of memory segment */
	unsigned long type;		/* type of memory segment */
    } map[E820_MAX];
}__attribute__((packed));

extern struct e820map e820map;
extern int init_e820(mb_info_t *);
extern void display_e820(void);
extern void reserve_e820(void);
#endif/*__E820_H*/
#endif
