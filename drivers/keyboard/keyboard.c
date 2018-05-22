/* MIR-OS Keyboard driver
   ;   a.r.karthick@gmail.com ; a_r_karthic@rediffmail.com
   ;  
   ;    Copyright (C) 2003 
   ;              i) TABOS team.
   ;	      ii) A.R.Karthick
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
   ;    Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307 
   ;    USA
   ;
   ; This keyboard driver has been rewritten from scratch based on the design
   ; and code of TABOS.Its been quite heavily modified to work with MIR.
   ;
*/

#include <mir/kernel.h>
#include <mir/types.h>
#include <asm/irq.h>
#include <asm/io.h>
#include <asm/kbd.h>
#include <asm/setup.h>
#include <asm/page.h>
#include <asm/reboot.h>
#include <mir/slab.h>
#include <mir/init.h>
#include <asm/text.h>
#include <mir/terminal.h>
#include <mir/tasklet.h>

/* some keyboard key definitions */

#define KEYB_BUFFER 0x20

#define empty(ptr)  ( (ptr)->out_ptr == (ptr)->in_ptr )

typedef struct  
{
  int data[KEYB_BUFFER];
  int raw_data[KEYB_BUFFER];
  int *cir_buff;
  int *raw_cir_buff;
  unsigned int size, in_ptr, out_ptr;
} queue_t;
static queue_t kbd_q;


static volatile int key_extended = 0;
static volatile int key_altgr = 0;
static volatile int key_pad_seq = 0;
static volatile int key_shifts = 0;
/* lookup table for converting hardware scancodes */
static unsigned char hw_to_mycode[ 128 ] =
  {
    /* 0x00 */  0,              KEY_ESC,        KEY_1,          KEY_2,
    /* 0x04 */  KEY_3,          KEY_4,          KEY_5,          KEY_6,
    /* 0x08 */  KEY_7,          KEY_8,          KEY_9,          KEY_0,
    /* 0x0C */  KEY_MINUS,      KEY_EQUALS,     KEY_BACKSPACE,  KEY_TAB,
    /* 0x10 */  KEY_Q,          KEY_W,          KEY_E,          KEY_R,
    /* 0x14 */  KEY_T,          KEY_Y,          KEY_U,          KEY_I,
    /* 0x18 */  KEY_O,          KEY_P,          KEY_OPENBRACE,  KEY_CLOSEBRACE,
    /* 0x1C */  KEY_ENTER,      KEY_LCONTROL,   KEY_A,          KEY_S,
    /* 0x20 */  KEY_D,          KEY_F,          KEY_G,          KEY_H,
    /* 0x24 */  KEY_J,          KEY_K,          KEY_L,          KEY_COLON,
    /* 0x28 */  KEY_QUOTE,      KEY_TILDE,      KEY_LSHIFT,     KEY_BACKSLASH,
    /* 0x2C */  KEY_Z,          KEY_X,          KEY_C,          KEY_V,
    /* 0x30 */  KEY_B,          KEY_N,          KEY_M,          KEY_COMMA,
    /* 0x34 */  KEY_STOP,       KEY_SLASH,      KEY_RSHIFT,     KEY_ASTERISK,
    /* 0x38 */  KEY_ALT,        KEY_SPACE,      KEY_CAPSLOCK,   KEY_F1,
    /* 0x3C */  KEY_F2,         KEY_F3,         KEY_F4,         KEY_F5,
    /* 0x40 */  KEY_F6,         KEY_F7,         KEY_F8,         KEY_F9,
    /* 0x44 */  KEY_F10,        KEY_NUMLOCK,    KEY_SCRLOCK,    KEY_7_PAD,
    /* 0x48 */  KEY_8_PAD,      KEY_9_PAD,      KEY_MINUS_PAD,  KEY_4_PAD,
    /* 0x4C */  KEY_5_PAD,      KEY_6_PAD,      KEY_PLUS_PAD,   KEY_1_PAD,
    /* 0x50 */  KEY_2_PAD,      KEY_3_PAD,      KEY_0_PAD,      KEY_DEL_PAD,
    /* 0x54 */  KEY_PRTSCR,     0,              KEY_BACKSLASH2, KEY_F11,
    /* 0x58 */  KEY_F12,        0,              0,              KEY_LWIN,
    /* 0x5C */  KEY_RWIN,       KEY_MENU,       0,              0,
    /* 0x60 */  0,              0,              0,              0,
    /* 0x64 */  0,              0,              0,              0,
    /* 0x68 */  0,              0,              0,              0,
    /* 0x6C */  0,              0,              0,              0,
    /* 0x70 */  KEY_KANA,       0,              0,              KEY_ABNT_C1,
    /* 0x74 */  0,              0,              0,              0,
    /* 0x78 */  0,              KEY_CONVERT,    0,              KEY_NOCONVERT,
    /* 0x7C */  0,              KEY_YEN,       0,              0
  };

