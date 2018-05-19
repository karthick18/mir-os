/* MIR-OS IDE driver
   ;   a.r.karthick@gmail.com ; a_r_karthic@rediffmail.com
   ;  
   ;    Copyright (C) 2003-2004
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
   ; This IDE driver has been taken from TABOS code and modified to work
   ; with MIR
   ;
   ;
*/

#include <mir/kernel.h>
#include <mir/sched.h>
#include <mir/buffer_head.h>
#include <mir/blkdev.h>
#include <mir/ide.h>
#include <mir/ide_partition.h>
#include <asm/page.h>
#include <asm/string.h>
#include <asm/io.h>
#include <asm/irq.h>
#include <mir/init.h>

volatile ushort ide_done;
ushort force_chs = 1;

struct ide_devices *ide_device[4]={0,0,0,0};
struct blkdev_struct blkdev[ MAX_BLKDEVS ];

void ide_sleep( uint msecs)
{
#ifdef SLOW_COMPUTER
  delay(msecs);
#endif
}

/* getascii()
 * converts the ide information into a readable string
 *
 * params: input string, output string, offset start, size
 * return: -
 */
static void ide_getascii( ushort *in_data, uchar *data, ushort off_start,
    ushort size )
{
    int loop;
    uchar data2[ 256 ], ch, *p;

    memset( data2, 0, size );
    memcpy( data2, in_data + off_start, size );

    for ( loop = 0; loop < size; loop += 2 )
    {
	ch = data2[ loop ];
	data2[ loop ] = data2[ loop + 1 ];
	data2[ loop + 1 ] = ch;
    }

    loop = size - 2;

    while ( loop >= 0 &&  data2[ loop ] == ' ' )
    {
	loop--;
    }
    data2[ loop + 1 ] = '\0';

    p = ( uchar * ) data2;
    while ( ( *p ) == 0x20 )
    {
	p++;
    }
    loop = 0;

    while ( *p != '\0' )
	data[ loop++ ] = *p++;

    data[ loop ] = '\0';
}

void ide_outsw( ushort port, ushort *data, uint count )
{
#ifdef IDE_DEBUG
  printf("Writing %d bytes..\n",count << 1 );
#endif
  if(!data) 
    return;
    for ( ; count != 0; count-- )
    {
      ushort c = *data++;
      outw( port, c );
    }
}

void ide_insw( ushort port, ushort *data, uint count )
{
#ifdef IDE_DEBUG
  printf("Reading %d bytes ..\n", count << 1 );
#endif
  if(! data) 
    return;
    for ( ; count != 0; count-- )
    {
	*data = inw( port );
	data++;
    }
}

/* wait_for_interrupt()
 * waits for ide occurence with a timeout
 *
 * params: irq_mask, timeout
 * return: negative: failed; 0: success
 */
ushort ide_wait_for_interrupt( ushort irq_mask, uint timeout )
{
    ushort intr = 0;
    uint time;
    int err = -1;
#ifdef IDE_DEBUG
    printf( "wait_for_interrupt: " );
#endif

    for ( time = timeout; time != 0; time-- )
    {
	/* check if ide_done has this mask */
	intr = ide_done & irq_mask;
	if ( intr != 0 )
	{
	    break;
	}
	delay( 1 );
    }

    /* time out? */
    if ( time == 0 )
    {
#ifdef IDE_DEBUG
	printf( "timeout\n" );
#endif
	goto out;
    }
#ifdef IDE_DEBUG
    printf( "finished in %d msec.\n", timeout - time );
#endif
    /* remove ide_done mask */
    ide_done &= ~intr;
    err = intr;
 out:
    return err;
}

/* wait_ide()
 * waits for specific mask to become ready
 *
 * params: port, status mask, status bits, timeout
 * return: ESUCCESS or EFAIL
 */
