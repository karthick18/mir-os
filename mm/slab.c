/*   MIR-OS :SLAB CACHE ALLOCATOR
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
;  A.R.Karthick for MIR:
;  An Implementation of the Slab Cache Allocator for MIR.
;  This is documented in the Unix Kernel Internals guide,
;  and is used as a standard for most of the Kernels.
;  Objects exists in various caches of different sizes.
;  Each object of a particular size belongs to a cache.
;  Each cache has a list of partially full,full and empty slabs.
;  Objects are allocated from the partially full slabs.
;  We will keep the slab and the cache pointers for each object
;  in the nearest page boundary.
;
*/

#include <mir/kernel.h>
#include <mir/sched.h>
#include <asm/pgtable.h>
#include <mir/slab.h>
#include <mir/data_str/dlist.h>
#include <mir/init.h>

#define NR  1024
#define MAX_ORDER 5
#define SLAB_MARKER -1
#define slab_buffer(slab)   ((int*)(slab+1))
#define CACHE_NAMELEN   40
#define INC_ACTIVE(cache) do { ++cache->nr_used; } while(0)
#define INC_ALLOCATED(cache) do { ++cache->nr_allocated;}while(0)
#define DEC_ACTIVE(cache)    do { --cache->nr_used; }while(0)
/*Max 32k with slab_alloc*/
#define SLAB_ALLOC_MAX      32768
#define OFF_SLAB     0x4
#define SLAB_LIMIT  ( PAGE_SIZE >> 3 )
#define SET_SLAB_POINTERS(obj,cache,slab) \
  do { \
    unsigned long *addr= (unsigned long *)obj;\
    *addr++ = (unsigned long)cache;\
    *addr++ = (unsigned long)slab;\
  }while(0)

#define GET_BASE_SLAB(obj) ((unsigned long*)((unsigned long)(obj) & PAGE_MASK ) )
#define GET_SLAB_CACHE(obj) ({ \
   struct slab_cache *_cache = (struct slab_cache *)*GET_BASE_SLAB(obj);\
   _cache;})
#define GET_SLAB(obj) ({\
   unsigned long *_ptr = GET_BASE_SLAB(obj);\
   ++_ptr;\
   (struct slab* )*_ptr;\
})

/*Define the slab management structure for object*/

struct slab {
  struct list_head s_head; /*index into the list of slabs*/
  void *s_mem; /*base memory start for the objects allocated*/
  int s_used; /*the nr. of objects used from this slab*/
  int s_offset; /*The offset for the start of the object allocation*/
  int s_free; /*the index into the free object for this slab*/
};

/*The definition of cache structure*/
struct slab_cache { 
  struct list_head next; /*index into the next cache*/
  struct list_head slab_full;
  struct list_head slab_partial; 
  struct list_head slab_free;
  struct slab_cache  *slab_p; /*This is the pointer to the slab management object,which is kept OFF SLAB*/

  char name[CACHE_NAMELEN]; /*The namelen of the cache*/
  int size; /*object size*/
  int order; /*order of allocations per slab*/
  int nr_objects;/*Nr. of objects that can be allocated per slab*/
  int priority; /*priority of the order of allocations:unused*/
  int nr_allocated;/*nr of objects allocated in the slab cache*/
  int nr_used; /*nr of objects that are in use or active*/
  int off_slab;/*whether off_slab or not*/
  int growing; /*whether cache is growing or not*/
};

/*Define the sizes cache:
  This is a list of object sizes that are already 
  initialised in the slab cache*/
struct cache_sizes { 
  int size;
  struct slab_cache *ptr;
};


/*Initialise the mother of all caches or the head of the slab cache*/
#define INIT_CACHE_HEAD(cache) \
name: "cache_head", \
size: sizeof(struct slab_cache), \
order: 0 , \
next: INIT_LIST_HEAD(cache.next),\
slab_full: INIT_LIST_HEAD(cache.slab_full),\
slab_partial: INIT_LIST_HEAD(cache.slab_partial),\
slab_free: INIT_LIST_HEAD(cache.slab_free),\
slab_p: NULL

static struct slab_cache cache_head = { INIT_CACHE_HEAD(cache_head) };

/*Initialise the sizes structure*/

/*This can hold a max. of order 5 pages.*/

static struct cache_sizes cache_sizes[] = { 
  { 1 << 5, NULL } ,
  { 1 << 6, NULL },
  { 1 << 7, NULL },
  { 1 << 8, NULL },
  { 1 << 9, NULL },
  { 1 << 10,NULL },
  { 1 << 11,NULL },
  { 1 << 12,NULL },
  { 1 << 13,NULL },
  { 1 << 14,NULL },
  { 1 << 15,NULL },
  { 0      ,NULL },
};

