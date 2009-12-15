/*   MIR-OS : VESA driver
;    Copyright (C) 2003 Pratap Pv 
;    Pratap Pv - pratap@starch.tk
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
;  A.R.Karthick             : Changes to vesa_scroll to call string.h interface
;                            Some random minor changes w.r.t some funs.
;
;  A.R.Karthick: Hacks for virtual consoles
*/

#ifdef VESA

#include <mir/kernel.h>
#include <mir/types.h>
#include <mir/terminal.h>
#include <asm/vesa.h>
#include <asm/font.h>
#include <asm/io.h>
#include <asm/string.h>
#include <asm/kbd.h>
#include <asm/irq.h>

#define COLS 1024// 47 * 128 lines * chars


typedef struct {
   unsigned int size;               /* Header size in bytes      */
   int width,height;                /* Width and height of image */
   unsigned short int planes;       /* Number of colour planes   */
   unsigned short int bits;         /* Bits per pixel            */
   unsigned int compression;        /* Compression type          */
   unsigned int imagesize;          /* Image size in bytes       */
   int xresolution,yresolution;     /* Pixels per meter          */
   unsigned int ncolours;           /* Number of colours         */
   unsigned int importantcolours;   /* Important colours         */
} infoheader;

typedef struct {
   unsigned char b,g,r,junk;
} palette;


typedef struct {
  unsigned long         ID;             // 0x41534556 = "VESA"
  unsigned short        Ver;
  unsigned char         *OEMName;
  unsigned long         Capatibs;
  unsigned short        *ModeList;
  unsigned short        TotalMemory;
  unsigned char         Res[492];
} VESA_INFO;

typedef struct {
  short   ModeAttributes;         /* Mode attributes                  */
  char    WinAAttributes;         /* Window A attributes              */
  char    WinBAttributes;         /* Window B attributes              */
  short   WinGranularity;         /* Window granularity in k          */
  short   WinSize;                /* Window size in k                 */
  short   WinASegment;            /* Window A segment                 */
  short   WinBSegment;            /* Window B segment                 */
  void    *WinFuncPtr;            /* Pointer to window function       */
  short   BytesPerScanLine;       /* Bytes per scanline               */
  short   XResolution;            /* Horizontal resolution            */
  short   YResolution;            /* Vertical resolution              */
  char    XCharSize;              /* Character cell width             */
  char    YCharSize;              /* Character cell height            */
  char    NumberOfPlanes;         /* Number of memory planes          */
  char    BitsPerPixel;           /* Bits per pixel                   */
  char    NumberOfBanks;          /* Number of CGA style banks        */
  char    MemoryModel;            /* Memory model type                */
  char    BankSize;               /* Size of CGA style banks          */
  char    NumberOfImagePages;     /* Number of images pages           */
  char    res1;                   /* Reserved                         */
  char    RedMaskSize;            /* Size of direct color red mask    */
  char    RedFieldPosition;       /* Bit posn of lsb of red mask      */
  char    GreenMaskSize;          /* Size of direct color green mask  */
  char    GreenFieldPosition;     /* Bit posn of lsb of green mask    */
  char    BlueMaskSize;           /* Size of direct color blue mask   */
  char    BlueFieldPosition;      /* Bit posn of lsb of blue mask     */
  char    RsvdMaskSize;           /* Size of direct color res mask    */
  char    RsvdFieldPosition;      /* Bit posn of lsb of res mask      */
  char    DirectColorModeInfo;    /* Direct color mode attributes     */

  /* VESA 2.0 variables */
  long    PhysBasePtr;           /* physical address for flat frame buffer */
  long    OffScreenMemOffset;            /* pointer to start of off screen memory */
  short   OffScreenMemSize;      /* amount of off screen memory in 1k units */
  char    res2[206];              /* Pad to 256 byte block size       */
} VESA_MODE_INFO;


VESA_MODE_INFO *vesa;

unsigned char *video_pointer;


static void update_vesa_cur () {
int the_x = (current_term->x-1)*8, the_y = current_term->y*16, i, j;

for (i = 0; i<8; i++) {
	for (j = 0; j<16; j++) {
		plotpix (the_x+i, the_y+j, 9);
	}
}
}

static void vesa_clrscr(void) {
unsigned char *start = (unsigned char *)((vesa->PhysBasePtr)+55*1024*2+2*1024);
memset (start, 0, 192512 * 4);
current_term->x = 0,current_term->y= 15;
}

