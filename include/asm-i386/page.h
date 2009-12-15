/*A.R.Karthick for MIR:
  Define the standard definitions for MM.
*/

#ifdef __KERNEL__

#ifndef _PAGE_H
#define _PAGE_H

#include <asm/string.h>
#include <asm/multiboot.h>

typedef struct {
  unsigned long pgd;
}pgd_t;

typedef struct {
  unsigned long pmd;
}pmd_t;

typedef struct {
  unsigned long pte;
}pte_t;

extern void *vmalloc(unsigned long size,int flags);
extern void *vfree(void *addr);

#define PAGE_SHIFT  12
#define PAGE_SIZE  ( 1 << PAGE_SHIFT)
#define PAGE_MASK  (~(PAGE_SIZE - 1) )
#define PAGE_ALIGN(val)  ( ( (val) + ~PAGE_MASK ) & PAGE_MASK )

/*These inline macros would be changing with page tables*/

#define phys_to_virt(address) ({address;})
#define virt_to_phys(address) ({address;})

#define phys_addr(pfn)  ( (pfn) << PAGE_SHIFT )
#define MAP_NR(addr)    ( (addr) >> PAGE_SHIFT )

/*Take the default memory to be 16MB*/
#define DEFAULT_MEM   ( (16UL << 20 ) >> PAGE_SHIFT )

/*This MACRO again would change to max_low_pfn once page tables
  are sanitized and high_mem is needed.For Phys addresses above 1 GB)
*/

/*Right now ,this is 1GB coz:*/

#define MAX_LOW_MEM   (1UL << 30 ) 
#define MAX_PFN       ( MAX_LOW_MEM >> (PAGE_SHIFT - 2) )

/*Leave a 50 MB gap between 1 GB,maybe for vmalloc pages*/
#define SPECIAL_RESERVE (64 << 20UL)
#define VMALLOC_RESERVE (128 << 20UL)

/*Leave an 8 MB hole to catch references to that region:*/
#define VMALLOC_HOLE   ( 8 << 20UL)
#define VMALLOC_START  (unsigned long) ( -SPECIAL_RESERVE - VMALLOC_RESERVE - VMALLOC_HOLE )
#define VMALLOC_END    (unsigned long) (-SPECIAL_RESERVE)
#define VMALLOC_START_ADDR  ( VMALLOC_START + VMALLOC_HOLE )

#define pages_to_kb(pages)  ( (pages) << (PAGE_SHIFT - 10) )
#define pages_to_mb(pages)  ( (pages) >> (20 - PAGE_SHIFT) )

extern unsigned long max_pfn,max_low_pfn,max_low;
extern unsigned long _bssend;
extern int reserved;
extern int init_mm(mb_info_t *);
extern int test_mm(void);
extern int fill_mm(void);
extern int init_mem_allocator(unsigned long,unsigned long,unsigned long,unsigned long);
extern unsigned long __get_free_pages(int,int);
extern unsigned char *alloc_mem_allocator(unsigned long,unsigned long);
extern int free_mem_allocator(unsigned long,unsigned long);
extern int free_mem_allocator_all(void);
extern int reserve_mem_allocator(unsigned long,unsigned long);
extern int op_mem_allocator(unsigned long,unsigned long,int);
extern int display_mem_stats(void);
extern int __free_pages(unsigned long,int);
extern void *vmalloc(unsigned long,int);
extern void *vfree(void *);

#define __get_free_page(priority)  __get_free_pages(0,priority)
#define __alloc_page(priority)     __get_free_page(priority)
#define __alloc_pages(order,priority) __get_free_pages(order,priority)
#define alloc_page(priority)           get_free_page(priority)
#define alloc_pages(order,priority)    get_free_pages(order,priority)
#define __free_page(area)              __free_pages(area,0)
#define free_page(area)                __free_pages(area,0)
#define free_pages(area,order)         __free_pages(area,order)
#define __free_pages_all               free_mem_allocator_all
#define free_pages_all                 free_mem_allocator_all


/*Define the allocation interfaces that zero off the memory after allocating.
  These are slow:
 */
static __inline__ unsigned long get_free_page(int priority) {
  unsigned long area = 0UL;
  area = __get_free_page(priority);
  if(area) {
    memset((void*)area,0x0,PAGE_SIZE);
  }
  return area;
}

static __inline__ unsigned long get_free_pages(int order,int priority) {
  unsigned long area = 0UL;
  area = __get_free_pages(order,priority); 
  if(area) {
    unsigned long size = ( 1 << order) << PAGE_SHIFT;
    memset((void*)area,0x0,size);
  }
  return area;
}

/*get the order based on size:
  Order is 2^n
 */
static __inline__ int get_order(int size) {
  int order = -1;
  size = (size - 1) >> (PAGE_SHIFT - 1);
  do { 
    size >>= 1;
    ++order;
  }while(size);
  return order;
}
  
#endif
#endif