uchar ide_wait( int port, uchar status_mask, uchar status_bits, uint timeout )
{
    uint time, decr;
    uchar status = 0;
    int err = -1;
    decr = ( timeout < 500 ) ? 1 : 50;      

    for ( time = timeout; timeout > 0 ; timeout -= decr )
    {
	status = inb( port + ATA_REG_STATUS );

	/* is there a match? */
	if ( ( status & status_mask ) == status_bits )
	{
#ifdef IDE_DEBUG
	  printf("Success in ide_wait for status..\n");
#endif
	  err = 0;
	  goto out;
	}
	delay( decr );
    }
 out:
    return err;
}

/* wait_irq()
 * same as wait_ide but here waits for interrupt to become ready
 *
 * params: port, irq mask, status mask, status bits, timout
 * return: ESUCCESS or EFAIL
 */
uchar ide_wait_irq( ushort port, ushort irq_mask, uchar status_mask,
    uchar status_bits, uint timeout )
{
    int temp;
    uchar status;
    int err =-1;
    temp = ide_wait_for_interrupt( irq_mask, timeout / 10 );

    if ( temp == 0 )
    {
      goto out;
    }

    status = inb( port + ATA_REG_STATUS );
    /* check mask */
    if ( ( status & status_mask ) != status_bits )
    {
#ifdef IDE_DEBUG
      printf("Failure in ide_wait_irq w.r.t to status..\n");
#endif
      goto out;
    }
    err = 0;
 out:
    return err;
}

/* ata_select()
 * select the drive we want to use
 *
 * params: port, selection
 * return: ESUCCESS or EFAIL
 */
int ide_select( ushort port, uchar sel )
{
    /* select drive */
    outb( port + ATA_REG_DRVHD, sel );
    ide_sleep( 400 );
    /* return ide status */
    return ide_wait( port, 0x88, 0x00, 500 );
}

static int __init check_lba( ide_id *id )
{
    ulong sectors_chs, sectors_lba, head, tail;
    if(force_chs ) 
      return 0;
    /* check some special cases */
    if ( ( id->cyls == 16383 || ( id->cyls == 4092 &&
	id->cur_cyls == 16383 ) ) &&
	id->sectors == 63 &&
	( id->heads == 15 || id->heads == 16 ) &&
	id->lba_capacity >= 16383 * 63 * id->heads )
    {
	printf( "case 1\n" );
	return 1;
    }
    sectors_lba = id->lba_capacity;
    sectors_chs = id->cyls * id->heads * id->sectors;

    /* sanity check */
    if ( ( sectors_lba - sectors_chs ) < sectors_chs / 10 )
    {
	printf( "case 2\n" );
	return 1;
    }

    /* maybe word order reversed? */
    head = ( ( sectors_lba >> 16 ) & 0xFFFF );
    tail = ( sectors_lba & 0xFFFF );
    sectors_lba = ( head | ( tail << 16 ) );
    if ( ( sectors_lba - sectors_chs ) < sectors_chs / 10 )
    {
	id->lba_capacity = sectors_lba;
	printf( "case 3\n" );
	return 1;
    }
    /* no lba! */
    printf( "no lba!\n" );
    return 0;
}

/* ata_identify()
 * identify the specific device
 *
 * params: drive number
 * return: ESUCCESS or EFAIL
 */
