/*Define the standard bitops.
Author: A.R.Karthick for MIR
*/
#ifndef _BITOPS_H
#ifdef __KERNEL__
#define _BITOPS_H


/* byte/word splitting macros : Pratap */

#define LOW(arg)   ( (arg) & 0x0000FFFF )
#define HIGH(arg) ( ( (arg) & 0xFFFF0000) >> 16 )
#define LOW_B(arg)  ( (arg) & 0x00FF)
#define HIGH_B(arg) ( ( (arg) & 0xFF00) >> 8)
#define BCD2BIN(arg) (( (arg) & 0xf) + (( (arg) >> 4)*10))

#define ADDR(addr)  ( *(volatile long *)addr )

#define set_bit(nr,addr) do {\
__asm__ __volatile__("btsl %1,%0\n"\
                    :"=m"(ADDR(addr)):"Ir"(nr) : "memory" );\
}while(0)

#define clear_bit(nr,addr) do {\
__asm__ __volatile__("btrl %1,%0\n"\
                   :"=m"(ADDR(addr)) : "Ir"(nr) : "memory" );\
}while(0)

#define change_bit(nr,addr) do {\
__asm__ __volatile__("btcl %1,%0\n"\
                    :"=m"(ADDR(addr)) : "Ir"(nr) : "memory" );\
}while(0)

#define test_and_set_bit(nr,addr) ({\
int _old;\
__asm__ __volatile__("btsl %2,%1; sbbl %0,%0\n"\
                :"=r"(_old),"=m"(ADDR(addr) ) : "Ir"(nr) : "memory" );\
_old;\
})

#define test_and_clear_bit(nr,addr) ({\
int _old;\
__asm__ __volatile__("btrl %2,%1;sbbl %0,%0\n"\
                  :"=r"(_old),"=m"(ADDR(addr)) : "Ir"(nr) : "memory" );\
_old;\
})

#define test_and_change_bit(nr,addr) ({\
int _old;\
__asm__ __volatile__("btcl %2,%1;sbbl %0,%0\n"\
                  :"=r"(_old),"=m"(ADDR(addr)) : "Ir"(nr) : "memory" );\
_old;\
})

/*Define the first_first_zero and first_bit versions*/

static __inline__ int find_first_zero_bit(void *addr,int size) { 
  int res;
  int _a1,_c1,_d1;
  __asm__ __volatile__(
                       "repe;scasl\n" 
                       "je 1f\n"
                       "subl $4,%1\n"
                       "xorl 0(%1),%3\n"
                       "bsfl %3,%0\n"
                       "subl %6,%1\n"
                       "shll $3,%1\n"
                       "addl %1,%0\n"
                       "1:"
		       :"=d"(res),"=&D"(_d1),"=&c"(_c1),"=&a"(_a1)
                       :"2"( ( (size) + 31 ) >> 5 ),"1"(addr),"b"(addr),"3"(-1),"0"(-1) );
  return res;
}

/*Find first bit that is set in a long of size bits:
 */
static __inline__ int find_first_bit(void *addr,int size) {
  int res;
  int _a1,_c1,_d1;
  __asm__ __volatile__(
		       "repe; scasl\n"
                       "je 1f\n"
		       "subl $4,%1\n"
		       "bsfl 0(%1),%0\n"
                       "subl %6,%1\n"
                       "shll $3,%1\n"
                       "addl %1,%0\n"
                       "1:"
                       :"=d"(res),"=&D"(_d1),"=&c"(_c1),"=&a"(_a1)
                       :"2"( ( (size) + 31 ) >> 5 ),"1"(addr),"b"(addr),"3"(0),"0"(-1) );
  return res;
}

/*Find next zero bit and find_next_bit versions*/

static __inline__ int find_next_zero_bit(void *addr,int offset,int size) { 
  long *ptr = (long*)addr;
  int temp,res=-1,flag; 

  if(offset < 0 || offset >= size )
    goto out;
 
  ptr += ( offset >> 5);
  flag = (offset & 31 );
  
  if(flag) { 
    /*then we take the next byte into account and see if we can find a zero bit*/
    __asm__ __volatile__("bsfl %1,%0\n"
                         "jne 1f\n"
                         "movl $32,%0\n"
                         "1:"
			 :"=r"(temp) : "r"( ~(*ptr >> flag) )
			 );
    if(temp < 32 - flag) 
      return temp + offset;

    temp = 32 - flag; 
    offset += temp;
    ++ptr;
  }

  res = find_first_zero_bit((void*)ptr,size - 32*(ptr - (long*)addr) );
  if(res < 0 )
    goto out;
  res += offset;
 out:
  return res;
}

