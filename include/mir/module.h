/*Author : A.R.Karthick for MIR
*/
#ifdef __KERNEL__
#ifndef _MODULE_H
#define _MODULE_H

#define MOD_MAX  0x7F

#include <mir/init.h>
/*The modules structure*/

struct module {

struct list_head next;

struct hash_struct module_hash;

int (*hash_search)(const struct module *,const struct module *);

char name[MOD_MAX+1]; /* module name */

unsigned long addr; /* the address grub loaded the module into */

unsigned long status; /* status of module */

/* below are the status's */

#define MOD_RUNNING 0x1

unsigned long size; /* size of the module , end - start*/

int nr; /*module number*/

unsigned long load_address; /*modules load address*/

int argc; /*the module args*/

char **argv; /*the module argument list*/

int count; /*the modules symbol table count*/

/*the modules symbol table*/

struct module_symbol_table *symbol_table;

 /*the symbols on which the module is dependent*/
struct module_symbol_table *dep_table;

/*module information*/
struct module_info  *mod_info;

int (*init_module) (int argc,char **argv);

void (*cleanup_module) (int argc,char **argv);

};

/*Group of symbol tables per module*/

struct module_symbol_table { 
 /*the list of module export table or symbol table*/
 struct list_head module_export_table;

 struct list_head module_sorted_table;
 /*the next module symbol table*/
 struct list_head next;

};

/*Module information structure*/

struct module_info { 
  char *author;
  char *version;
};

/*The symbol table*/

struct ksyms {

struct list_head next;

struct list_head sort_next;

struct module_symbol_table *module_symbol_table;

struct hash_struct symbol_hash;

int (*hash_search) (const struct ksyms *,const struct ksyms *);

int (*compare)     (const struct ksyms *,const struct ksyms *);

int used; /*whether this symbol is being referenced by another guy*/

struct ksyms *dep; /*link to our dependent symbol*/

char name[MOD_MAX+1];

unsigned long addr;		   /* Symbol addr (value) */

unsigned long size;		   /* Symbol size */

unsigned char info;                /* Symbol type and binding */

unsigned char other;               /* Symbol visibility */

unsigned long shndx;               /* Section index */

};

struct module_export_entry { 
  const char *name;
  unsigned long address;
};

#define __ksymtab_init SECTION_NAME(.ksymtab)
#define __modinfo_init SECTION_NAME(.modinfo)

#define _EXPORT_SYMBOL(var) \
static struct module_export_entry __ksymtab_##var  __ksymtab_init = { \
name : #var,\
address: (unsigned long )&var\
}


#define EXPORT_SYMBOL(var) _EXPORT_SYMBOL(var)

#define MODULE_INFO(au,ve,name,ver)  \
static struct module_info __ksymtab_##name##ver __modinfo_init = { \
author: au,\
version:ve\
}


extern int load_module(struct module *);
extern void insmod(int );
extern void lsmod(int);
extern void rmmod(int);
extern void init_mod_sym_table(struct module_symbol_table *);
extern void link_mod_sym_table(struct module_symbol_table *);
extern struct ksyms *search_symbol(const struct module_symbol_table *,const char *);

static __inline__ void remove_syms(struct ksyms *ptr) { 
  list_del(&ptr->next);
  list_del(&ptr->sort_next);
  del_hash(&ptr->symbol_hash);
}


static __inline__ int mod_in_use(struct module *module) { 
  if(module->symbol_table) { 
    struct list_head *head = &module->symbol_table->module_export_table;
    struct list_head *traverse;
    list_for_each(traverse,head) { 
      struct ksyms *ptr = list_entry(traverse,struct ksyms,next);
      if(ptr->used) {
	return 1;
      }
    }
  }
  return 0;
}

#endif
#endif



