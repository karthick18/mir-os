/*Author: A.R.Karthick for MIR:
 pgtable macros and misc. stuffs
*/

#ifdef __KERNEL__
#ifndef _PGTABLE_H
#define _PGTABLE_H

#define USER_PGD_PTRS   768
#define KERNEL_PGD_PTRS 256

/*These are the page flags*/
#define PAGE_PRESENT_BIT   0x0
#define PAGE_RW_BIT        0x1
#define PAGE_USER_BIT      0x2
#define PAGE_ACCESSED_BIT  0x5
#define PAGE_DIRTY_BIT     0x6

/*Define the page flags*/
#define PAGE_PRESENT       ( 1 << PAGE_PRESENT_BIT)
#define PAGE_RW            ( 1 << PAGE_RW_BIT)
#define PAGE_USER          ( 1 << PAGE_USER_BIT)
#define PAGE_ACCESSED      ( 1 << PAGE_ACCESSED_BIT)
#define PAGE_DIRTY         ( 1 << PAGE_DIRTY_BIT)

#define __PAGE_KERNEL    ( PAGE_PRESENT | PAGE_RW | PAGE_ACCESSED | PAGE_DIRTY)
#define __PAGETABLE_KERNEL __PAGE_KERNEL

#define __PAGE_USER       (__PAGE_KERNEL | PAGE_USER)
#define __PAGETABLE_USER  __PAGE_USER


/*Define the page specific access macros for page tables*/
#define PAGE_OFFSET    0x00000000
#define __PAGE_OFFSET  PAGE_OFFSET

#define __pa(addr)     ( (unsigned long)(addr) - PAGE_OFFSET)

#define __va(addr)     ( (unsigned long)(addr) + PAGE_OFFSET)

#define PGDIR_SHIFT     22
#define PMD_SHIFT       22
#define PTE_SHIFT       12

#define PGDIR_SIZE     ( 1 << PGDIR_SHIFT)
#define PMD_SIZE       ( 1 << PMD_SHIFT )
#define PTE_SIZE       ( 1 << PMD_SHIFT )

#define PGDIR_MASK     (~(PGDIR_SIZE - 1) )
#define PMD_MASK       (~(PMD_SIZE - 1) )
#define PTE_MASK       (~(PTE_SIZE - 1) )

#define PTRS_PER_PGD   ( 1 << 10)
#define PTRS_PER_PMD    0x1
#define PTRS_PER_PTE   ( 1 << 10)

#define __MAP_NR(val)  ( __pa(val) >> PAGE_SHIFT )

/*Now define the page specific page table macros*/

#ifndef __ASSEMBLY__

struct pgtable_cache { 
  unsigned long *pgd_cache;
  unsigned long *pmd_cache;
  unsigned long *pte_cache;
  unsigned long pgd_cache_size;
  unsigned long pmd_cache_size;
  unsigned long pte_cache_size;
};
struct pgtable_cache_limit {  
  unsigned long pgd_cache_limit;
  unsigned long pmd_cache_limit;
  unsigned long pte_cache_limit;
};

extern struct pgtable_cache pgtable_cache;
extern struct pgtable_cache_limit pgtable_cache_limit;

#define PGD_CACHE  (pgtable_cache.pgd_cache)
#define PMD_CACHE  (pgtable_cache.pmd_cache)
#define PTE_CACHE  (pgtable_cache.pte_cache)

#define PGD_CACHE_LIMIT (pgtable_cache_limit.pgd_cache_limit)
#define PMD_CACHE_LIMIT (pgtable_cache_limit.pmd_cache_limit)
#define PTE_CACHE_LIMIT (pgtable_cache_limit.pte_cache_limit)

#define PGD_CACHE_SIZE  (pgtable_cache.pgd_cache_size)
#define PMD_CACHE_SIZE  (pgtable_cache.pmd_cache_size)
#define PTE_CACHE_SIZE  (pgtable_cache.pte_cache_size)

#define PGD_CACHE_MAX 0x20
#define PMD_CACHE_MAX 0x20
#define PTE_CACHE_MAX 0x100

extern pgd_t init_pgdir[PTRS_PER_PGD];

/*Switch mms, happens on a context switch:
  Switch pgdirectories.
  This is equivalent to a TLB flush:
*/
#define __switch_mm(prev_mm,next_mm) do {\
__asm__ __volatile__("movl %0,%%cr3\n\t"\
                     : :"r"(__pa((next_mm)->pgd) ) );\
}while(0)

#define get_pte(pte_entry)  ( __va((pte_entry).pte ))
#define get_pgd(pgd_entry)  ( __va((pgd_entry).pgd) )
#define get_pmd(pmd_entry)  ( __va((pmd_entry).pmd) )

#define pgd_index(addr) ({\
( ( (addr) >> PGDIR_SHIFT ) & ( PTRS_PER_PGD  - 1) );\
})

#define pgd_offset(mm,addr) ({\
mm->pgd + pgd_index(addr);\
})

#define pte_index(addr) ({\
( ( (addr) >> PTE_SHIFT ) & ( PTRS_PER_PTE - 1) );\
})

#define pmd_offset(pgd,addr) ({ (pmd_t*)pgd;})

#define pte_offset(pmd,addr) ({\
(pte_t*)( get_pmd(*pmd) & PAGE_MASK ) + pte_index(addr);\
})

#define mk_prot(val,prot)  ( __pa(val) | (prot) )
#define mk_phys_prot(val,prot) ( (val) | (prot) )

#define mk_page(type,val)  (type) { (val) }


