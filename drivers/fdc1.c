/*
This is an opensource floppy driver code which i found on the net
Some parts of it wasent working on bochs. Heavily modified src code.
Ported to MIR-OS (and works !).

TODO :
	floppy disk change
	optimisations

When i get hdd to work, i plan on rewriting the entire thing and
implement a type of VFS (fat12/fat32/ext3/joilet).

-pratap
*/

/*
 * fdc.c - Floppy controller handler functions.
 * 
 * Copyright    (C) 1998  Fabian Nunez
 *              (C) 2003  Pratap Pv
 * Ported and modified to MIR-OS by Pratap <pratap@starch.tk>
 * 
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 * 
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 * 
 * The author can be reached by email at: fabian@cs.uct.ac.za
 * 
 * or by airmail at: Fabian Nunez
 *                   10 Eastbrooke
 *                   Highstead Road
 *                   Rondebosch 7700
 *                   South Africa
 *  
 *    A.R.Karthick  : Made some random changes and hacks.
                      Changed the interrupt handler 
                      to work without destroying the stack by 
                      movl %ebp,%esp; pop %ebp; iret
                      Changed the entry point 
                      of interrupt handler to entry.S.
                      Filled up floppy motor oFF routine:)-Not working:-(
                      Made the fd_init get inside .inittext section.
		      Changed the setidt interface
 */

#include <mir/kernel.h>
#include <mir/sched.h>
#include <asm/string.h>
#include <asm/page.h>
#include <asm/io.h>
#include <asm/irq.h>
#include <asm/irq_stat.h>
#include <mir/init.h>
#include <mir/blkdev.h>
#include <mir/vfs/fs.h>
#include <mir/buffer_head.h>
#include <mir/floppy.h>

const static DmaChannel dmainfo[] = { /* only 8 bit DMA */
   { 0x87, 0x00, 0x01 },
   { 0x83, 0x02, 0x03 },
   { 0x81, 0x04, 0x05 },
   { 0x82, 0x06, 0x07 }
};

static FdcStruct fdc_devices[] = { 
  { 
    fdc_drive : 0,
    done      : FALSE,
    motor     : FALSE,
    status    : { [ 0 ... 6] = 0 ,},
    fdc_track : 0xff,
    geometry  : { DG144_HEADS,DG144_TRACKS,DG144_SPT },
  },
};

static int current_drive = 0;

static void dma_xfer(int channel,long physaddr,int length,int cmd)
{
   long page,offset;
   page = physaddr >> 16;
   offset = physaddr & 0xffff;
   length -= 1;
   cli();
   outb(0x0a,channel | 4);
   outb(0x0c,0);
   outb(0x0b,(cmd == READ ? 0x44 : 0x48 ) + channel);
   outb(dmainfo[channel].page,page);
   outb(dmainfo[channel].offset,offset & 0xff);  /* low byte */
   outb(dmainfo[channel].offset,offset >> 8);    /* high byte */
   outb(dmainfo[channel].length,length & 0xff);  /* low byte */
   outb(dmainfo[channel].length,length >> 8);    /* high byte */
   outb(0x0a,channel);
   sti();
}

/* This is the IRQ6 handler */
#if 0
static int fdc_interrupt(int irq,void *device, struct thread_regs *regs)
{
        /* signal operation finished */
        fdc_devices[current_drive].done = TRUE;
	return 0;
}
#else
void floppy_interrupt_handler(struct thread_regs *regs) { 
  int irq = regs->error_code;
  IRQ_INC(irq);
  fdc_devices[current_drive].done = TRUE;
}
#endif

/*
 * converts linear block address to head/track/sector
 */
void fd_block2hts(int block,int *head,int *track,int *sector)
{
  DrvGeom  *geometry = &fdc_devices[current_drive].geometry;
  *head = (block % (geometry->spt * geometry->heads)) / (geometry->spt);
  *track = block / (geometry->spt * geometry->heads);
  *sector = block % geometry->spt + 1;
}

/* this turns the motor on */
void fd_motoron(void)
{
  int fdc_drive = fdc_devices[current_drive].fdc_drive;
  if (! fdc_devices[current_drive].motor)
    {
      outportb(FDC_DOR,0x1c|fdc_drive);
      fdc_devices[current_drive].motor = TRUE;
    }
}


/*A.R.Karthick: this turns the motor off:
*/

