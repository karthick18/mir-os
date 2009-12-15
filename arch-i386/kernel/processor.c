/*   MIR-OS : CPUID + Feature Processing routines
;    Copyright (C) 2003 A.R.Karthick 
;    <a_r_karthic@rediffmail.com,karthick_r@infosys.com>
;
;    This program is free software; you can redistribute it and/or modify
;    it under the terms of the GNU General Public License as published by
;    the Free Software Foundation; either version 2 of the License, or
;    (at your option) any later version.
;
;    This program is distributed in the hope that it will be useful,
;    but WITHOUT ANY WARRANTY; without even the implied warranty of
;    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
;    GNU General Public License for more details.
;
;    You should have received a copy of the GNU General Public License
;    along with this program; if not, write to the Free Software
;    Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
*/

#include <mir/kernel.h>
#include <mir/sched.h>

struct cpu_features cpu_features;

static void process_edx_features(void) { 
  /*check for the bits of the cpu features and print the info.*/
  unsigned int features = cpu_features.edx_cpu_features.features;
  if(features & (1 << FPU_BIT) ) {
    printf("CPU has FPU unit\n");
  }
  if(features & ( 1 <<  VME_BIT) ) {
    printf("CPU has Virtual MODE Extension feature:\n");
  }
  if(features & ( 1 << DE_BIT) ) {
    printf("CPU has Debug Extensions bit set:\n");
  }
  if(features & ( 1 << PSE_BIT) ) {
    printf("CPU has Page Size Extensions bit set:\n");
  }
  if(features & (1 << TSC_BIT) ) {
    printf("CPU has Time stamp count feature:\n");
  }
  if(features & ( 1 << MSR_BIT) ) {
    printf("CPU has Model Specific Register:\n");
  }
  if(features & ( 1 << PAE_BIT) ) { 
    printf("CPU has Physical Address Extension feature:\n");
  }
  if(features & ( 1 << MCE_BIT) ) {
    printf("CPU has Machine Check Exception feature:\n");
  }
  if(features & (1 << CX8_BIT) ) {
    printf("CPU has CMPXCHG8B instruction:\n");
  }
  if(features & ( 1 << APIC_BIT) ) {
    printf("CPU has APIC (Advanced Programmable Interrupt controller)\n");
  }
  if(features & ( 1 << MTRR_BIT) ) {
    printf("CPU has Memory Type Range Register:\n");
  }
  if(features & ( 1 << PGE_BIT) ) {  
    printf("CPU has PGE or Page table Global Bit set for 4MB pages:\n");
  }
  if(features & ( 1 << MCA_BIT) ) {
    printf("CPU has Machine Check Architecture feature:\n");
  }
  if(features & ( 1 << CMOV_BIT) ) {
    printf("CPU has Conditional MOV :\n");
  }
  if(features & ( 1 << PAT_BIT) ) {
    printf("CPU has Page attribute table:\n");
  }
  if(features & ( 1 << PSE_36_BIT) ) {
    printf("CPU has Page size extension for 36 bits:\n");
  }
  if(features & ( 1 << PS_BIT) ) {
    printf("CPU has processor serial number(PIII specific)\n");
  }
  if(features & ( 1 << CLFLUSH_BIT) ){
    printf("CPU has Cachline flush feature:\n");
  }

  if(features & ( 1 << DS_BIT) ) {
    printf("CPU has Debug Store feature:\n");
  }
  if(features & ( 1 << ACPI_BIT) ) {
    printf("CPU has ACPI feature for performance monitoring:\n");
  }
  if(features & ( 1 << MMX_BIT) ) {
    printf("CPU has MMX feature:\n");
  }
  if(features & ( 1 << FXSR_BIT) ) {
    printf("CPU has floating point save and restore feature:\n");
  }
  if(features & ( 1 << SSE_BIT) ) {
    printf("CPU has Streaming set extensions:\n");
  }
  if(features & ( 1 << SSE_2_BIT)) {
    printf("CPU has Streaming Set extensions 2:\n");
  }
  if(features & ( 1 << SS_BIT) ) {
    printf("CPU has Self Snoop feature:\n");
  }
  if(features & ( 1 << TM_BIT) ) {
    printf("CPU has Thermal Monitoring feature:\n");
  }

}
/*EBX contains brand string,clflush_size,apic_id*/

