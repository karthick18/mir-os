/*Karthick :
  This ones a subset of my generic data structures
  which allows oneself to do things in a generic fashion
*/

#ifndef _SLIST_H
#define _SLIST_H

#include "slist_struct.h"

#define INIT_SLIST_HEAD(name) { &(name) }

#define DECLARE_SLIST_HEAD(name) \
struct slist name = INIT_SLIST_HEAD(name)

#define SLIST_EMPTY(head)  ( (head) == (head)->next )

#define SLIST_HEAD_INIT(ptr) \
  ptr->next = ptr

#define slist_for_each(traverse,head) \
for(traverse=head->next;traverse != head;traverse = traverse->next)

#define __slist_add(element,before) do {\
 element->next = before->next;\
 before->next  = element;\
}while(0)

#define __slist_del(element,before) do {\
 before->next = element->next;\
}while(0)

static __inline__ void slist_head_init(struct slist *ptr) {
  SLIST_HEAD_INIT(ptr);
}

static __inline__ struct slist *slist_tail(struct slist *head) { 
  struct slist *traverse = NULL,*prev = NULL;
  for(prev = head,traverse = prev->next; traverse != head ; prev = traverse,traverse = prev->next);
  return prev;
}

static __inline__ struct slist *slist_find(struct slist *element,struct slist *head) {
  struct slist *traverse = NULL;
  struct slist *prev = head;
  slist_for_each(traverse,head) {
    if(traverse == element)
      return prev;
    prev = traverse;
  }
  return NULL;
}

static __inline__ void slist_add(struct slist *element,struct slist *head) {
  __slist_add(element,head);
}
static __inline__ void slist_add_tail(struct slist *element,struct slist *head) {
  struct slist *pos = slist_tail(head);
  __slist_add(element,pos);
}

static __inline__ int slist_del(struct slist *element,struct slist *head) {
  struct slist *prev = slist_find(element,head);
  int err = 1;
  if(prev) {
    __slist_del(element,prev);
    SLIST_HEAD_INIT(element);
    err = 0;
  }
  return err;
}

static __inline__ void slist_splice(struct slist *new_list,struct slist *head_list) {
  if(!SLIST_EMPTY(new_list) ) {
    struct slist *head_first = head_list->next;
    struct slist *new_tail = slist_tail(new_list);
    head_list->next = new_list->next;
    new_tail->next = head_first;
    SLIST_HEAD_INIT(new_list);
  }
}

#define slist_entry(slist,cast,field) \
(cast *) ((unsigned char *)slist - (unsigned long)(&(((cast*)0)->field) ) )

#define slist_sort_add(element,head,cast,field) ({\
struct slist *__traverse ,*__prev;\
cast *__reference = slist_entry(element,cast,field);\
for(__prev = head,__traverse = __prev->next;__traverse != head;__prev = __traverse,__traverse = __prev->next) {\
cast *__node = slist_entry(__traverse,cast,field);\
if(__node->compare && __node->compare(__reference,__node) < 0) {\
  break;\
}\
}\
__slist_add(element,__prev);\
0;\
})
#endif


  