/*Initialise all the caches of different sizes*/

static int __init slab_cache_sizes_init(void) { 
  int i;  
  for( i = 0; cache_sizes[i].size ; ++i) { 
    char name[CACHE_NAMELEN];
    struct slab_cache *ptr = NULL;
    /*Create a name for the cache*/
    sprintf(name,"%s-%d","size",cache_sizes[i].size);
    /*Now we create the cache*/
    ptr = slab_cache_create(name,cache_sizes[i].size,0,0);
    if(! ptr) { 
      /*Panic straight away*/
      panic("Unable to create cache for object size:%d\n",cache_sizes[i].size); 
    }
    /*Now link this guy into the cache sizes*/
    cache_sizes[i].ptr = ptr;
  }
  return 0;
}

/*Estimate the number of objects in the slab cache:
  For the slab being OFF_SLAB,there shouldnt be that much wastage.
 */
static int slab_cache_estimate(int order,int size,int *nr,int *wastage) {
  int i = 0,extra = 0,base = 0;
  int ctrl_data = 2 *sizeof(void*);
  int total = ( 1 << order) << PAGE_SHIFT;
  if(size < SLAB_LIMIT)
    {
      base = sizeof(struct slab);
      extra = sizeof(int);
    }
  while( (i*size + L1_CACHE_ALIGN(base + i*extra + ctrl_data ) )  <= total) 
    ++i;
  if(i)
    --i;
  *wastage = total - ( i * size + L1_CACHE_ALIGN(base + i*extra + ctrl_data ) );
  *nr = i;
  return i;
}

static int slab_get_objects(int *obj_size,int align,int *ord,int *left) {
  int order = 0; 
  int nr = 0,wastage = 0,size = *obj_size;
  /*Fix the alignment issues :
    We align the objects to the nearest 2^n boundary.
    This way we can have multiple objects in a single cacheline boundary 
  */
  while (size < align/2) 
    align /= 2;  
  if(align) 
    size = ALIGN_VAL(size,align);
  do { 
    slab_cache_estimate(order,size,&nr,&wastage); 
    if(!nr) 
      continue;
    /*Wastage or internal fragmentation is acceptable to 1/8th*/
    if( (wastage*8) > ( (1 << order)<< PAGE_SHIFT ) ) {
 #if 0
      printf("Quite High internal fragmentation for object size(%d),order(%d)\n",size,order);
 #endif
    }
    break;
  } while(++order <= MAX_ORDER );

  *ord = order;
  *left = wastage;
  *obj_size = size;
  return nr;
}
 
static struct slab_cache *slab_cache_find_sizes(int size,int priority) { 
  int i;
  struct slab_cache *ptr = NULL;
  PARAMETER_NOT_USED(priority);
  for( i = 0; cache_sizes[i].size ; ++i) { 
    if(size > cache_sizes[i].size) 
      continue;
    ptr = cache_sizes[i].ptr;
    break;
  }
  return ptr;
}

/*Initialise the main cache*/
static int __init slab_cache_head_init(void) { 
  int size = sizeof(struct slab_cache); 
  int align = SLAB_HWCACHE_ALIGN;
  int nr = 0,wastage = 0,order = 0;
  nr = slab_get_objects(&size,align,&order,&wastage);
  if(! nr || order > MAX_ORDER) {
    panic("Unable to initialise cache_head:\n");
    goto out;
  }
  cache_head.nr_objects = nr;
  cache_head.size = size;
  cache_head.order = order;
 out:
  return 0;
}

/*Initialist the slab cache*/
int __init slab_cache_init() {
  slab_cache_head_init();
  slab_cache_sizes_init();
  return 0;
}
  
/*Create a cache for the size specified*/

struct slab_cache *slab_cache_create( char *name,int size,int align,int priority)
{
  struct slab_cache *cache = NULL;
  int nr = 0,wastage= 0,order = 0,slab_size = 0; 
  struct list_head *traverse = &cache_head.next;
  /*Sanity checking*/
  if(! name || strlen(name) > CACHE_NAMELEN) {
    goto out;
  }
  if( MAX_ORDER < get_order(size) ) {
    goto out;
  }
  /*See if the cache doesnt already exist by this name*/
  do { 
    struct slab_cache *ptr = list_entry(traverse,struct slab_cache,next);
    if(!strcmp(ptr->name,name) ) {
      goto out;
    }
  }while((traverse  = traverse->next) != &cache_head.next );

