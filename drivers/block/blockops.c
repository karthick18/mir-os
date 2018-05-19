/*   MIR-OS : Block device operations
;    Copyright (C) 2003-2004
;     A.R.Karthick
;    <a.r.karthick@gmail.com,a_r_karthic@rediffmail.com>
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
;
;
*/

#include <mir/kernel.h>
#include <mir/sched.h>
#include <mir/vfs/fs.h>
#include <mir/vfs/vfs.h>
#include <mir/blkdev.h>
#include <mir/buffer_head.h>
#include <mir/ide.h>
#include <mir/floppy.h>
#include <mir/init.h>

/*Read the data from the block device : dev
*/
static int block_read_data(int dev,int block,int size,unsigned char *data) { 
  int major;
  int blksize = 0;
  int ret = 0;
  int blocks = 0;
  int rem ;
  if (! size )
    goto out;

  major = MAJOR(dev);
  blksize = blkdev[major].block_size;
  if(! blksize) {
    goto out;
  }
  blocks = size / blksize; /*blocks to read*/
  rem = size % blksize;
  ret = size;
  while(blocks) {
    struct buffer_head *bh;
    bh = bread(dev,block++);
    if(! bh ) { 
      goto out;
    }
    memcpy(data,bh->data,blksize);
    data += blksize;
    --blocks;
  }
  if(rem) { 
    struct buffer_head *bh;
    bh = bread(dev,block);
    if(! bh ) { 
      goto out;
    }
    memcpy(data,bh->data,rem);
  }
 out:
  return ret - blocks*blksize;
}

/*Write the block*/
static int block_write_data(int dev,int block,int size,const unsigned char *data) { 
  int major;
  int ret = 0;
  int blocks = 0;
  int blksize = 0;
  int rem ;
  if(! size ) {
    goto out;
  }

  major = MAJOR(dev);
  blksize = blkdev[major].block_size;
  if(! blksize) {
    goto out;
  }
  blocks = size / blksize;
  rem = size % blksize;
  ret = size;
  while(blocks) { 
    struct buffer_head *bh ;
    bh = bwrite(dev,block++,blksize,data);
    if(! bh ) {
      goto out;
    }
    data += blksize;
    --blocks;
  }
  if( rem ) {
    struct buffer_head *bh;
    bh = bwrite(dev,block,rem,data);
    if(! bh ) {
      goto out;
    }
  }
 out:
  return ret - blocks*blksize;
}

struct device_operations block_device_operations = {
  read_data:     block_read_data,
  write_data:    block_write_data,
};

