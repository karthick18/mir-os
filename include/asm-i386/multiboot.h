/* Pratap for MIR: multiboot info from grub sources */
#ifdef __KERNEL__
#ifndef __MULTIBOOT_H
#define __MULTIBOOT_H


typedef struct 
{
  /* physical start and end addresses of the module data itself.  */
  unsigned long start;
  unsigned long end;
  char *string; /* string associated wid module */
  unsigned long res;
} __attribute__ ((packed)) mb_module_t;


typedef struct 
{
  unsigned long size1; /* dunno wat this is used for */
  unsigned long long addr;
  unsigned long long size;
/*
  unsigned long base_addr_low;
  unsigned long base_addr_high;
  unsigned long len_low;
  unsigned long len_high;
*/
  unsigned long type;
} __attribute__ ((packed)) mem_map_t;

typedef struct elf_section_header_table
{
        unsigned long num;
        unsigned long size;
        unsigned long addr;
        unsigned long shndx;
} elf_section_header_table_t;


typedef struct 
{
  unsigned long flags; /* flags that were set in the multiboot header */

 /* upper and lower memory */
  unsigned long mem_lower;
  unsigned long mem_upper;

  unsigned long boot_device;

  char *cmd_line; /* kernel command line */

  unsigned long mod_count; /* module count */
  mb_module_t *modules; /* the modules */

     elf_section_header_table_t elf_sec;

  /* memory map (e820) */
  unsigned long mmap_length;
  mem_map_t *mmap_addr;
} __attribute__ ((packed)) mb_info_t;

#endif
#endif

