/* MIR-OS IDE partition reader
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
#include <mir/vfs/fs.h>
#include <mir/ide.h>
#include <mir/ide_partition.h>
#include <asm/page.h>
#include <asm/string.h>
#include <asm/io.h>

extern struct ide_devices *ide_device[MAX_IDE_DEVICES];

/*Karthick: Introduced a partition map
  Keep the list sorted by index:
*/

struct partition_map { 
  int index;
  const char *name;
} partition_map [] = {
  { 0x01, "FAT12" },
  { 0x04, "FAT16" },
  { 0x05, "Extended" },
  { 0x06,  "FAT12 >32MB"},
  { 0x07,  "Windows NT (NTFS)"},
  { 0x0b,  "FAT32" },
  { 0x0c,  "FAT32" },
  { 0x0e,  "FAT16" },
  { 0x82,  "Linux Swap"},
  { 0x83,  "Linux Native"},
};

#define NR(element)  ( sizeof(element)/sizeof(element[0]) )
		    
#define load_byte(byte) ( *(uchar *)(byte) )
#define load_dword(dword) ( *(uint *)(dword) )

/*get the partition map based on the index*/
const char *get_partition_map(int index) { 
  int mid, left, right;
  int nr = NR(partition_map);
  struct partition_map *ptr;
  const char *map = "Unknown";
  left = 0;
  right = nr-1;
  if(index < 0 ) 
    goto out;

  while( left <= right ) { 
    mid = (left + right )/2;
    ptr = partition_map + mid;
    if ( index == ptr->index) {
      map = ptr->name;
      break;
    } else if (index < ptr->index ) {
      right = mid - 1;
    }else {
      left = mid + 1;
    }
  }
 out:
  return map;
}
    
/* get_partitiontable()
 * reads the partitiontable into a predefined block
 *
 * params: device
 * return:status -success or failure
 */
int get_partitiontable( ulong dev)
{
  uchar *partition_buffer, *start_offset, *end_offset, *temp;
  int i, extended = 0;
  int err = -1;
  int *offsets,*sizes;
  struct buffer_head *bh;
  int device = get_device(dev);
  int major  = MAJOR(dev);
  part *partitions;
  if( ! ( partitions = slab_alloc( sizeof( part) * ( 1 << PARTITION_SHIFT ), 0 ) ) ) {
    panic("slab_alloc error in Line %d,routine %s\n",__LINE__,__FUNCTION__);
  }
  memset(partitions,0x0,sizeof(part) * ( 1 << PARTITION_SHIFT ) );
  ide_device[device]->partitions = partitions;

  if(! blkdev[major].offsets ) {
    offsets = slab_alloc( sizeof(int) * ( MAX_IDE_DEVICES << PARTITION_SHIFT ), 0 );
    if(! offsets) {
      panic("slab alloc error in Line %d, routine %s\n",__LINE__,__FUNCTION__);
    }
    sizes = slab_alloc(sizeof(int) * (MAX_IDE_DEVICES << PARTITION_SHIFT ) , 0 );
    if(! sizes ) {
      panic("slab alloc error in Line %d,routine %s\n",__LINE__,__FUNCTION__);
    }
    memset(offsets,0x0,sizeof(int)* ( MAX_IDE_DEVICES << PARTITION_SHIFT ) );
    memset(sizes,0x0,sizeof(int) * ( MAX_IDE_DEVICES << PARTITION_SHIFT )  );
    blkdev[major].offsets = offsets;
    blkdev[major].sizes   = sizes;
    blkdev[major].block_size = ide_device[device]->bytes_per_block;
    blkdev[major].block_transfer_function = ide_hd_rw;
    list_head_init(&blkdev[major].request_head);
  }
  /* read the first sector (it contains the partition table) */
  bh = bread( dev, 0 );
  
  if ( bh == NULL )
    {
      goto out;
    }

  partition_buffer = ( uchar * ) bh -> data;

  /* check if it is a valid partition table */
  if ( partition_buffer[ 510 ] != 0x55 || partition_buffer[ 511 ] != 0xaa )
    {
        printf( "%x %x (%x %x)\n", partition_buffer[ 510 ], partition_buffer[ 511 ], partition_buffer[0], partition_buffer[1] );
      printf( "Invalid partition table!\n" );
      goto out_release;
    }

  /* now extract all usefull information and put it in our table */
  for ( i = 0; i < 4; i++ )
    {
      temp = partition_buffer + PARTITION_OFFSET + PARTITION_SIZE * i;
	
      partitions[ i + 1 ].active = ( uchar ) load_byte( temp );
      partitions[ i + 1 ].system = ( uchar ) load_byte( temp + 4 );

      start_offset = temp + 1;
      end_offset = temp + 5;

      partitions[ i + 1].start_track = ( uint ) load_byte( start_offset + 2 ) |
	( ( ( uint ) load_byte( start_offset + 1 ) & 0xc0 ) << 2 );
      partitions[ i + 1].start_head = ( uint ) load_byte( start_offset );
      partitions[ i + 1].start_sector = ( uint ) load_byte( start_offset + 1 )
	& 0x3f;

      partitions[ i + 1].end_track = ( uint ) load_byte( end_offset + 2 ) |
	( ( ( uint ) load_byte( end_offset + 1 ) & 0xc0 ) << 2 );
      partitions[ i + 1].end_head = ( uint ) load_byte( end_offset );
      partitions[ i + 1].end_sector = ( uint ) load_byte( end_offset + 1 ) & 0x3f;

      partitions[ i + 1].offset = ( ulong ) load_dword( temp + 8 );
      partitions[ i + 1].size = ( ulong ) load_dword( temp + 12 );

      if ( partitions[ i + 1 ].system == 0x05 )
	{
	  extended = partitions[ i + 1].offset;
	}
    }
  /* scan for extended partitions (BUGGY) */
  if ( extended != 0 )
    {
      struct buffer_head *bh1;
      bh1=bread( dev, extended );
      if( bh1==NULL )
	{
	  goto out_release;
	}
      brelse(bh);
      bh = bh1;
      partition_buffer = (uchar*)bh->data;
      for ( ; i < 8; i++ )
	{
	    
	  temp = partition_buffer + PARTITION_OFFSET + PARTITION_SIZE *(i-4);

	  partitions[ i + 1].active = ( uchar ) load_byte( temp );
	  partitions[ i + 1].system = ( uchar ) load_byte( temp + 4 );

	  start_offset = temp + 1;
	  end_offset = temp + 5;

	  partitions[ i + 1].start_track = ( uint ) load_byte( start_offset + 2 ) |
	    ( ( ( uint ) load_byte( start_offset + 1 ) & 0xc0 ) << 2 );
	  partitions[ i + 1].start_head = ( uint ) load_byte( start_offset );

	  partitions[ i + 1].start_sector = ( uint ) load_byte( start_offset + 1 ) & 0x3F;

	  partitions[ i + 1 ].end_track = ( uint ) load_byte( end_offset + 2 ) |
	    ( ( ( uint ) load_byte( end_offset + 1 ) & 0xC0 ) << 2 );
	  partitions[ i + 1].end_head =   ( uint ) load_byte( end_offset );
	  partitions[ i + 1].end_sector = ( uint ) load_byte( end_offset + 1 ) & 0x3F;

	  partitions[ i + 1].offset =  ( ulong ) load_dword( temp + 8 );
	  partitions[ i + 1].size =    ( ulong ) load_dword( temp + 12 );
	}
    }
  err = 0;
 out_release:
  brelse( bh );
 out:
  return err;
}

