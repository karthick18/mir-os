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
#include <asm/irq.h>
#include <asm/io.h>

#define min( a, b ) ( ( ( a ) < ( b ) ) ? ( a ) : ( b ) )

extern struct ide_devices *ide_device[MAX_IDE_DEVICES];

/*Validate the block transfer first*/
static int validate_block(const struct buffer_head *bh,int size,int cmd ) { 
  int err = -1;
  int major,minor;
  if(! bh ) {
    goto out;
  }
  major = MAJOR(bh->dev);
  if(! blkdev[major].offsets || !blkdev[major].sizes ) {
    goto out;
  }
  minor = MINOR(bh->dev);
  if(blkdev[major].sizes[minor] && minor != 1 && bh->block + size  > blkdev[major].sizes[minor] ) {
    printf("%s command validation failed.Transfer sector %d: size %d,out of range for device size %d\n",cmd == READ ? "READ" : "WRITE",bh->block,size, blkdev[major].sizes[minor]);
    goto out;
  }
  /*temporary support for minor 1 for testing ide devices- remove later*/
  err = bh->block + ( minor == 1  ? 0 :blkdev[major].offsets[minor] );
#ifdef IDE_DEBUG
  printf("Validate block success for block %d,size %d\n",err,size);
#endif  
 out:
  return err;
}


/* ide_hd_block2chs()
 * translate block into cylinder, head, sector
 *
 * params: drive, block, num of blocks
 * return: -
 */

static void ide_hd_block2chs( uchar drive, ulong start_sector, ulong nr_of_sectors )
{
    ulong cylinder;
    uchar head, sector;
    ulong nr_sectors;

#ifdef IDE_DEBUG
    printf( "ata_block2chs (C-%d:H-%d:S-%d): %d\n",
	ide_device[ drive ] -> cylinders,
	ide_device[ drive ] -> heads,
	ide_device[ drive ] -> sectors,
	start_sector);
#endif

    if ( ide_device[ drive ] -> lba == 1 )
    {
	/* create the informations with the lba method */
	sector = start_sector & 0xff;
	start_sector >>= 8;
	cylinder = start_sector & 0xffff;
	start_sector >>= 16;
	head = 0x40 | ide_device[ drive ] -> connection | ( start_sector & 0x0f );
    }
      else
    {
	/* create the informations with the chs methode */
	sector = start_sector % ide_device[ drive ] -> sectors + 1;
	start_sector /= ide_device[ drive ] -> sectors;
	head = start_sector % ide_device[ drive ] -> heads;
	start_sector /= ide_device[ drive ] -> heads;
	cylinder = start_sector;
	head |= ide_device[ drive ] -> connection;
    }
#ifdef IDE_DEBUG
    printf("CHS: %d:%d:%d\n", cylinder, head, sector );
#endif
    nr_sectors = nr_of_sectors;
    outb( ide_device[ drive ] -> base + ATA_REG_COUNT,  nr_sectors & 0xff );
    outb( ide_device[ drive ] -> base + ATA_REG_SECTOR, sector );
    outb( ide_device[ drive ] -> base + ATA_REG_LOCYL,  cylinder );
    outb( ide_device[ drive ] -> base + ATA_REG_HICYL,  cylinder >> 8 );
    outb( ide_device[ drive ] -> base + ATA_REG_DRVHD,  head );
    ide_sleep( 400 );
}

/* ide_hd_rw()
 * main ide read/write function
 *
 * params: device, block, size, buffer address, r/w command
 * return: -
 */
