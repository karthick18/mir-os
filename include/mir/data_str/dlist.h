/*A.R.Karthick :
  This ones a subset of my generic data structures
  which allows oneself to do things in a generic fashion
*/

#ifndef _DLIST_H
#define _DLIST_H


#include "dlist_struct.h"

#define INIT_LIST_HEAD(name) { &(name), &(name) }

#define DECLARE_LIST_HEAD(name) \
struct list_head name = INIT_LIST_HEAD(name)


#define LIST_HEAD_INIT(ptr) do { \
 ptr->next = ptr->prev = ptr;\
}while(0)

#define LIST_EMPTY(list)  ( (list) == (list)->next )

#define __list_add(element,a,b) do { \
element->next = b;\
element->prev = a;\
a->next = element;\
b->prev = element;\
}while(0)

#define __list_del(a,b) do {\
a->next = b;\
b->prev = a;\
}while(0)

static __inline__ void list_head_init(struct list_head *head) {
  LIST_HEAD_INIT(head);
}

static __inline__ void list_add_tail(struct list_head *element,struct list_head *head) {
  struct list_head *tail = head->prev;
  __list_add(element,tail,head);
}

static __inline__ void list_add(struct list_head *element,struct list_head *head) {
  struct list_head *first = head->next;
  __list_add(element,head,first);
}

static __inline__ void list_del(struct list_head *element) {
  struct list_head *before = element->prev;
  struct list_head *after  = element->next;
  __list_del(before,after);
  element->next = element->prev = NULL;
}

static __inline__ void list_del_init(struct list_head  *element) { 
  list_del(element); 
  LIST_HEAD_INIT(element);
} 

static __inline__ void list_splice(struct list_head *new,struct list_head *head) {
  if(!LIST_EMPTY(new) ) {
    struct list_head *head_first = head->next;
    struct list_head *new_last  = new->prev;
    head->next = new->next;
    new->next->prev = head;   
    new_last->next = head_first;
    head_first->prev = new_last;
    LIST_HEAD_INIT(new);
  }
}

#define list_entry(ptr,cast,field) \
(cast*) ( (unsigned char*)ptr - (unsigned long)(&((cast*)0)->field) )

#define list_for_each(list,head) \
for(list = head->next; list != head; list = list->next)

#define list_sort_add(element,head,cast,field) do { \
struct list_head *__tmp = NULL;\
cast *__reference = list_entry(element,cast,field);\
list_for_each(__tmp,head) {\
  cast *__node = list_entry(__tmp,cast,field);\
  if(__node->compare && __node->compare(__reference,__node) < 0) {\
    list_add_tail(element,__tmp);\
    goto __out;\
   }\
}\
list_add_tail(element,head);\
__out:\
 (void)"dlist";\
}while(0)

#endif

