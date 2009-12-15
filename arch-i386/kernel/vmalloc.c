/*   MIR-OS : Vmalloc or virtual contiguous alloc :
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
;    The VMALLOC reserve and allocation:
;    Usage:
;
;    To allocate:
;    vmalloc(size,flags)
;  
;    To free:
;    vfree((void*)addr)
;
;    vmalloc is another type of memory request
;    through non-contiguous allocation of large chunks
;    that works by tweaking the Kernel page tables
;    after getting a free address:
;    Use it for large allocations,but restricted to allocations
;    less than 128MB:
;    
;    The vmalloc virtual address space starts at:
;    4GB - 128MB - 64MB.
;    The last 64 MB is reserved for special purposes
;    The allocations above 1 MB are bit slow due to usage of bitmap allocators,
;    but that should be okay once we promote the allocators to something like
;    the buddy system.
;
*/

#include <mir/kernel.h>
#include <mir/sched.h>
#include <asm/pgtable.h>

#ifndef MIN
#define MIN(a,b)           ( (a) < (b) ? (a) : (b) )
#endif

static struct vmalloc_struct *vmalloc_head ;
/*This is a list of structs sorted by address*/

struct vmalloc_struct { 
  struct vmalloc_struct *next;
  unsigned long addr;
  unsigned long size;
  int flags;
};

/*Once the address is obtained,setup the page tables for those addresses:*/
static int free_pte(pte_t *pte,unsigned long addr,unsigned long size,int flags) { 
  unsigned long end ;
  int err = -1;
  addr &= ~PMD_MASK;
  end = MIN(addr + size, PMD_SIZE);
  /*Now free up the page tables*/
  do {
    /*get the pte and freeup the entry*/
    if(pte_none(*pte) ) {
      printf("null pte_entry in %s for addr(%#08lx)\n",__FUNCTION__,addr);
      goto next;
    }
    pte_free(pte);
  next:
    ++pte; 
    addr += PAGE_SIZE;
  }while( addr < end );
  err = 0; 
 out:
  return err;
}

static int free_pmd(pmd_t *pmd,unsigned long addr,unsigned long size,int flags) { 
  unsigned long end;
  int err =-1;
  addr &= ~PGDIR_MASK;
  end = MIN(addr + size, PGDIR_SIZE);

  do { 
    pte_t *pte = NULL;
    if(pmd_none(*pmd) ) {
     printf("Trying to free null pmd entry in %s.Continuing..\n",__FUNCTION__);
     goto out;
     } 
    pte = pte_offset(pmd, addr);
    err = free_pte(pte,addr,end - addr,flags);
    if(err < 0) 
      goto out;
    ++pmd;
    addr = (addr + PMD_SIZE) & PMD_MASK;
  }while(addr < end);
  err =0;
 out:
  return err;
}

/*Now free up the page tables*/
static int free_pgd(unsigned long addr,unsigned long size,int flags) {
  pgd_t *pgd = NULL;
  unsigned long end = addr + size;
  int err = -1;
  pgd = init_pgdir + pgd_index(addr);
  do {
    pmd_t *pmd = NULL;
    if(pgd_none(*pgd) ) {
      printf("Trying to free null pmd entry in %s.Continuing..\n",__FUNCTION__);
      goto next;
    }
    pmd = pmd_offset(pgd,addr);
    err = free_pmd(pmd,addr,end - addr,flags);   
    if(err < 0 )
      goto out;
  next:
    ++pgd;
    addr = (addr +  PGDIR_SIZE) & PGDIR_MASK;
  }while(addr < end);
  err =0;
 out:
  return err;
}    

/*setup pte or the page tables:
*/

static int setup_pte(pte_t *pte,unsigned long addr,unsigned long size,int flags) { 
  unsigned long end ;
  int err = -1;
  addr &= ~PMD_MASK;
  end = MIN(addr + size,PMD_SIZE);

  do {
#if 0
    unsigned long page = __get_free_page(0);
#else
    unsigned long page = pte_alloc_page();
#endif
    if(! page) {
      printf("Unable to get page in %s\n",__FUNCTION__);
      goto out;
    }
    if(!pte_none(*pte) ) {
      printf("trying to overwrite an existing pte entry %#08lx.Freeing the old one..\n",get_pte(*pte) ); 
      pte_free(pte);
    }
    /*Now we have got the page:Setup the pte entry*/
    set_pte(pte,page);
    ++pte;
    addr += PAGE_SIZE;
  }while(addr < end);
  err = 0;
 out:
  return err;
}

/*setup pmd*/

static int setup_pmd(pmd_t *pmd,unsigned long addr,unsigned long size,int flags)
{
  unsigned long end ;
  int err = -1;
  addr &= ~PGDIR_MASK;
  end = MIN(addr + size, PGDIR_SIZE) ;

  do { 
    pte_t *pte = pte_alloc(pmd,addr);
    if(! pte) {
      printf("unable to get pte in %s\n",__FUNCTION__);
      goto out;
    }
    err = setup_pte(pte,addr,end - addr,flags);
    if(err < 0 )
      goto out;
    ++pmd;
    addr = (addr + PMD_SIZE) & PMD_MASK;
  }while(addr < end);
  err = 0;
 out:
  return err;
}
 

