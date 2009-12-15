/*Author: Pratap. P.V. 
   &      A.R.Karthick for MIR
*/

#ifdef __KERNEL__
#ifndef _VFS_H
#define _VFS_H

#define MAX_PATH 255
#define FILE_SEPARATOR '/'
#define FILE_SEPARATOR_STR "/"

struct filesystem;
struct mountpoint;
struct dentry;

struct vname {
  char name[MAX_PATH+1];
  char abs_name[MAX_PATH+1]; /*absolute name*/
  struct vnode *node;
};

struct stat {
  int         ino;                 /*  inode's number  */
  int         size;                /*  file size, in bytes  */
  int         blocks;              /*  blocks allocated for file  */
};


struct vnode {
  struct list_head next;
  struct list_head child_head; /*list of children*/
  struct hash_struct hash;
  int (*hash_search) ( const struct vnode *,const struct vnode *);
  struct vname name;
  struct device *dev; /*device it is on */
  struct stat ss;
	
  int ismounted; /* is another filesystem mounted at this vnode */

  struct vnode *parent;
  struct filesystem *fs;
};

extern struct vnode *vnode_lookup(const char *abs_name);
extern struct vnode *vnode_find(const char *name);
extern struct vnode *vnode_create(const char *file,const char *abs_path,const struct stat *ss,const struct vnode *pnode,const struct filesystem *fs,const struct device *dev);
extern void vnode_delete(struct vnode *vnode);
extern void vnode_delete_all(struct vnode *vnode);
extern int vfs_stat(struct vnode *pnode,const char *file,struct stat *ss,struct mountpoint *mp);
extern int vfs_open(const char *file,int mode);
extern int vfs_read(int fd,char *buf,int len);
extern int vfs_close(int fd);
extern struct dentry **vfs_getdentry(const char *dir,int *size);
extern struct dentry *vfs_lookup_dentry(struct vnode *v,const char *name);
extern void list_mp(void);
extern void fstab(int arg);
extern int vfs_mount(const char *pathname,const char *fsname,int dev);
extern int vfs_umount(const char *pathname);
extern int vfs_init(void);
extern int vfs_init_caches(void);
extern int parse_path(const char *path,char *result,struct vnode **vnode,int (*error_callback) (struct dentry *dentry,const char *name), int (*verify_callback)(struct dentry *dentry ) );
extern int traverse_path(const char *path,char *ret);
extern void trim_file_separator(const char *input,char *output,int max);
extern slab_cache_t *vnode_cache;
extern slab_cache_t *file_cache;
extern slab_cache_t *mnt_cache;
extern slab_cache_t *filesystem_cache;
extern slab_cache_t *dev_cache;
#endif
#endif

