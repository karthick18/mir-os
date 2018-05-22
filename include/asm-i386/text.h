#ifndef VESA
#ifndef _TEXT_H
#ifdef __KERNEL__
#define _TEXT_H

#define ROWS 24
#define COLS 80
#define _ROWS (ROWS + 1)
#define BACKUP_ROWS (300)
#define SCREEN_WIDTH (COLS * 2)
#define SCREEN_BUFFER ( SCREEN_WIDTH * _ROWS)
#define SCREEN_BUFFER_BACKUP ( SCREEN_WIDTH * BACKUP_ROWS)
#define SCREEN_VIDEO   ( (unsigned char *)0xB8000)
struct term;
extern void text_init (struct term *tty);
#endif
#endif
#endif

