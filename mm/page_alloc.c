/*   MIR-OS :Memory Allocator
;    Copyright (C) 2003 A.R.Karthick 
;    <a.r.karthick@gmail.com,a_r_karthic@rediffmail.com>
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
; This is a simple bitmap memory allocator 
; that keeps track of free pages through
; a bitmap. The resemblance to the linux boot memory allocator
; might be there,but there it ends. 
; Its been written from scratch,and moreover this is 
; now the main memory allocator for MIR. This would/should 
; be replaced by buddy allocator which can take this guys place
; after the system has started up.
; If you need  page sized allocatios,use this guy.
; If you need lesser than page allocations,
; then use the slab cache allocator sitting on top of this guy.
; But then,this guy also supports less than page size allocations,
; but freeing would end up freeing a page.So be careful
; if you are going for less than page sized allocations.
*/

#include <mir/kernel.h>
#include <asm/bitops.h>
#include <mir/sched.h>
#include <asm/pgtable.h>
#include <asm/processor.h>
#include <asm/setup.h>
#include <mir/init.h>

/*The linker should be giving us a variable
  named _end for bss_end,but it isnt giving us,even when loaded as ELF by GRUB.
  So we fetch it from the kernels symbol table or ELF header provided to us by GRUB.Apart from that,we also fetch init section start so that we can rip
  that code off.
*/

#define BSS_END   _bssend + (0x200000 - _bssend)

#define KERNEL_START 0x100000
#define MM_INIT   (mem_alloc.bitmap != NULL )
#define BITMAP_BYTES(nr)     ( ( (nr) + 7 )/8 )
#define RESERVE 1
#define CLEAR   0

#define TOTAL_FREE mem_alloc.mem_stats.free
#define TOTAL_MEM  mem_alloc.mem_stats.total

#define UPDATE_FREE(amount,action) do { \
  int _amount = amount * action;\
  mem_alloc.mem_stats.free += _amount;\
  mem_alloc.mem_stats.active -= _amount;\
  if(mem_alloc.mem_stats.active < 0) { \
    mem_alloc.mem_stats.active = 0;\
  }\
  if(mem_alloc.mem_stats.free > TOTAL_MEM ) { \
    mem_alloc.mem_stats.free = TOTAL_MEM;\
  }\
  if(mem_alloc.mem_stats.active > max_pfn) { \
   mem_alloc.mem_stats.active = max_pfn;\
 }\
  if(mem_alloc.mem_stats.free < 0 ) { \
   mem_alloc.mem_stats.free = 0; \
 }\
}while(0)


/*This keeps track of memory statistics*/

struct mem_stats { 
  int free;
  int active;
  int total;
  int bitmap_pages;
};

/*The structure for the memory allocator: 
  This guy is local to this file and shouldnt be exported
  to others. Access through interfaces
*/
struct mem_alloc { 
  unsigned long start; /*start page frame number*/
  unsigned long end;  /*end page frame number*/
  unsigned char *bitmap; /*the bitmap for the memory allocator*/
  unsigned long mem_start; /*The start of the allocations for the bitmap allocator*/
  /*This parameter keeps track of merging allocations.
    This wouldnt/shouldnt be used.But anyway,will be useful
    for future migrations to buddy systems
  */
  unsigned long last_pos; 
  /*This does the same as above.
    But maintains the offset correctly
  */
  unsigned long last_offset; 
  /*This keeps track of querying about memory statistics*/
  struct mem_stats mem_stats;
};

static struct mem_alloc mem_alloc = { bitmap: NULL } ;

extern int reserve_mem_allocator(unsigned long address,unsigned long size);

/*Initialise the bitmap allocator:*/