int ide_hd_rw(struct buffer_head *bh,int size,int cmd) {
  uchar *buffer;
  ulong block,device,temp;
  ushort ide_base ;
  int mult_sect,err = -1;
  if(! bh || !bh->data) { 
    goto out;
  }
  device = get_device(bh->dev);
  buffer = bh->data;
  ide_base = ide_device[device]->base;
  mult_sect = (ide_device[device]->multi_count ?: 1);

#ifdef IDE_DEBUG
  printf( "ide_hd_rw: %d %x\n", device, ide_base );
  printf( "ide_hd_rw: %d\n", size );
#endif

  temp = size / ide_device[ device ] -> bytes_per_block;

  if ( ( size % ide_device[ device ] -> bytes_per_block ) != 0 )
    {
      temp++;
    }
  size = temp;

  if( ( block = validate_block(bh,size,cmd) ) < 0 ) {
    goto out;
  }
  /* calculate the necessary information and set them */
  ide_select( ide_base, ide_device[ device ] -> connection );
  ide_hd_block2chs( device, block, size );

  switch(cmd) { 

  case READ:
    {
      /* read part */
      outb( ide_base + ATA_REG_CMD, ide_device[ device ] -> multi_count  ? ATA_CMD_READMUL : ATA_CMD_READ );
      ide_sleep( 400 );

      while ( size != 0 )
	{
	  ushort num_blocks, num_bytes;
#if 0	
	  ide_wait_irq( ide_base, ide_device[ device ] -> io_irq, 0x89, 0x08,	10000 );
#endif
	  num_blocks = min( size, mult_sect);
	  num_bytes = num_blocks * ide_device[ device ] -> bytes_per_block;
	  ide_insw( ide_base + ATA_REG_DATA, ( ushort *)buffer, num_bytes >> 1 );
	  size -= num_blocks;
	  buffer += num_bytes;
	}
    } 
    break;

  case WRITE:
    {
      /* write part */
      outb( ide_base + ATA_REG_CMD, ide_device[ device ] -> multi_count 
	    ? ATA_CMD_WRITEMUL : ATA_CMD_WRITE );
      ide_sleep( 400 );

      while ( size != 0 )
	{
	  ushort num_blocks, num_bytes;
#if 0    
	  ide_wait_irq( ide_base, ide_device[ device ] -> io_irq, 0x89, 0x08, 10000 );
#endif
	  num_blocks = min( size, mult_sect );
	  num_bytes = num_blocks * ide_device[ device ] -> bytes_per_block;
	  ide_outsw( ide_base + ATA_REG_DATA, ( ushort * ) buffer,  num_bytes >> 1 );
	  size -= num_blocks;
	  buffer += num_bytes;
	}
    }
    break;
  default:
    printf("Invalid COMMAND %d specified to function %s...\n",cmd,__FUNCTION__);
  }
  err = 0;
 out:
  return err;
}

/* init_atadevice()
 * initialize the ata devices
 *
 * params: internal drive number
 * return: -
 */
void  init_atadevice( uchar drive )
{
    char ide_hd_name[ 8 ];
    int dev;
    sprintf( ide_hd_name, "hd%c", 'a' + drive );

    if ( ide_device[ drive ] -> cylinders * ide_device[ drive ] -> heads *
	ide_device[ drive ] -> sectors / 2048 == 0 )
    {
	return;
    }
    printf( "/dev/%s: ATA drive, [%s], %ld.%02ldMB%s%s\n",
	ide_hd_name,
        ide_device[ drive ] -> model_number,
        ide_device[ drive ] -> cylinders *
        ide_device[ drive ] -> heads *
        ide_device[ drive ] -> sectors / 2048,
	(ide_device[ drive ] -> cylinders * ide_device[ drive ] -> heads * ide_device[ drive ] -> sectors % 2048)/10,
        ide_device[ drive ] -> lba ? ", LBA": ", CHS",
        ide_device[ drive ] -> dma ? ", DMA": "" );
    get_partitiontable(( dev = MKDEV( IDE_BLKDEV_MAJOR_NR, drive << PARTITION_SHIFT ) ) );
    show_partitions( ide_hd_name, dev );
    register_partitions( drive, IDE_BLKDEV_MAJOR_NR );
}
