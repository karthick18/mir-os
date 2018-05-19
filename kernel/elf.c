/*   MIR-OS : Dynamic ELF Loader
;    Copyright (C) 2003-2004 A.R.Karthick 
;    <a.r.karthick@gmail.com,a_r_karthic@rediffmail.com>
;    Pratap P.V - pratap_theking@hotmail.com
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
;
;    This one loads the modules dynamically to the running kernel.
;    Its a dynamic elf loader.
*/

#include <mir/slab.h>
#include <asm/system.h>
#include <asm/string.h>
#include <mir/sched.h>
#include <mir/elf.h>
#include <mir/kernel.h>
#include <mir/mir_utils.h>
#include <asm/multiboot.h>
#include <mir/module.h>

#define MOD_MAX   0x7F

static DECLARE_LIST_HEAD(module_sym_head);
extern unsigned long _ksymtab_start, _ksymtab_end;

/*the kernel symbol table*/
static struct module_symbol_table kernel_symbol_table;

/*initialise the symbol table*/
void init_mod_sym_table(struct module_symbol_table *table) {
  /*initialise the various chains*/
  list_head_init(&table->module_export_table);
  list_head_init(&table->module_sorted_table);
}

void link_mod_sym_table(struct module_symbol_table *table) { 
  list_add_tail(&table->next,&module_sym_head);
}


/*sort the ksyms by address*/
static int cmp_ksyms (const struct ksyms *ptr1,const struct ksyms *ptr2) {
  return ptr1->addr - ptr2->addr;
}

void show_ksyms(void) { 
  register struct list_head *traverse;
  struct list_head *head = &module_sym_head;
  list_for_each(traverse,head) { 
    struct module_symbol_table *mod_sym_ptr =  list_entry(traverse,struct module_symbol_table, next);
    struct list_head *sorted_table = &mod_sym_ptr->module_sorted_table;
    struct list_head *i;
    list_for_each (i, sorted_table ) { 
      struct ksyms *ptr = list_entry(i,struct ksyms,sort_next);
      printf("%#08lx %-10s\n",ptr->addr,ptr->name);
    }
  }
}

static int hash_search(const struct ksyms *a,const struct ksyms *b) { 
  return !strcmp(a->name,b->name) ;
}

static __inline__ void set_links(struct ksyms *ptr) { 
  unsigned int hash = symbol_hash_fn((const unsigned char *)ptr->name);
  struct list_head *head = &ptr->module_symbol_table->module_sorted_table;
  struct list_head *element = &ptr->sort_next;
  ptr->hash_search = hash_search;
  ptr->compare     = cmp_ksyms;
  list_add_tail(&ptr->next,&ptr->module_symbol_table->module_export_table);
  list_sort_add(element,head,struct ksyms, sort_next);
  add_hash(&ptr->symbol_hash,hash);
}

/*Lookup a symbol in the hash table*/
static struct ksyms *lookup_symbol(const char *name) {
  struct ksyms syms, *ptr = &syms;
  struct ksyms *lookup = NULL;
  unsigned int hash = symbol_hash_fn((const unsigned char*)name);
  memset((void*)ptr,0x0,sizeof(*ptr));
  strncpy(ptr->name,name,MOD_MAX);
  if( ! (lookup = search_hash(hash,ptr,struct ksyms,symbol_hash) ) ) {
    printf("Unable to find kernel symbol(%s) in hash table:\n",name);
  }
  return lookup;
}

static void add_sym(const char *name,Elf32_Sym *symbol_table,struct module_symbol_table *mod_symbol_table) { 
struct ksyms *ksymsptr;
ksymsptr = slab_alloc (sizeof(struct ksyms),  0);
 if(! ksymsptr) 
   panic("Unable to allocate kernel symbols:\n");
memset((void*)ksymsptr,0x0,sizeof(*ksymsptr) );
strncpy (ksymsptr->name,name,MOD_MAX);
ksymsptr->addr = symbol_table->st_value;
ksymsptr->size = symbol_table->st_size;
ksymsptr->info = symbol_table->st_info;
ksymsptr->other = symbol_table->st_other;
ksymsptr->shndx = symbol_table->st_shndx;
ksymsptr->module_symbol_table = mod_symbol_table;
set_links(ksymsptr);

}

/* the below function attempts to get all kernel symbols and put them in a list
*/