int init_mem_allocator(unsigned long start,unsigned long end,unsigned long map_start,unsigned long mem_start) { 
  unsigned int map_bytes = 0,map_pages = 0,err = -1;
  /*Initialise the value to the defaults*/
  mem_alloc.start = 0;
  mem_alloc.end   = DEFAULT_MEM;
  mem_alloc.last_offset = 0;
  mem_alloc.last_pos    = 0;
  mem_alloc.mem_start   = 0;
  if(!map_start || map_start >= max_pfn ) { 
    /*We panic out here. We can return a bad status
      and let the panic happen elsewhere.
     */
    panic("Bad map_start %#08lx\n",phys_addr(map_start) );
  }

  mem_alloc.bitmap =(unsigned char*)phys_to_virt( phys_addr(map_start) ) ;
  
  /*Check for overflows*/
  if(start + end < start  || end < start) { 
    panic("Bad memory addresses:start(%#08lx),end(%#08lx)\n",phys_addr(start),phys_addr(end) );
  }
  
  if(start >= max_pfn) {
    panic("Bad memory addresses:start(%#08lx)\n",phys_addr(start) );
  }
  
  if(mem_start >= end) {
    panic("Bad mem start: mem_start(%#08lx)\n",phys_addr(mem_start) );
  }

  mem_alloc.start = start;
  if(end <= max_pfn)
   mem_alloc.end   = end;
  mem_alloc.mem_start = phys_addr(mem_start);
  map_bytes = BITMAP_BYTES(mem_alloc.end - mem_alloc.start);
  map_pages = map_bytes >> PAGE_SHIFT; 
  if(! map_pages) 
    ++map_pages;
  mem_alloc.mem_stats.bitmap_pages = map_pages;
  mem_alloc.mem_stats.free = mem_alloc.end - mem_alloc.start;
  mem_alloc.mem_stats.active = 0;
  mem_alloc.mem_stats.total  = mem_alloc.end - mem_alloc.start;
  /*Initially all pages are free*/
  memset(mem_alloc.bitmap,0x0,map_bytes);
  err = op_mem_allocator(map_start,map_start + map_pages,RESERVE); 
  if(err < 0) {
    printf("Error in reserving bitmap pages (%#08lx)\n",map_pages);
  }
  return err;
}

/*Panic if the MM hasnt been initialised and 
  the user tries to perform [de]allocations:
*/
static volatile void mem_panic(void) { 
  panic("Oops!!! MM not initialised:\n");
}

/*Perform operations: Set,or clear bit on the memory allocator*/

int op_mem_allocator(unsigned long sidx,unsigned long eidx,int op) {
  int i,err = -1,amount = 0,action;
  /*We do some sanity checking even though it may seem ridiculous.
    Thats because it would make this guy pretty robust to be called
    from outside.
  */
  if(sidx > eidx || eidx > mem_alloc.end) {
    goto out;
  }
  for( i = sidx; i < eidx; ++i ) { 
    switch(op) { 
     case RESERVE:
       if(test_and_set_bit(i,(void*)mem_alloc.bitmap) ) { 
 #ifdef DEBUG
	 printf("Oops!! Page(%d) already seems to be reserved:\n",i);
 #endif
       }
       else {
         ++amount;
       }
       break;
    case CLEAR:
      if(!test_and_clear_bit(i,(void*)mem_alloc.bitmap) ) { 
 #ifdef DEBUG
        printf("Oops!! Page(%d) already seems to be cleared:\n",i);
 #endif
      }else{ 
	++amount;
      }
      break;
    default: 
      /*Nothing*/
      do { }while(0);
    }
  }
  action = (op == CLEAR) ? 1 : -1;
  if(amount)
   UPDATE_FREE(amount,action);
  err = amount;
 out:
  return err;
}
        
  
int reserve_mem_allocator(unsigned long addr,unsigned long size) {
  unsigned long sidx,eidx,start;
  int err = -1;
  if(! MM_INIT) {
    mem_panic();
  }

  if(addr + size < addr ) { 
    printf("Address range overflow:Addr (%#08lx),Size(%#08lx)\n",addr,size);
    goto out;
  }
  if(addr < mem_alloc.mem_start) {
    printf("Address less than init mem start(%#08lx)\n",addr);
    goto out;
  }

  start = addr - mem_alloc.mem_start;
  sidx = (start + ~PAGE_MASK ) >> PAGE_SHIFT;
  eidx = (start + size + ~PAGE_MASK) >> PAGE_SHIFT;
  
  if(sidx > eidx || eidx > mem_alloc.end) {
    goto out;
  }
  err  = op_mem_allocator(sidx,eidx,RESERVE);
 out:  
  return err;
}

