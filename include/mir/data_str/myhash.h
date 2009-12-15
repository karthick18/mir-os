/*Author: A.R.Karthick
  A bit of a generic implementation of a hashing structure
  -This ones simple and doesnt contain collision detection.
  I have one which does that,but we dont need that 
  right now.
*/

#ifndef _MYHASH_H
#define _MYHASH_H

#define HASH_TABLE 101

#include "util.h"

struct hash_struct {
  struct hash_struct *next;
  struct hash_struct **pprev; 
};

/*You can make this global if you desire*/
static struct hash_struct **hash_table;

static unsigned int hash_table_size = HASH_TABLE;

static __inline__ int init_hash(int size) { 
  struct hash_struct **ptr = NULL;
  if(size)
    hash_table_size = size;
  if( ! ( ptr = (struct hash_struct **)ALLOC_MEM(ptr,hash_table_size,0) ) )
    return -1;
  memset(ptr,0x0,sizeof(struct hash_struct *) * hash_table_size);
  hash_table = ptr;
  return 0;
}

static __inline__ unsigned int symbol_hash_fn(const unsigned char *name) { 
  unsigned int hash = 0;
  for(; *name; ++name) 
    hash = hash + 31*(*name);

  return hash % hash_table_size;
}

static __inline__ void destroy_hash(void) {
  free((void*)hash_table);
  hash_table = NULL;
}

static __inline__ int add_hash(struct hash_struct *node,unsigned int key) { 
  int err = -1;
  if(key >= hash_table_size) 
    goto out;
  else { 
    struct hash_struct **ptr ;
    if(! hash_table && init_hash(0) < 0) 
      goto out;
    ptr = &hash_table[key];
    node->next = NULL,node->pprev = NULL;
    if( (node->next = *ptr) ) {
      (*ptr)->pprev = &node->next;
    }
    node->pprev = ptr;
    *ptr = node;
  }
  err = 0;
 out:
  return err;
}
    
static __inline__ void del_hash(struct hash_struct *node) { 
  if(node->next) 
    node->next->pprev = node->pprev;
  *node->pprev = node->next;
  node->next = NULL,node->pprev = NULL;
}

#if 0
/*Not needed*/
extern inline void check_collisions(void) { 
  int i;
  for( i = 0; i < hash_table_size; ++i) { 
    struct hash_struct *head = hash_table[i];
    struct hash_struct *traverse ;
    int count ;
    for( count = 0 ,traverse = head ; traverse && ++count ; traverse = traverse->next);
    printf("Collisions for key (%d)=(%d)\n",i,count);
  }
}
#endif
/*This should be called after all the hash nodes have been unhashed*/
static __inline__ int check_hash(void) {
  int i;
  int err = 1;
  for( i = 0; i < hash_table_size; ++i) {
    struct hash_struct *ptr = hash_table[i];
    struct hash_struct *traverse;
    for(traverse = ptr; traverse ; traverse = traverse->next)
      goto out;
  }
  err = 0; 
 out:  
  return err;
}
#define hash_entry(hash_node,cast,field) \
(cast *) ((unsigned char *)hash_node - (unsigned long)&(((cast*)0)->field) )

#define search_hash(key,search_node,cast,field) ({\
cast * __found = NULL;\
if(key >= hash_table_size) \
 goto __out;\
else { \
struct hash_struct *__ptr ;\
struct hash_struct *__traverse = NULL;\
if( ! hash_table ) { \
 goto __out;\
}\
__ptr = hash_table[key];\
for(__traverse = __ptr ; __traverse ; __traverse = __traverse->next) { \
  cast *__reference = hash_entry(__traverse,cast,field);\
  if(__reference->hash_search && __reference->hash_search(__reference,search_node) == 1) { \
    __found = __reference;\
    goto __out;\
  }\
}\
}\
__out:\
__found;\
})

#endif
