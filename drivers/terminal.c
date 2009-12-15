/*
;
;    MIR-OS 002 Terminal Driver
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
;    A.R.Karthick:
;        Introduced support for virtual consoles.
; 
*/

#include <mir/kernel.h>
#include <mir/types.h>
#include <asm/string.h>
#include <asm/bitops.h>
#include <asm/text.h>
#include <asm/vesa.h>
#include <mir/terminal.h>
#include <asm/kbd.h>

/* NYI = Not Yet Implemented */

struct term *current_term; /* pointer to current terminal */
struct term consoles[MAX_CONSOLES];

/*Initialise the virtual consoles*/
void terminal_vt_init(void) { 
  int i;
  for(i = 0; i < MAX_CONSOLES; ++i) {
    struct term *console = consoles + i;
    #ifndef VESA
    text_init(console);
    #else
    vesa_init(console);
    #endif
  }
}

/* initialize the terminal to default values */
void terminal_init (struct term *tty) {
  tty->termio.cc[CC_EOF] = 0; /* NYI */
  tty->termio.cc[CC_EOL] = 0; /* NYI */
  tty->termio.cc[CC_ERASE] = BACKSPACE;
}

void terminal_clear () {
current_term->clear();
}

int terminal_switch(int console) {
  return current_term->tty_switch(console);
}

/* Perform output processing */

int terminal_write (char *buf, int len) {
struct term *tty = current_term;
int oflag = tty->termio.oflag;

if (!len || !buf || !tty->write)
	return 0;
tty->outputlen = 0;

while (len > 0) {

	if (oflag & OPOST) { /* Output Processing Enabled */

            /*  OLCUC = convert lower case to upper-case  */
            if ((oflag & OLCUC) && buf[0] >= 'a' && buf[0] <= 'z')
                buf[0] -= 32;

            /*  ONLCR = transmit NL as CR+NL:  */
            if ((oflag & ONLCR) && buf[0] == '\n')
              {
                  tty->outputbuf[tty->outputlen++] = '\r';
                  tty->outputbuf[tty->outputlen++] = '\n';
		  continue;
              }

            /*  OCRNL = transmit CR as NL:  */
            if ((oflag & OCRNL) && buf[0] == '\r')
                buf[0] = '\n';
	}


	if (tty->outputlen >= MAX_OUTPUT - 256) /* not much space left */
		return tty->outputlen;

            tty->outputbuf[tty->outputlen++] = buf[0];
	    len --;
	    buf++;

}

/* now output to the terminal */
	tty->write (tty->outputbuf, tty->outputlen);


return tty->outputlen;

}