  nr = slab_get_objects(&size,align,&order,&wastage);

  if(! nr || order > MAX_ORDER) {  
    printf("Unable to allocate object(slab_cache_create):(%d-%d)\n",nr,order);
    goto out;
  }
  /*Now allocate the cache*/
  cache = (struct slab_cache *)slab_cache_alloc(&cache_head,priority);
  if(! cache)
    {
      printf("Unable to allocate a cache from cache_head:\n");
      goto out; 
    }
  memset(cache,0x0,sizeof(*cache));
  /*Fill up the fields*/
  list_head_init(&cache->slab_full);
  list_head_init(&cache->slab_partial);
  list_head_init(&cache->slab_free);
  cache->nr_objects = nr;
  cache->size = size;
  cache->order = order;
  cache->priority = priority;
  strcpy(cache->name,name);
  cache->slab_p = NULL;
  slab_size = sizeof(struct slab) + sizeof(int) * nr;
  /*Check if this can be accomodated within the slab*/
  if(size >= SLAB_LIMIT && wastage > L1_CACHE_ALIGN(slab_size+ 2*sizeof(void*) ) ) { 
    wastage -= L1_CACHE_ALIGN(slab_size + 2*sizeof(void*) );
  }
  else if(size >= SLAB_LIMIT) {
    cache->off_slab |= OFF_SLAB;
    cache->slab_p = slab_cache_find_sizes(slab_size,priority);
   if(! cache->slab_p) 
    {
      printf("Unable to get slab management object for the cache %s\n",name);
      goto out_free;
    }
  }
  /*Now link this guy into the cache chain*/
  list_add(&cache->next,&cache_head.next);  
  goto out;
 out_free:
  slab_cache_free(&cache_head,(void*)cache);
  cache = NULL;
 out:
  return cache;
}

/*Get the pages from the slab*/
static unsigned long slab_get_pages(struct slab_cache *cache) { 
  unsigned long addr = 0UL;
  addr = __get_free_pages(cache->order,cache->priority);
  if(! addr) {
    printf("Unable to get pages for the slab:\n");
  }
  if(addr  && (addr & ~PAGE_MASK) ) 
    printf("Addr (%#08lx)is not Page aligned:\n",addr);
  return addr;
}

static void slab_free_pages(struct slab_cache *cache,struct slab *slab) { 
  unsigned long addr = 0UL,order = cache->order;
  addr = (unsigned long) ( (unsigned char *)slab->s_mem - slab->s_offset );
  if( __free_pages((unsigned long)addr,order) < 0) {
    printf("Unable to free slab pages for %#08lx\n",(unsigned long)addr);
  }
}

/*Initialise the slab and return the pointer to the slab*/

struct slab *slab_cache_get_slab(struct slab_cache *cache,void *obj) { 
  struct slab *slab = NULL;
  int slab_size = 0,i;
  /*Check if the slab can be placed on the object itself
    or OFF_SLAB*/
  if((cache->off_slab & OFF_SLAB) ) {
    if(cache->slab_p)
     slab = slab_cache_alloc(cache->slab_p,cache->priority);  
    if(! slab) 
      goto out;
    slab->s_offset = L1_CACHE_ALIGN(2*sizeof(void*));
  }
  else { 
    slab = (struct slab *)((unsigned char *)obj + 2*sizeof(void*) );
    slab_size = sizeof(struct slab) + sizeof(int) * cache->nr_objects;
    slab_size += 2*sizeof(void*);
    slab->s_offset = L1_CACHE_ALIGN(slab_size);
  }
  slab->s_mem   = (void *) ((unsigned char*)obj + slab->s_offset );
  slab->s_used = 0;
  slab->s_free = 0;
  list_head_init(&slab->s_head);
  for(i = 0; i < cache->nr_objects; ++i) 
    slab_buffer(slab)[i] = i+1;
   
  slab_buffer(slab)[i-1] = SLAB_MARKER;
  /*Now set up the obj to the correct slab and cache pointer*/
  SET_SLAB_POINTERS(obj,cache,slab);
 out:
  return slab;  
}

/*Slab allocation routines*/

/*Grow the slab cache by a slab*/
void *slab_cache_grow(struct slab_cache *cache) { 
  unsigned long addr = 0UL;
  struct slab *slab = NULL;
  ++cache->growing;
  if(!( addr = slab_get_pages(cache) ) ) {
    printf("Unable to get pages from the bitmap allocator:\n");
    goto out;
  }
  /*Now get the slab object*/
  if(! (slab = slab_cache_get_slab(cache,(void*)addr ) ) ) {
    printf("Unable to get the slab for the cache %s\n",cache->name);
    goto out_free;
  }
  /*Now we have the slab:Link it up in the cache in the free slab list*/    
  list_add(&slab->s_head,&cache->slab_free);
  goto out;
 out_free:
  __free_pages(addr,cache->order);
  addr = 0UL;
 out:
  --cache->growing;
  return (void*)addr;
}