void fd_motoroff(void)
{
#if 0
  if (fdc_devices[current_drive].motor) {
    fdc_devices[current_drive].motor = 0;
    /*Karthick : output 0 into 0x3f2:Not working.Dont know why */
    MOTOR_OFF;
  }
#endif
}

static BOOL real_seek (int track) {
  int k= FALSE;
  while (1) { /* brute force */
    k = fd_seek(track);
    if (k==TRUE) break;
  }
  return k;
}

static int fdc_rw(struct buffer_head *bh,int size,int cmd) { 
  int head,track,sector,tries;
  unsigned long tbaddr = fdc_devices[current_drive].tbaddr;
  int block_size = blkdev[FDC_BLKDEV_MAJOR_NR].block_size;
  int block = bh->block;
  int err = -1;
  unsigned char *blockbuff ;
  if(! bh || !bh->data) { 
    printf("No buffer_head in Function %s\n",__FUNCTION__);
    goto out;
  }
  blockbuff = bh->data;
  /* convert logical address into physical address */
  fd_block2hts(block,&head,&track,&sector);

  /* spin up the disk */
  fd_motoron();

  for (tries = 0;tries < FD_MAX_TRIES;tries++)
    {
      /* move head to right track */
      if (!real_seek(track))
	{
	  printf ("FDC: ERROR!\n");
	  fd_motoroff();
	  return FALSE;
	}

      /* program data rate (500K/s) */
      outportb(FDC_CCR,0);
      
      /* send command */
      switch(cmd) {
      case READ:
	{
	  dma_xfer(2,tbaddr,block_size,READ);
	  fd_sendbyte(CMD_READ);
	}
	break;
      case WRITE:
	{
          memcpy((void *)tbaddr, (const void *) blockbuff,block_size);
	  dma_xfer(2,tbaddr,block_size,WRITE);
	  fd_sendbyte(CMD_WRITE);
	}
	break;
      default:
	printf("Invalid COMMAND %d in Function %s\n",cmd,__FUNCTION__);
      }
      fd_sendbyte(head << 2);
      fd_sendbyte(track);
      fd_sendbyte(head);
      fd_sendbyte(sector);
      fd_sendbyte(2);               /* 512 bytes/sector */
      fd_sendbyte(fdc_devices[current_drive].geometry.spt);
      fd_sendbyte(DG144_GAP3RW);  /* gap 3 size for 1.44M read/write */
      fd_sendbyte(0xff);            /* DTL = unused */
      
      fd_waitfdc (TRUE);
      
      if ( (fdc_devices[current_drive].status[0] & 0xc0) == 0) break;   /* worked! outta here! */
   
      fd_recalibrate();  /* oops, try again... */
    }
   
  fd_motoroff();

  if (cmd == READ)
    {
      /* copy data from track buffer into data buffer */
      memcpy((void *)blockbuff,     (const void *)  tbaddr,  block_size );
    }
  err = !(tries != FD_MAX_TRIES );
 out:
  return err;
}

/*Initialise the BLKDEV interface for the FDC*/
static __inline__ void fdc_blkdev_init(void) {
  unsigned long tbaddr ;
  char name[256];
  /*Allocate the track buffer for the floppy*/
#if 1
  tbaddr = __get_free_page(0);
#else
  tbaddr = 0x90000;
#endif
  if(! tbaddr ) {
    panic("Unable to get_free_page in Function %s,Line %d\n",__FUNCTION__,__LINE__);
  }
  fdc_devices[current_drive].tbaddr = tbaddr;
  blkdev[FDC_BLKDEV_MAJOR_NR].offsets = 0;
  blkdev[FDC_BLKDEV_MAJOR_NR].sizes   = 0;
  blkdev[FDC_BLKDEV_MAJOR_NR].block_size = SECTOR_SIZE;
  list_head_init(&blkdev[FDC_BLKDEV_MAJOR_NR].request_head);
  blkdev[FDC_BLKDEV_MAJOR_NR].block_transfer_function = fdc_rw;
  sprintf(name,"/dev/fd%d",current_drive);
  register_device(name,MKDEV(FDC_BLKDEV_MAJOR_NR,0),BLK_DEVICE,SECTOR_SIZE,NULL);
}

