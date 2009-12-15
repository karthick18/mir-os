/*Author: Pratap PV. for MIR*/
#ifdef __KERNEL__
#ifndef _FILE_FS_H
#define _FILE_FS_H

#include <mir/vfs/vfs.h>

#define MAX_FDS           1024
#define MAX_OPEN_FDS       (MAX_FDS)

struct files_struct { 
  int curr_fd;
  int free_fd;
  int max_fd;
  int fd_size;
  /*this is a fast map*/
  unsigned char fd_map[4]; 
  /*bitmap of free fds: close would free them first*/
  unsigned char *fd_map_p; 
  struct file *file[32]; /*fast table*/
  struct file **file_p;
  struct list_head file_head;
  /*current working directory of the process*/
  char cwd[MAX_PATH+1];
};

struct file {

#define READ_MODE 0x0
#define WRITE_MODE 0x1
#define READ_WRITE_MODE  0x2
	char *name;
	int mode;
	int size;
	int offset;
	int start_inode;
	int curr_inode;
	int total_ino; /* total inode's traversed so far */
	struct vnode *v;
        struct list_head file_head;
};

extern int get_file(struct files_struct *);
extern int release_file(struct files_struct *,int fd);
extern int release_files_struct(struct files_struct *);
extern void destroy_file(struct file *fptr);
extern int init_files_struct(struct files_struct *);
#endif
#endif