static void vesa_scroll () {
/* scroll 1 vesa line */
unsigned char *start = (unsigned char *)((vesa->PhysBasePtr)+55*1024*2+2*1024);
unsigned char *end = (unsigned char *)((vesa->PhysBasePtr)+2*8*1024+55*1024*2+2*1024);
 cli();

/* A.R.Karthick: This shouldnt be doing anything.
  Thats because lodsb instruction loads the string in SI 
  to the accumulator AX.
  Here I presume Pratap wanted to zero off the DI (stosb)
  or the "start" location.
*/

/* Pratap: karthik the below works fine, but is slow */
 memcpy(start,end,(192512 * 4) );
 sti();
}

static int
vesa_write (char *str, int len) {
  int i;
  for (i=0; i < len; i++) {
    switch (str[i]) {

    case '\n':
      current_term->y+=2; current_term->x = 0;
      if (current_term->y>=94) {
	current_term->y-=2;
	vesa_scroll();
      }
      break;

    case '\t':
      if(! current_term->x) ++current_term->x;
      current_term->x = ALIGN_VAL(current_term->x,8);
      break;

    case BACKSPACE: 
      current_term->x-=2;
      printf (" ");
      break;

    default:
      plot_char (str[i], current_term->x*8, current_term->y*8, 255);
      ++current_term->x;
      if (current_term->x==128) { current_term->x = 0; current_term->y+=2;}
      if (current_term->y>=94) {current_term->y-=2; vesa_scroll();}
      break;
	
    }

  }
  return len;
}

void bmpread(int x1, int y1, unsigned long addr) {
int i, j, ix=0, iy=0;
infoheader *inf = (infoheader *)(addr+14);
palette *pal = (palette *)(addr+14+40);
unsigned char *curcol = (unsigned char *)(addr+1078);

for (i=0; i<256; i++) 
	palset (i, pal[i].r >> 2, pal[i].g >> 2, pal[i].b>>2);

for (i=0; i< inf->height; i++) {
	for (j=0; j<inf->width; j++) {
		plotpix (x1+ix, y1+iy, *curcol);
		curcol++;
		ix++;
	}
ix = 0; iy++;
}
}

void rect (int x1, int y1, int x2, int y2, int col) {
int i, j;
for (i = x1; i < x2; i++) 
	for (j=y1; j< y2; j++)
		plotpix (i, j, col);


}

void palget(char colornum, char *R, char *G, char *B) {
    outb(0x03C7,colornum);
    (*R) = inb(0x03C9);
    (*G) = inb(0x03C9);
    (*B) = inb(0x03C9);
  }

void palset(char colornum, char R, char G, char B) {
    outb(0x03C8,colornum);
    outb(0x03C9,R);
    outb(0x03C9,G);
    outb(0x03C9,B);
  }


void plotpix (int x, int y, unsigned char col) { /* pixel plot in 1024x768 VESA */
video_pointer[y*COLS+x] = col;
}

static void printmap (char chr) {
int i;
for (i = 7; i >= 0; i--) {
if (1<<i & chr)
	printf ("1");
else
	printf ("0");
}
printf ("\n");
}

void plot_char (char the_char, int x, int y, int col) {
int start;
int i, ix = 0, iy = 0, j;
#ifdef A16
#define FONT_LEN 16
#endif
#ifndef A16
#define FONT_LEN 8
#endif

for (i = 0; i < FONT_LEN; i++) {
start = the_char * FONT_LEN +i;
	for (j = 7; j >= 0; j--) {
		if (1<<j & fonts[start]) 
			plotpix (x+ix, y+iy, col);
		ix++;	

	}
iy++;
ix = 0;
}
}

static int vesa_switch(int nr) { 
  /*Karthick: Pratap!! please fill this up.*/
  return nr;
}

void vesa_init(struct term *tty) {
  /* NO error checking */
  vesa = (VESA_MODE_INFO *) 0x80000;
  video_pointer = (unsigned char *)(vesa->PhysBasePtr);
  terminal_init (tty);
  tty->x = 0, tty->y = 15;
  /* print info */
  printf ("VESA: VBE2. Bytes/Scanline = %d, Resolution = %dx%d, Bpp = %d, PhysBasePtr = %#08lx\n", vesa->BytesPerScanLine, vesa->XResolution, vesa->YResolution, vesa->BitsPerPixel, vesa->PhysBasePtr); 
  tty->read = NULL;
  tty->write = vesa_write;
  tty->clear = vesa_clrscr;
  tty->scroll = vesa_scroll;
  tty->tty_switch = vesa_switch;
  /* Initialize VESA terminal */
}
#endif
