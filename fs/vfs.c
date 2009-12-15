/*   MIR-OS : VFS routines
     ;    Copyright (C) 2003-2004
     ;
     ;                   i) Pratap P.V. (pratap_theking@hotmail.com)
     ;                  ii)A.R.Karthick
     ;    <a_r_karthic@rediffmail.com,karthick_r@infosys.com>
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
#include <asm/page.h>
#include <mir/vfs/vfs.h>
#include <mir/vfs/fs.h>
#include <mir/vfs/file.h>
#include <mir/fs/fat.h>
#include <asm/string.h>
#include <mir/init.h>
#define VFS_HASH_SIZE 2087

static DECLARE_LIST_HEAD(fslist);
static DECLARE_LIST_HEAD(mntlist);

slab_cache_t *vnode_cache;
slab_cache_t *file_cache;
slab_cache_t *mnt_cache;
slab_cache_t *filesystem_cache;
slab_cache_t *dev_cache;

int file_modes[3] = { ( 1 << READ_MODE ) , ( 1 << READ_MODE ) | ( 1 << WRITE_MODE), ( 1 << READ_MODE) | ( 1 << WRITE_MODE) };

int convert_mode(int mode) { 
  int modes = sizeof(file_modes)/sizeof(file_modes[0]);
  if(mode < 0 || mode >= modes) {
    mode = -1;
    goto out;
  }
  mode = file_modes[mode];
 out:
  return mode;
}
    
/*trim multiple occurences of FILE_SEPARATOR to a single occurence*/
void trim_file_separator(const char *input,char *output,int max) { 
  char *str = output;
  if( input == NULL ||  str == NULL  || max == 0) 
    return ;
  while(max-- && *input) { 
    if ( *input == FILE_SEPARATOR) { 
      while( *input && *input == FILE_SEPARATOR ) 
	++input;
      if(*input) 
	*str++ = FILE_SEPARATOR;
      continue;
    }
    *str++ = *input++;
  }
  *str = 0;
  /*for only a FILE_SEPARATOR_STR input, keep the FILE_SEPARATOR*/
  if( ! *output ) { 
    *output++ = FILE_SEPARATOR;
    *output = 0;
  }
}

static __inline__ void vnode_initialise(struct vnode *vnode) { 
  list_head_init(&vnode->child_head);
}

static __inline__ int permute(const unsigned char c ) { 
  return ( ( ( c >> 4) & 0xF ) + ( c & 0xF ) );
}

static __inline__ unsigned int vnode_hash(const char *abs_name) { 
  unsigned int key = 0;
  const char *str = abs_name;
  while (*str) {  
    key += 31 * permute((const unsigned char )(*str) );
    ++str;
  }
  return key % hash_table_size;
}

static int vnode_hash_search(const struct vnode *reference,const struct vnode *search) { 
  return (!strcmp (reference->name.abs_name,search->name.abs_name) );
}

/*Vnode hashed by absolute names.
  Make sure to unlink and unhash them during mounts,umounts
*/
struct vnode *vnode_lookup(const char *abs_name) { 
  unsigned int key ;
  struct vnode vnode, *vnode_ptr = &vnode;
  struct vnode *search = NULL;
  if(! abs_name) {
    goto out;
  }
  key = vnode_hash(abs_name);
  memset(vnode_ptr,0x0,sizeof(vnode));
  vnode_ptr->hash_search = vnode_hash_search;
  strncpy(vnode_ptr->name.abs_name,abs_name,sizeof(vnode_ptr->name.abs_name)-1);
  search =  search_hash(key,vnode_ptr,struct vnode,hash);
 out:
  return search;
}

int register_fs (struct filesystem *fs)
{
  list_add_tail(&fs->next,&fslist);
  return 0;
}

void list_fs (void)
{
  register  struct list_head *traverse;
  struct list_head *head = &fslist;
  printf ("Registered filesystems.. \n");

  list_for_each(traverse,head) { 
    struct filesystem *ptr = list_entry(traverse,struct filesystem,next);
    printf ("%s\n", ptr->name);
  }
}

struct filesystem *get_fs (const char *fs)
{
  register struct list_head *traverse;
  struct list_head *head = &fslist;
  list_for_each(traverse,head) { 
    struct filesystem *ptr = list_entry(traverse,struct filesystem,next);
    if(! strcmp(ptr->name,fs) )  
      return ptr;
  }
  return NULL;
}
/*We add the mount points in  LIFO way and _not_ in a FIFO way coz,this way we can stack the mount points one after the another.
*/