static __inline__ int find_next_bit(void *addr,int offset,int size) {
  long *ptr = (long*)addr;
  int flag,temp,res = -1;
  if(offset < 0 || offset >= size) 
    goto out;
  ptr += (offset >> 5);
  flag = offset & 31;
  if(flag) { 
    __asm__ __volatile__("bsfl %1,%0\n"
                         "jne 1f\n"
                         "movl $32,%0\n"
			 "1:"
			 :"=r"(temp) : "r"( *ptr >> flag ) );
    if(temp < 32 - flag)
      return temp + offset;
    temp = 32 - flag;
    offset += temp;
    ++ptr;
  }
  res = find_first_bit(ptr, size - 32 * (ptr - (long*)addr ) );
  if(res < 0 )
    goto out;
  res += offset;
 out:
  return res;
}

/*Find first zero bit*/

static __inline__ int ffz(int value) { 
  int bit;
  __asm__ __volatile__("bsfl %1,%0\n"
                       "jne 1f\n"
		       "movl $-1,%0\n"
		       "1:"
                       :"=r"(bit) : "r"(~(value) ) );
  return bit;
}

/*Find first set bit*/
static __inline__ int ffs(int value) {
  int bit;
  __asm__ __volatile__("bsfl %1,%0\n"
		       "jne 1f\n"
		       "movl $-1,%0\n"
		       "1:"
		       :"=r"(bit) : "r"( value ) );
  return bit;
}

static __inline__ int test_bit(int nr,void *addr) { 
  int old;
  __asm__ __volatile__("btl %2,%1; sbbl %0,%0\n"
		       :"=r"(old) : "m"(ADDR(addr)) , "Ir"(nr) );
  return old;
}

/*Exchange operations on different sizes giving scope to exchange
  values in a single cycle:
*/

struct dummy { 
  unsigned long dummy[100];
};

#define dummy_type(ptr)  ((struct dummy *)ptr)
#define LOCK_PREFIX "lock;"

/*Compare and exchange sizes of 1,2 and 4 bytes*/
static __inline__ unsigned long __compare_and_exchange(volatile void *dest,
					    unsigned long old,unsigned long new,int size) { 
  unsigned long prev = 0;
  switch(size) { 
  case 1:
    /*compare and exchange a single byte*/
    __asm__ __volatile__(LOCK_PREFIX "cmpxchgb %b1,%2\n"
			 :"=a"(prev) 
			 :"q"(new) ,"m"(*dummy_type(dest) ),"0"(old) : "memory");
    break;
  
  case 2:
    __asm__ __volatile__(LOCK_PREFIX "cmpxchgw %w1,%2\n"
			 :"=a"(prev)
			 :"q"(new),"m"(*dummy_type(dest) ),"0"(old) : "memory" );
    break;

  case 4:
    __asm__ __volatile__(LOCK_PREFIX "cmpxchgl %1,%2\n"
			 :"=a"(prev)
			 :"q"(new),"m"(*dummy_type(dest) ),"0"(old) : "memory");
    break;

  default:;
  }
  return prev;
}

#define compare_and_exchange(ptr,old,new,size) \
(__typeof__(*(ptr) ) ) __compare_and_exchange(ptr,old,new,size)


/*compare and exchange 8 bytes atomically:
  new value should be in ecx: ebx
  old value in edx:eax
  destination in EDI:
  If the old value is same as the one in EDI, copy the new value
  to destination, else copy the destination value in old
*/

static __inline__ unsigned long long __compare_and_exchange_8b(volatile void *dest,unsigned long old_low,unsigned long old_high,unsigned long new_low,unsigned long new_high) { 
  unsigned long long prev;
  unsigned long low;
  unsigned long high;
  __asm__ __volatile__(LOCK_PREFIX "cmpxchg8b %4\n"
		       :"=a"(low),"=d"(high)
		       :"b"(new_low),"c"(new_high),"m"(*dummy_type(dest) ),"0"(old_low),"1"(old_high) : "memory" );
  __asm__ __volatile__("":"=A"(prev) : "a"(low),"d"(high) );
  return prev;
}

static __inline__ unsigned long __exchange(volatile void *ptr,unsigned long new,int size) { 
  switch(size) { 
  case 1:
    __asm__ __volatile__("xchgb %b1,%2\n" 
			 :"=q"(new) :"0"(new),"m"(*dummy_type(ptr) )  :"memory" );
    break;

  case 2:
    __asm__ __volatile__("xchgw %w1,%2\n"
			 :"=q"(new) : "0"(new),"m"(*dummy_type(ptr)) :"memory");
    break;

  case 4:
    __asm__ __volatile__("xchgl %1,%2\n"
			 :"=q"(new):"0"(new),"m"(*dummy_type(ptr)) :"memory");
    break;
  default:
    new = 0;
  }
  return new;
}

#define exchange(ptr,new,size) \
( __typeof__(*(ptr)) ) __exchange(ptr,new,size)

#define low_long(val) ( (val) & 0xffffffffUL)
#define high_long(val) ( ((val) >> 32  ) & 0xffffffffUL )

#define compare_and_exchange_8b(dest,old,new) \
 __compare_and_exchange_8b(dest,low_long(old),high_long(old),low_long(new),high_long(new) )

#endif
#endif