/* lookup table for converting extended hardware codes */
static unsigned char hw_to_mycode_ex[ 128 ] =
  {
    /* 0x00 */  0,              KEY_ESC,        KEY_1,          KEY_2,
    /* 0x04 */  KEY_3,          KEY_4,          KEY_5,          KEY_6,
    /* 0x08 */  KEY_7,          KEY_8,          KEY_9,          KEY_0,
    /* 0x0C */  KEY_MINUS,      KEY_EQUALS,     KEY_BACKSPACE,  KEY_TAB,
    /* 0x10 */  KEY_CIRCUMFLEX, KEY_AT,         KEY_COLON2,     KEY_R,
    /* 0x14 */  KEY_KANJI,      KEY_Y,          KEY_U,          KEY_I,
    /* 0x18 */  KEY_O,          KEY_P,          KEY_OPENBRACE,  KEY_CLOSEBRACE,
    /* 0x1C */  KEY_ENTER_PAD,  KEY_RCONTROL,   KEY_A,          KEY_S,
    /* 0x20 */  KEY_D,          KEY_F,          KEY_G,          KEY_H,
    /* 0x24 */  KEY_J,          KEY_K,          KEY_L,          KEY_COLON,
    /* 0x28 */  KEY_QUOTE,      KEY_TILDE,      0,              KEY_BACKSLASH,
    /* 0x2C */  KEY_Z,          KEY_X,          KEY_C,          KEY_V,
    /* 0x30 */  KEY_B,          KEY_N,          KEY_M,          KEY_COMMA,
    /* 0x34 */  KEY_STOP,       KEY_SLASH_PAD,  0,              KEY_PRTSCR,
    /* 0x38 */  KEY_ALTGR,      KEY_SPACE,      KEY_CAPSLOCK,   KEY_F1,
    /* 0x3C */  KEY_F2,         KEY_F3,         KEY_F4,         KEY_F5,
    /* 0x40 */  KEY_F6,         KEY_F7,         KEY_F8,         KEY_F9,
    /* 0x44 */  KEY_F10,        KEY_NUMLOCK,    KEY_PAUSE,      KEY_HOME,
    /* 0x48 */  KEY_UP,         KEY_PGUP,       KEY_MINUS_PAD,  KEY_LEFT,
    /* 0x4C */  KEY_5_PAD,      KEY_RIGHT,      KEY_PLUS_PAD,   KEY_END,
    /* 0x50 */  KEY_DOWN,       KEY_PGDN,       KEY_INSERT,     KEY_DEL,
    /* 0x54 */  KEY_PRTSCR,     0,              KEY_BACKSLASH2, KEY_F11,
    /* 0x58 */  KEY_F12,        0,              0,              KEY_LWIN,
    /* 0x5C */  KEY_RWIN,       KEY_MENU,       0,              0,
    /* 0x60 */  0,              0,              0,              0,
    /* 0x64 */  0,              0,              0,              0,
    /* 0x68 */  0,              0,              0,              0,
    /* 0x6C */  0,              0,              0,              0,
    /* 0x70 */  0,              0,              0,              0,
    /* 0x74 */  0,              0,              0,              0,
    /* 0x78 */  0,              0,              0,              0,
    /* 0x7C */  0,              0,              0,              0
  };

/* convert scancodes into key_shifts flag bits */
static unsigned int modifier_table[KEY_MAX - KEY_MODIFIERS] =
  {
    KB_SHIFT_FLAG,    KB_SHIFT_FLAG,    KB_CTRL_FLAG,
    KB_CTRL_FLAG,     KB_ALT_FLAG,      KB_ALT_FLAG,
    KB_LWIN_FLAG,     KB_RWIN_FLAG,     KB_MENU_FLAG,
    KB_SCROLOCK_FLAG, KB_NUMLOCK_FLAG,  KB_CAPSLOCK_FLAG
  };

/* convert numeric pad scancodes into arrow codes */
static unsigned char numlock_table[10] =
  {
    KEY_INSERT, KEY_END,    KEY_DOWN,   KEY_PGDN,   KEY_LEFT,
    KEY_5_PAD,  KEY_RIGHT,  KEY_HOME,   KEY_UP,     KEY_PGUP
  };


/* set_leds()
 * sets keyboard leds on and off
 *
 * params: leds state
 * return: -
 */

static __inline__ void set_leds( int leds )
{
  kbd_send_data(0xED);
  kbd_send_data((unsigned char) ( (leds >> LEDS_SHIFT) & 7 ) );
}

