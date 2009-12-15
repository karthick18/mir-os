/*   MIR-OS : Kernel Module Management Routines
;    Copyright (C) 2003-2004 A.R.Karthick 
;    <a_r_karthic@rediffmail.com,karthick_r@infosys.com>
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
;    Module Management Code:Modules are loaded with the help of grub.
;
;Below will be the shell interface
;
;lsmod - display all available modules (both loaded and not loaded)
;
;insmod <module number | module_name> - load the module associated from 
;the respective serial number or module name from lsmod 
;
;rmmod <module_number | module_name> - remove a module not loaded.
; 
;As in linux, modules should have init_module. this will be run at module 
;startup and cleanup_module called at module exit.
;ELF specifics are located in kernel/elf.c, this file will handle only 
;module management
;
*/


#include <mir/slab.h>
#include <asm/system.h>
#include <asm/string.h>
#include <mir/sched.h>
#include <mir/elf.h>
#include <mir/kernel.h>
#include <asm/multiboot.h>
#include <mir/mir_utils.h>
#include <asm/setup.h>
#include <mir/init.h>
#include <mir/module.h>

static DECLARE_LIST_HEAD(modules_list);
static int nr;

static __inline__ int module_hash_fn(int nr) { 
  unsigned int hash = 0;
  hash +=  (nr >> 24) &  0xff;
  hash += (hash ^ ( ( nr >> 16) & 0xff ) );
  hash += (hash ^ ( (nr >> 8) & 0xff ) );
  hash += (hash ^ (nr & 0xff) );
  return hash % hash_table_size; 
}
 
static int hash_search(const struct module *a,const struct module *b) { 
  return a->nr == b->nr;
}

static __inline__ struct module *lookup_module(int nr) { 
  struct module mod, *ptr= &mod;
  struct module *lookup = NULL;
  unsigned int hash = module_hash_fn(nr);
  memset(ptr,0x0,sizeof(*ptr));
  ptr->nr = nr;
  if(! (lookup = search_hash(hash,ptr,struct module,module_hash) ) ) 
    printf("Unable to locate module #(%d)\n",nr);

  return lookup;
}

static __inline__ void set_links(struct module *mod) {
unsigned int hash = module_hash_fn(mod->nr);
list_add_tail(&mod->next,&modules_list);
mod->hash_search = hash_search;
add_hash(&mod->module_hash,hash);
}

static __inline__ void remove_links(struct module *mod) {
  list_del(&mod->next);
  del_hash(&mod->module_hash);
}

static __inline__ void get_mod_name(char *dst,const char *src) { 
  const char *ptr = src;
  ptr += strlen(src);
  while(--ptr >  src ) 
    if(*ptr == '/'){
      break;
    } 

  if( *ptr == '/') ++ptr;
  strncpy(dst,ptr,MOD_MAX);
}
 
/* lists */
void add_mod (char *name, unsigned long addr, unsigned long size) {
struct module *modptr;
modptr = slab_alloc (sizeof(struct module),  0);
if(!modptr) 
  panic("Unable to allocate module pointer:\n");
memset(modptr,0x0,sizeof(*modptr));
get_mod_name(modptr->name,name);
modptr->addr = addr;
modptr->size = size;
modptr->status = 0; /* not running in the beginning */
modptr->nr = nr++;
set_links(modptr);
}

void modules_init (mb_info_t *mb) {
int i;
for (i = 0; i < mb->mod_count; i++) { /* put the modules into the list */
add_mod (mb->modules[i].string, mb->modules[i].start, mb->modules[i].end - 
mb->modules[i].start);
}
printf ("Found %d modules in memory !\n", i);
}

/* shell interfaces */

void lsmod (int arg) {
int i = 0;
printf ("%-10s%-30s%-20s%-10s\n","#", "Name", "Size", "Status");
do {
  struct list_head *head = &modules_list;
  struct list_head *traverse = NULL;
  list_for_each(traverse,head) {
    struct module *mod = list_entry(traverse,struct module,next);
    printf ("%-10d%-30s%-20ld%-20s\n", i++, mod->name, mod->size, mod->status ? "Running" : "Not Running");
  }
}while(0);
}

/*release a module:
*/
int release_module(struct module *module) { 
  struct list_head *head;
   /*unresolve the dependencies of other modules w.r.t to this module*/
  if(module->dep_table) { 
    head = &module->dep_table->module_export_table;
    while(! LIST_EMPTY(head) ) {
      struct ksyms *ptr = list_entry(head->next,struct ksyms,next);
      if(ptr->dep && ptr->dep->used) {
	--ptr->dep->used;
      }
      list_del(head->next);
      slab_free((void*)ptr);
    }
    slab_free((void*)module->dep_table);
    module->dep_table = NULL;
  }
  if(module->mod_info) { 
    if(module->mod_info->author) 
      slab_free((void*)module->mod_info->author);
    if(module->mod_info->version)
      slab_free((void*)module->mod_info->version);
    slab_free((void*)module->mod_info);
    module->mod_info = NULL;
  }
  /*cleanup the module symbol table*/
  if(module->symbol_table) {
    head = &module->symbol_table->module_export_table;
    while(! LIST_EMPTY(head) ) {
      struct ksyms *ptr = list_entry(head->next,struct ksyms, next);
      remove_syms(ptr); /*unlink the symbol from the various chains*/
      slab_free((void*)ptr);
    }
    /*unlink from the global symbol table*/
    list_del(&module->symbol_table->next);
    slab_free((void*)module->symbol_table);
    module->symbol_table = NULL;
  }
  vfree((void*)module->load_address);
  module->load_address = 0;
  module->status = 0;
  return 0;
}