void get_ksyms(mb_info_t *mb) {
  Elf32_Shdr *ksec = (Elf32_Shdr *) (mb->elf_sec.addr);
  Elf32_Sym *ksym;
  int no_sections = mb->elf_sec.num, i, symtab_sec = -1, no_syms, symstr_sec = -1;
  struct module_export_entry *start = (struct module_export_entry *)_ksymtab_start;
  struct module_export_entry *end   = (struct module_export_entry *)_ksymtab_end;
  struct module_export_entry *traverse;
  char *symstr;
  link_mod_sym_table(&kernel_symbol_table);
  init_mod_sym_table(&kernel_symbol_table);

  /* find sym tab */
  for (i = 0; i < no_sections; i++) {
    if (ksec[i].sh_type == SHT_SYMTAB) {
      symtab_sec = i;
      break;
    }
  }

  if (symtab_sec < 0)
    panic ("Kernel Symbol Table NOT Found\n");

  no_syms = ksec[symtab_sec].sh_size / sizeof (Elf32_Sym);
  ksym = (Elf32_Sym *) ksec[symtab_sec].sh_addr;
  printf ("KSYMS: Found %d kernel symbols\n", no_syms);

  /* find symbol string table index */
  for (i = 0; i < no_sections; i++) {
    if (ksec[i].sh_type == SHT_STRTAB && i!=(mb->elf_sec.shndx)) {
      symstr_sec = i;
      break;
    }
  }

  if (symstr_sec < 0)
    panic ("Kernel Symbol String Section NOT Found");
  symstr = (char *) ksec[symstr_sec].sh_addr;
#if 1
  /* put symbols in list */
  for (i = 0; i < no_syms; i++) {
    if( ( ELF32_ST_BIND(ksym[i].st_info) == STB_GLOBAL ) 
	&&
	ksym[i].st_shndx != SHN_UNDEF
	)
      add_sym(symstr + ksym[i].st_name,ksym + i,&kernel_symbol_table);
  }
#else
  for( traverse = start ; traverse < end ; ++traverse ) { 
    Elf32_Sym table;
    memset((void*)&table,0x0,sizeof(table));
    table.st_value = traverse->address;
    add_sym ( traverse->name,&table,&kernel_symbol_table);
  }
#endif
}
/* return address of the specified symbol */
static struct ksyms *ret_addr (const char *sym) {
  struct ksyms *ptr = NULL;
#if 1
  ptr = lookup_symbol(sym) ;
  return ptr;
#else
  do {
    struct list_head *head = &module_sym_head;
    struct list_head *traverse;
    list_for_each(traverse,head) {
      struct module_symbol_table *mod_sym_ptr = list_entry(traverse,struct module_symbol_table,next);
      struct list_head *i;
      struct list_head *symbol_table = &mod_sym_ptr->module_export_table;
      list_for_each(i, symbol_table) {
	struct ksyms *sy = list_entry(i,struct ksyms,next);
        if (!(strcmp (sy->name, sym)))
	  return sy;
      }
    }
  }while(0);
  return NULL;
#endif
}

/*Change the base address of the SECTIONS*/
static __inline__ void relocate_base(unsigned long base,Elf32_Ehdr *ehdr) { 
  Elf32_Shdr *shdr = (Elf32_Shdr *) ( (unsigned long)ehdr + ehdr->e_shoff );
  register int i;
  for(i = 0; i < ehdr->e_shnum ; ++i )  
    shdr[i].sh_addr = (unsigned long)base + shdr[i].sh_offset;
}

static Elf32_Shdr *get_section_by_name(Elf32_Ehdr *ehdr,const char *name) { 
  Elf32_Shdr *shdr = (Elf32_Shdr * ) ( (unsigned char*)ehdr + ehdr->e_shoff );
  int strndx = ehdr->e_shstrndx;
  unsigned char *strtab = (unsigned char *) ( (unsigned char *)ehdr + shdr[strndx].sh_offset );
  int i;
  for( i = 0; i < ehdr->e_shnum ; ++i) { 
    if(! strcmp ( shdr[i].sh_name + strtab , name ) ) 
      return shdr + i;
  }

  return NULL;
}

/*search the modules symbol table to see if this symbol is already present*/

