/*Author :A.R.Karthick for MIR:
 rdtsc versions:
*/
#ifdef __KERNEL__

#ifndef _TIME_H
#define _TIME_H

#define rdtsc(ax,dx) do { \
__asm__ __volatile__ ("rdtsc\n\t" :"=a"(ax),"=d"(dx) : );\
}while(0)

/*read the first 32 bits of TSC*/

#define rdtscl(ax) do {\
__asm__ __volatile__("rdtsc\n\t" : "=a"(ax) : : "dx" );\
}while(0)

/*Read the rdtsc in a single shot 64 bit read:*/
#define rdtscll(ax) do {\
__asm__ __volatile__("rdtsc\n\t" :"=A"(ax) : );\
}while(0)

#define wrmsr(cx,ax,dx) do {\
__asm__ __volatile__("wrmsr\n\t" \
                     : :"c"(cx),"a"(ax),"d"(dx) );\
}while(0)

#define wrtsc(low,high)  wrmsr(0x10,low,high)

#endif
#endif
