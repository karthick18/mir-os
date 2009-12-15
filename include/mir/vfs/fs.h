/*Author: Pratap. P.V. 
   &      A.R.Karthick for MIR
*/
  
#ifdef __KERNEL__
#ifndef _VFS_FS_H
#define _VFS_FS_H

#include <mir/kernel.h>
#include <mir/vfs/vfs.h>
#include <mir/vfs/file.h>

struct buffer_head;

struct dentry {
        char name[MAX_PATH+1];
	int attr;
#define ATTR_READ_ONLY 0x01
#define ATTR_HIDDEN 0x02
#define ATTR_SYSTEM 0x04
#define ATTR_VOLUME_ID 0x08
#define ATTR_DIRECTORY 0x10
#define ATTR_ARCHIVE 0x20
        struct vnode *v;
};
struct stat;
struct mountpoint;

struct superblock {
	int blocksize;
	int root_inode;
	int root_size;
	void *spc_fs;
};

struct filesystem {
  struct list_head next;
#define MAX_FS_NAME 30
  char name[MAX_FS_NAME+1];
  struct superblock fssb;
	
  /* functions follow */
  int             (*read_superblock) (struct mountpoint *);
  int		(*namestat) (struct vnode *,const char *name, struct stat *ss, int inroot, struct mountpoint *);
  int             (*read) (struct file *, char *, int); /* the file struct, buffer, offset, length */
  int		(*mount) (char *mp);

  struct dentry**	(*getdentry) (const char *, struct vnode *, int *);
  struct dentry *       (*lookup_dentry)(struct vnode *v,const char *name);
};

typedef enum {
  BLK_DEVICE = 0,
  CHR_DEVICE,
  MAX_DEVICES,
}device_type;

struct device {
  struct list_head next;
  char name[MAX_PATH+1];
  int blksize;
  int dev;
  device_type type; /*type of device*/
  struct device_operations *device_operations;
};

/*Define the device supported operations here*/
struct device_operations { 
  int (*read_data) ( int dev, int block, int size, unsigned char *data);
  int (*write_data)( int dev, int block, int size, const unsigned char *data);
};

struct mountpoint {
  struct list_head next;
  char path[MAX_PATH+1];
  struct filesystem *fs;
  struct device *dev;
  struct vnode  *v;
};
extern int register_device(const char *name,int dev,device_type type,int blksize,struct device_operations *device_operations);
extern void list_dev(void);
extern void devices(int arg);
extern struct device *get_dev_by_name(const char *name);
extern struct device *get_dev(int dev);
extern int do_data_transfer(struct device *,int block,int size,unsigned char *data,int cmd);
extern int data_transfer(int dev,int block,int size,unsigned char *data,int cmd);
extern int data_transfer_name(const char *dev_name,int block,int size,unsigned char *data,int cmd);
extern int register_fs(struct filesystem *fs);
extern void list_fs(void);
extern struct filesystem *get_fs(const char *fs);
extern struct mountpoint *ismp(const char *path);
extern struct mountpoint *get_mount(const char *path);
extern struct device_operations block_device_operations;

#endif
#endif