static void process_ebx_features(void) { 
  char *brand_string[ ] = { 
    "Brand String unsupported",
    "Celeron Processor",
    "Pentium III Processor",
    "Intel PIII Xeon Processor",
    [4 ... 7 ] = "Reserved for Future Processor",
    "Pentim IV Processor",
    [9 ... 255 ] = "Reserved for Future Processor",
  };
  printf("Processor Brand String =%s\n",brand_string[cpu_features.ebx_cpu_features.brand_index]);
  printf("Processor Cache line flush size=%d\n",cpu_features.ebx_cpu_features.clflush_size << 3);
  printf("APIC physical id=%d\n",cpu_features.ebx_cpu_features.apic_id);
}

/*EAX contains stepping_id,model,family,type info*/
static void process_eax_features(void) { 
  char *types [ ] = { 
    "Original OEM Processor",
    "Intel Overdrive Processor",
    "Dual Processor",
    "Reserved",
  };
  unsigned int model ,family;
  printf("Processor Stepping ID=%d\n",cpu_features.eax_cpu_features.stepping_id);
  printf("Processor Type=%s\n",types[cpu_features.eax_cpu_features.type]);
  model = cpu_features.eax_cpu_features.model;
  family = cpu_features.eax_cpu_features.family;
  /*Check for extended information*/
  if( (model & 0xF) == 0xF) 
    model |= ( cpu_features.eax_cpu_features.extended_model << 4);
    
  if( (family & 0xF) == 0xF) 
    family |= ( cpu_features.eax_cpu_features.extended_family << 4);
  
  model &= 0xff;
  family &= 0xfff;
  printf("Processor Model =%d\n",model);
  printf("Processor family =%d\n",family);
}

/*This routine gives us the cache and TLB info*/
static void process_cache_tlb_info(void) { 
  char *cache_info[ ] = { 
    "Null Descriptor",
    "Instruction TLB:4k pages 4 way Set 32 entries",
    "Instruction TLB: 4MB pages 4 way set 2 entries",
    "Data TLB: 4k pages 4 way set 64 entries",
    "Data TLB: 4MB pages 4 way set 8 entries",
    "Undefined slot",
    "L1 instruction Cache: 8K size 4 way set 32 byte line size",
    "Undefined slot",
    "L1 instruction Cache: 16k size 4 way set 32 byte line size",
    "Undefined slot",
    "L1 data Cache: 8k size 2 way set associative 32 byte line size",
    "Undefined slot",
    "L1 data Cache: 16k size 4 way set associate 32 byte line size",
    [ 13 ... 33 ] = "Undefined slot",
    "L3 cache: 512k size 4 way set associate 64 byte line size",
    "L3 cache: 1MB size 8 way set associate 64 byte line size",
    "Undefined slot",
    "L3 cache: 2MB size 8 way set associate 64 byte line size",
    [ 38 ... 40 ] = "Undefined slot",
    "L3 cache: 4MB size 8 way set associate 64 byte line size",
    [ 42 ... 63 ] = "Undefined slot",
    "No L2 cache,or if L2 present, no L3 cache",
    "L2 cache : 128k size 4 way set associative 32 byte line size",
    "L2 cache : 256k size 4 way set associative 32 byte line size",
    "L2 cache:  512k size 4 way set associative 32 byte line size",
    "L2 cache:  1MB  size 4 way set associative 32 byte line size",
    "L2 cache:  2MB  size 4 way set associative 32 byte line size",
    [ 0x46 ... 0x4F ] = "Undefined slot",
    "Instruction TLB: 4k pages and 2 MB or 4 MB pages with 64 entries",
    "Instruction TLB: 4k pages and 2 MB or 4 MB pages with 128tries",
    "Instruction TLB: 4k pages and 2 MB or 4 MB pages with 256 entries",
    [ 0x53 ... 0x5A ] = "Undefined slot",
    "Data TLB: 4k pages and 4MB pages with 64 entries",
    "Data TLB: 4k pages and 4MB pages with 128 entries",
    "Data TLB: 4k pages and 4MB pages with 256 entries",
    [ 0x5E ... 0x65 ] = "Undefined slot",
    "L1 data Cache: 8k size 4 way set associative 64 byte line size",
    "L1 data Cache: 16k size 4 way set associative 64 byte line size",
    "L1 data Cache: 32k size 4 way set associative 64 byte line size",
    "Undefined slot",
    "Trace Cache: 12k micro op: 8 way set associative",
    "Trace Cache: 16k micro op: 8 way set associative",
    "Trace Cache: 32k micro op: 8 way set associative",
    [ 0x73 ... 0x78 ] = "Undefined slot",
    "L2 cache : 128k size, 8 way set associative,sectored,64 byte line size",
    "L2 cache : 256k size, 8 way set associative,sectored,64 byte line size",
    "L2 cache : 512k size, 8 way set associative,sectored,64 byte line size",
    "L2 cache : 1MB  size, 8 way set associative,sectored,64 byte line size",
    [ 0x7D ... 0x81 ] = "Undefined slot",
    "L2 cache : 256k size, 8 way set associative, 32 byte line size",
    "Undefined slot",
    "L2 cache : 1MB size, 8 way set associative, 32 byte line size",
    "L2 cache : 2MB size, 8 way set associative, 32 byte line size",
    [ 0x86 ... 0xFF ] = "Undefined slot",
  };
  struct cache_tlb_info *ptr = &cpu_features.cpu_cache_tlb_info.eax_info;
  int i;
  for(i = 3 ; i >= 0; --i,++ptr) 
    /*check for validity: most significant bit cleared*/
    if( (*(unsigned long*)ptr) && !(ptr->bits24_31 & 0x80) ) { 
      /*access all the bytes*/
      if(i != 3 && ptr->bits0_7)
	printf("%s\n",cache_info[ptr->bits0_7]);
      if(ptr->bits8_15)
	printf("%s\n",cache_info[ptr->bits8_15]);
      if(ptr->bits16_23)
	printf("%s\n",cache_info[ptr->bits16_23]);
      if(ptr->bits24_31)
	printf("%s\n",cache_info[ptr->bits24_31]);
    }
}

