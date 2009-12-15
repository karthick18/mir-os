/*   MIR-OS : Setup routines
;    Copyright (C) 2003 A.R.Karthick 
;    <a_r_karthic@rediffmail.com,karthick_r@infosys.com>
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
;  Copy the MM initialisation :e820 biosmap
;  from the stored location into the structure:
;  Added support for localizing codes in specific sections
;  to enable freeing them off.
;  Added paging_init:
;  Pratap: Added grub multiboot compatibility!.
;  Thanks go out to Brian Moyle for sanitize_e820 w.r.t E820 BIOSMAP-
;  but we dont use it with BOCHS.
*/

#include <mir/kernel.h>
#include <mir/sched.h>
#include <asm/pgtable.h>
#include <asm/setup.h>
#include <mir/elf.h>
#include <mir/init.h>

#define PFN_UP(val)            ( ( (val) + ~PAGE_MASK ) >> PAGE_SHIFT )
#define PFN_DOWN(val)          ( (val) >> PAGE_SHIFT )

#define MAKE_VIRT_ADDR(i,j,k)  ( ((i) *PGDIR_SIZE) + ((j)*PMD_SIZE) + ( (k)*PAGE_SIZE ) )

static int setup_e820(mb_info_t *);
static int sanitize_e820_map(struct e820entry *,char *);
static int copy_e820(struct e820entry *,char );
static void setup_bss_end(void);
static void setup_ksyms(void);

struct e820map e820map;
unsigned long max_pfn ;
int reserved ;

#define _END       "_end"
#define _BSS_START "_sbss"
#define _KSYMTAB_START "_ksymtab_start"
#define _KSYMTAB_END   "_ksymtab_end"

const char *symbols[] = {_END,INIT_START,INIT_END,_BSS_START,_KSYMTAB_START,_KSYMTAB_END,NULL};
unsigned long elf_limit;

struct pgtable_cache pgtable_cache = {
  pgd_cache     :    NULL,
  pmd_cache     :    NULL,
  pte_cache     :    NULL,  
  pgd_cache_size:    0,
  pmd_cache_size:    0,
  pte_cache_size:    0,
};

extern unsigned long _ksymtab_start,_ksymtab_end;

struct pgtable_cache_limit pgtable_cache_limit = {
  pgd_cache_limit: PGD_CACHE_MAX,
  pmd_cache_limit: PMD_CACHE_MAX,
  pte_cache_limit: PTE_CACHE_MAX,
};


#define ELF_ALLOC(size)    elf_alloc(size)
#define ELF_INIT_HASH(size)  ({\
  struct hash_struct **_ptr = (struct hash_struct **)ELF_ALLOC( (size) * sizeof(struct hash_struct *) );\
  if(_ptr)\
   memset(_ptr,0x0,sizeof(struct hash_struct *) * size);\
 hash_table = _ptr;\
 hash_table_size = size;\
  _ptr;\
})
   
/*Memory isnt initialised and we need to store the values:Store it in Page 6,coz the elf section header gets overwritten when memory is initialised:
*/

#define ELF_MEM  ( PAGE_SIZE * 6)
#define ELF_MEM_LIMIT PAGE_SIZE

static DECLARE_LIST_HEAD(symbol_head);

static __inline__ unsigned long elf_alloc(unsigned long size) { 
  unsigned long addr = 0UL;
  if(elf_limit + size > ELF_MEM_LIMIT)
    goto out;
  addr = ELF_MEM + elf_limit;
  elf_limit += size;
 out:
  return addr;
}

int __init init_e820(mb_info_t *mb) { 
  setup_e820(mb);
  setup_bss_end();
  setup_ksyms();
  return 0;
} 

unsigned long find_max_pfn(void) {
  int i;
  unsigned long max = 0;
  for(i = 0; i < e820map.nr_map; ++i) {
    unsigned long start,end;
    if(e820map.map[i].type ==  E820_RAM) {
    start = PFN_UP(e820map.map[i].addr);
    end   = PFN_DOWN(e820map.map[i].addr + e820map.map[i].size);
    if(start >= end) 
      continue;
    if(end  > max) 
      max = end;
    }
  }
  return max;
}
  