static __inline__ void register_mountpoint(struct mountpoint *mp) { 
  list_add(&mp->next,&mntlist);
}

static __inline__ void unregister_mountpoint(struct mountpoint *mp) {
  if(mp) {
    list_del(&mp->next);
  }
}

struct mountpoint* ismp (const char *path) {
  struct list_head *head = &mntlist;
  register struct list_head *traverse;
  list_for_each(traverse,head) { 
    struct mountpoint *ptr = list_entry(traverse,struct mountpoint,next);
    if (!strcmp (path, ptr->path))
      return ptr;
  }
  return NULL;
}

/*Get a mountpoint corresponding to a full pathname*/
struct mountpoint *get_mount(const char *path) { 
  struct list_head *head = &mntlist;
  register struct list_head *traverse;
  list_for_each(traverse,head) { 
    struct mountpoint *mp = list_entry(traverse,struct mountpoint,next);
    if(! strncmp (path,mp->path,strlen(mp->path) ) ) 
      return mp;
  }
  return NULL;
}

/*link the vnode in the various chains*/
static __inline__ void link_vnode(struct vnode *vnode) { 
  struct list_head *head = NULL;
  unsigned int hash;
  if ( vnode->parent) { 
    head = &vnode->parent->child_head;
    list_add_tail(&vnode->next,head);
  }
  hash = vnode_hash(vnode->name.abs_name);
  vnode->hash_search = vnode_hash_search;
  add_hash(&vnode->hash,hash); /*link into the hash chain*/
}

static __inline__ void unlink_vnode(struct vnode *vnode) { 
  if ( vnode->parent ) { 
    list_del(&vnode->next);
  }
  del_hash(&vnode->hash);
}

struct vnode *vnode_create (const char *file,const char *abs_name,const struct stat *ss, const struct vnode *pnode,const struct filesystem *fs,const struct device *dev) {
  struct vnode *v = NULL;
  if (!file || !ss) {
    printf ("vnode_create(): please check args\n");
    goto out;
  }
  v = (struct vnode *) slab_cache_alloc (vnode_cache, 0);
  if( ! v ) { 
    printf("Unable to alloc vnode in Function %s\n",__FUNCTION__);
    goto out;
  }

  memset (v, 0, sizeof(struct vnode));
  /*some GCCs can be buggy with such a stack copy*/
#if 0
  v->ss = *ss;
#else
  memcpy(&v->ss,ss,sizeof(struct stat) );
#endif
  strncpy(v->name.name,file,sizeof(v->name.name)-1);
  strncpy(v->name.abs_name,abs_name,sizeof(v->name.abs_name)-1);
  v->name.node = v;
  v->fs = (struct filesystem *)fs;
  v->parent = (struct vnode *)pnode;
  v->dev = (struct device *)dev;
  /*add the vnode into the various chains*/
  link_vnode(v);
 out:
  return v;
}

void vnode_delete(struct vnode *vnode) { 
  if(vnode) { 
    unlink_vnode(vnode);
    slab_cache_free(vnode_cache,(void *)vnode);
  }
}

/*Delete all the child VNODES of a mounted vnode during a umount recursively:
  We have to unlink also the mounted vnode:
*/

void vnode_delete_all(struct vnode *vnode) { 
  struct list_head *traverse,*next;
  struct list_head *head = &vnode->child_head;
  for(traverse = head->next ; traverse != head ; traverse = next ) { 
    struct vnode *vnode = list_entry(traverse,struct vnode, next);
    next = traverse->next;
    vnode_delete_all(vnode);
  }
  vnode_delete(vnode); 
}

int vfs_stat (struct vnode *pnode, const char *file, struct stat *ss, struct mountpoint *cmp) {
  if (!pnode) { /* root FS */
    struct mountpoint *tmp;
    tmp = get_mount (file);
    if (!tmp) 
      panic ("vfs_stat(): root FS not specified\n");
    ss->ino = tmp->fs->fssb.root_inode;
    ss->size = tmp->fs->fssb.root_size;
    return 0;
  } 

  if (!pnode->fs->namestat) {
    panic("Inconsistent FS operations. Namestat routine not initialised..\n");
  }

  pnode->fs->namestat (pnode, file, (struct stat *)ss,pnode->ismounted, cmp);

  return 0;
}
 

