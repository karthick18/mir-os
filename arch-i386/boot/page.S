/*   MIR-OS : Paging for MM:
;    Copyright (C) 2003 A.R.Karthick 
;    <a.r.karthick@gmail.com,a_r_karthic@rediffmail.com>
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
;
;  
;  Enable paging:
;  We dont _yet_ go for a 3:1 split with user and kernel space.
;  That is anything above 3GB -4GB is mapped to the Kernel directly
;  with 0-1GB physical with that split.
;  To achieve accessing the first 0-8MB during initialisation,
;  we setup the page tables to have a mapping of
;  0-8MB virt. to 0-8MB physical
;  and 3 to 3.8 GB virtual to 0-8MB physical.
;  This will allow us to go for a direct pointer access
;  for 0-8MB and also allow us to call the kernel code segments
;  falling in the first 8 MB (7MB actual (considering that kernel is loaded at ;1 MB if you take the loading point of the Kernel at 1 MB.)
*/


#include <asm/pgtable.h>
#include <asm/thread.h>
#include <asm/segment.h>
	
#define PAGE_SIZE 0x1000
/*We start at a new PAGE BOUNDARY for setting up the page tables:
  This should make the things clear:
  We initialise the first 8 MB of page tables as thats needed
  before the real paging init.
*/

.align PAGE_SIZE
.text
.globl pg0,pg1,empty_zero_page,init_pgdir,start_paging,start_kernel;

/*pg0 and pg1 contain the 8MB mappings for virtual addresses
  as explained above:
  They are placed at 2 and 3 MB relative to 1 MB of kernel
*/


/*Bits for the page: 
  Used 0 -11 and the remaining are for addressing:
  Bit 0 - Page Present
  Bit 1 - Page RW(read write)
  Bit 2 - Page User/Supervisor( USER-1, SUPER:0)
  Bit 3 -4 : Reserved:
  Bit 5- Page accessed:
  Bit 6 - Page Dirty
  7-8 are reserved
  9-11 can be used for any purpose:
*/

start_paging:
   cld;
   xorl %eax,%eax;
  /*Now we are still at offset 3GB or PAGE_OFFSET:so account for that
    while accessing the relevant fields*/
   movl $pg0 - __PAGE_OFFSET,%edi;
   movl $007,%eax; /*7 means PAGE_PRESENT | PAGE_RW | PAGE_USER*/
  2:
   stosl;
   addl $0x1000,%eax; /*Now add the next page into eax*/
   /*check if we have hit empty_zero_page*/
   cmpl $empty_zero_page - __PAGE_OFFSET ,%edi;
   jne 2b;

 /*Now that we have loaded the mappings,
  Setup the page tables*/
  movl $init_pgdir - __PAGE_OFFSET,%eax;
  movl %eax, %cr3;
  movl %cr0,%eax; /*setup the paging flags*/
  orl $0x80000000,%eax;
  movl %eax,%cr0; /*this should do*/
  /*No need to flush the CPUs prefetch queue as we are in 32 bit mode already
  */
  ret;


.org 0x1000

/*Fill the init_pgdir page directory:This should contain the 0-8MB
  mappings for virt. address pairs:
  0-8MB and 3-3.8GB:
*/
init_pgdir:
	.long 0x00103007 /*store the addresses of pg0 and pg1: +7 again for __PAGE_USER */
        .long 0x00104007
        .fill PTRS_PER_PGD - 2,4,0


.org 0x2000			 
pg0:

.org 0x3000
pg1:

.org 0x4000
empty_zero_page:

.org 0x5000
/*The remaining sections enter here*/ 
   