/* show_partitions()
 * list the partitiontable to the screen
 *
 * params: device name, partition structure
 * return: -
 */
int show_partitions(const char *device_name, ulong dev )
{
  ushort partition_index;
  int device = get_device(dev);
  part *partitions ;
  int err = -1;
  partitions = ide_device[device]->partitions;
  if(! partitions ) 
    goto out;
  for ( partition_index = 1; partition_index <= 8; partition_index++ )
    {
      /* check if the partition is in use */
      if ( partitions[ partition_index ].system != 0 )
	{
	  printf( "* %s%d: ", device_name, partition_index  );
	  printf("%s", get_partition_map (partitions[partition_index].system ) );
	  printf( " start LBA=%d, start CHS=%d:%d:%d, end CHS=%d:%d:%d, Size: %d.%02dMB\n",
		  partitions[ partition_index ].offset,
		  partitions[ partition_index ].start_track,
		  partitions[ partition_index ].start_head,
		  partitions[ partition_index ].start_sector,
		  partitions[ partition_index ].end_track,
		  partitions[ partition_index ].end_head,
		  partitions[ partition_index ].end_sector,
		  partitions[ partition_index ].size / 2048,
		  (partitions[ partition_index ].size % 2048)/10
		  );
	}
    }
  err = 0;
 out:
  return err;
}

/* register_partitions()
 * register the detected partitions into the system
 *
 * params: internal device number, major number of device
 * return: -
 */
int register_partitions( ulong dev, ulong major )
{
  char devname[ 256];
  uchar number;     
  
  for ( number = 1; number <= 8; number++ )
    {
      /* exclude extended and unused partitions */
      if ( ide_device[ dev ] -> partitions[ number ].system == 0x5
	   || ide_device[ dev ] -> partitions[ number ].system == 0 )
	{
	  continue;
	}
      sprintf( devname, "/dev/hd%c%d", 'a' + ( uchar ) dev, number);
      blkdev[major].offsets[(dev << PARTITION_SHIFT) + number] = ide_device[dev]->partitions[number].offset;
      blkdev[major].sizes[(dev << PARTITION_SHIFT) + number ]  = ide_device[dev]->partitions[number].size;
      register_device(devname,MKDEV(major, (dev << PARTITION_SHIFT) + number),BLK_DEVICE,blkdev[major].block_size,NULL);
    }
  return 0;	
}