struct vnode *vnode_find (const char *filename) {
  struct stat tmpstat;
  struct vnode *v = NULL, *parent = NULL;
  struct mountpoint *currentmp;
  char str[MAX_PATH+1],abs_path[MAX_PATH+1]="", *ptr, *ptr1;
  char path[MAX_PATH+1];
  if (!filename) {
    printf ("vnode_find(): (filename) = NULL\n");
    goto out;
  }
  trim_file_separator(filename,str,sizeof(str)-1);
  ptr = str;
  /*get the mount point for this vnode*/
  currentmp = get_mount(str);
  if(! currentmp) {
    printf("Unable to find the mount point for path %s...\n",str);
    goto out;
  }
  if(! currentmp->v ) {
    printf("mount point %s doesnt have a vnode...\n",currentmp->path);
    goto out;
  }
  v = currentmp->v;
  ptr += strlen(currentmp->path);

  if(strcmp(currentmp->path,FILE_SEPARATOR_STR) ) {
    strncpy(abs_path,currentmp->path,sizeof(abs_path)-1);
  }

  parent = v;
  while ( ptr && *ptr ) { 
    ptr1 = strchr(ptr,FILE_SEPARATOR);

    if(ptr1) 
      *ptr1++ = 0;

    if(*ptr) { 
      sprintf(path,"%s%s",FILE_SEPARATOR_STR,ptr);
      strncat(abs_path,path,sizeof(abs_path) - strlen(abs_path) -1 );

      if( ! ( v = vnode_lookup(abs_path) ) ) {
	/*vnode is not found: set it up
	 */
	vfs_stat (parent, ptr, &tmpstat, currentmp);
	v = vnode_create (ptr, abs_path,&tmpstat, parent,currentmp->fs,currentmp->dev);
	if(!v) {
	  goto out;
	}
	vnode_initialise(v);
      }
      parent = v;
    }
    ptr = ptr1;
  }
 out:
  return v;
}

static int open_error_callback(struct dentry *dentry,const char *name) { 
  if(! dentry) { 
    printf("open error: invalid path %s\n",name);
  } else {
    printf("open error: invalid path %s.Not a directory...\n",name);
  }
  return 0;
}

static int open_verify_callback(struct dentry *dentry) {
  return (dentry->attr & ATTR_DIRECTORY ) != 0;
}

int traverse_path (const char *path,char *ret) {
  char input[MAX_PATH+1],output[MAX_PATH+1];
  struct vnode *vnode = vnode_lookup(current->files_struct.cwd);
  char *str;
  struct dentry *dentry ;
  int relative = 0,len,err = -1;
  if(! vnode) { 
    printf("current working directory vnode not found..\n");
    goto out;
  }
  if( ! path ) { 
    goto out;
  }
  len = strlen(path);
  if (path[len-1] == FILE_SEPARATOR) { 
    goto out_relative;
  }
  trim_file_separator(path,input,sizeof(input)-1);
  strncpy(output,input,sizeof(output)-1);
  /*find the pointer to the last SEPARATOR*/
  str = strrchr(input,FILE_SEPARATOR);
  if(str) { 
    char  result[MAX_PATH+1];
    *str++ = 0;
    if( !*str || !strcmp(str,".")  ||
	!strcmp(str,"..")
	) {
      goto out_relative;
    }
    if(input[0]) { 
      if (parse_path(input,result,&vnode,open_error_callback,open_verify_callback) ) {
	goto out;
      }
      relative = 1;
      strncpy(output,str,sizeof(output)-1);
    } else { 
      struct mountpoint *mp;
      str = output;
      if ( ! ( mp = get_mount( output ) ) ) {
	printf("Invalid path %s\n",path);
	goto out;
      }
      vnode = mp->v;
      str += strlen(mp->path);
      if(! *str ) { 
	goto out_relative;
      }
      relative = 1;
      strncpy(output,str,sizeof(output)-1);
    }
  } else if(strcmp(input,".") && strcmp(input,"..") ) {
    relative = 1;
  }
  if(! relative) {
  out_relative:
    printf("Cannot open directories...\n");
    goto out;
  }
  /*first try getting the dentry*/
  dentry = vfs_lookup_dentry(vnode,output);

  if( !dentry) {
    printf("Invalid relative path %s\n", output);
    goto out;
  }
  if ( (dentry->attr & ATTR_DIRECTORY ) ) { 
    /*display the single entry:*/
    goto out_relative;
  }
  /*its a file: copy the absolute location*/
  sprintf(ret,"%s%s%s",vnode->name.abs_name,vnode->name.abs_name[-1] == FILE_SEPARATOR ? "" : FILE_SEPARATOR_STR,output);
  err = 0;
 out:
  return err;
}

