/*
;
;    MIR-OS Timer Initialization
;       Initialize the system PIT!
;
;    Copyright (C) 2003 Pratap <pratap@starch.tk>
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
     A.R.Karthick  : Moved the timer interrupt handler from misc.asm
                     Made it more organised.
                     The original entry point of the timer interrupt 
                     is in entry.S.
		     Changed the timer interrupt frequency to 100 HZ or 10 milli secs. (mir/param.h) from 20 HZ or 50 milli secs.
*/

#include <mir/param.h>
#include <mir/sched.h>
#include <asm/irq.h>
#include <asm/io.h>
#include <mir/init.h>

static int loops_per_msec; /*calibrate loops for jiffy*/
static int loops_per_jiffy;

static int timer_interrupt(int irq,void *device,struct thread_regs *thread_regs) {
  (*(volatile unsigned long *)&jiffies)++;
  if(current)
    do_timer(thread_regs); 
  return 0;
}

/*simple calibration of loops per jiffy*/

static void __init calibrate_bogomips(void) {
  int i,old;
  long old_jiffies = jiffies;
  /*loop till the timer strikes*/
  for(i = 0; old_jiffies == jiffies;++i);

  /*do it again,ignore the last*/
  old_jiffies = jiffies;
  for( i = 0; old_jiffies == jiffies; ++i );
  old = i;
  old_jiffies = jiffies;
  for( i = 0; old_jiffies == jiffies ; ++i );
  
  loops_per_jiffy = (old+i)/2;
  loops_per_msec =  loops_per_jiffy/MILLISECS_PER_HZ;
  printf("Loops per jiffy = %d..\n", loops_per_jiffy);
}

void __init timer_init()
{
        static unsigned int count = LATCH; 
        /*set the timer*/
	outb(0x43, 0x36);	
	outb(0x40,  count & 0xFF);
	outb(0x40,  count >> 8);	
	/*enable the timer IRQ line*/
	request_irq(0,(irq_action_t*)&timer_interrupt,"PIT",INTERRUPT_IRQ,NULL);
	calibrate_bogomips();
}

void delay(int msecs) { 
  int val = msecs * loops_per_msec;
  int i;
  for(i = 0; i < val ; ++i );
}


  