static int deq(queue_t *q, int *data,int *raw_data)
{
  /* if out_ptr reaches in_base, the queue is empty */
  if(empty(q) || !data)
    return -1;
  *data = q->cir_buff[q->out_ptr];
  if(raw_data) 
    *raw_data = q->raw_cir_buff[q->out_ptr];
  q->out_ptr = (q->out_ptr + 1) % q->size;
  return 0;
}

static void putch (char ch) {
  char arr[2] = {ch, '\0'};
  printf (arr);
}

int getch() {
  int  data;
  do {
    while (empty(&kbd_q));
  } while (deq(&kbd_q, &data,NULL) != 0);
  return data;
}

int getstr (char *ptr) {
  int ch;
  char *str ;
  str = ptr;
  while (1) {
    ch = getch();
    if(ch & 0xffff00) {
      continue; /*ignore ctrl characters*/
    }
    if(ch == BACKSPACE && str == ptr)
      continue;
    putch (ch); 
    if(ch == BACKSPACE) {
      --str;
      continue;
    }  
    *str++=ch;
    if (ch=='\n') {
      --str;
      break;
    }
  }
  /*safe*/
  *str = '\0';
  return str - ptr;
}

static void terminal_tasklet(void *arg) { 
  /*do the terminal switch down here*/
  terminal_switch((int)arg);
}

/*mark a keyboard tasklet to run*/
static __inline__ int register_tasklet(struct tasklet_struct *tasklet,tasklet_function_t *routine,void *data) {
  tasklet->routine = routine;
  tasklet->data    = data;
  /*schedule the tasklet to run*/
  tasklet_schedule(tasklet,TASKLET_TYPE0);
  return 0;
}