int vfs_open (const char *file, int mode) {
  struct file *f ;
  struct vnode *v;
  int err = -1,fd;
  char output[MAX_PATH+1];

  if(! current) { 
    printf("No process found in the kernel...\n");
    goto out;
  }
  if(! file ) {
    goto out;
  }

  if( mode & ~(READ_WRITE_MODE ) ) { 
    goto out;
  }

  if( ( mode = convert_mode(mode) ) < 0 ) {
    printf("Invalid mode given %d\n",mode);
    goto out;
  }
  /*parse the path*/
  if(traverse_path(file,output) < 0 ) {
    goto out;
  }

  fd = get_file(&current->files_struct);

  if(fd < 0 ) {
    goto out;
  }
  f = current->files_struct.file_p[fd];

  if(! f ) {
    printf("Unable to allocate file pointer in %s...\n",__FUNCTION__);
    goto out;
  }

  f->name = (char *) slab_alloc (strlen(output)+1, 0);

  if( ! f->name ) { 
    release_file(&current->files_struct,fd);
    goto out;
  }

  strcpy (f->name, output);
  f->mode = mode;
  v = vnode_find (output);

  if(! v ) {
    printf("Vnode for file %s not found in Function %s...\n",file,__FUNCTION__);
  }
  f->size = v->ss.size;
  f->offset = 0;

  f->start_inode = f->curr_inode = v->ss.ino;
  f->total_ino = 0;
  f->v = v;
  err = fd;
 out:
  return err;
}

int vfs_read (int fd, char *buf, int len)
{
  int err = -1;
  struct file *fp;
  if( fd < 0 || fd >= current->files_struct.free_fd ) {
    goto out;
  }
  if(! (fp = current->files_struct.file_p[fd] ) ) {
    goto out;
  }  
  if(! (fp->mode & ( 1 << READ_MODE ) ) ) { 
    goto out;
  }
  if( fp && fp->v && fp->v->fs && fp->v->fs->read) {
    err = fp->v->fs->read (fp, buf,len);
  }
 out:
  return err;
}

int vfs_close(int fd) { 
  int err;
  err = release_file(&current->files_struct,fd);
  return err;
}

struct dentry **vfs_getdentry (const char *dir, int *size)
{
  struct vnode *v;
  struct dentry **ptr = NULL;
  v = vnode_find (dir);
  if(v && v->fs && v->fs->getdentry) {
    ptr = v->fs->getdentry (dir, v, size);
  }
  return ptr;
}

struct dentry *vfs_lookup_dentry(struct vnode *v,const char *name) { 
  if(v && v->fs && v->fs->lookup_dentry) 
    return v->fs->lookup_dentry(v,name);
  return NULL;
}

void list_mp ()
{
  register struct list_head *head = &mntlist;
  struct list_head *traverse ;
  printf ("%-17s%-17s%-17s\n","Path","Filesystem","Device");
  list_for_each(traverse,head) { 
    struct mountpoint *tmp = list_entry(traverse,struct mountpoint,next);
    printf ("%-17s%-17s%-17s\n", tmp->path, tmp->fs->name, tmp->dev->name);
  }
}

static int mount_error_callback(struct dentry *dentry,const char *name) { 
  if(! dentry ) { 
    printf("mount error: Invalid path %s...\n",name);
  } else {
    printf("mount error: %s Not a Directory...\n",name);
  }
  return 0;
}

static int mount_verify_callback(struct dentry *dentry) { 
  return ( dentry->attr & ATTR_DIRECTORY ) != 0;
}

