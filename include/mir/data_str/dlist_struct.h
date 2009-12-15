#ifndef _DLIST_STRUCT_H
#define _DLIST_STRUCT_H
struct list_head {
  struct list_head *next,*prev;
};
#endif
