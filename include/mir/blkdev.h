/*Author: A.R.Karthick for MIR
*/
#ifdef __KERNEL__
#ifndef _BLK_DEV_H
#define _BLK_DEV_H_

#define READ  0x1
#define WRITE 0x0

#define SECTOR_SHIFT  9
#define SECTOR_SIZE ( 1 << SECTOR_SHIFT )
#define BLOCK_SIZE    1024
#define MAX_BLKDEVS  0x100
#define IDE_BLKDEV_MAJOR_NR  0x0
#define IDE_CDROM_MAJOR_NR   0x1
#define MINOR_SHIFT 0x8
#define MINOR_MASK  ( ( 1 << MINOR_SHIFT) - 1 )
#define MAJOR_MASK  MINOR_MASK
#define MINOR(dev)   ( (dev) & MINOR_MASK )
#define MAJOR(dev)   ( ( (dev) >> MINOR_SHIFT  ) & MAJOR_MASK )
#define MKDEV(major,minor)  ( ( ( (major) & MAJOR_MASK ) << MINOR_SHIFT ) | ((minor) & MINOR_MASK ) )
#define PARTITION_SHIFT 6 /*64 partitions possible*/
#define get_device(dev) ( MINOR(dev) >> PARTITION_SHIFT )

struct buffer_head;
/*request structure per block device*/
typedef int request_function_t ( struct list_head *request_head);
typedef int block_transfer_t (struct buffer_head *,int size,int cmd);

struct blkdev_struct { 
  request_function_t *request_function;
  /*temporary till request queue is through*/
  block_transfer_t   *block_transfer_function;
  struct list_head request_head; /*list head of requests per block device*/
  int *offsets; /*offsets per block device that includes all sub-devices+partitions*/
  int *sizes; /*sizes per block device that includes all sub-devices + partitions*/
  int block_size;
};

extern struct blkdev_struct blkdev[MAX_BLKDEVS];

/*the request structure per block device*/
struct io_request { 
  struct list_head request_head;
  int start_sector;
  int nr_sectors; /*nr. of sectors per request*/
  unsigned char *data; /*data corresponding to the request*/
  int dev; /*the dev. number that includes MAJOR+MINOR for the request*/
};

#endif
#endif
