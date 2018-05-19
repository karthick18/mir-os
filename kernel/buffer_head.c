   /* MIR-OS BUFFER Cache
   ;   a.r.karthick@gmail.com ; a_r_karthic@rediffmail.com
   ;  
   ;    Copyright (C) 2004
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
   ; Buffer Cache for MIR : Has to be modified once VFS changes are integrated
   ;
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
#include <mir/init.h>
#define  BUFFER_HEAD_HASH_TABLE 301

static DECLARE_LIST_HEAD (buffer_head);
static int buffer_heads; /*count of buffer heads*/

/*cache of buffer head objects*/
static struct slab_cache *buffer_head_cache;

#define hash_bh(dev,block) \
 ( ( (dev) ^(block) ) % hash_table_size )

static __inline__ int link_buffer_head( struct buffer_head *bh ) {
  unsigned int key;
  int err = -1;
  if(buffer_heads >= BUFFER_HEAD_LIMIT ) 
    goto out;
  list_add_tail (&bh->buffer_head, &buffer_head );
  key = hash_bh(bh->dev, bh->block );
  add_hash (&bh->buffer_hash, key );
  ++buffer_heads;
  err = 0;
 out:
  return err;
}

/*unlink from the hash and cyclic chains*/

static __inline__  int unlink_buffer_head ( struct buffer_head *bh ) { 
  int err = -1;
  if(!buffer_heads ) {
    goto out;
  }
  --buffer_heads;
  list_del( &bh->buffer_head );
  del_hash( &bh->buffer_hash );
  err = 0 ;
 out:
  return err;
}

static int search_buffer_head ( const struct buffer_head *bh1,const struct buffer_head *bh2 ) { 
  return bh1->dev == bh2->dev && bh1->block == bh2->block ;
}

/*Look for a buffer head for the device and the block
*/

static struct buffer_head *lookup_buffer_head (int dev, int block ) {
  struct buffer_head bh , *ptr = &bh;
  struct buffer_head *res;
  unsigned int key;
  ptr->dev = dev , ptr->block = block;
  ptr->hash_search = search_buffer_head ;
  key = hash_bh (dev, block );
  res = search_hash (key,ptr,struct buffer_head,buffer_hash);
  return res;
}

/*add a new buffer head*/

static struct buffer_head *add_buffer_head( int dev, int block ) { 
  struct buffer_head *bh;
  int major = MAJOR(dev);
  if( !(bh  = slab_cache_alloc (buffer_head_cache, 0 ) ) ) {
    panic ("slab_cache_alloc error in Line %d,Routine %s\n",__LINE__,__FUNCTION__);
  }
  memset(bh,0x0,sizeof(struct buffer_head) );
  bh->dev = dev;
  bh->block = block;
  bh->state = BUFFER_INIT; /*will have to read it*/
  bh->hash_search = search_buffer_head;
  if( link_buffer_head (bh ) < 0 ) {
    slab_cache_free(buffer_head_cache,bh );
    return NULL;
  }
  /*allocate the data to read in*/
  bh->block_size = MAX(BLOCK_SIZE, blkdev[major].block_size );
  bh->data = slab_alloc(bh->block_size,0);
  if(! bh->data ) {
    panic ("slab_alloc error in Line %d, Routine %s\n",__LINE__,__FUNCTION__);
  }
  return bh;
}

static int block_device_transfer(struct buffer_head *bh,int size,int cmd) {
  int major = MAJOR(bh->dev);
  int status = -1;
  if(blkdev[major].block_transfer_function) {
    status = blkdev[major].block_transfer_function(bh,size,cmd);
  }
  return status;
}

static struct buffer_head *read_buffer_head( int dev, int block) { 
  struct buffer_head *bh;
  /*look up the hash chain*/
  bh = lookup_buffer_head(dev,block);
  if( ! bh ) {
    /*add the buffer head*/
    if ( !( bh = add_buffer_head (dev, block ) ) &&
	 ! (bh = release_buffer_head (dev,block ) ) )
      return NULL;
    block_device_transfer(bh,bh->block_size,READ);
    bh->state = BUFFER_CLEAN ;
  }
  return bh;
}

struct buffer_head *bread (int dev, int block ) { 
  return read_buffer_head (dev, block );
}

static struct buffer_head *write_buffer_head ( int dev, int block ,const unsigned char *data, int size) { 
  struct buffer_head *bh;
  if (! (bh = lookup_buffer_head (dev,block) ) ) {

    if( ! (bh = add_buffer_head (dev,block ) ) &&
	! (bh = release_buffer_head(dev,block ) ) 
	)
      return NULL ;
  }
  bh->state = BUFFER_DIRTY;
  size = MIN(bh->block_size, size );
  memcpy((void*)bh->data,(void *)data , size );
  if(size < bh->block_size ) {
    memset((void *) (bh->data + size ),0x0,bh->block_size - size);
  }
  block_device_transfer(bh,size,WRITE);
  bh->state = BUFFER_CLEAN;
  return bh;
}

struct buffer_head *bwrite(int dev, int block,int size,const unsigned char *data) {
  return write_buffer_head (dev,block,data,size);
}

static struct buffer_head *get_buffer_head(void) {
  struct list_head *head= &buffer_head;
  struct list_head *traverse;
  list_for_each(traverse, head ) { 
    struct buffer_head *bh = list_entry(traverse, struct buffer_head,buffer_head);
    if( bh->state == BUFFER_CLEAN ) {
       return bh;
    }
  }
  return NULL;
}
/*This would take a buffer head from the top
  and then add that buffer head to the tail.
 */
struct buffer_head *release_buffer_head (int dev, int block ) { 
  struct buffer_head *bh;
  if (LIST_EMPTY (&buffer_head) || buffer_heads < BUFFER_HEAD_LIMIT ) {
    return NULL;
  }
  if(! (bh = get_buffer_head() ) ) {
    bh = list_entry (buffer_head.next , struct buffer_head, buffer_head );
  }
  if(bh->dev == dev && bh->block == block ) {
    return NULL;
  }
  if(bh->state == BUFFER_DIRTY ) {
    block_device_transfer(bh,bh->block_size,WRITE);
    bh->state = BUFFER_CLEAN;
  }
  unlink_buffer_head (bh ); /*unlink this buffer head */
  bh->dev = dev;
  bh->block = block;
  
  bh->state = BUFFER_INIT;
  if (link_buffer_head(bh) < 0 ) { /*relink this guy at the tail*/
    panic ("Buffer head inconsistent..\n");
  }
  return bh;
}
  
int brelse(struct buffer_head *bh) {
  return 0;
}

int __init buffer_head_init(void) { 
  if( init_hash ( BUFFER_HEAD_HASH_TABLE ) < 0 ) {
    panic("Unable to initialise buffer head hash table\n");
  }
  if( ! ( buffer_head_cache = slab_cache_create("buffer_head_cache",sizeof(struct buffer_head), SLAB_HWCACHE_ALIGN,0 ) ) ) {
    panic ("Unable to create buffer head cache:\n");
  }
  return 0;
}
