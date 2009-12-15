/*Author: A.R.Karthick for MIR
 Buffer cache routines
*/

#ifdef __KERNEL__
#ifndef _BUFFER_HEAD_H
#define _BUFFER_HEAD_H

typedef enum {
  BUFFER_CLEAN,
  BUFFER_DIRTY,
  BUFFER_INIT,
} buffer_state;

/*limit is set as of now,but would become dynamic in future*/

#define BUFFER_HEAD_LIMIT 0x100

struct buffer_head {
  int dev; /*the device it points*/
  int block; /*the block that it refers to*/
  int block_size; /*block size of the buffer head*/
  unsigned char *data; /*the data that it refers to */
  buffer_state state ; /*the state it refers to*/
  struct list_head buffer_head; /*list head of cyclic buffer heads*/
  struct hash_struct buffer_hash; /*hash linkage*/
  int (*hash_search) ( const struct buffer_head *, const struct buffer_head * );
};

extern struct buffer_head *bread(int dev, int block );
extern struct buffer_head *bwrite(int dev,int block,int size,const unsigned char *);
extern int brelse( struct buffer_head *bh );
extern int buffer_head_init(void);
extern struct buffer_head *release_buffer_head(int dev,int block);

static __inline__ struct buffer_head *read_sec(int dev,int block) { 
  struct buffer_head *bh;
  bh = bread(dev,block);
  return bh;
}

#endif
#endif