/*Free up the pages in the bitmap*/
int free_mem_allocator(unsigned long addr,unsigned long size) {
  unsigned long sidx,eidx,start;
  int err = -1;

  if(! MM_INIT) {
    mem_panic();
  }
  
  if(addr + size < addr ) { 
    goto out;
  }
  if(addr < mem_alloc.mem_start) {
    goto out;
  }
  if(size < PAGE_SIZE) 
    goto out;

  start = addr - mem_alloc.mem_start;
  sidx = (start + ~PAGE_MASK) >> PAGE_SHIFT;
  eidx = (start + size + ~PAGE_MASK ) >> PAGE_SHIFT;
  if(sidx > eidx || eidx > mem_alloc.end) {
    goto out;
  }
  err = op_mem_allocator(sidx,eidx,CLEAR);
  mem_alloc.last_pos = mem_alloc.last_offset = 0;  
 out:
  return err;
}

/*The main bitmap allocator:
  This has support to alloc less than
  1 page granularity.
  But shouldnt be used because you cannot free
  anything less than 1 page:
*/
static unsigned char * __alloc_mem_allocator(unsigned long area,unsigned long align) {
  int i,j,start,offset ;
  unsigned char *addr = NULL;
  unsigned long pages =(area + ~PAGE_MASK) >>  PAGE_SHIFT;
  
  /*Now align the offset to SMP_CACHELINE if it isnt*/ 
  offset = mem_alloc.last_offset;

  if(offset & (align - 1) ) { 
    offset = ALIGN_VAL(offset,SMP_CACHE_ALIGN);
  }    

  /*Okay now for an alloc hit.
    We scan to get contiguous _pages_ 
    while scanning.
  */
 
  for(i = 0;  i < mem_alloc.end ; ++i ) {
    if(test_bit(i,(void*)mem_alloc.bitmap) ) {
      continue;
    }
    /*Now go for a contiguous scan to see if you get a alloc hit*/
    for( j  = i+1 ; j < i + pages; ++j ) 
      {
	if(j >= mem_alloc.end) {
	  goto fail;
        }
        if(test_bit(j,(void*)mem_alloc.bitmap) ) {
	  goto next;
	} 
      }
    /*If we are here:We are successfull*/
    start = i;
    goto found;
  next:;
  }
 fail: 
  printf("Unable to allocate contiguous pages:\n");
  goto out;

 found:
 /*We are here when we are successfull*/
  
  /*Okay!! Lets try to merge the allocations,if its really possible*/
  if(offset < PAGE_SIZE && 
    mem_alloc.last_offset &&
    start == mem_alloc.last_pos + 1)
    {
      int size = area;
      int remaining_size = PAGE_SIZE - offset;
      addr = (unsigned char *)phys_to_virt(mem_alloc.mem_start + phys_addr(mem_alloc.last_pos) + offset);
      if(size < remaining_size ) {
        mem_alloc.last_offset = offset + size;
        pages = 0; /*No pages allocated*/ 
      }else { 
	remaining_size = size - remaining_size;
        pages = (remaining_size + ~PAGE_MASK) >> PAGE_SHIFT;
        mem_alloc.last_offset =  (remaining_size & ~PAGE_MASK);
        if(pages)
         mem_alloc.last_pos += pages - 1;
      }
    }else {
      mem_alloc.last_pos = start + pages - 1;
      mem_alloc.last_offset = area & ~PAGE_MASK;
      addr = (unsigned char *)phys_to_virt(mem_alloc.mem_start + phys_addr(start) );
    }

  /*Now reserve the pages*/
  if(pages) {
    int err = op_mem_allocator(start,start+pages,RESERVE);
#ifdef DEBUG
    printf("Reserving pages (%#08lx-%08lx):\n",(unsigned long)addr,(unsigned long) addr + (pages * PAGE_SIZE));
#endif
    if(err < 0) 
      addr = NULL;
  }
 out:
  return addr;
}

