/*Author: A.R.Karthick for MIR:
  Changed these guys to inline and added the
  inb_p and outb_p inlines:
 */
#ifndef _IO_H
#ifdef __KERNEL__
#define _IO_H

#define inb(address)    ({\
unsigned char _val ;\
__asm__ __volatile__("inb %%dx,%%al\n\t" :"=a"(_val) : "d"(address) );\
_val; \
})

#define outb(address,byte) ({\
__asm__ __volatile__("outb %%al,%%dx\n\t" : :"a"(byte),"d"(address) );\
0;\
})

#define inw(address) ({\
unsigned short _ret;\
__asm__ __volatile__("inw %%dx,%%ax\n" : "=a"(_ret) : "d" (address) );\
_ret;\
})

#define outw(address,word) ({\
__asm__ __volatile__("outw %%ax,%%dx\n" : : "a" (word),"d"(address) );\
0;\
})

#define inl(address) ({\
unsigned long _ret;\
__asm__ __volatile__("inl %%dx,%%eax\n"\
                    :"=a"(_ret) : "d" (address) \
);\
_ret;\
})

#define outl(address,dword) ({\
__asm__ __volatile__("outl %%eax,%%dx\n"\
                   : :"a"(dword),"d"(address) \
);\
0;\
})


#define SLOW_DOWN_IO \
"jmp 1f\n\t" \
"1:jmp 2f\n\t" \
"2:\n\t"

/*Read and pause*/

#define inb_p(address) ({\
unsigned char _val = 0;\
__asm__ __volatile__("inb %%dx,%%al\n\t" \
                      SLOW_DOWN_IO \
                      :"=a"( (_val) ) :"d"((address)) \
                    );\
_val;\
})

/*Output and pause*/

#define outb_p(address,byte) ({\
__asm__ __volatile__("outb %%al,%%dx\n\t" \
                      SLOW_DOWN_IO \
                      : :"a"((byte)),"d"((address)) \
                    );\
0;\
})

#define inw_p(address) ({\
unsigned short _ret;\
__asm__ __volatile__("inw %%dx,%%ax\n"\
                      SLOW_DOWN_IO \
                     :"=a"(_ret) : "d" (address) \
      );\
_ret;\
})

#define outw_p(address,word) ({\
__asm__ __volatile__("outw %%ax,%%dx\n"\
                     SLOW_DOWN_IO \
                     : : "a" (word) , "d"(address) \
 );\
0;\
})

#define inl_p(address) ({\
unsigned long _ret;\
__asm__ __volatile__("inl %%dx,%%eax\n"\
                     SLOW_DOWN_IO \
                     : "=a"(_ret) : "d" (address) \
 );\
_ret;\
})

#define outl_p(address,dword) ({\
__asm__ __volatile__("outl %%eax,%%dx\n" \
                     SLOW_DOWN_IO \
                    : :"a"(dword),"d"(address) \
 );\
0;\
})

#endif
#endif