static int __init ide_identify( ushort drivenr )
{
    ushort ide_base, temp, ide_cmd, ide_delay;
    uchar temp1, temp2;
    ide_id id;
    int err = -1;
    ide_base = ide_device[ drivenr ] -> base;

    ide_select( ide_base, ide_device[ drivenr ] -> connection );

    temp1 = inb( ide_base + ATA_REG_COUNT );
    temp2 = inb( ide_base + ATA_REG_SECTOR );
       
    /* does the reset work? */
    if ( temp1 != 0x01 || temp2 != 0x01 )
    {
      goto out;
    }

    temp1 = inb( ide_base + ATA_REG_LOCYL );
    temp2 = inb( ide_base + ATA_REG_HICYL );
    temp = inb( ide_base + ATA_REG_STATUS );

    if ( temp1 == 0x14 && temp2 == 0xeb )
    {
	ide_device[ drivenr ] -> flags = ATAPI;
	ide_device[ drivenr ] -> detect = 1;
	ide_device[ drivenr ] -> bytes_per_block = BLOCK_SIZE << 1;
	ide_cmd = ATA_CMD_PID;
	ide_delay = 1000;
    }
    /* is it a harddisc? */
   else if ( temp1 == 0 && temp2 == 0 && temp != 0 )
    {
	ide_device[ drivenr ] -> flags = ATA;
	ide_device[ drivenr ] -> detect = 1;
	ide_device[ drivenr ] -> bytes_per_block = BLOCK_SIZE >> 1;
	ide_cmd = ATA_CMD_ID;
	ide_delay = 3000;
    }
      else
    /* unkown device */
    {
      goto out;
    }
    ide_done = 0;
    outb( ide_base + ATA_REG_CMD, ide_cmd );
    ide_sleep( 400 );
    if ( ide_wait_for_interrupt( 0xC000, ide_delay ) == 0 )
    {
#ifdef IDE_DEBUG
      printf("Error in ide_wait_for_interrupt in line %d,routine %s\n",__LINE__,__FUNCTION__);
#endif
      goto out;
    }
    /* read information block */
    ide_insw( ide_base + ATA_REG_DATA, ( ushort * ) &id, 256 );

    /* convert information strings */
    ide_getascii( ( ushort * ) &id, ide_device[ drivenr ] -> model_number,
	27, 40 );
    ide_getascii( ( ushort * ) &id, ide_device[ drivenr ] -> serial_number,
	10, 20 );
    ide_getascii( ( ushort * ) &id, ide_device[ drivenr ] -> firmware, 23, 8 );

    /* check if this drives can work with lba */
    if ( check_lba( &id ) )
    {
        ide_device[ drivenr ] -> lba = 1;
	ide_device[ drivenr ] -> sectors = id.lba_capacity;
	ide_device[ drivenr ] -> cylinders = 1;
	ide_device[ drivenr ] -> heads = 1;
    }
      else
    {
      	ide_device[ drivenr ] -> lba = 0;
	ide_device[ drivenr ] -> sectors = id.sectors;
	ide_device[ drivenr ] -> cylinders = id.cyls;
	ide_device[ drivenr ] -> heads = id.heads;
    }

    if ( id.capability & 1 )
    {
	ide_device[ drivenr ] -> dma = 1;
    }
    ide_device[ drivenr ] -> multi_count = id.max_multsect ;
    err =0;
 out:
    return err;
}

/* ide_probe_devices()
 * check for present of a ide device
 *
 * params: internal drive number
 * return: -
 */
static void __init ide_probe_devices( uchar master )
{
    ushort ide_base = ide_device[ master ] -> base;

#ifdef IDE_DEBUG
    printf( "ide: checking port: %x\n", ide_base );
#endif
    /* soft reset both devices */
    outb( ide_base + ATA_REG_DEVCTRL, 0x06 );
    ide_sleep( 400 );

    /* release soft reset AND enable interrupts from drive */
    outb( ide_base + ATA_REG_DEVCTRL, 0x00 );
    ide_sleep( 400 );

    ide_wait( ide_base, 0xC1, 0x40, 500 );
#ifdef IDE_DEBUG
    printf( " checking master\n" );
#endif
    if ( ide_identify( master ) )
    {
	ide_identify( master );
    }

    /* check slave on port */
    if ( ide_select( ide_base, 0xb0 ) == 0 )
    {
#ifdef IDE_DEBUG
	printf( "  slave drive found\n" );
#endif
        if ( ide_identify( master + 1 ) )
	{
	    ide_identify( master + 1 );
	}
    }
}

/* ide_int14()
 * ide interrupt for int 14
 *
 * params: -
 * return: -
 */