/*Free off all the pages of the Mem Allocator*/

int free_mem_allocator_all(void) { 
  unsigned int end = mem_alloc.end;
  unsigned int start = mem_alloc.start;
  unsigned int map_bytes ;

  if(! MM_INIT ) {
    mem_panic();
  }
  map_bytes =  BITMAP_BYTES(end - start);
  memset(mem_alloc.bitmap,0x0,map_bytes);
  mem_alloc.mem_stats.free = mem_alloc.end - mem_alloc.start;
  mem_alloc.mem_stats.active = 0;
  mem_alloc.mem_stats.total = mem_alloc.mem_stats.free;
  mem_alloc.mem_stats.bitmap_pages = 0;
  return  0;
}

/*If you want to directly give in size,
  which is not prescribed:
  Use this function
*/

unsigned char *alloc_mem_allocator(unsigned long size,unsigned long align) { 
  unsigned char *addr = NULL;
  unsigned long pages = (size + ~PAGE_MASK) >> PAGE_SHIFT;
  /*Ok!! Sanity tests*/

  if(! MM_INIT ) {
    mem_panic();
  }
  if(!pages || pages > TOTAL_FREE) {
    goto out;
  }
  /*Check that the alignment is less than a power of 2 
    and less than page_size
  */
  if( ( align & (align - 1) )  
     || 
      (align >= PAGE_SIZE)
      )
    {
      printf("Invalid alignment specified to the allocator:(%#08lx)\n",align);
      goto out;
    }
  addr = __alloc_mem_allocator(size,align);

#ifdef DEBUG
  if(! addr)
   printf("Error in Memory allocation for pages(%#08lx),File(%s),Line(%d):\n",pages,__FILE__,__LINE__);
#endif

 out:
  return addr;
}

/*Okay!!. Give user the leverage
  to command pages in Orders.
  We dont have priority.But should be future stuff!!
*/

unsigned long __get_free_pages(int order,int priority) {
  unsigned long size = ( 1 << order) << PAGE_SHIFT;
  unsigned long area = 0UL;
  unsigned char *addr = alloc_mem_allocator(size,0);
  PARAMETER_NOT_USED(priority);
  if(addr) 
    area = (unsigned long)addr;
  return area;
}

/*Okay!! Let the hard work till now be reflected to the user.*/

int display_mem_stats(void) { 
  int free,active,total,bitmap,special,err = -1;
  unsigned long end = BSS_END;
  if(! MM_INIT) { 
    printf("MM hasnt been initialised yet:\n"); 
    goto out;
  }
  free =   pages_to_kb(mem_alloc.mem_stats.free);
  active = pages_to_kb(mem_alloc.mem_stats.active);
  total =  pages_to_kb(mem_alloc.mem_stats.total);
  bitmap = pages_to_kb(mem_alloc.mem_stats.bitmap_pages);
  special = pages_to_kb(reserved);
  printf("%13s%13s%13s%13s%13s\n","Total","Used","Free","Bitmap","Special"); 
  printf("%12dkb%11dkb%12dkb%7dkb%12dkb\n",total,active,free,bitmap,special); 
  err = 0;
 out:
  return err;
}

