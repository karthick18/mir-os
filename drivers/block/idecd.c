/* MIR-OS IDE CDROM Driver
   ;   a_r_karthic@rediffmail.com ; karthick_r@infosys.com
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
   ; This IDE CDROM driver has been taken from TABOS code and modified to work
   ; with MIR
   ;
   ;
*/

#include <mir/kernel.h>
#include <mir/sched.h>
#include <mir/blkdev.h>
#include <mir/vfs/fs.h>
#include <mir/buffer_head.h>
#include <mir/ide.h>
#include <mir/audiocd.h>
#include <asm/io.h>

/* ide_atapi_read_discard()
 * reads the incoming bytes
 *
 * params: ide base, buffer address, soll
 * return: count
 */
int cdrom_drives[MAX_IDE_DEVICES] = { [ 0 ... MAX_IDE_DEVICES - 1] = -1 };
static int drives;

ushort ide_atapi_read_discard( ushort ide_base, uchar *buffer, ushort soll )
{
  ushort got, count = 0;
  if(! buffer) { 
    goto out;
  }
  got = inb( ide_base + ATAPI_REG_HICNT );
  got <<= 8;
  got |= inb( ide_base + ATAPI_REG_LOCNT );
  count = MIN( soll, got );
  ide_insw( ide_base + ATA_REG_DATA, ( ushort * ) buffer, count >> 1 );
  if ( got > count )
    {
      int i;
      for ( i = got - count; i > 1; i -= sizeof(ushort) )
	{
	  inw( ide_base + ATA_REG_DATA );
	}
      if (i)
	{
	  inb( ide_base + ATA_REG_DATA );
	}
    }
 out:
  return count;
}

/* ide_atapi_error()
 * shows errors and sets the error entry
 *
 * params: ide base, entry
 * return: zero = success, negativ = fault
 */

#define MAX_TRIES 0x3

static int ide_atapi_error( ushort ide_base)
{
  static int tries;
#ifdef IDE_DEBUG
    static const char *error_message[] = {
	"no sense", "recovered error", "not ready", "medium error",
	"hardware error", "illegal request", "unit attention", "data protect",
	"?", "?", "?", "aborted command","?", "?", "miscompare", "?" };
#endif
    uchar temp;

    temp = inb( ide_base + ATA_REG_STATUS );
#ifdef IDE_DEBUG
    printf( "error bit: %d, ", temp & 1 );
#endif
    temp = inb( ide_base + ATA_REG_ERROR );
#ifdef IDE_DEBUG
    printf( "sense key. %d (%s)\n", temp >> 4, error_message[ temp >> 4 ] );
    printf( "MCR: %d, ", ( temp & 8 ) >> 3 );
    printf( "ABRT: %d, ", ( temp & 4 ) >> 2 );
    printf( "EOM: %d, ", ( temp & 2 ) >> 1 );
    printf( "ILI: %d\n", temp & 1 );
#endif
    if ( ++tries >= MAX_TRIES )
    {
        tries = 0;
	return -1;
    }
    return 0;
}

/* ide_atapi_cmd()
 * sends a specific command packet to cdrom and handle it
 *
 * params: device, num, buffer address, packet
 * return: 0 = success, negativ = fault
 */