/*Setup pgd*/
static int setup_pgd(unsigned long addr,unsigned long size,int flags) {
  pgd_t *pgd = NULL;
  unsigned long end ;
  int err =-1;
  pgd = init_pgdir + pgd_index(addr);
  end = addr + size;
  do {
    pmd_t *pmd = pmd_alloc(pgd,addr);
    if(! pmd) { 
      printf("Unable to allocate pmd entry in %s\n",__FUNCTION__);
      goto out;
    }
    err = setup_pmd(pmd,addr,end - addr,flags);
    if(err < 0 ) 
      goto out;
    addr = (addr + PGDIR_SIZE) & PGDIR_MASK;
    ++pgd;
  }while(addr < end);
  err = 0; 
 out:
  return err;
}
  
/*Obtain a vmalloc address*/
static struct vmalloc_struct *get_vmalloc_addr(unsigned long size,int flags) {
  unsigned long addr = VMALLOC_START_ADDR; /*the new address:*/  
  struct vmalloc_struct *ptr = NULL,*next = NULL;
  struct vmalloc_struct **head = NULL;
  if(size > VMALLOC_RESERVE) {
    printf("size(%#08lx) exceeds vmalloc reserve(%#08lx)\n",size,VMALLOC_RESERVE);
    goto out;
  }
  size += PAGE_SIZE; /*additional to catch out of bound references*/

  /*Now get the address*/
  for( head = &vmalloc_head ; (ptr = *head); head = &ptr->next ) {
    if(addr + size <= ptr->addr)
      goto found;
    addr =  ptr->addr + ptr->size;
    if(addr + size > VMALLOC_END ) 
      goto out;
  }
  /*we are here when we can add an addr. to the vmalloc list:*/
 found:
  if( ! ( next = slab_alloc(sizeof(struct vmalloc_struct),0) ) ) {
    printf("Unable to allocate vmalloc_struct:\n");
    goto out;
  }
  next->addr = addr;
  next->size = size;
  next->flags= flags;
  /*Now link this guy into the vmalloc_struct*/
  next->next = ptr;
  *head = next;      
 out:
  return next;
}  

/*Find a vmalloc_struct matching the given address*/
static struct vmalloc_struct *search_vmalloc(unsigned long addr) { 
  struct vmalloc_struct **head = NULL;
  struct vmalloc_struct *ptr = NULL;
  for(head = &vmalloc_head; (ptr = *head); head = &ptr->next ) 
    if(ptr->addr == addr) { 
      /*Bulls Eye!! Unlink the struct:*/
      *head = ptr->next;
      break;
    }
  return ptr;
} 

/*Vmalloc or virtual alloc that returns an address that
 is virtually contiguous but physically discontiguous:
*/
void *vmalloc(unsigned long size,int flags) { 
  struct vmalloc_struct *ptr = NULL;
  struct vmalloc_struct *search = NULL;
  void *addr = NULL;
  if(! size ) 
    goto out;

  size = PAGE_ALIGN(size);
  if(! (ptr = get_vmalloc_addr(size,flags) ) ) 
    goto out;
  
  /*Now setup the page tables for the address*/
  if(setup_pgd(ptr->addr,size,ptr->flags) < 0 ) {
    printf("Error in setting up the page tables:\n");
    /*Undo the changes that were done if any:*/
    free_pgd(ptr->addr,size,ptr->flags);
    goto out_free;
  }
  addr = (void*)ptr->addr;
  goto out;
 out_free:
  /*Rip off the vmalloc_struct*/
  if(! (search = search_vmalloc(ptr->addr) ) ) {
    printf("Unable to obtain vmalloc struct for addr=%#08lx\n",ptr->addr);
  }
  slab_free((void*)search);
 out:
  return addr;
}

/*vfree: or free a vmalloc object:
*/
void *vfree(void *addr) { 
  unsigned long virt_addr = (unsigned long )addr;
  void *address = NULL;
  struct vmalloc_struct *ptr = NULL;
  if(! virt_addr) 
    goto out;
 
  if(! (ptr = search_vmalloc(virt_addr) ) ) {
    printf("Unable to find vmalloc_struct for addr:%#08lx\n",virt_addr);
    goto out;
  }
  address = (void*)ptr->addr;
  /*Now rip off the page tables:*/
  if(free_pgd(ptr->addr,ptr->size - PAGE_SIZE,ptr->flags) < 0 ) {
    printf("Error in freeing up the page tables for addr(%#08lx)\n",ptr->addr);
  }
  slab_free((void*)ptr);
 out:
  return address;
}
  