/* init driver */
void __init fd_init(void)
{
        int i;

        printf("FDC: Floppy driver initializing...\n");
	/*This is required coz the floppy handler seems to get into a loop in waitfdc:
	  Hence we dont register the irq line in the normal way for FDC:
	*/
#if 1
	setidt(0x20 + FDC_IRQ,(unsigned long)&floppy_interrupt,INTR_TYPE,INTR_DPL);
	enable_interrupt(FDC_IRQ);
#else
	request_irq(FDC_IRQ,&fdc_interrupt,"FDC",INTERRUPT_IRQ,NULL);
#endif
	fdc_blkdev_init();
        fd_reset();
        /* get floppy controller version */
        fd_sendbyte(CMD_VERSION);
        i = fd_getbyte();

        if (i == 0x80)
                printf("\t\tNEC765 controller found.                  \n");
        else
                printf("\t\tEnhanced controller found(0x%x).                  \n", i);

}

/* fd_sendbyte() routine from intel manual */
void fd_sendbyte(int byte)
{
        volatile int msr;
        int tmo;

        for (tmo=0; tmo<128; tmo++)
        {
                msr = inportb(FDC_MSR);
                if( (msr & 0xc0)==0x80 )
                {
                        outportb(FDC_DATA,byte);
                        return;
                }

                inportb(0x80);   /* delay */
        }
}

/* fd_getbyte() routine from intel manual */
int fd_getbyte()
{
        volatile int msr;
        int tmo;

        for (tmo=0; tmo<FD_GETBYTE_TMOUT; tmo++)
        {
                msr = inportb(FDC_MSR);
                if ((msr & 0xd0) == 0xd0)
                {
                        return inportb(FDC_DATA);
                }
                inportb(0x80);   /* delay */
        }

        return -1;   /* read timeout */
}

/* this waits for FDC command to complete */
BOOL fd_waitfdc(BOOL sensei)
{
  int k;
  BYTE statsz = 0;

  while(!fdc_devices[current_drive].done)
    {
      /* wait */
    }

  /* read in command result bytes */
  while ((statsz < 7) && (inportb(FDC_MSR) & (1<<4)))
    {
      k = fdc_devices[current_drive].status[statsz++] = fd_getbyte();
		
    }

  if (sensei)
    {
      /* send a "sense interrupt status" command */
      fd_sendbyte(CMD_SENSEI);
      fdc_devices[current_drive].sr0 = fd_getbyte();
      fdc_devices[current_drive].fdc_track = fd_getbyte();
    }
  fdc_devices[current_drive].done = FALSE;
  fdc_devices[current_drive].statsz = statsz;
  return TRUE;
}



/* this gets the FDC to a known state */
void fd_reset(void)
{
  int fdc_drive = fdc_devices[current_drive].fdc_drive;
  /* stop the motor and disable IRQ/DMA */
  outportb(FDC_DOR,0|fdc_drive);
   
  /* re-enable interrupts */
  outportb(FDC_DOR,0x0c|fdc_drive);

  /* resetting triggered an interrupt - handle it */
  fdc_devices[current_drive].done = TRUE;
  fd_waitfdc(TRUE);

  /* specify drive timings (got these off the BIOS) */
  fd_sendbyte(CMD_SPECIFY);
  fd_sendbyte(0xdf);  /* SRT = 3ms, HUT = 240ms */
  fd_sendbyte(0x02);  /* HLT = 16ms, ND = 0 */
  /* clear "disk change" status */

#if 0 /*causing BOCHS to hang- Karthick*/
  fd_seek(1);
        
  fd_recalibrate();
#endif

}


/* recalibrate the drive */
void fd_recalibrate(void)
{
        /* turn the motor on */
        fd_motoron();
   
        /* send actual command bytes */
        fd_sendbyte(CMD_RECAL);
        fd_sendbyte(0);

        /* wait until seek finished */
        fd_waitfdc(TRUE);
   
        /* turn the motor off */
        fd_motoroff();
}


/* seek to track */
BOOL fd_seek(int track)
{
        if (fdc_devices[current_drive].fdc_track == track)  /* already there? */
                return TRUE;
   
        fd_motoron();
	
        /* send actual command bytes */
        fd_sendbyte(CMD_SEEK);
        fd_sendbyte(0);
        fd_sendbyte(track);
        /* wait until seek finished */
        if (!fd_waitfdc(TRUE))
                return FALSE;     /* timeout! */

        fd_motoroff();
   
        /* check that seek worked */
        if ((fdc_devices[current_drive].sr0 != 0x20) || (fdc_devices[current_drive].fdc_track != track))
                return FALSE;
        else
                return TRUE;
}



