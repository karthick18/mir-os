/*A.R.Karthick for MIR:
  Reboot the system by setting the pulse reset-line low 
  in the keyboard controller:
*/
#ifdef __KERNEL__

#ifndef _REBOOT_H
#define _REBOOT_H

/*this determines the wait lapse for a reboot failure:- 5 ticks
  or 50 ms
*/
#define REBOOT_WAIT 5

/*Wait till the keyboard input buffer is full*/
static __inline__ void keyb_wait(void) {
  __asm__ __volatile__("1:\n"
                       "inb $0x64,%%al\n"
                       "testb $0x02,%%al\n"
                       "jne 1b\n"
                       : : : "ax" );
}

extern volatile void mir_reboot(void);
extern volatile void triple_fault(void);

#endif
#endif

                       