int __free_pages(unsigned long area,int order) {
  int err = -1;
  unsigned long size = 0UL;
  if(order < 0 || area <= 0) {
    goto out;
  }
  size = ( 1 << order) << PAGE_SHIFT;
  err = free_mem_allocator(area,size);
 out:
  return err;
}

/*This guy reserves the Kernel upto bss_end.  
  Reserves the first,second page.*/


static int __init reserve_special_pages(mb_info_t *mb) {  
  unsigned long kernel_start = KERNEL_START;
  unsigned long size = BSS_END - kernel_start;
  reserve_e820();
  reserved += reserve_mem_allocator(0,0x100000);
  reserved += reserve_mem_allocator(kernel_start,size);
  return 0;
}

int __init init_mm(mb_info_t *mb) { 
  unsigned long map_start_pfn = 0;
  unsigned long end = 0UL;
  init_e820(mb);

#if 0
  #ifdef MEM
   max_pfn = MEM;
   max_pfn = (max_pfn << 20) >> PAGE_SHIFT;
  #else
   max_pfn = DEFAULT_MEM;
  #endif
#endif

   end = BSS_END;
   if(!max_pfn ) {
     printf("MAX_PFN not evaluated.Resetting Max pfn to default...:\n");
     max_pfn = DEFAULT_MEM;
   }
   printf("Displaying the E820 BIOS MAP:\n");
   display_e820();
   map_start_pfn = PAGE_ALIGN(end) >> PAGE_SHIFT;
   printf("Bss end =%#08lx,map_start_pfn=%#08lx,max_pfn=%#08lx:\n",end,map_start_pfn,max_pfn);
   init_mem_allocator(0,max_pfn,map_start_pfn,0UL);
   printf("MM Initialised with (%#08lx-%#08lx) pages\n",0UL,max_pfn);
   /*Once the MM has been initialised,reserve the special pages*/
   reserve_special_pages(mb);  
   paging_init();
   return 0;
}


struct test_region { 
    char name[40];
    int age;
    unsigned char padding[PAGE_SIZE - (40 + sizeof(int)) ];
  }__attribute__((packed));

int test_mm() { 
  struct test_region *test_region = NULL;
  int samples[10] = { 1,3,4 }, *ptr, i;
  test_region = (struct test_region*)__get_free_pages(2,0);
  if(!test_region) { 
    printf("Unable to allocate memory:\n");
    goto out;
  }
  strcpy(test_region->name,"A.R.Karthick for MIR OS");
  test_region->age = 26;
  printf("Test name=%s,age=%d\n",test_region->name,test_region->age);
  ptr = (int *) ( test_region + 1); 
  memset(ptr,0x0,sizeof(samples));
  memcpy(ptr,samples,sizeof(int) * 3);
  printf("Displaying Test Samples:\n");
  for(i = 0 ; i < sizeof(samples)/sizeof(samples[0]) ; ++i) 
    printf("Sample nr(%d) = (%d)\n",i+1,ptr[i]);

  __free_pages((unsigned long)test_region,2);
 out: 
  return 0;
}

struct region { 
  char *name; 
  int age;
  unsigned char padding[PAGE_SIZE - 8];
}__attribute__ ((packed));

int  fill_mm(void) 
{
    /*Now do a test with chunks*/
    struct region *ptr[1024];
    int i;
    for(i = 0; i < 1024; ++i) {
      if( ! (ptr[i] = (struct region *)alloc_mem_allocator(sizeof(struct region) + 40,SMP_CACHE_ALIGN) ) ) {
        printf("Unable to allocate memory:\n");
        goto out;
      } 
      ptr[i]->name = (char *) ( ptr[i] + 1 );
      /*Just fill it up with values*/
      strcpy(ptr[i]->name,"A.R.Karthick for MIR OS:");
      ptr[i]->age = 26;
    }
    for(i = 20; i < 22; ++i) { 
      printf("Name=%s,Age=%d\n",ptr[i]->name,ptr[i]->age);
    }
 out:
    return 0;
}