/*Reserve all the pages apart from E820_RAM*/
void reserve_e820(void) {
  int i;
  unsigned long long max = ( max_pfn << PAGE_SHIFT) ;
  for( i = 0; i < e820map.nr_map;++i) { 
    int type = e820map.map[i].type;
    unsigned long long addr = e820map.map[i].addr;
    unsigned long long size = e820map.map[i].size;
    unsigned long long end  = addr + size;
    const char *str = NULL;
    if(type != E820_RAM && end < (unsigned long long)max ) {
      int res;
      printf("Reserving memory:%08lx -%08lx ",(unsigned long)addr,(unsigned long)end);
      if(size < PAGE_SIZE)
	size = PAGE_SIZE;
      res= reserve_mem_allocator((unsigned long)addr,(unsigned long)size);
      if(res < 0 ) {
        printf("\n Error in reserving memory:\n");
        continue;
      }
      printf("Pages(%d) ",res);
      reserved += res;

      switch(type) {

      case E820_RESERVED:
        str = "Type Reserved:\n";
        break;

      case E820_ACPI: 
        str = "Type ACPI tables:\n";
        break;

      case E820_NVS:
        str = "Type ACPI NVS:\n";
        break;

      default:
        str = "Type Unknown:\n";
      }
      printf("%s\n",str);
    }
  }
}    
 
static void __init add_memory_region(unsigned long long start,unsigned long long size,int type) {
  int pos = e820map.nr_map;
  if(pos >= E820_MAX) {
    printf("Too many entries in E820 bios map:\n");
    goto out;
  }
  e820map.map[pos].addr = start;
  e820map.map[pos].size = size;
  e820map.map[pos].type = type;
  ++e820map.nr_map;
 out:
  return ;
}

static int __init copy_e820(struct e820entry *map,char nr) { 
  int i;
  for( i = 0; i < nr; ++i) { 
    struct e820entry *ptr = map + i;
    int type = ptr->type;
    int flag = 0;
    unsigned long long start = ptr->addr;
    unsigned long long size  = ptr->size;
    unsigned long long end   = start + size;

    /*We add all the regions:
      Now we add all regions,but check for 640k - 1 MB in RAM.
      Some of the bioses dont report it in their RAM as reserved
    */
    if(type == E820_RAM) { 
      if(start < 0x100000ULL && end > 0xA0000ULL ) {
	if(start < 0xA0000ULL ) {
          size = 0xA0000ULL - start;
          add_memory_region(start,size,type);
	}
	if(end <= 0x100000ULL) 
          { 
            if(!flag) {
              start = 0xA0000ULL;
              size  = 0x100000ULL - start;
              type  = E820_RESERVED;
              flag = 1;
	    }
            else continue;
	  }
        if(end > 0x100000ULL) {
          start = 0x100000ULL;
          size =  end - start;   
	}
      }
    }
    add_memory_region(start,size,type);
  }
  return 0;
}

/*The real paging init*/

int __init paging_init(void) { 
  int i,j,k;
  pgd_t *pgd;pmd_t *pmd;pte_t *pte;
  unsigned long virt_addr = 0UL;
  unsigned long end = VMALLOC_START;
  int end_i = pgd_index(end); /*stop at vmalloc start point*/ 
  printf("Setting up Page Tables till (%#08lx)..\n",VMALLOC_START);
  for(i = pgd_index(__PAGE_OFFSET) ,pgd = init_pgdir + i; i < end_i; ++i,++pgd) {
    /*Now allocate a pmd entry*/
    if( ! (pmd = pmd_alloc(pgd,0) ) ) {
      panic("Unable to allocate pmd entry:\n");
    }
    /*Enter into the PMD */
    for(j = 0; j < PTRS_PER_PMD; ++pmd,++j) {
      
      /*Allocate a pte_entry for that pmd:
       */
      if(! (pte = pte_alloc(pmd, 0 ) ) ) {
	panic("Unable to allocate pte entry:\n");
      }
      /*Now setup the PTES*/
      for(k = 0; k < PTRS_PER_PTE; ++k,++pte) { 
        virt_addr = MAKE_VIRT_ADDR(i,j,k); /*get a virtual address*/
        set_pte(pte,virt_addr); /*setup the pte entry*/
     }
    }
  }
  printf("Paging initialised:\n");

  return 0;
}                
      
static int hash_search(struct symbol_table *a,struct symbol_table *lookup) { 
  return !strcmp(a->sym,lookup->sym) ;
}

static __inline__ unsigned int symbol_hash_routine(const unsigned char *name) { 
  unsigned int hash = 0;
  for(; *name; ++name) {
    hash = hash + ( ( (*name) >> 4) & 0xF) + (*name & 0xf);
  }
  return hash %hash_table_size;
}

static __inline__ void set_links(struct symbol_table *ptr) { 
  unsigned int hash = symbol_hash_fn(ptr->sym);
  list_add_tail(&ptr->symbol_head,&symbol_head);
  ptr->hash_search = hash_search;
  add_hash(&ptr->symbol_hash,hash);
}