static int ide_atapi_cmd( ulong device, ulong num, uchar *buffer, uchar *pkt )
{
#ifdef IDE_DEBUG
  /*char *phase_name[] = {
    "ABORT", "invalid", "invalid", "DONE", "!", "!", "!", "!",
    "DATA OUT", "CMD OUT", "DATA_IN", "invalid" };*/
#endif
  int err = -1;
  ushort ide_base = ide_device[ device ] -> base;
  uchar phase;
  ushort num_of_blocks=0, num_bytes=0;

#ifdef IDE_DEBUG
  printf( "* atapi_cmd: %d %x\n", device, ide_base );
  printf( "select\n" );
#endif
  err = ide_select( ide_base, ide_device[ device ] -> connection );
  if ( err != 0 )
    {
#ifdef IDE_DEBUG
      printf( "err\n" );
#endif
      goto out;
    }
 AGAIN:
  outb( ide_base + ATA_REG_FEAT, 0 );
  outb( ide_base + ATAPI_REG_LOCNT, 32768 );
  outb( ide_base + ATAPI_REG_HICNT, 32768 >> 8 );
  outb( ide_base + ATA_REG_DRVHD, 0x40 | ide_device[ device ] -> connection );
  ide_sleep( 400 );

#ifdef IDE_DEBUG
  printf( "packet cmd\n" );
#endif
  outb( ide_base + ATA_REG_CMD, ATA_CMD_PKT );
  ide_sleep( 400 );
  if ( (err = ide_wait( ide_base, 0x88, 0x08, 5000 ) ) )	/* ? check! ? */
    {
#ifdef IDE_DEBUG
      printf( "atapi drive does not want the packet command\n" );
#endif
      goto out;
      /* goto ERR; */
    }
  phase = inb( ide_base + ATA_REG_STATUS ) & 0x08;
  phase |= ( inb( ide_base + ATAPI_REG_REASON ) & 3 );
  if ( phase != ATAPI_PH_CMDOUT )
    {
#ifdef IDE_DEBUG
      printf( "atapi drive is not in command out phase\n" );
#endif
      goto ERR;
    }
  ide_outsw( ide_base + ATA_REG_DATA, ( ushort * ) pkt, 6 );
#ifdef IDE_DEBUG
  printf( "ok, lets read\n" );
#endif
  for ( ; ; )
    {
#if 0
      if ( ide_wait_for_interrupt( 0xC000, 10000 ) == 0 )
	{
#ifdef IDE_DEBUG
	  printf( "atapi drive: command time out\n" );
#endif
	  /* goto ERR;*/
	}
      delay( 10 );
#endif
      phase = inb( ide_base + ATA_REG_STATUS ) & 0x08;
      delay( 10 );
      phase |= ( inb( ide_base + ATAPI_REG_REASON ) & 3 );

#ifdef IDE_DEBUG
      /*printf( "atapi_cmd: ATAPI drive now in phase '%s'\n", ( phase < 12 )
	? phase_name[ phase ] : "can't happen" );
	printf( "num: %d\n", num );*/
#endif
      if ( phase == ATAPI_PH_DONE )
	{
	  if ( num == 0 )
	    {
	      printf("ATAPI DRIVE PHASE_DONE..\n");
	      err = 0;
	      break;
	    }
#ifdef IDE_DEBUG
	  printf( "atapi drive: error. data shortage\n" );
#endif
	  goto ERR;
	}
      if ( phase != ATAPI_PH_DATAIN )
	{
#ifdef IDE_DEBUG
	  printf( "atapi drive: error. unexpected phase\n" );
#endif
	ERR:
	  err = ide_atapi_error( ide_base );
	  if ( err != 0 )
	    {
	      goto out;
	    }
	  /* return err; */
	  goto AGAIN;
	}
      num_of_blocks = MIN( num, 16 );
#ifdef IDE_DEBUG
      printf( "num_of_blocks: %d\n", num_of_blocks );
#endif
      num_bytes = ide_atapi_read_discard( ide_base, buffer, num_of_blocks *
					  ide_device[ device ] -> bytes_per_block );
      num_of_blocks = num_bytes / ide_device[ device ] -> bytes_per_block;
      if ( num_bytes % ide_device[ device ] -> bytes_per_block != 0 )
	{
	  num_of_blocks++;
	}
	
      buffer += num_bytes;
      num -= num_of_blocks;
    }
 out:
  return err;
}

/* ide_atapi_read_sectors()
 * read blocks from the disc
 *
 * params: device, block, num of blocks, buffer address
 * return: 0 = success, negativ = fault
*/
static int ide_atapi_read_sectors( ulong device, ulong block, ulong num, uchar *buffer )
{
    uchar pkt[ 12 ] =
    {
	ATAPI_CMD_READ10, 0,
	0, 0, 0, 0,
	0, 0, 0,
	0, 0, 0,
    };
    ulong size=0;

#ifdef IDE_DEBUG
    printf( "* atapi_read_sectors: " );
#endif
    size = num / ide_device[ device ] -> bytes_per_block;
    if ( ( num % ide_device[ device ] -> bytes_per_block ) != 0 )
    {
	size++;
    }
#ifdef IDE_DEBUG
    printf( "num: %d, size: %d\n", num, size );
#endif

    pkt[ 2 ] = (block >> 24) & 0xff;
    pkt[ 3 ] = (block >> 16) & 0xff;
    pkt[ 4 ] = (block >> 8)  & 0xff;
    pkt[ 5 ] = block & 0xff;
    pkt[ 7 ] = (size >> 8) & 0xff;
    pkt[ 8 ] = size & 0xff;

    return ide_atapi_cmd( device, size, buffer, pkt );
}

static int ide_atapi_transfer_sectors(struct buffer_head *bh,int size,int cmd) {
  int device = MINOR(bh->dev);
  int sector = bh->block;
  unsigned char *buffer = bh->data;
  int err = -1;
  switch(cmd) {

  case READ : 
    err = ide_atapi_read_sectors(device,sector,size,buffer);
    break;
  case WRITE:
    printf("ATAPI CDROM Write isnt supported..\n");
    break;
  default:
    break;
  }

  return err;
}
/* ide_atapi_mode_sense()
 * reads the information of the cd
 *
 * params: device number
 * return: -
 */
 
