/*   MIR-OS : Scheduler helper routine.
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
;  This file now simply contains the definition
;  of __switch_to:
;  The above statement is no longer true :-)
;  Modified this file to include TSS states on task switches:
*/

#include <mir/sched.h>
#include <asm/system.h>
#include <asm/segment.h>
#include <asm/irq.h>
/*deactivation would result in clearing the busy flag and we dont want
  to do that as iret would take care about such situations
*/
static __inline__ void deactivate_tss(struct task_struct *task) { 
}

static __inline__ void setup_tss(struct task_struct *next,struct task_struct *prev) { 
  init_tss_state.esp0 = next->thread_struct.esp0;
  deactivate_tss(prev);
}

FASTCALL_DEFINE(void __switch_to(struct task_struct *next,struct task_struct *prev)) {
  struct thread_struct *next_thread = &next->thread_struct;  
  /*Load the esp0 in the TSS*/
  setup_tss(next,prev);
  /*Just load the debug registers*/
  if(next_thread->debugreg[7]) {
    load_debugreg(next_thread,0);
    load_debugreg(next_thread,1);
    load_debugreg(next_thread,2);
    load_debugreg(next_thread,3); 
    load_debugreg(next_thread,4);
    /*no number:4 and 5*/
    load_debugreg(next_thread,6);
    load_debugreg(next_thread,7);
  }
 }