struct symbol_table *lookup_symbol(const unsigned char *symbol) { 
  struct symbol_table ref , *ptr = &ref;
  struct symbol_table *lookup = NULL;
  unsigned int hash = symbol_hash_fn(symbol);
  memset(ptr,0x0,sizeof(*ptr));
  strncpy(ptr->sym,symbol,MAX_SYM_LENGTH);
  ptr->hash_search = hash_search;
  if( ! (lookup = search_hash(hash,ptr,struct symbol_table,symbol_hash) ) ) {
    printf("Unable to find symbol (%s) in hash table:\n",symbol);  
  }
  return lookup;
}

void show_elf_variables(void) { 
  struct list_head *traverse = NULL;
  struct list_head *head = &symbol_head;
  list_for_each(traverse,head) { 
    struct symbol_table *ptr=  list_entry(traverse,struct symbol_table,symbol_head);
    printf("var=%s,addr=%#08lx:\n",ptr->sym,ptr->addr);
  }
}

/*This one loops looks for symbols and stores their addresses in a table
  for lookup later:
*/
int __init add_elf_variables(elf_section_header_table_t *table) { 
  Elf32_Shdr *elf_shdr; /*elf section header*/
  int num,i,j,err = -1;
  Elf32_Sym *sym;
  unsigned char *strtab = NULL;
  elf_shdr = (Elf32_Shdr *)table->addr;
  /*Manually initialise the hash table*/

  if(!ELF_INIT_HASH(HASH_TABLE) ) {
    panic("Unable to initialise ELF symbol hash table:\n");
  }
  for ( i = 0; i < table->num; ++i ) { 
    if(elf_shdr[i].sh_type == SHT_SYMTAB) 
      break;
  }
  if(i >= table->num) {
    panic("Unable to find SYMTAB in elf header:\n");
    goto out;
  }
  sym = (Elf32_Sym*) elf_shdr[i].sh_addr;
  num = elf_shdr[i].sh_size/sizeof(Elf32_Sym);
  /*Now locate the string table index*/
  for(i = 0;  i < table->num; ++i) { 
    if(elf_shdr[i].sh_type == SHT_STRTAB && i != table->shndx) 
      break;
  }
  if(i >= table->num) {
    printf("Unable to locate STRTAB in elf header:\n");
    goto out; 
  }
  strtab = (unsigned char *)elf_shdr[i].sh_addr;
  /*Now the symbols are relative to the symbol table*/
  for(j = 0; symbols[j]; ++j) {
  const char *name = symbols[j];
  for(i = 0; i < num; ++i) {
    if(! strcmp( strtab + sym[i].st_name, name ) ) {
      struct symbol_table *ptr = (struct symbol_table *)ELF_ALLOC(sizeof(struct symbol_table));
      if(!ptr) {
       panic("Unable to allocate memory for symbol table:\n");
      }
      memset(ptr,0x0,sizeof(*ptr));
      strncpy(ptr->sym,name,MAX_SYM_LENGTH);
      ptr->addr = sym[i].st_value;
      set_links(ptr);
#ifdef DEBUG
      printf("Value of %s=%#08lx,size=%#08lx,info=%#08lx,shndex=%#08lx\n",name,sym[i].st_value,sym[i].st_size,sym[i].st_info,sym[i].st_shndx);
#endif

      break;
    }
  }
  if(i >= num) {
    printf("Unable to find variable %s in kernels symbol table:\n",name);
    goto out;
  }
  }
  err = 0; 
 out:
 return err;
}

static int __init setup_e820(mb_info_t *mb) { 
   int nr = (mb->mmap_length)/sizeof(mem_map_t), i;
  struct e820entry map[E820_MAX];
  int err = -1;
  e820map.nr_map = 0;
/* transfer stuff from grub MB header */
 for (i = 0 ; i < nr; i++ ) {
  map[i].addr = mb->mmap_addr[i].addr;
  map[i].size = mb->mmap_addr[i].size;
  map[i].type = mb->mmap_addr[i].type; 
 }
  if(nr <= 0 || nr > E820_MAX) { 
    printf("No E820 entries in the BIOS map:\n");
    goto out;
  }
  /*We dont need it with BOCHS*/
#if 0
  if(sanitize_e820_map(map,(char *)&nr) < 0 ) {
    printf("Sanitizing e820 map failed:\n");
    goto out;
  }
#endif
  /*Now copy the e820 map*/
  if(copy_e820(map,nr) < 0) {
    printf("Error in copying the e820 map:\n");
    goto out;
  }
  max_pfn= find_max_pfn();
  if(max_pfn > MAX_PFN) {
    printf("HIGH MEM is postponed to future:-)Resetting to 1GB RAM:\n");
    max_pfn = MAX_PFN;
  }
  err = add_elf_variables(&mb->elf_sec);
  err = 0;
 out:
  return err ;
}