/*Get the CPU type info:*/
void process_cpu_type(int unused) { 
  process_eax_features();
}
/*Get the CPU cache info*/
void process_cpu_cache(int unused) { 
  process_cache_tlb_info();
}

/*Get the CPU brand info*/
void process_cpu_brand(int unused) { 
  process_ebx_features();
  if(cpu_features.brand_string[0])
    printf("CPU Brand String:%s\n",cpu_features.brand_string);
}

void process_cpu_features(int unused) { 
  process_edx_features();
}

void cpuid_c(void) {
  unsigned char vendor[VENDOR_MAX];
  unsigned int ecx;
  int repeat,i;
  unsigned char *brand_string;
  cpuid_vendor(vendor); 
  /*get the CPU vendor*/
  vendor[VENDOR_MAX-1] = 0;
  printf ("\t\t\t\t\t\t\tCPU ID:  %s\n", vendor);
  /*first get the cpu features*/
  cpuid_info((unsigned int *)&cpu_features.eax_cpu_features,(unsigned int *)&cpu_features.ebx_cpu_features,&ecx,(unsigned int *)&cpu_features.edx_cpu_features,1);
  /*next get the cache and tlb info for the processor*/
  cpuid_info( (unsigned int*)&cpu_features.cpu_cache_tlb_info.eax_info,
	      (unsigned int*)&cpu_features.cpu_cache_tlb_info.ebx_info,
	      (unsigned int*)&cpu_features.cpu_cache_tlb_info.ecx_info,
	      (unsigned int*)&cpu_features.cpu_cache_tlb_info.edx_info,
	      2);
  /*Low byte in the eax gives us the repeat count*/
  repeat = cpu_features.cpu_cache_tlb_info.eax_info.bits0_7 - 1;
  for(i = repeat; i > 0 ; --i ) {
    cpuid_info( (unsigned int *)&cpu_features.cpu_cache_tlb_info.eax_info,
		(unsigned int *)&cpu_features.cpu_cache_tlb_info.ebx_info,
		(unsigned int *)&cpu_features.cpu_cache_tlb_info.ecx_info,
		(unsigned int *)&cpu_features.cpu_cache_tlb_info.edx_info,
		2);
  }
  brand_string = cpu_features.brand_string;
#if 0
  for(i = 0; i <= 2; ++i )  
    cpuid_brand_string(brand_string + i*16,0x80000000UL + i);
#endif
}

