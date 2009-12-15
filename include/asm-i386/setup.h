#ifndef __SETUP_HEADER
#ifdef __KERNEL__
#define __SETUP_HEADER

#include <asm/e820.h>
#include <asm/multiboot.h>
#include <mir/data_str/dlist.h>
#include <mir/data_str/myhash.h>

struct elf_section_header_table;/*forward decl.*/
extern void init_memory ();
extern void print_memory_map(int no_entries);

#define MAX_SYM_LENGTH 0x10
#define INIT_START "_init_start"
#define INIT_END   "_init_end"

struct symbol_table { 
  unsigned char sym[MAX_SYM_LENGTH+1];
  unsigned long addr;
  struct list_head symbol_head;
  struct hash_struct symbol_hash;
  int (*hash_search)(struct symbol_table *,struct symbol_table *);
}__attribute__((packed));

extern struct symbol_table *lookup_symbol(const unsigned char *);
extern int add_elf_variables(elf_section_header_table_t *);
extern void reclaim_init(void);
extern unsigned long find_max_pfn(void);
extern void reserve_e820(void);
extern void display_e820(void);
extern void show_elf_variables(void);
extern void clear_bss(void);
extern int paging_init(void);
#endif
#endif