void display_e820(void) { 
  int nr = e820map.nr_map,i;
  if(nr <= 0 || nr > E820_MAX) 
    {
      printf("Invalid #entries(%d)\n",nr);
      goto out;
    }
  printf("Displaying %d entries from the E820 BIOS MAP:\n",nr);
  for(i = 0; i < nr ; ++i) { 
    struct e820entry *ptr = e820map.map + i;
    printf("addr=%#08lx,size=%#08lx,type=%lu\n",(unsigned long)ptr->addr,(unsigned long)ptr->size,ptr->type);
  }
 out:  
  return;
}

unsigned long _bssend  = 0UL;

/*Setup _end variable NOW*/
static void __init setup_bss_end(void) { 
  struct symbol_table * lookup = NULL;
  /*Lookup out for symbol _end*/
  if(! (lookup =  lookup_symbol(_END) ) ) 
    panic("Unable to find (%s a.k.a BSS_END) variable:\n",_END);

  _bssend = lookup->addr;
}


static void __init setup_ksyms(void) { 
  struct symbol_table *start,*end;
  if( ! ( start = lookup_symbol(_KSYMTAB_START) ) ) {
    panic("Unable to find symbol %s\n",_KSYMTAB_START);
  }
  if( ! (end = lookup_symbol(_KSYMTAB_END) ) ) {
    panic("Unable to find symbol %s\n",_KSYMTAB_END);
  }
  _ksymtab_start = start->addr;
  _ksymtab_end   = end->addr;
  printf("ksymtab start :%#08lx,ksymtab end :%#08lx\n",_ksymtab_start,_ksymtab_end);
}

/*Zero off the bss:*/
void __init clear_bss(void) { 
  struct symbol_table *lookup = NULL;
  unsigned long start,end;
  if(! _bssend)  
    setup_bss_end();
  if(! (lookup = lookup_symbol(_BSS_START) ) ) 
    panic("Unable to locate symbol (%s)\n",_BSS_START);

  start = lookup->addr;
  end   = _bssend;
  memset((void*)start,0x0,end - start);
}

/*Rip off the init sections: 
  The init or the code initialisation sections 
  start from _init_start and end at _init_end: Page ALIGNED
 */
void reclaim_init(void) { 
  struct symbol_table *lookup = NULL;
  unsigned long init_start,init_end,pages;  
  if(! (lookup = lookup_symbol(INIT_START) ) ) 
    panic("Unable to get var(%s)\n",INIT_START);
  init_start = lookup->addr;
  if(! (lookup = lookup_symbol(INIT_END) ) ) 
    panic("Unable to get var(%s)\n",INIT_END);
  init_end =  lookup->addr;
  pages = ( (init_end - init_start ) + ~PAGE_MASK ) >> PAGE_SHIFT;
  printf("Freeing off Init code of size(%ldkb)\n",pages_to_kb(pages));
  if(free_mem_allocator(init_start,pages*PAGE_SIZE) < 0 )
    panic("Unable to free INIT mem:\n");
}
 
/* 
 * Author:Brian Moyle  from Linux Kernel arch/i386/kernel/setup.c
 * Sanitize the BIOS e820 map.
 *
 * Some e820 responses include overlapping entries.  The following 
 * replaces the original e820 map with a new one, removing overlaps.
 *
 */