struct ksyms *search_symbol(const struct module_symbol_table *symbol_table,const char *name) {  
  struct list_head *head = &symbol_table->module_export_table;
  struct list_head *traverse;
  list_for_each(traverse,head) { 
    struct ksyms *ptr = list_entry(traverse,struct ksyms, next);
    if(!strcmp(ptr->name,name) ) 
      return ptr;
  }
  return NULL;
}
 

/* load a module which is at *addr*, no error checking
   resolves dependencies and exports global objects into ksym table */

int load_module (struct module *module_load) {
  unsigned char *module = (unsigned char *) module_load->load_address;
  Elf32_Ehdr *hdr;
  Elf32_Shdr *sections,*custom_sections;
  Elf32_Sym *symbols = NULL;
  Elf32_Rel *rel;
  int i, symbol_cnt = 0, symstr_sec = -1,err = -1,count = 0;
  char *symstr;
  struct module_symbol_table *symbol_table = NULL;
  struct module_symbol_table *dep_table = NULL;
  struct module_info *mod_info = NULL,*temp_info;

  if( ! (symbol_table = slab_alloc(sizeof(struct module_symbol_table),0 ) ) ) {
    printf("Unable to alloc. memory for module symbol table\n");
    goto out;
  }
  if ( ! (dep_table = slab_alloc(sizeof(struct module_symbol_table),0) ) ) {
    printf("Unable to alloc. memory for module dependency table\n");
    goto out_free;
  }
  if( ! (mod_info = slab_alloc(sizeof(struct module_info),0) ) ) {
    printf("Unable to alloc. memory for module info\n");
    goto out_free;
  }

  init_mod_sym_table(symbol_table);
  init_mod_sym_table(dep_table);

  hdr = (Elf32_Ehdr*) module;
  relocate_base(module_load->load_address,hdr);
  sections = (Elf32_Shdr *) (module + hdr->e_shoff);
  for (i = 0; i < hdr->e_shnum; i++) {
    if (sections[i].sh_type == SHT_SYMTAB) {
      symbols = (Elf32_Sym *) (module +sections[i].sh_offset);
      symbol_cnt = (sections[i].sh_size)/sizeof (Elf32_Sym);
    }
  }

  if (!symbols) 
    panic ("Module symbol table not found, something fucked!\n");

  /* find symbol string table index */
  for (i = 0; i < hdr->e_shnum; i++) {
    if (sections[i].sh_type == SHT_STRTAB && i != hdr->e_shstrndx ) {
      symstr_sec = i;
      break;
    }
  }

  if(symstr < 0 )
    panic("Module string table not found!\n");

  symstr = module + sections[symstr_sec].sh_offset;

  /* resolve global dependencies */
  for (i = 0; i < symbol_cnt ; i++) {
    struct ksyms *ptr;
    struct ksyms *ksyms;

    if (ELF32_ST_BIND(symbols[i].st_info) == STB_LOCAL || ELF32_ST_TYPE(symbols[i].st_info)!= STT_NOTYPE )
      continue;

    if(! (ptr = ret_addr( symstr + symbols[i].st_name ) ) ) {
      printf ("Global Symbol %s not resolved\n", symstr+symbols[i].st_name);
      goto out_free;
    }
    if( ! (ksyms = slab_alloc(sizeof(struct ksyms ), 0 ) ) ) {
      printf("slab_error error in Routine %s,Line %d\n",__FUNCTION__,__LINE__);
      goto out_free;
    }
    memset(ksyms,0x0,sizeof(struct ksyms));
    ksyms->dep = ptr;
    list_add_tail(&ksyms->next,&dep_table->module_export_table);
    ++ptr->used; /*mark the symbol as used*/
    symbols[i].st_value = ptr->addr;
  }

  /*zero off BSS*/
  for(i = 0; i < hdr->e_shnum; ++i) { 
    if( (sections[i].sh_flags & SHF_ALLOC) &&
	(sections[i].sh_type == SHT_NOBITS) 
	) {
      memset((void*)sections[i].sh_addr,0x0,sections[i].sh_size);
    }
  }

  /* put proper address in symbol value */
  for (i = 0; i < symbol_cnt ; i++) {
    int n;
    if (symbols[i].st_shndx == SHN_ABS || symbols[i].st_shndx == SHN_UNDEF || ELF32_ST_TYPE(symbols[i].st_info) == STT_NOTYPE)
      continue;

    n = symbols[i].st_shndx;
    symbols[i].st_value += sections[n].sh_addr;
  }
  /* perform reallocations */
  for (i=0; i< hdr->e_shnum; i++) {
    int rel_cnt, n;
    unsigned char *ptr;
    if (sections[i].sh_type != SHT_REL)
      continue;

    rel_cnt = sections[i].sh_size/ sizeof (Elf32_Rel);
    rel = (Elf32_Rel *) (module + sections[i].sh_offset);
    ptr = (void *)sections [sections[i].sh_info].sh_addr;

    for (n = 0; n < rel_cnt; n++) {
      int *r = (int *)(ptr + rel[n].r_offset);
      switch(ELF32_R_TYPE(rel[n].r_info))
	{
	case R_386_32:
	  *r += symbols[ELF32_R_SYM(rel[n].r_info)].st_value;
	  break;
	case R_386_PC32:
	  *r += symbols[ELF32_R_SYM(rel[n].r_info)].st_value - (int)r;
	  break;

	
	}

    }
  }
  custom_sections = get_section_by_name(hdr,".modinfo");
  if(custom_sections) {
    temp_info = (struct module_info *)custom_sections->sh_addr;
    if(temp_info->author) { 
      mod_info->author = slab_alloc(strlen(temp_info->author)+1,0);
      if(!mod_info->author) {
	printf("Unable to alloc. memory for mod_info->author\n");
	goto out_free;
      }
    }
    if(temp_info->version) {
      mod_info->version = slab_alloc(strlen(temp_info->version)+1,0);
      if(!mod_info->version) {
	printf("Unable to alloc. memory for mod_info->version\n");
	slab_free((void*)mod_info->author);
	goto out_free;
      }
    }
    strcpy(mod_info->author,temp_info->author);
    strcpy(mod_info->version,temp_info->version);
    printf("Module Author : %s,Version : %s\n",mod_info->author,mod_info->version);
  }
    
  custom_sections = get_section_by_name (hdr,".ksymtab");
  for(i = 0; custom_sections && i < custom_sections->sh_size/sizeof(struct module_export_entry);++i) { 
    struct module_export_entry *ptr = (struct module_export_entry *)custom_sections->sh_addr + i;
    Elf32_Sym table;
    memset(&table,0x0,sizeof(table));
    table.st_value = ptr->address;
    add_sym(ptr->name,&table,symbol_table);
    ++count;
  }

  for (i = 0; i < symbol_cnt ; i++) {
    if (!strcmp(symstr+symbols[i].st_name, "init_module")) {
      module_load->init_module =  (  int (*) (int ,char **) ) symbols[i].st_value;
      continue;
    }
    if(! strcmp(symstr + symbols[i].st_name,"cleanup_module") ) {
      module_load->cleanup_module = ( void (*) (int ,char **) ) symbols[i].st_value;
      continue;
    }

#ifdef DEBUG 
     printf("module symbol %s to the modules symbol table:(shndx %d: type :%d,bind : %d)\n",symstr + symbols[i].st_name ,symbols[i].st_shndx,ELF32_ST_TYPE(symbols[i].st_info),ELF32_ST_BIND(symbols[i].st_info));
#endif

    if (ELF32_ST_BIND(symbols[i].st_info) != STB_GLOBAL || symbols[i].st_shndx == SHN_UNDEF)
      continue;
    
    /*look for the symbol in our table*/

    if(! search_symbol(symbol_table,symstr + symbols[i].st_name) )
      continue;
    
    ++count;

    add_sym (symstr+symbols[i].st_name, symbols + i, symbol_table);
  }

  if (!module_load->init_module) {
    printf ("\tsymbol init_module not found. aborting load\n");
    goto out_free;
  }
#if 0
  if(! module_load->cleanup_module) {
    printf("\tsymbol cleanup_module not found.aborting load\n");
    goto out_free;
  }
#endif
  module_load->dep_table = dep_table;
  module_load->symbol_table = symbol_table;
  module_load->mod_info = mod_info;
  link_mod_sym_table(symbol_table);
  printf ("\tadded %d symbols from module\n", count);
  err = 0;
 out:
  return err;
 out_free:
  if(symbol_table) slab_free((void*)symbol_table);
  if(dep_table)    slab_free((void*)dep_table);
  if(mod_info)     slab_free((void*)mod_info);
  goto out;

}

