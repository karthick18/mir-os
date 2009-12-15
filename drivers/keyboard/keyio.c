/*   MIR-OS : Keyboard standard IO functions:
;    Copyright (C) 2003 A.R.Karthick 
;    <a_r_karthic@rediffmail.com,karthick_r@infosys.com>
;    Pratap Pv - pratap@starch.tk
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
;   Standard keyboard IO functions:
*/
#include <mir/kernel.h>
#include <mir/ps2.h>
#include <asm/io.h>
#include <asm/kbd.h>

int kbd_wait_write(unsigned short port, unsigned char data)
{
  unsigned long timeout;
  unsigned char stat;

  for (timeout = 50000UL; timeout != 0; timeout--)
    {
      stat = inb(KB_STATUS_PORT);

      if ( !(stat & 0x02) ) {
	outb(port,data);
	break;
      }
    }
  return timeout;
}

int kbd_wait_read(void)
{
  unsigned long timeout;
  unsigned char stat,data;

  for (timeout = 50000L; timeout != 0; timeout--)
    {
      stat = inb(KB_STATUS_PORT);

      /* loop until 8042 output buffer full */
      if ((stat & 0x01) != 0)
	{
	  data = inb(KB_DATA_PORT);
	  /* loop if parity error or receive timeout */
	  if((stat & 0xC0) == 0)
	    return (int)data;
	}
    }

  return -1;
}

/* write and also chk for ACK */
unsigned char kbd_rw_ack(unsigned char data)
{
  unsigned char  ret;

  kbd_wait_write(KB_STATUS_PORT, KCTRL_WRITE_AUX);

  kbd_wait_write(KB_DATA_PORT, data);

  ret = kbd_wait_read();

  if (ret!= KB_ACK)
    {
      printf ("Wrong ACK recieved: expected 0x%x, got 0x%x\n",
	      KB_ACK, ret);
      return ret;
    }

  return 0;
}


int kbd_send_data( unsigned char data )
{
  int resends = 4;
  int err = -1,timeout ;
  do
    {
      int val;
      timeout = 4000;
      if ( !kbd_wait_write(KB_DATA_PORT,data) )
	{
	  goto out;
	}

      while ( timeout-- )
	{
	  if ( (val = kbd_wait_read() ) < 0 )
	    {
	      goto out;
	    }

	  if ( val == KB_ACK )
	    {
	      err = 0;
	      goto out;
	    }

	  if ( val == KB_REBOOT )
	    {
	      break;
	    }
	}
    } while (--resends && timeout );
 out:
  return err;
}