static int __init sanitize_e820_map(struct e820entry * biosmap, char * pnr_map)
{
	struct change_member {
		struct e820entry *pbios; /* pointer to original bios entry */
		unsigned long long addr; /* address for this change point */
	};
	struct change_member change_point_list[2*E820_MAX];
	struct change_member *change_point[2*E820_MAX];
	struct e820entry *overlap_list[E820_MAX];
	struct e820entry new_bios[E820_MAX];
	struct change_member *change_tmp;
	unsigned long current_type, last_type;
	unsigned long long last_addr;
	int chgidx, still_changing;
	int overlap_entries;
	int new_bios_entry;
	int old_nr, new_nr;
	int i;

	/*
		Visually we're performing the following (1,2,3,4 = memory types)...

		Sample memory map (w/overlaps):
		   ____22__________________
		   ______________________4_
		   ____1111________________
		   _44_____________________
		   11111111________________
		   ____________________33__
		   ___________44___________
		   __________33333_________
		   ______________22________
		   ___________________2222_
		   _________111111111______
		   _____________________11_
		   _________________4______

		Sanitized equivalent (no overlap):
		   1_______________________
		   _44_____________________
		   ___1____________________
		   ____22__________________
		   ______11________________
		   _________1______________
		   __________3_____________
		   ___________44___________
		   _____________33_________
		   _______________2________
		   ________________1_______
		   _________________4______
		   ___________________2____
		   ____________________33__
		   ______________________4_
	*/

	/* if there's only one memory region, don't bother */
	if (*pnr_map < 2)
		return -1;

	old_nr = *pnr_map;

	/* bail out if we find any unreasonable addresses in bios map */
	for (i=0; i<old_nr; i++)
		if (biosmap[i].addr + biosmap[i].size < biosmap[i].addr)
			return -1;

	/* create pointers for initial change-point information (for sorting) */
	for (i=0; i < 2*old_nr; i++)
		change_point[i] = &change_point_list[i];

	/* record all known change-points (starting and ending addresses) */
	chgidx = 0;
	for (i=0; i < old_nr; i++)	{
		change_point[chgidx]->addr = biosmap[i].addr;
		change_point[chgidx++]->pbios = &biosmap[i];
		change_point[chgidx]->addr = biosmap[i].addr + biosmap[i].size;
		change_point[chgidx++]->pbios = &biosmap[i];
	}

	/* sort change-point list by memory addresses (low -> high) */
	still_changing = 1;
	while (still_changing)	{
		still_changing = 0;
		for (i=1; i < 2*old_nr; i++)  {
			/* if <current_addr> > <last_addr>, swap */
			/* or, if current=<start_addr> & last=<end_addr>, swap */
			if ((change_point[i]->addr < change_point[i-1]->addr) ||
				((change_point[i]->addr == change_point[i-1]->addr) &&
				 (change_point[i]->addr == change_point[i]->pbios->addr) &&
				 (change_point[i-1]->addr != change_point[i-1]->pbios->addr))
			   )
			{
				change_tmp = change_point[i];
				change_point[i] = change_point[i-1];
				change_point[i-1] = change_tmp;
				still_changing=1;
			}
		}
	}

	/* create a new bios memory map, removing overlaps */
	overlap_entries=0;	 /* number of entries in the overlap table */
	new_bios_entry=0;	 /* index for creating new bios map entries */
	last_type = 0;		 /* start with undefined memory type */
	last_addr = 0;		 /* start with 0 as last starting address */
	/* loop through change-points, determining affect on the new bios map */
	for (chgidx=0; chgidx < 2*old_nr; chgidx++)
	{
		/* keep track of all overlapping bios entries */
		if (change_point[chgidx]->addr == change_point[chgidx]->pbios->addr)
		{
			/* add map entry to overlap list (> 1 entry implies an overlap) */
			overlap_list[overlap_entries++]=change_point[chgidx]->pbios;
		}
		else
		{
			/* remove entry from list (order independent, so swap with last) */
			for (i=0; i<overlap_entries; i++)
			{
				if (overlap_list[i] == change_point[chgidx]->pbios)
					overlap_list[i] = overlap_list[overlap_entries-1];
			}
			overlap_entries--;
		}
		/* if there are overlapping entries, decide which "type" to use */
		/* (larger value takes precedence -- 1=usable, 2,3,4,4+=unusable) */
		current_type = 0;
		for (i=0; i<overlap_entries; i++)
			if (overlap_list[i]->type > current_type)
				current_type = overlap_list[i]->type;
		/* continue building up new bios map based on this information */
		if (current_type != last_type)	{
			if (last_type != 0)	 {
				new_bios[new_bios_entry].size =
					change_point[chgidx]->addr - last_addr;
				/* move forward only if the new size was non-zero */
				if (new_bios[new_bios_entry].size != 0)
					if (++new_bios_entry >= E820_MAX)
						break; 	/* no more space left for new bios entries */
			}
			if (current_type != 0)	{
				new_bios[new_bios_entry].addr = change_point[chgidx]->addr;
				new_bios[new_bios_entry].type = current_type;
				last_addr=change_point[chgidx]->addr;
			}
			last_type = current_type;
		}
	}
	new_nr = new_bios_entry;   /* retain count for new bios entries */

	/* copy new bios mapping into original location */
	memcpy(biosmap, new_bios, new_nr*sizeof(struct e820entry));
	*pnr_map = new_nr;

	return 0;
}

