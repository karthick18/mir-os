/*   MIR-OS : MOUSE driver
;    Copyright (C) 2003 Pratap Pv 
;    Pratap Pv - pratap@starch.tk
;    A.R.Karthick - <a.r.karthick@gmail.com,a_r_karthic@rediffmail.com>
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
; Karthick: Removed the keyboard IO routines to key_io.c.
; Karthick: Made ps2_init get inside .inittext reclaimable section.
*/

#include <mir/kernel.h>
#include <asm/irq.h>
#include <mir/ps2.h>
#include <asm/io.h>
#include <asm/kbd.h>
#include <mir/init.h>

int mouse_interrupt(int irq,void *device, struct thread_regs *regs) {
printf ("MOUSE !\n");
return 0; 
}
void __init ps2_init () {
unsigned char *ch;
int etc;
static unsigned char  s1[] = { 0xF3, 0xC8, 0xF3, 0x64, 0xF3, 0x50, 0 };

outb (KB_STATUS_PORT, KB_DATA_PORT);
outb (KB_DATA_PORT, 0x3);

kbd_wait_write(KB_STATUS_PORT, KCTRL_ENABLE_AUX);
for (ch = s1; *ch; ch++) 
	kbd_rw_ack(*ch);
kbd_rw_ack(AUX_IDENTIFY); 
etc = kbd_wait_read() ;

if (etc == 0) 
	printf ("MOUSE: Standard PS2 Mouse detected (Non-Intellimouse)\n");
else if (etc == 3) 
	printf ("MOUSE: PS2 Intellimouse Detected\n");
else 
	printf ("MOUSE: No mouse detected\n");

kbd_rw_ack(0xf3);
kbd_rw_ack(0xff);

/* get mouse information */
kbd_rw_ack (AUX_INFORMATION);
kbd_wait_read(); /* not parsing this info */
printf ("\tResolution - %d counts/mm, ", kbd_wait_read());
printf ("Sampling Rate - %d samples/sec\n", kbd_wait_read());

kbd_rw_ack (AUX_ENABLE);
request_irq(12,&mouse_interrupt,"PS2/mouse",INTERRUPT_IRQ,NULL);
}