void ide_atapi_mode_sense( ulong device )
{
    uchar buffer[ BLOCK_SIZE << 1];
    uchar pkt[ 12 ] =
    {
	0x5A, 0, 0x2A,
	0, 0, 0, 0,
	0, 24,
	0, 0, 0
    };

    ide_atapi_cmd( device, 1, ( uchar * ) buffer, pkt );

    printf( "multi-session: %s\n", ( buffer[ 12 ] & 0x40 ) ? "YES": "NO" );
    printf( "mode 2/form 2: %s\n", ( buffer[ 12 ] & 0x20 ) ? "YES": "NO" );
    printf( "mode 2/form 1: %s\n", ( buffer[ 12 ] & 0x10 ) ? "YES": "NO" );
    printf( "XA           : %s\n", ( buffer[ 12 ] & 0x02 ) ? "YES": "NO" );
    printf( "audio-play   : %s\n", ( buffer[ 12 ] & 0x01 ) ? "YES": "NO" );
    printf( "UPC          : %s\n", ( buffer[ 13 ] & 0x40 ) ? "YES": "NO" );
    printf( "ISRC         : %s\n", ( buffer[ 13 ] & 0x20 ) ? "YES": "NO" );
    printf( "C2 pointers  : %s\n", ( buffer[ 13 ] & 0x10 ) ? "YES": "NO" );
    printf( "R-W deinterlace and correction: %s\n", ( buffer[ 13 ] & 0x08 )
	? "YES": "NO" );
    printf( "R-W          : %s\n", ( buffer[ 13 ] & 0x04 ) ? "YES": "NO" );
    printf( "accurate CD-DA stream: %s\n", ( buffer[ 13 ] & 0x02 ) ? "YES"
	: "NO" );
    printf( "loading mechanism type: %d\n", ( buffer[ 14 ] >> 5 ));
    printf( "eject        : %s\n", ( buffer[ 14 ] & 0x08 ) ? "YES": "NO" );
    printf( "prevent jumper: %s\n", ( buffer[ 14 ] & 0x04 ) ? "YES": "NO" );
    printf( "lock state   : %s\n", ( buffer[ 14 ] & 0x02 ) ? "YES": "NO" );
    printf( "lock         : %s\n", ( buffer[ 14 ] & 0x01 ) ? "YES": "NO" );
    printf( "separate channel: %s\n", ( buffer[ 15 ] & 0x02 ) ? "YES": "NO" );
    printf( "separate volume : %s\n", ( buffer[ 15 ] & 0x01 ) ? "YES": "NO" );
    printf( "max speed    : %dKBps\n", read_be16( buffer + 16 ) );
    printf( "volume levels: %d\n", read_be16( buffer + 18 ) );
    printf( "buffer size  : %dK\n", read_be16( buffer + 20 ) );
    printf( "current speed: %dKBps\n", read_be16( buffer + 22 ) );
}

/* ide_atapi_eject()
 * ejects the cd-rom
 *
 * params: device, load/unload
 * return: 0 = success, negativ = fault
 */
int ide_atapi_eject( ulong device, int load )
{
    uchar pkt[ 12 ] =
    {
	ATAPI_CMD_START_STOP,
	0, 0, 0, 0,
	0, 0, 0, 0,
	0, 0, 0
    };

    pkt[ 4 ] = 2 +  (load ? 1 : 0 ) ;

    return ide_atapi_cmd( device, 0, NULL, pkt );
}

/* ide_atapi_pause()
 * pause the cd rom if it is plays audio
 *
 * params: device, play/pause
 * return: 0 = success, negativ = fault
 */
int ide_atapi_pause( ulong device, int play )
{
    uchar pkt[ 12 ] =
    {
	ATAPI_CMD_PAUSE,
	0, 0, 0, 0,
	0, 0, 0, 0,
	0, 0, 0
    };
    pkt[ 8 ] = play ? 1 : 0;

    return ide_atapi_cmd( device, 0, NULL, pkt );
}

/* ide_atapi_play()
 * play audio tracks
 *
 * params: device, play/pause
 * return: 0 = success, negativ = fault
 */
int ide_atapi_play( ulong device, int play )
{
    uchar pkt[ 12 ] =
    {
	ATAPI_CMD_PLAY,
	0, 0, 0, 0,
	0, 0, 0, 0,
	0, 0, 0
    };
    pkt[ 8 ] = play ? 1 : 0;

    return ide_atapi_cmd( device, 0, NULL, pkt );
}

/* ide_atapi_toc_ent()
 * read the toc entry
 *
 * params: device, toc address, length of toc
 * return: 0 = success, negativ = fault
 */
