/*Author:Pratap
  Karthick: introduced support for virtual consoles
*/

#ifndef _TERMINAL_H
#ifdef __KERNEL__
#define _TERMINAL_H

#include <mir/types.h>

#define MAX_OUTPUT 4096
#define MAX_CONSOLES 12

/* control character definitions */
#define MAX_CC 3

#define CC_EOF            0               /*  EOF   character     */
#define CC_EOL            1               /*  EOL   character     */
#define CC_ERASE          2               /*  ERASE character     */


/*
 *  Input flags:
 *
 *  POSIX compatible (not all are implemented in MIR 002)
 */

#define BRKINT          0x00000002      /*  Signal interrupt on break.  */
#define ICRNL           0x00000100      /*  Map CR to NL on input.  */
#define IGNBRK          0x00000001      /*  Ignore break condition.  */
#define IGNCR           0x00000080      /*  Ignore CR.  */
#define IGNPAR          0x00000004      /*  Ignore characters with parity errors.  */
#define INLCR           0x00000040      /*  Map NL to CR on input.  */
#define INPCK           0x00000010      /*  Enable input parity check.  */
#define ISTRIP          0x00000020      /*  Strip 8th bit of character.  */
#define IUCLC           0x00001000      /*  Map upper case to lower case on input.  */
#define IXANY           0x00000800      /*  Enable any character to restart output.  */
#define IXOFF           0x00000400      /*  Enable start/stop input control.  */
#define IXON            0x00000200      /*  Enable start/stop output control.  */
#define PARMRK          0x00000008      /*  Mark parity errors.  */

/*
 *  Output flags:
 *
 *  POSIX compatible (not all are implemented in MIR 002)
 */

#define OPOST           0x00000001      /*  Perform output processing.  */
#define OLCUC           0x00000020      /*  Map lower case to upper on output.  */
#define ONLCR           0x00000002      /*  Map NL to CR-NL on output.  */
#define OCRNL           0x00000010      /*  Map CR to NL on output.  */
#define ONOCR           0x00000040      /*  No CR output at column 0.  */
#define ONLRET          0x00000080      /*  NL performs CR function.  */


/*
 *  Local flags:
 *
 *  POSIX compatible (not all are implemented in MIR 002)
 */

#define ECHO            0x00000008      /*  Enable echo.  */
#define ECHOE           0x00000002      /*  Echo ERASE as an error correcting backspace.  */
#define ECHOK           0x00000004      /*  Echo NL after line kill  */
#define ECHONL          0x00000010      /*  Echo NL even if ECHO is off.  */
#define ICANON          0x00000100      /*  Canonical input (erase/kill processing).  */
#define IEXTEN          0x00000400      /*  Enable extended functions.  */
#define ISIG            0x00000080      /*  Enable signals: INTR, QUIT, SUSP  */
#define NOFLSH          0x80000000      /*  Disable flush after interrupt, quit, or suspend.  */
#define TOSTOP          0x00400000      /*  Send SIGTTOU for background output.  */
#define XCASE           0x01000000      /*  Canonical upper/lower presentation.  */




struct termios {
	u_int32_t iflag; /* input flags */
	u_int32_t oflag; /* output flags */
	u_int32_t lflag; /* local flags */
	unsigned char cc[MAX_CC]; /* control characters */
};


struct term {
  struct termios termio;
  char            outputbuf [MAX_OUTPUT];
#ifndef VESA
  unsigned char   backup[SCREEN_BUFFER];
  unsigned char   *screen;
  int attr;
#endif
  int has_shell;
  int x,y;
  int		outputlen;
  
  /* IO functions to device follow */
  /* Devices implemented on MIR 002
     1) VBE 2: Vesa Linear FrameBuffer- 1024x768x256
     2) Text output
  */
  int (*read)();
  int (*write) ();
  int (*tty_switch)(int);  
  /* below only for screen terminals */
  void (*clear) ();
  void (*scroll) ();

};

extern void terminal_init (struct term *tty);
extern void terminal_clear ();
extern int terminal_write (char *buf, int len);
extern int terminal_switch(int console);
extern void terminal_vt_init(void);
extern struct term *current_term;
extern struct term consoles[]; /* the console ! */

/*Set the current virtual terminal*/
static __inline__ void set_current_terminal(int console) {
  if(console < MAX_CONSOLES)
    current_term = consoles + console;
}

#endif
#endif
