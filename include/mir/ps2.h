/*Author: Pratap.
  Karthick: Made some minor changes:
*/

#ifndef _PS2_H
#ifdef __KERNEL__
#define _PS2_H

#define KCTRL_ENABLE_AUX                0xA8    /* enable aux port (PS/2 mouse) */
#define KCTRL_WRITE_CMD_BYTE    0x60    /* write to command register */
#define KCTRL_WRITE_AUX                 0xD4    /* write next byte at port 60 to aux port */

/* commands to aux device (PS/2 mouse) */
#define AUX_INFORMATION                 0xE9
#define AUX_ENABLE                              0xF4
#define AUX_IDENTIFY                    0xF2
#define AUX_RESET                               0xFF
#define AUX_DEFAULT			0xF6

extern void ps2_init (void);
extern void mouseISR (void);
#endif
#endif


