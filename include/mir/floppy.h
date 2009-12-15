/*Author: A.R.Karthick for MIR:
  Moved some definitions from fdc1.c
*/
#ifdef __KERNEL__
#ifndef _FLOPPY_H
#define _FLOPPY_H

#define MOTOR_OFF do { \
__asm__ __volatile__ ("push %dx\n\t"\
                      "movb $0,%al\n\t"\
                      "mov $0x3f2,%dx\n\t"\
                      "outb %al,%dx\n\t"\
                      "pop %dx\n\t"\
                      );\
}while(0)

#define inportb(x) inb(x)
#define outportb(x,y) outb(x,y)

#define FDC_IRQ  0x6
#define FDC_DEVICES 0x2
#define FDC_BLKDEV_MAJOR_NR 0x2
#define BYTE unsigned char
#define BOOL int
#define DG144_HEADS       2     /* heads per drive (1.44M) */
#define DG144_TRACKS     80     /* number of tracks (1.44M) */
#define DG144_SPT        18     /* sectors per track (1.44M) */
#define DG144_GAP3FMT  0x54     /* gap3 while formatting (1.44M) */
#define DG144_GAP3RW   0x1b     /* gap3 while reading/writing (1.44M) */

#define FD_GETBYTE_TMOUT 128
#define FD_MAX_TRIES 3

#define FDC_DOR  (0x3f2)   /* Digital Output Register */
#define FDC_MSR  (0x3f4)   /* Main Status Register (input) */
#define FDC_DATA (0x3f5)   /* Data Register */
#define FDC_DIR  (0x3f7)   /* Digital Input Register (input) */
#define FDC_CCR  (0x3f7)   /* Configuration Control Register (output) */

#define CMD_SPECIFY (0x03)  /* specify drive timings */
#define CMD_WRITE   (0xc5)  /* write data (+ MT,MFM) */
#define CMD_READ    (0xe6)  /* read data (+ MT,MFM,SK) */
#define CMD_RECAL   (0x07)  /* recalibrate */
#define CMD_SENSEI  (0x08)  /* sense interrupt status */
#define CMD_FORMAT  (0x4d)  /* format track (+ MFM) */
#define CMD_SEEK    (0x0f)  /* seek track */
#define CMD_VERSION (0x10)  /* FDC version */


#define FALSE 0
#define TRUE 1

struct buffer_head;
struct thread_regs;

typedef struct DrvGeom {
   BYTE heads;
   BYTE tracks;
   BYTE spt;     /* sectors per track */
} DrvGeom;

typedef struct DmaChannel {
   int page;     /* page register */
   int offset;   /* offset register */
   int length;   /* length register */
} DmaChannel;

typedef struct FdcStruct { 
  int fdc_drive;
  unsigned long tbaddr;
  volatile BOOL done ;
  BOOL motor;
  BYTE status[7] ;
  BYTE statsz ;
  BYTE sr0 ;
  BYTE fdc_track ;
  DrvGeom geometry ;
}FdcStruct;

extern void fd_sendbyte(int byte);
extern int fd_getbyte();
extern void fd_irq6(void);
extern BOOL fd_seek(int track);
extern BOOL fd_waitfdc(BOOL sensei);
extern void fd_reset(void);
extern void fd_recalibrate(void);
extern void floppy_interrupt(void);
extern void floppy_interrupt_handler(struct thread_regs *);
extern void fd_init(void);
#endif
#endif