int vfs_mount (const char *pathname, const char *fsname,int dev)
{
  struct filesystem *fs ;
  struct device *device ;
  struct mountpoint *mp = NULL;
  struct vnode *v = NULL,*parent = NULL;
  char output[MAX_PATH+1];
  int err = -1;
  if (!pathname || !fsname ) {
    printf ("vfs_mount(): pass proper arguments\n");
    goto out;
  }
  strncpy(output,pathname,sizeof(output)-1);

  /*parse the path for any mount errors: Get the VNODE for the mount point
   */
  if(strcmp(pathname,FILE_SEPARATOR_STR) && ( ( parse_path(pathname,output,&v,mount_error_callback,mount_verify_callback) < 0 ) || 
					      !v
					      )
     ) {
    goto out;
  }
  /*support for multiple mountpoints having the same path:*/
  if( ( mp = ismp(output) ) ) {
     parent = mp->v->parent;
     v = NULL;
  }

  if(v)
    parent = v->parent; /*get the parent as we are going to rip this guy off*/
  
  fs = get_fs (fsname);
  if (!fs) {
    printf ("vfs_mount(): unregistered FS requested\n");
    goto out;
  }
  device = get_dev(dev);
  if (!device) {
    printf ("vfs_mount(): unknown device requested\n");
    goto out;
  }
  mp =  slab_cache_alloc (mnt_cache, 0);
  if(! mp ) {
    printf("Unable to allocate memory for mountpoint %s in %s\n",output,__FUNCTION__);
    goto out;
  }
  memset(mp,0x0,sizeof(struct mountpoint));
  strncpy (mp->path,output,sizeof(mp->path)-1);
  mp->fs = fs;
  mp->dev = device;
  register_mountpoint(mp);
  if(! mp->fs->read_superblock ) {
    printf("No superblock method defined for FS %s\n",fsname);
    goto out_free;
  }
  mp->fs->read_superblock(mp);

  if(v) { 
    /*we free this vnode and its children as we are going to create a new one*/
    vnode_delete_all(v);
  }

  /*create a VNODE entry for the mountpoint now*/
  {
    char *ptr ;
    const char *name = output;
    struct stat ss;
    ptr = strrchr(output,FILE_SEPARATOR);
    if(ptr) 
      name = (const char *)(ptr + 1);
    ss.ino  = mp->fs->fssb.root_inode;
    ss.size = mp->fs->fssb.root_size;
    v = vnode_create(*name ? name : FILE_SEPARATOR_STR,output,(const struct stat *)&ss,parent,(const struct filesystem *)fs,(const struct device *)device);
    if(! v ) { 
      printf("Unable to create vnode in Function %s...\n",__FUNCTION__);
      goto out_free;
    }
    v->ismounted = 1; /*mark that its a mountpoint.*/
    vnode_initialise(v);
  }
  mp->v = v;
  err = 0;
 out:
  return err;
 out_free:
  slab_cache_free(mnt_cache,(void*)mp);
  goto out;
}

/*Unmount a mounted point*/
int vfs_umount(const char *pathname) { 
  struct mountpoint *mp;
  int err = -1;
  /*cannot unmount when inside a mounted working directory:
  */
  if( strstr(current->files_struct.cwd,pathname) ) {
    printf("umount %s: Device is busy...\n",pathname);
    goto out;
  }
  mp = ismp(pathname);
  if(! mp ) {
    printf("umount %s: not found...\n",pathname);
    goto out;
  }
  if(! mp->v ) {
    printf("umount %s: vnode not found...\n",pathname);
    goto out;
  }
  if(! mp->v->ismounted) {
    printf("umount %s: vnode  %s doesnt seem to be a mounted vnode...\n",pathname,mp->v->name.abs_name);
    goto out;
  }
  /*Now rip off all the vnodes for this guy,including this guy*/
  vnode_delete_all(mp->v);
  /*unregister the mountpoint*/
  unregister_mountpoint(mp);
  slab_cache_free(mnt_cache,(void*)mp);
  err = 0;
 out:
  return err;
}
  
/*initialise the various caches*/
 int __init vfs_init_caches(void) { 
  file_cache  = slab_cache_create("file_cache",sizeof(struct file),SLAB_HWCACHE_ALIGN,0);
  mnt_cache   = slab_cache_create("mnt_cache",sizeof(struct mountpoint),SLAB_HWCACHE_ALIGN,0);
  vnode_cache = slab_cache_create("vnode_cache",sizeof(struct vnode),SLAB_HWCACHE_ALIGN,0);
  filesystem_cache = slab_cache_create("filesystem_cache",sizeof(struct filesystem),SLAB_HWCACHE_ALIGN,0);
  dev_cache = slab_cache_create("dev_cache",sizeof(struct device),SLAB_HWCACHE_ALIGN,0);
  if( !vnode_cache || !file_cache || !mnt_cache || !filesystem_cache || !dev_cache) { 
    panic("Unable to initialise vfs caches...\n");
  }
  return 0;
}

static int __init vfs_fs_init(void) {
  vfs_fatinit();
  return 0;
}

int __init vfs_init(void) { 
  init_hash(VFS_HASH_SIZE);
  vfs_fs_init();
  return 0;
}