static int ide_int14(int irq, void *device, struct thread_regs *thread_regs)
{
  ide_done |= 0x4000;
  return 0;
}

/* ide_int15()
 * ide interrupt for int 15
 *
 * params: -
 * return: -
 */
static int ide_int15(int irq, void *device, struct thread_regs *thread_regs)
{
    ide_done |= 0x8000;
    return 0;
}

/* ide_probe_controller()
 * scans for the controller interfaces
 *
 * params: -
 * return: -
 */
static void __init ide_probe_controller()
{
    ushort ide_base, index;

    for ( index = 0; index < MAX_IDE_DEVICES; index += 2 )
    {
	ide_base = ide_device[ index ] -> base;
#ifdef IDE_DEBUG
	printf("** probing port %x\n",ide_base);
#endif

	outb( ide_base + ATA_REG_COUNT, 0x55 );
	outb( ide_base + ATA_REG_SECTOR, 0xAA );

	/* is there a ide device on this port? */
	if ( ( inb( ide_base + ATA_REG_COUNT ) != 0x55 )
    	    || ( inb( ide_base + ATA_REG_SECTOR ) != 0xAA ) )
	{
#ifdef IDE_DEBUG
	    printf( "-  nothing at this port (%x)\n", ide_base );
#endif
    	    continue;
	}

#ifdef IDE_DEBUG
	printf("+  ide-controller %d found \n", index / 2 + 1 );
#endif

	switch ( index / 2 + 1 )
	{
	    case 1:
	    {
		request_irq(14,(irq_action_t*)&ide_int14,"IDE0",INTERRUPT_IRQ,NULL);
		break;
	    }
	    case 2:
	    {
		request_irq(15, (irq_action_t*) &ide_int15,"IDE1",INTERRUPT_IRQ,NULL);
		break;
	    }
	}
    }
}

/* init_ide()
 * initialize the whole ide structure and scans for the devices
 *
 * params: -
 * return: ESUCCESS
 */
int __init init_ide( int argc, char **argv )
{
  uchar index;
  short i = 0;

  printf( "IDE driver for ATA/ATAPI devices\n" );
  force_chs = 1;
  if ( argc && argv)
    {
      printf( "Option(s) requested: " );
      for ( i = 0; i < argc; i++ )
	{
	  printf( "%s ", argv[ i ] );
	  if ( !strcmp( argv[ i ], "force-chs" ) )
	    {
	      force_chs = 1;
	    }
	}
      printf( "\n" );
    }
  for ( i = 0; i < 4; i++ )
    {
      ide_device[ i ] = slab_alloc( sizeof( struct ide_devices ), 0 );
      if( ! ide_device[i] ) {
	panic("slab alloc error in Line %d,Routine %s\n",__LINE__,__FUNCTION__);
      }
      memset(ide_device[i], 0x0, sizeof(struct ide_devices ) );
    }

  ide_device[ 0 ] -> io_irq = ide_device[ 1 ] -> io_irq = 0x4000;
  ide_device[ 2 ] -> io_irq = ide_device[ 3 ] -> io_irq = 0x8000;
  ide_device[ 0 ] -> base = ide_device[ 1 ] -> base = 0x1f0;
  ide_device[ 2 ] -> base = ide_device[ 3 ] -> base = 0x170;
  ide_device[ 0 ] -> connection = ide_device[ 2 ] -> connection = 0xa0;
  ide_device[ 1 ] -> connection = ide_device[ 3 ] -> connection = 0xb0;
  ide_probe_controller();

  ide_probe_devices( 0 );
  ide_probe_devices( 2 );

  for ( index = 0; index < 4; index++ )
    {
      if ( ide_device[ index ] -> detect == 0 )
	{
	  continue;
	}
      if(ide_device[index]->flags & ATAPI ) {
	init_atapidevice(index);
      }
      else if ( ide_device[ index ] -> flags & ATA )
	{
	  init_atadevice( index );
	}
    }
  return 0;
}
