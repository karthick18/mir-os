/* CPUid processing routines
  Author : A.R.Karthick for Mir
*/
#ifndef _PROCESSOR_H

#ifdef  __KERNEL__
#define _PROCESSOR_H

#include <asm/segment.h>

#define SMP_CACHE_BYTES 0x20
#define SMP_CACHE_ALIGN SMP_CACHE_BYTES

#define L1_CACHE_ALIGN(val)  ( ( (val) + SMP_CACHE_ALIGN - 1) & ~(SMP_CACHE_ALIGN -1 ) )

#define __cachelinealigned  __attribute__((__aligned__(SMP_CACHE_ALIGN)))

#define VENDOR_MAX 13

/*Define CPU feature bitmasks*/
struct edx_cpu_features {

#define FPU_BIT      0x0
#define VME_BIT      0x1
#define DE_BIT    0x2
#define PSE_BIT      0x3
#define TSC_BIT      0x4
#define MSR_BIT      0x5
#define PAE_BIT      0x6
#define MCE_BIT      0x7
#define CX8_BIT      0x8
#define APIC_BIT     0x9
#define SEP_BIT      0xB
#define MTRR_BIT     0xC
#define PGE_BIT      0xD
#define MCA_BIT      0xE
#define CMOV_BIT     0xF
#define PAT_BIT      0x10
#define PSE_36_BIT   0x11
#define PS_BIT       0x12 
#define CLFLUSH_BIT  0x13
#define DS_BIT       0x15
#define ACPI_BIT     0x16
#define MMX_BIT      0x17
#define FXSR_BIT     0x18
#define SSE_BIT      0x19
#define SSE_2_BIT    0x1A
#define SS_BIT       0x1B
#define TM_BIT       0x1D
unsigned long features;

} NOT_ALIGNED;

struct ebx_cpu_features { 
  unsigned int brand_index: 8;
  unsigned int clflush_size : 8;
  unsigned int reserved : 8;
  unsigned int apic_id : 8;
}NOT_ALIGNED;

struct eax_cpu_features { 
  unsigned int stepping_id : 4;
  unsigned int model  : 4;
  unsigned int family : 4;
  unsigned int type   : 2; 
  unsigned int reserved_1 : 2;
  unsigned int extended_model : 4;
  unsigned int extended_family : 8;
  unsigned int reserved_2 : 4;
}NOT_ALIGNED;
 
/*Cache and TLB info. from cpuid op. 2*/

struct cache_tlb_info {
  unsigned int bits0_7 :  8;
  unsigned int bits8_15 : 8;
  unsigned int bits16_23 :8;
  unsigned int bits24_31 : 8;
} NOT_ALIGNED;

struct cpu_cache_tlb_info { 
  struct cache_tlb_info eax_info;
  struct cache_tlb_info ebx_info;
  struct cache_tlb_info ecx_info;  
  struct cache_tlb_info edx_info;
};

struct cpu_features { 
  struct eax_cpu_features eax_cpu_features;
  struct ebx_cpu_features ebx_cpu_features;
  struct edx_cpu_features edx_cpu_features;
  struct cpu_cache_tlb_info cpu_cache_tlb_info;
#define MAX_BRAND_STRING  0x30
  char brand_string[MAX_BRAND_STRING];
};


extern void process_cpu_features(int);
extern void process_cpu_type(int);
extern void process_cpu_cache(int);
extern void process_cpu_brand(int);
extern void cpuid_c(void);
extern struct cpu_features cpu_features;

/*Get the CPU vendor*/

static __inline__ void cpuid_vendor(unsigned char *vendor) {
  __asm__ __volatile__ ("cpuid\n\t"
                        "movl %%ebx,0(%0)\n\t"
                        "movl %%ecx,8(%0)\n\t"
                        "movl %%edx,4(%0)\n\t" 
                        : :"r"((vendor)),"a"(0) : "bx","cx","dx" );
}

/*Get the cpu features as a 32 bit long*/

static __inline__ void cpuid_info(unsigned int *eax,unsigned int *ebx,unsigned int *ecx,unsigned int *edx,unsigned int op) { 
  __asm__ __volatile__("cpuid\n"
		       :"=a"(*eax),"=b"(*ebx),"=c"(*ecx),"=d"(*edx) : "0" ( op  ) );
}

static __inline__ void cpuid_brand_string(unsigned char *brand_string,unsigned long op) {
  __asm__ __volatile__("cpuid\n"
		       "movl %%eax,0(%0)\n"
		       "movl %%ebx,4(%0)\n"
		       "movl %%ecx,8(%0)\n"
		       "movl %%edx,12(%0)\n"
		       : "=r"(brand_string) : "a" ( op )
		       );
}

#endif
#endif 