static int ide_atapi_toc_ent( uchar device, uchar *toc, ushort toc_len )
{
    uchar pkt[ 12 ] =
    {
	0x43,
	2, 0, 0, 0,
	0, 0, 0, 0,
	0, 0, 0
    };
    pkt[ 7 ] = toc_len >> 8;
    pkt[ 8 ] = toc_len;

    return ide_atapi_cmd( device, 1, ( uchar * ) toc, pkt );
}

#define MAX_TRACKS	32

typedef struct
{
    uchar min, sec, frame;
} msf_t;

/* ide_atapi_toc()
 * read all toc entries
 *
 * params: device, buffer address
 * return: 0 = success, negativ = fault
 */
int ide_atapi_toc( uchar device, uchar *buffer )
{
    uchar buf[ BLOCK_SIZE << 1 ];
    int temp,err = -1;
    /* msf_t track[ MAX_TRACKS ]; */
    uint num_tracks;
    /* uint index; */

#ifdef IDE_DEBUG
    printf( "atapi_toc:\n" );
#endif
    temp = 4;
    temp = ide_atapi_toc_ent( device, buf, temp );
    if ( temp != 0 )
    {
      goto out;
    }
    num_tracks = buf[ 3 ] - buf[ 2 ] + 1;
    if ( num_tracks <= 0 || num_tracks > 99 )
    {
#ifdef IDE_DEBUG
	printf( "atapi_toc: error: bad number of tracks %d\n", num_tracks );
#endif
	goto out;
    }
#ifdef IDE_DEBUG
    printf( "atapi_toc: detected %d tracks\n", num_tracks );
#endif
    if ( num_tracks > MAX_TRACKS )
    {
#ifdef IDE_DEBUG
	printf( "atapi_toc: warning: too many tracks (%u); "
		"reducing to %u.\n", num_tracks, MAX_TRACKS );
#endif
	num_tracks = MAX_TRACKS;
    }
    temp = 4 + 8 * ( num_tracks + 1 );

    temp = ide_atapi_toc_ent( device, buffer, temp );
    if ( temp != 0 )
    {
      goto out;
    }
    err = 0;
 out:
    return err;
}

/* ide_atapi_play_msf()
 * plays audio track, format in msf
 *
 * params: device, play ioctl struct
 * return: 0 = success, negativ = fault
 */
int ide_atapi_play_msf( uchar device, struct play_ioctl *play )
{
    uchar pkt[ 12 ] =
    {
	ATAPI_CMD_PLAY,
	0, 0, 0, 0,
	0, 0, 0, 0,
	0, 0, 0
    };
#ifdef IDE_DEBUG
    printf( "atapi_play_msf: %02d:%02d:%02d %02d:%02d:%02d\n",
	play -> start_m, play -> start_s, play -> start_f,
	play -> end_m, play -> end_s, play -> end_f );
#endif

    pkt[ 3 ] = play -> start_m;
    pkt[ 4 ] = play -> start_s;
    pkt[ 5 ] = play -> start_f;
    pkt[ 6 ] = play -> end_m;
    pkt[ 7 ] = play -> end_s;
    pkt[ 8 ] = play -> end_f;

    return ide_atapi_cmd( device, 0, NULL, pkt );
}

static __inline__ void ide_blkdev_init(uchar drive,const char *name) {
  blkdev[IDE_CDROM_MAJOR_NR].block_size = ide_device[drive]->bytes_per_block;
  blkdev[IDE_CDROM_MAJOR_NR].block_transfer_function = ide_atapi_transfer_sectors;
  list_head_init(&blkdev[IDE_CDROM_MAJOR_NR].request_head);
  cdrom_drives[drives++] = drive;
  register_device(name,MKDEV(IDE_CDROM_MAJOR_NR,drive),BLK_DEVICE,blkdev[IDE_CDROM_MAJOR_NR].block_size,NULL);
}

/* init_atapidevice()
 * initialize the atapi devices
 *
 * params: internal device number
 * return: -
 */
void init_atapidevice( uchar drive )
{
  char atapi_name[256];

  sprintf( atapi_name, "/dev/hd%c", 'a' + drive );

  printf( "%s: ATAPI drive", atapi_name);

  /* first info block */
  ide_device[ drive ] -> start = 16;

  ide_blkdev_init(drive,atapi_name);

  if ( strcmp( ide_device[ drive ] -> model_number, "" ) )
    {
      printf( " [%s]", ide_device[ drive ] -> model_number );
    }
  if ( strcmp( ide_device[ drive ] -> firmware, "" ) )
    {
      printf( " [%s]", ide_device[ drive ] -> firmware );
    }
  printf( "\n" );
}
