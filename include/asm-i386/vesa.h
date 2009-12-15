/*Author:Pratap:
  Karthick:Minor change for virtual console
*/
  
#ifdef VESA
#ifndef _VESA_H
#ifdef __KERNEL__
#define _VESA_H

#include <mir/terminal.h>

extern unsigned char *video_pointer;
extern void vesa_init(struct term *);
extern void plotpix (int x, int y, unsigned char col);
extern void plot_char (char the_char, int x, int y, int);
extern void rect (int x1, int y1, int x2, int y2, int col);
extern void palget(char colornum, char *R, char *G, char *B);
extern void palset(char colornum, char R, char G, char B);
extern void bmpread(int, int, unsigned long);
#endif
#endif
#endif