/*load this module*/
static int install_module(struct module *module) { 
  unsigned long address;
  int err = -1;
  if( ! module->addr) {
    printf("Invalid module address %#08lx for module %s\n",module->addr,module->name);
    goto out;
  }
  if( ! (address = (unsigned long )vmalloc(module->size,0) ) ) {
    printf("Vmalloc error while allocating memory for module %s,line %d,routine %s\n",module->name,__LINE__,__FUNCTION__);
    goto out;
  }
  /*this should be slow, but cant help it*/
  memcpy((void *)address,(void *)module->addr,module->size);
  module->load_address = address;
  module->symbol_table = module->dep_table = NULL;
  module->mod_info = NULL;
  if ( (err = load_module ( module ) ) < 0 ) {
    /*error while loading module: undo all the chains*/
    goto out_release;
  }
  /*module has been linked successfully: Now load the module*/
  if( ! module->init_module ) { 
    printf("init_module symbol not defined in module %s\n",module->name);
    goto out_release;
  }
  if ( module->init_module(module->argc,module->argv) < 0 ) {
    printf("Error in init_module routine for module %s\n",module->name);
    goto out_release;
  }
  module->init_module = NULL;
  module->status = 1;
  printf("Module %s successfully loaded at addr %#08lx\n",module->name,module->load_address);
  err = 0;
 out:
  return err;
 out_release:
  release_module(module);
  goto out;
}

/*remove a module*/
static int deinstall_module(struct module *module) { 
  int err = -1;
  if( ! module->addr || !module->load_address) {
    printf("Trying to release invalid module address %#08lx for module %s\n",module->load_address,module->name);
    goto out;
  }
  if(mod_in_use(module) ) {
    printf("Module %s symbols in use..\n",module->name);
    goto out;
  }
  if( module->cleanup_module) {
    /*call the cleanup routine*/
    module->cleanup_module(module->argc,module->argv);
    module->cleanup_module = NULL;
  }
  /*undo deps and vfree the module memory:*/
  release_module(module);  
  module->status = 0;
  printf("Module %s unloaded.\n",module->name);
  err = 0;
 out:
  return err;
}

void insmod (int arg) {
  char *mods = (char *) arg;
  int mod_no;
  struct module *mod = NULL;
  if(!mods) 
    goto noname;
  while(isspace(*mods++) ) ;
  --mods;
  if(! *mods) {
  noname:
    printf("No module name given:\n");
    goto out;
  }
  mod_no = atoi(mods);
  if(errno < 0 ) {
    struct list_head *head = &modules_list;
    struct list_head *traverse = NULL; 
    errno = 0;
    list_for_each(traverse,head) { 
      struct module *ptr = list_entry(traverse,struct module,next);
      if(! strcmp(ptr->name,mods) ) {
	mod = ptr;
	break;
      } 
    }
    if(!mod) { 
      printf("Module %s not found\n",mods);
      goto out; 
    }
  }
  else {
    if(! (mod = lookup_module(mod_no) ) ) {
      printf ("Invalid module number specified !\n");
      goto out;
    }
  }
  /*module has been found */
  if(mod->status) {
    printf("Module %s Already loaded:\n",mod->name);
  }
  else {
    install_module(mod);
  }
 out:
  return ;  
}


/*Remove a module installed*/

void rmmod (int arg) {
char *mods = (char *) arg;
int mod_no;
struct module *mod = NULL;
if(!mods) 
  goto noname;
while(isspace(*mods++) ) ;
 --mods;
if(! *mods) {
noname:
 printf("No module name given:\n");
 goto out;
}
mod_no = atoi(mods);
if(errno < 0 ) {
   struct list_head *head = &modules_list;
   struct list_head *traverse = NULL; 
   errno = 0;
   list_for_each(traverse,head) { 
       struct module *ptr = list_entry(traverse,struct module,next);
       if(! strcmp(ptr->name,mods) ) {
	 mod = ptr;
         break;
       } 
     }
     if(!mod) { 
       printf("Module %s not found\n",mods);
       goto out; 
     }
}
else {
if(! (mod = lookup_module(mod_no) ) ) {
   printf ("Invalid module number specified !\n");
   goto out;
  }
}
/*module has been found */
if(!mod->status) {
printf("Module %s not loaded:\n",mod->name);
}
else {
  deinstall_module(mod);
}
out:
 return ;  
}

