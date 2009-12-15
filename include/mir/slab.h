#ifdef __KERNEL__
#ifndef _SLAB_H
#define _SLAB_H

#include <asm/processor.h>
#define SLAB_HWCACHE_ALIGN SMP_CACHE_ALIGN
typedef struct slab_cache slab_cache_t;
extern slab_cache_t *slab_cache_create(char *,int,int,int);
extern int slab_cache_init(void);
extern void *slab_cache_alloc(slab_cache_t *,int);
extern int slab_cache_free(slab_cache_t *,void *);
extern void *slab_alloc(int,int);
extern int slab_free(void *);
extern int slab_cache_destroy(slab_cache_t *);
extern int slab_cache_shrink(slab_cache_t *);
extern int slab_cache_init(void);
extern void display_slab_caches_stats();
extern void display_slab_cache_stats(slab_cache_t *);
extern int test_slab_cache(void);
extern slab_cache_t *mm_cache_p;
extern slab_cache_t *vm_area_cache_p;

#endif
#endif
