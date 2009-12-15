/*A.R.Karthick for MIR:
  This ones a subset of my generic data structures.
*/

#ifndef _MYQUEUE_H
#define _MYQUEUE_H

struct queue_struct {
  struct queue_struct *next;
  struct queue_struct **pprev;
};

#define QUEUE_INIT(queue_struct) do { \
queue_struct->tail = &queue_struct->head;\
queue_struct->head = NULL;\
}while(0)

#define QUEUE_EMPTY(queue_struct)  ({ queue_struct->tail == &queue_struct->head;})

/*Add in a LIFO way*/
#define QUEUE_ADD(element,queue_struct) do {\
  if( (element->next = queue_struct->head) ) { \
   element->next->pprev = &element->next;\
 }else { \
  queue_struct->tail = &element->next;\
 }\
*(element->pprev = &queue_struct->head) = element;\
}while(0)

#define QUEUE_ADD_TAIL(element,queue_struct) do { \
 *(element->pprev = queue_struct->tail) = element;\
 queue_struct->tail = &element->next;\
}while(0)

#define QUEUE_DEL(element,queue_struct) do { \
if(element->next) \
 element->next->pprev = element->pprev;\
else \
 queue_struct->tail = element->pprev;\
*element->pprev = element->next;\
element->pprev = NULL,element->next = NULL;\
}while(0)

#define queue_entry(element,cast,field) \
(cast *) ( (unsigned char*)element - (unsigned long)( & (( (cast*)0)->field ) ) )

#define queue_for_each(element,queue_struct) \
for(element = queue_struct->head ; element ; element = element->next)

#define queue_add QUEUE_ADD
#define queue_add_tail QUEUE_ADD_TAIL
#define queue_del QUEUE_DEL
#define queue_init QUEUE_INIT

#endif

