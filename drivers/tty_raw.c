/*Author: Pratap
   A.R.Karthick : Reorganised this file.
                  Some random minor additions.
                  Fixed a Bug with TAB handling in printstr
		  Introduced support for virtual consoles.
*/
#ifndef VESA
#include <mir/kernel.h>
#include <mir/sched.h>
#include <asm/page.h>
#include <asm/io.h>
#include <asm/bitops.h>
#include <asm/kbd.h>
#include <asm/text.h>
#include <mir/terminal.h>
#include <asm/string.h>
#include <asm/irq.h>
  
/*Clear the screen contents*/
static void text_clrscr(void) { 
  unsigned char *src = current_term->screen;
  memset_words(src,0x0720,SCREEN_BUFFER/2);
  current_term->x = current_term->y = 0;
}

static void updatecur (int x, int y) {
outb (0x3D4, 0xF);
outb (0x3D5, LOW_B(x+COLS*y));
outb (0x3D4, 0xE);
outb (0x3D5, HIGH_B(x+COLS*y));
}
/*Switch active consoles:*/
static int text_switch(int nr) 
{
  int err =-1;
  struct term *console = consoles + nr, *old;
  if( (console == current_term ) || nr >= MAX_CONSOLES  )
    goto out;
  /*backup*/
  old = current_term;
  current_term = console;
  memcpy(old->backup,old->screen,SCREEN_BUFFER);
  /*now copy the dump*/
  memcpy(current_term->screen,current_term->backup,SCREEN_BUFFER);
  updatecur(current_term->x,current_term->y); 
  err = 0;
 out:
  return err;
}

static void scroll(int rows, int dir) {
  unsigned char *dst = current_term->screen;
  unsigned char *src;
  int start_src;
  int backup_row;
  if(current_term->cur_scroll_row < 0) {
    if(dir > 0) {
      return;
    }
    current_term->cur_scroll_row = current_term->cur_backup_row;
  }
  backup_row = current_term->cur_scroll_row - rows;
  if((backup_row + dir) < 0) {
    return;
  }
  start_src = (backup_row + dir)*SCREEN_WIDTH;
  src = current_term->backup  + start_src;
  memcpy(dst,src,SCREEN_BUFFER);
  current_term->cur_scroll_row += dir;
  if(current_term->cur_scroll_row >= current_term->cur_backup_row) {
    current_term->cur_scroll_row = -1;
    memset_words(dst + (SCREEN_BUFFER - SCREEN_WIDTH),0x0720,SCREEN_WIDTH/2);    
    updatecur(current_term->x, current_term->y);
  }
}

static void scroll_up(int pages) {
  scroll(pages * ROWS, -1);
}

static void scroll_down(int pages) {
  scroll(pages * ROWS, 1);
}

static void text_scroll (void) {
  unsigned char *dst = current_term->screen;
  unsigned char *src = (current_term->screen+ SCREEN_WIDTH );
  memcpy(dst,src,SCREEN_BUFFER - SCREEN_WIDTH);
  memset_words(dst + (SCREEN_BUFFER - SCREEN_WIDTH),0x0720,SCREEN_WIDTH/2);
}

static int text_write(char *r, int len) {
  unsigned char *d = (char *)current_term->screen;
  unsigned char *backup = (char *)current_term->backup;
  int i, k, k_backup, attr = current_term->attr;
  for (i=0; i< len; i++) {
    k = SCREEN_WIDTH*current_term->y + current_term->x*2;
    k_backup = SCREEN_WIDTH * current_term->cur_backup_row + current_term->x*2;
    switch (r[i]) {
    case '\n': {
      current_term->cur_backup_row++;
      current_term->y++; current_term->x=0;
      if (current_term->y==ROWS) {
	text_scroll();
	--current_term->y;
      }
      if(current_term->cur_backup_row >= BACKUP_ROWS) {
	current_term->cur_backup_row = BACKUP_ROWS-1;
      }
      updatecur (current_term->x, current_term->y);
      break;
    }
    case BACKSPACE: {
      --current_term->x; k-=2; k_backup -= 2;
      if (current_term->x==0){
	--current_term->y; current_term->x = COLS;
	--current_term->cur_backup_row;
      }
      updatecur(current_term->x, current_term->y);
      d[k] = ' ';
      backup[k_backup] = ' ';
      break;
    }
    case '\t': {
      if(!current_term->x) ++current_term->x;
      /*Align the value on a 8 byte boundary*/
      current_term->x = ALIGN_VAL((int)current_term->x,8);
      updatecur (current_term->x, current_term->y);
      break;
    }
		  
    default: {
      backup[k_backup] = d[k] = (char) r[i];
      backup[k_backup+1] = d[k+1] = (char) attr;
      ++current_term->x;

      if (current_term->x==COLS) {
	current_term->x = 0;
	++current_term->y;
	++current_term->cur_backup_row;
      }
		
      if (current_term->y==ROWS) {
	text_scroll();
	--current_term->y;
      }
      if(current_term->cur_backup_row >= BACKUP_ROWS) {
	current_term->cur_backup_row = BACKUP_ROWS - 1;
      }
      updatecur (current_term->x, current_term->y);
      break;
    }
    }
  }
  return len;
}

void text_init (struct term *tty)  {
  terminal_init (tty);
  memset_words(tty->backup,0x0720,SCREEN_BUFFER_BACKUP/2);
  tty->screen = SCREEN_VIDEO;
  tty->write = text_write;
  tty->clear = text_clrscr;
  tty->scroll = text_scroll;
  tty->scroll_up = scroll_up;
  tty->scroll_down = scroll_down;
  tty->tty_switch = text_switch;
  tty->attr   = 0xf;
  tty->has_shell = 0;
  tty->x = 0,tty->y = 0;
  tty->cur_backup_row = 0;
  tty->cur_scroll_row = -1;
}

#endif