/*Allocate an object from the slab*/
static void *slab_cache_alloc_slab(struct slab_cache *cache,struct slab *slab) { 
  int obj_index ;
  void *obj = NULL;
  if(! cache || !slab || !cache->size ) {
    char *str = NULL;
    if(!cache) {
      str = "Invalid Cache pointer";
    } 
    else if(!slab) {
      str = "Invalid slab pointer";
    }  
    else if(!cache->size) {
      str = "Invalid cache size"; 
    }
    panic("%s\n",str);
  }
  obj_index = slab->s_free;
  slab->s_free = slab_buffer(slab)[obj_index];
  
  /*Now get the object*/
  obj = (void*) ( (unsigned char*)slab->s_mem + obj_index*cache->size );

  INC_ACTIVE(cache);
  INC_ALLOCATED(cache);
  ++slab->s_used;

  /*Check if the slab has filled up*/
  if(slab->s_free == SLAB_MARKER) {
    /*Move to slab_full list*/
    list_del(&slab->s_head); 
    list_add(&slab->s_head,&cache->slab_full);
  }
  return obj;
}

void  *slab_cache_alloc(struct slab_cache *cache,int priority) { 
  void *obj = NULL;
  struct list_head *head = &cache->slab_partial;
  struct slab *slab = NULL;
  PARAMETER_NOT_USED(priority);
  /*First check if there is a slab already available*/
  if(LIST_EMPTY(head) ) { 
    struct list_head *traverse = &cache->slab_free;
    struct list_head *ptr = NULL;
    if(LIST_EMPTY(traverse) ) { 
      obj = slab_cache_grow(cache);
      if(obj == NULL) { 
	printf("Error in growing the cache %s\n",cache->name);
        goto out;
      }
    }
    ptr = traverse->next;
    list_del(ptr);
    list_add(ptr,&cache->slab_partial);
  }
  slab = list_entry(head->next,struct slab,s_head);
  obj =  slab_cache_alloc_slab(cache,slab);
 out:
  return obj;
}    

/*Slab alloc, and slab free work on the sizes cache*/    
  
void *slab_alloc(int size,int priority) { 
  struct slab_cache *cache = NULL;
  void *obj = NULL;
  if(size > SLAB_ALLOC_MAX) 
    goto out;
  if(! (cache = slab_cache_find_sizes(size,priority) ) ) {
    goto out;
  }
  obj = slab_cache_alloc(cache,priority);
 out:
  return obj;
}

static int slab_cache_free_slab(struct slab_cache *cache,struct slab *slab,void *obj) 
{
  int obj_index = 0;
  int used = slab->s_used;
  obj_index =  ( (unsigned char*)obj - (unsigned char*)slab->s_mem)/cache->size;
  slab_buffer(slab)[obj_index] = slab->s_free;
  slab->s_free = obj_index;
  --slab->s_used;
  DEC_ACTIVE(cache);
  if(! slab->s_used ) { 
    /*slab is empty.Move to free list from partial list*/
    list_del(&slab->s_head);
    list_add(&slab->s_head,&cache->slab_free);
  }
  else if(used == cache->nr_objects) { 
    /*Was full before:Now we have taken up one.
      Move to partial from full*/
    list_del(&slab->s_head);
    list_add(&slab->s_head,&cache->slab_partial);
  }
  return 0;
}

/*Free the slabs*/
int slab_cache_free(struct slab_cache *cache,void *obj) { 
 struct slab *slab = NULL;
 slab = GET_SLAB(obj);
 slab_cache_free_slab(cache,slab,obj);
 return 0;
}

int slab_free(void *obj) {
 struct slab_cache *cache = NULL;
 struct slab *slab = NULL;
 cache = GET_SLAB_CACHE(obj);
 slab  = GET_SLAB(obj);
 slab_cache_free_slab(cache,slab,obj);
 return 0;
}

/*Now shrink the caches.
  Destroy the slabs in the caches*/

static void slab_destroy(struct slab_cache *cache,struct slab *slab) { 
  if(slab->s_used) {
    printf("Trying to destroy a slab that is in use:Skipping...\n");  
    return;
  }
  slab_free_pages(cache,slab);

  if( (cache->off_slab & OFF_SLAB) && cache->slab_p ) { 
    slab_cache_free(cache->slab_p,(void*)slab);
  }
}