#define pgd_none(pgd)    ( __pa(get_pgd(pgd)) == 0UL )
#define pmd_none(pmd)    ( __pa(get_pmd(pmd)) == 0UL )
#define pte_none(pte)    ( __pa(get_pte(pte)) == 0UL )


#define set_pgd_phys(pgd,val) do {\
int _prot = __PAGETABLE_KERNEL;\
if(!val) \
  _prot = 0;\
*(pgd) = mk_page(pgd_t,mk_phys_prot(val,_prot) );\
}while(0)

#define set_pmd_phys(pmd,val) do {\
int _prot = __PAGETABLE_KERNEL;\
if(!val)\
 _prot = 0;\
*(pmd) = mk_page(pmd_t,mk_phys_prot(val,_prot) );\
}while(0)

#define set_pte_phys(pte,val) do {\
int _prot = __PAGE_USER;\
if(! val) \
 _prot = 0;\
*(pte) = mk_page(pte_t,mk_phys_prot(val,_prot) );\
}while(0)

#define set_pgd(pgd,val) do { \
 *(pgd) = mk_page(pgd_t,mk_prot(val,__PAGETABLE_KERNEL) );\
}while(0)

#define set_pmd(pmd,val) do {\
*(pmd) = mk_page(pmd_t,mk_prot(val,__PAGETABLE_KERNEL) );\
}while(0)


#define set_pte(pte,val)  do {\
*(pte) = mk_page(pte_t,mk_prot(val,__PAGE_USER) );\
}while(0)

/*Cache pops are LIFOs*/
static __inline__ unsigned long pgtable_alloc_cache(unsigned long **cache,unsigned long *cache_size) { 
  unsigned long *page = NULL; 
  if( (page = *cache) ) { 
    if(--(*cache_size)  < 0 ) 
      *cache_size = 0;
    *cache = (unsigned long *)**cache; 
    page[0] = 0;
  } else {
    page = (unsigned long *)get_free_page(0);
  }
  return (unsigned long )page;
}

static __inline__ unsigned long pgd_alloc_cache(void) { 
  return pgtable_alloc_cache(&PGD_CACHE,&PGD_CACHE_SIZE);
}

static __inline__ unsigned long pmd_alloc_cache(void) {
  return pgtable_alloc_cache(&PMD_CACHE,&PMD_CACHE_SIZE);
}

static __inline__ unsigned long pte_alloc_cache(void) {
  return pgtable_alloc_cache(&PTE_CACHE,&PTE_CACHE_SIZE);
}
   
static __inline__ pgd_t *pgd_alloc(struct mm_struct *mm,unsigned long addr) {
  unsigned long page = 0UL;
#if 1
  if(! (page = pgd_alloc_cache() ) ) 
    goto out;
#else
  if(! (page = get_free_page(0) ) ) 
    goto out;
#endif
  mm->pgd = (pgd_t*) page; 
  return pgd_offset(mm,addr);
 out:
  return NULL;
}

static __inline__ pmd_t *pmd_alloc(pgd_t *pgd,unsigned long addr) {
  return (pmd_t*)pgd;
}

static __inline__ unsigned long pte_alloc_page(void) {
  return pte_alloc_cache();
}

static __inline__ pte_t *pte_alloc(pmd_t *pmd,unsigned long addr) {
  unsigned long page = get_pmd(*pmd) & PAGE_MASK;
  if(! page && (page = pmd_alloc_cache() ) ) 
   set_pmd(pmd,page);
  if(! page) 
    goto out;
  return (pte_t*)page + pte_index(addr);
 out:
  return NULL;
}  

static __inline__ int pgtable_free_cache(unsigned long **cache,unsigned long *cache_size,unsigned long cache_limit,unsigned long page) { 
  if(*cache_size < cache_limit) {
    /*free the page to the cache*/
    ++(*cache_size); 
    *(unsigned long *)page = (unsigned long) *cache;
    *cache = (unsigned long *)page;
  }
  else {
    __free_page(page);
  }
  return 0;
}
static __inline__ int pgd_free_cache(unsigned long page) { 
  return pgtable_free_cache(&PGD_CACHE,&PGD_CACHE_SIZE,PGD_CACHE_LIMIT,page);
}

static __inline__ int pmd_free_cache(unsigned long page) {
  return pgtable_free_cache(&PMD_CACHE,&PMD_CACHE_SIZE,PMD_CACHE_LIMIT,page);
}

static __inline__ int pte_free_cache(unsigned long page) { 
  return pgtable_free_cache(&PTE_CACHE,&PTE_CACHE_SIZE,PTE_CACHE_LIMIT,page);
}
   
static __inline__ int pte_free(pte_t *pte) { 
  int err =-1;
  if(! pte_none(*pte) ) {  
    err = 0;
#if 1
    pte_free_cache(get_pte(*pte) & PAGE_MASK);
#else
    __free_page(get_pte(*pte) & PAGE_MASK);
#endif
 
    set_pte_phys(pte,0); 
  }
  return err;
}

static __inline__ int pmd_free(pmd_t *pmd) {
  PARAMETER_NOT_USED(pmd);
  return 0;
}

static __inline__ int pgd_free(pgd_t *pgd) { 
  int err = -1;
  if(! pgd_none(*pgd) ) { 
    err = 0;
#if 1
    pmd_free_cache(get_pgd(*pgd) & PAGE_MASK);
#else
    __free_page(get_pgd(*pgd) & PAGE_MASK );
#endif
    set_pgd_phys(pgd,0);
  }
  return err;
}

#endif /*__ASSEMBLY__*/

#endif
#endif

