/*   MIR-OS : Reboot routines
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
;
;   Reboot the system by setting the pulse reset-line low of the keyboard contr;   oller.
*/

#include <mir/sched.h>
#include <asm/io.h>
#include <asm/reboot.h>

static long fault_idt[2];

/*This causes a triple fault to forcefully halt the system
*/
volatile void triple_fault(void) {
#ifdef BOCHS
  __asm__ __volatile__("lidt %0\n" : : "m" (fault_idt) );
  __asm__ __volatile__("int3\n" : : );
#else
  __asm__ __volatile__("cli; hlt;");
#endif
}

volatile void mir_reboot(void) {
  int i;
  printf("Please wait while MIR reboots..\n");
  delay(500);
  for(i = 0;i < 0x100; ++i) {
    keyb_wait();
    outb_p(0x64,0xfe); /*pulse reset-line low*/
  }
  /*Cause a triple fault,if the last didnt work:
  */
  triple_fault();
}