int slab_cache_shrink(struct slab_cache *cache) { 
  struct list_head *head = &cache->slab_free;
  int count = 0;
  while(! LIST_EMPTY(head) ) { 
    struct slab *slab = list_entry(head->next,struct slab,s_head);
    list_del(&slab->s_head);
    if(slab->s_used ) {
      printf("Trying to free a used slab.Moving it to another slab list:\n");
      if(slab->s_used == cache->nr_objects) {
	list_add(&slab->s_head,&cache->slab_full);
      }  
      else {
	list_add(&slab->s_head,&cache->slab_partial);
      }
      goto next;
    }
    ++count;
    slab_destroy(cache,slab);
  next:;
  }
  return count << cache->order;
}

/*Destroy the cache*/

int slab_cache_destroy(struct slab_cache *cache) { 
  struct list_head *full = &cache->slab_full; 
  struct list_head *partial = &cache->slab_partial;
  int count = slab_cache_shrink(cache); /*shrink the cache*/
  if(! LIST_EMPTY(full) || !LIST_EMPTY(partial) ) {
    printf("Trying to destroy a cache with active slabs:\n");
    goto out;
  }
  list_del(&cache->next); 
  slab_cache_free(&cache_head,(void *)cache);
  printf("Freed (%d) pages from the slabs:\n",count );
 out:
  return count;
}

/*Display cache stats*/
void display_slab_cache_stats(struct slab_cache *cache) {
  int full = 0,partial = 0,free = 0;
  int objects = cache->nr_objects ,size = cache->size,order = cache->order;
  int used = cache->nr_used , allocated = cache->nr_allocated;
  struct list_head *full_list = NULL,*partial_list = NULL,*free_list = NULL,*traverse = NULL;
  full_list = &cache->slab_full;
  list_for_each(traverse,full_list) { 
    ++full;
  }
  partial_list = &cache->slab_partial;
  list_for_each(traverse,partial_list) {
    ++partial;
  } 
  free_list = &cache->slab_free;
  list_for_each(traverse,free_list) { 
    ++free;
  }
  printf("%12s%12s%12s%12s%12s%12s\n","Cache","Order","Objects","Size","Total","Active");
  printf("%12s%12d%12d%12d%12d%12d\n",cache->name,order,objects,size,allocated,used);
  printf("\n%12s%12s%12s    Slabs\n","Full","Partial","Free");
  printf("%12d%12d%12d\n",full,partial,free);
}

void display_slab_caches_stats(void) { 
  struct list_head *head = &cache_head.next;
  struct list_head *traverse = NULL;
  list_for_each(traverse,head) { 
    struct slab_cache *cache = list_entry(traverse,struct slab_cache,next);
    display_slab_cache_stats(cache);
  }
}                    

struct sample { 
  struct list_head head;
  int id;
}__attribute__((packed));

static DECLARE_LIST_HEAD(head) ;
 
int test_slab_cache(void) {
  struct list_head *traverse = NULL;
  struct list_head *start = &head;
  struct slab_cache *cache =  NULL;
  int nr  = NR;
  struct sample *ptr = NULL;

#ifndef SLAB_ALLOC
  cache = slab_cache_create("Cache1",sizeof(struct sample),SLAB_HWCACHE_ALIGN,0 );
  if(! cache) { 
    printf("Unable to allocate Cache1:\n");
    goto out;
  } 
#endif
  while(nr--) { 
#ifndef SLAB_ALLOC
    ptr= slab_cache_alloc(cache,0);
#else
    ptr = slab_alloc(sizeof(struct sample),0);
#endif
   
    if(! ptr) {
      printf("Unable to allocate object from cache %s\n","Cache1");
      goto out_free;
    }
    memset(ptr,0x0,sizeof(struct sample));
    ptr->id = nr; 
    list_add(&ptr->head,start);
  }
  nr = 10;
  list_for_each(traverse,start) { 
   if(!nr--) 
      break;
    ptr= list_entry(traverse,struct sample,head);
    printf("Sample Id=%d\n",ptr->id);
  }

 out_free:
 while(! LIST_EMPTY(start) ) {
    ptr = list_entry(start->next,struct sample,head);
    list_del(&ptr->head);
#ifndef SLAB_ALLOC
    slab_cache_free(cache,(void*)ptr);
#else
    slab_free((void*)ptr);
#endif
  }

#ifndef SLAB_ALLOC
 slab_cache_destroy(cache);
#endif
 out:
  return 0;
}   