static int ub_inq(int keycode,int scancode)
{
  int next;
  queue_t *q = &kbd_q;
  next = (q->in_ptr + 1 ) % q->size;
  /* if in_ptr reaches out_ptr, the queue is full */
  if(next == q->out_ptr)
    return -1;
  q->cir_buff[q->in_ptr] = keycode;
  q->raw_cir_buff[q->in_ptr] = scancode;
  q->in_ptr = next;
  return 0;
}
/* Hardware level keyboard interrupt handler */
static int keyb_interrupt(int irq,void *device,struct thread_regs *regs)
{
  int code = kbd_wait_read();
  int origcode, mycode, flag = 0, numlock, i = 0;
  if(code <  0 ) 
    goto end;    
  if ( IS_KEY_EXTENDED(code) )
    {
      /* flag that the next key will be an extended one */
      key_extended = 1;
      goto end;
    }
  
  /* convert from hardware to readable format */
  if ( key_extended )
    {
      mycode = hw_to_mycode_ex[ code & 0x7F ];
      key_extended = 0;
    }
  else
    {
      mycode = hw_to_mycode[ code & 0x7F ];
    }

  if ( !mycode )
    {
      goto end;
    }

  origcode = mycode;
  /*If its a modifier key, get the bitmask*/
  if ( mycode >= KEY_MODIFIERS )
    {
      flag = modifier_table[ mycode - KEY_MODIFIERS ];
    }
  numlock = (key_shifts & KB_NUMLOCK_FLAG) ;

  /* handle released keys */
  if ( code & 0x80 )
    {
      if ( flag & KB_ALT_FLAG )
	{
	  /* end of an alt+numpad numeric entry sequence */
	  if ( key_shifts & KB_INALTSEQ_FLAG )
	    {
	      key_shifts &= ~KB_INALTSEQ_FLAG;
	      ub_inq( key_pad_seq, code);
	    }
	}
      if ( flag & KB_MODIFIERS )
	{
	  /* turn off the shift state for this key */
	  key_shifts &= ~flag;
	  if ( mycode == KEY_ALTGR )
	    {
	      key_altgr = 0;
	    }
	}
      goto end;
    }

  /*Ctrl+ALT+F1 ->sets the standard keyboard layout :*/
  if ( ( mycode == KEY_F1 ) &&
       ( ( key_shifts & KB_CTRL_ALT ) == KB_CTRL_ALT ) )
    {
      /* switch to the standard keyboard layout */
      ub_inq( key_shifts, KEY_F1 );
      set_standard_keyboard();
      goto end;
    }
  /*FOR PC: Its ctrl F1 through ctrl F12*/
#ifndef BOCHS
  if(IS_KEY_F1(mycode,key_shifts) ){
    printf("\n");
    printhelp(0);
  }
  /*display the slab cache stats*/
  if(IS_KEY_F2(mycode,key_shifts) ) {
    printf("\n");
    display_slab_caches_stats();
  }
  /*display free mem*/
  if(IS_KEY_F3(mycode,key_shifts) ) {
    printf("\n");
    display_mem_stats();
  }
  if(IS_KEY_F4(mycode,key_shifts) ) {
    printf("\n");
    display_e820();
  }
#else /*for bochs : its key F1- F12*/
  if( (! (key_shifts & KB_CTRL_FLAG) ) && 
      (mycode >= KEY_F1 && mycode <= KEY_F12) )
    {
      static struct tasklet_struct tasklet;
      register_tasklet(&tasklet,terminal_tasklet,(void *)mycode - KEY_F1);
    }
#endif
  /*check for the modifier bitmasks*/
  if ( flag & KB_MODIFIERS )
    {
      /* turn on a modifier key */
      key_shifts |= flag;
      if ( mycode == KEY_ALTGR )
	{
	  key_altgr = 1;
	}
      ub_inq(key_shifts,mycode);
      goto end;
    }

  if ( IS_KEY_LED(flag))
    {
      /* toggle caps/num/scroll lock */
      key_shifts ^= flag;
      ub_inq(key_shifts, mycode );
      set_leds( key_shifts );
      goto end;
    }

  if ( key_shifts & KB_ALT_FLAG )
    {
      if ( ( mycode >= KEY_0_PAD ) && ( mycode <= KEY_9_PAD ) )
	{
	  /* alt+numpad numeric entry */
	  if ( key_shifts & KB_INALTSEQ_FLAG )
	    {
	      key_pad_seq = key_pad_seq * 10 + mycode - KEY_0_PAD;
	    }
	  else
	    {
	      key_shifts |= KB_INALTSEQ_FLAG;
	      key_pad_seq = mycode - KEY_0_PAD;
	    }
	  ub_inq( key_shifts, mycode );
	  goto end;
	}
      else
	{
	  /* check for alt+key: function key mappings are ignored.
	     The handling is a bit different for the right ALT key
	  */
          i = key_shifts;
#if 0
	  if ( key_altgr && key_ascii_table[mycode] != 0xffff )
	    {
	      i = key_altgr_lower_table[ mycode ];
	    }
#endif
	}
    }
  else
    if ( ( mycode >= KEY_0_PAD ) && ( mycode <= KEY_9_PAD ) )
      {
	/* handle numlock number -> arrow conversions */
	i = mycode - KEY_0_PAD;
	if(numlock)
	  {
	    mycode = numlock_table[ i ];
	    i = key_shifts;
	  }
	else
	  {
	    i = key_ascii_table[ mycode ];
	  }
      }
    else
      if ( mycode == KEY_DEL_PAD )
	{
	  /* handle numlock logic for the del key */
	  if ( numlock )
	    {
	      i = key_shifts;
	    }
	  else
	    {
	      i = key_ascii_table[ KEY_DEL_PAD ];
	    }
	}
      else
	if ( key_shifts & KB_CTRL_FLAG )
	  {
	    /* ctrl+key */

	    if ( mycode >= KEY_F1 && mycode <= KEY_F12 )
	      {
		/*switch virtual consoles*/
		static struct tasklet_struct tasklet;
		register_tasklet(&tasklet,terminal_tasklet,(void *)mycode - KEY_F1);
		goto end;
	      }
	    i = key_control_table[ mycode ];
	  }
	else
	  if ( key_shifts & KB_SHIFT_FLAG )
	    {
	      /* shift + key */
	      if ( key_shifts & KB_CAPSLOCK_FLAG )
		{
		  if ( key_ascii_table[ mycode ] == key_capslock_table[ mycode ] )
		    {
		      i = key_shift_table[ mycode ];
		    }
		  else
		    {
		      i = key_ascii_table[ mycode ];
		    }
		}
	      else
		{
		  i = key_shift_table[ mycode ];
		}
	    }
	  else
	    if ( key_shifts & KB_CAPSLOCK_FLAG )
	      {
		/* capslock + key */
		i = key_capslock_table[ mycode ];
	      }
	    else
	      {
		/* normal key */
		if(mycode == KEY_UP) {
		  if(current_term->scroll_up) {
		    current_term->scroll_up(1);
		  }
		  goto end;
		}
		if(mycode == KEY_DOWN) {
		  if(current_term->scroll_down) {
		    current_term->scroll_down(1);
		  }
		  goto end;
		}
		i = key_ascii_table[ mycode ];
	      }

  key_shifts &= ~KB_INALTSEQ_FLAG;
  ub_inq( i, mycode );

 end:
  /* reboot */
  if ( (  code == 0x4F  ||  code == 0x53  ) &&
       ( key_shifts & KB_CTRL_ALT ) == KB_CTRL_ALT
       )
    {
      mir_reboot();
    }
  return 0;
}
void __init kbd_init(void){
  kbd_q.size = KEYB_BUFFER;
  kbd_q.cir_buff = kbd_q.data;
  kbd_q.raw_cir_buff = kbd_q.raw_data;
  set_standard_keyboard();
  request_irq (1, &keyb_interrupt, "KEYB",INTERRUPT_IRQ,NULL);
}


