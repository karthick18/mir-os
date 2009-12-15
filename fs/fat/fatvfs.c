#include <mir/kernel.h>
#include <mir/sched.h>
#include <mir/ctype.h>
#include <mir/vfs/fs.h>
#include <mir/blkdev.h>
#include <asm/string.h>
#include <mir/fs/fat.h>
#include <mir/init.h>

unsigned char allfat[TOT_FSEC];
unsigned int fat_secinmem = 0; /* the FAT sectors in memory */

char *attr_read (unsigned char attr) {
  static char buf[128];

  if (attr&ATTR_READ_ONLY)
    strcpy (buf, "r/o, ");
  if (attr&ATTR_HIDDEN)
    strcpy (buf, "hidden, ");
  if (attr&ATTR_SYSTEM)
    strcpy(buf, "system, ");
  if (attr&ATTR_DIRECTORY)
    strcpy (buf, "dir, ");
  if (attr&ATTR_ARCHIVE)
    strcpy (buf, "archive, ");

  buf[strlen(buf)-2] = '\0';
  return buf;
}

static int read_fatsb (struct mountpoint *mp) {
  bpb *bootparam;
  int err = -1;
  int major;
  if(! mp->dev) {
    goto out;
  }
  major = MAJOR(mp->dev->dev);
  bootparam = (void *)slab_alloc (sizeof(bpb), 0);
  do_data_transfer(mp->dev,0,blkdev[major].block_size,(unsigned char *)bootparam, READ);
  printf ("FAT12: SuperBlock read\n");
  mp->fs->fssb.blocksize = bootparam->sec_clu * bootparam->bytes_sec;
  mp->fs->fssb.root_inode = bootparam->no_fats*bootparam->fat_sectors + bootparam->rsvd_sec;
  mp->fs->fssb.root_size = bootparam->no_root;
  mp->fs->fssb.spc_fs = bootparam;
  err = 0;
 out:
  return err;
}

static unsigned short ext_fat (int index, unsigned char *buf) { /* extract a fat entry from the buffer */
  unsigned short entry;
  int off = ((index *3)/2) ;
  if (index%2==1) {
    entry = *((unsigned short *) &buf[off]);
    entry >>= 4;
  } else {
    entry = *((unsigned short *) &buf[off]);
    entry &= 0x0fff;
  }
  return (entry);
}

static unsigned short getfat (unsigned short cfat, struct device *dev) {
  int sec;
  sec = cfat / 341; /* 341 = 512/1.5 */

  if (fat_secinmem & (1 << sec))  /* the fat sector is already in memory */
    return ext_fat (cfat, allfat);

  /* else get the sector */
  do_data_transfer(dev,1+sec,SECTOR_SIZE,(unsigned char *)allfat + SECTOR_SIZE*sec,READ);
  /* register it in bitmap */
  fat_secinmem |= (1 << sec);

  return ext_fat (cfat, allfat);
}

static char *convname (const char *name) { /*convert the fat12 filename to lower and remove characters..etc */
  static char buf[13]= {'\0'};
  int f;
  memset (buf, 0x0, sizeof(buf) );
  for (f = 0; f < 8; f++) {
    if (name[f]==' ') break;
    buf[f] = tolower(name[f]) ;
  }
  if (name[8] ==' ')
    return (buf);
  //printf (" f = %d ", f);
  buf[f] = '.';

  if (name[8]!= ' ') buf[f+1] = tolower( name[8] );
  else buf[f+1] = name[8];
  if (name[9]!= ' ') buf[f+2] = tolower(name[9]);
  else  buf[f+2] = name[9];
  if (name[10]!= ' ') buf[f+3] = tolower(name[10]);
  else buf[f+3] = name[10];

  return (buf);


}

static int vfat_namestat (struct vnode *pnode, const char *name, struct stat *ss, int inroot, struct mountpoint *cmp) {
  unsigned char buf[SECTOR_SIZE];
  char *fname; 
  int i;
  dir_entry *dir;
  int dirinode;

  dirinode = pnode->ss.ino;

  if (inroot) {
    while (1) {
      do_data_transfer(cmp->dev,dirinode,SECTOR_SIZE,buf,READ);
      for (i = 0; i < 16; i++) {
	dir = (dir_entry *) (buf + i*32);
	if (dir->attr == 0) return -1; /* file not found */
	/*skip invalid attributes: 1 is valid, rest are powers of 2*/
	if(dir->attr != 1 && (dir->attr & 1 ) )
	  continue;
	fname = convname (dir->name);
	if (!strcmp (fname, name)) { /* found the filename */
	  ss->ino = dir->cluster;
	  ss->size = dir->size;
	  return 0;
	}
      }	
      dirinode++;
    }

  }
  else {

    while (1) {
      do_data_transfer(cmp->dev,31+dirinode,SECTOR_SIZE,buf,READ);
      for (i = 0; i < 16; i++) {
	dir = (dir_entry *) (buf + i*32);
	if (dir->attr == 0) return -1; /* file not found */
	fname = convname (dir->name);
	if (!strcmp (fname, name)) { /* found the filename */
	  ss->ino = dir->cluster;
	  ss->size = dir->size;
	  return 0;
	}
      }	
      dirinode = getfat (dirinode, cmp->dev);
    }
  }
}

static int vfat_read (struct file *fp, char *buf, int len) {

  if(! fp || !fp->v || !fp->v->dev ) {
    len = -1;
    goto out;
  }

  if(fp->offset >= fp->size) {
    len = 0;
    goto out;
  }

  len = MIN(len,SECTOR_SIZE);

  if(fp->offset + len > fp->size ) 
    len = fp->size - fp->offset;

  if(!len)
    goto out;

  len = do_data_transfer(fp->v->dev,31+fp->curr_inode,len,buf,READ);
  
  fp->offset += len;

  if(fp->offset < fp->size ) {
    fp->curr_inode = getfat (fp->curr_inode, fp->v->dev);
    fp->total_ino ++ ;
  }

 out:
  return len;
}

/*look for directory dir in the vnode 
 */

static struct dentry *vfat_lookup_dentry(struct vnode *v, const char *name)
{
  dir_entry *dp;
  static struct dentry dentry;
  struct dentry *ret = NULL;
  int i;
  char buffer[MAX_PATH+1];
  struct vnode *vnode;
  char buf[SECTOR_SIZE];
  char *fname;
  int block;
  memset(&dentry,0x0,sizeof(dentry));
  if( ! v || !v->dev) {
    goto out;
  }
  block = v->ss.ino;
  if(!v->ismounted) 
    block += 31;
  do_data_transfer(v->dev,block, SECTOR_SIZE,(unsigned char *)buf,READ);
  for (i = 0; i < 16; i++) {
    dp = (dir_entry *) (buf + i*32);
    if (dp->attr == 0) continue; 
    if(dp->attr != 1 && (dp->attr & 1 ) ) {
      /*invalid attribute*/
      continue;
    }
    if (dp->name[0] == 0x00 || dp->name[0] == 0x05) continue;
    fname = convname (dp->name);
    if( !strcmp(fname,name) ) {
      strncpy(dentry.name,fname,sizeof(dentry.name)-1);
      dentry.attr = dp->attr;
      sprintf(buffer,"%s%s%s",v->name.abs_name,v->name.abs_name[-1] == FILE_SEPARATOR ? "" : FILE_SEPARATOR_STR,fname);
      if( (vnode = vnode_find(buffer) ) ) {
	dentry.v = vnode;
      } else { 
	printf("Error in finding vnode entry for %s...\n",buffer);
      }
      ret = &dentry;
      break;
    }
  }
 out:
  return ret;
}

static struct dentry ** vfat_getdentry (const char *dir, struct vnode *v, int *size)
{
  static struct dentry *ptr[256]; 
  dir_entry *dp;
  int  i, sz=0;
  char buf[SECTOR_SIZE], *fname, tbuf[SECTOR_SIZE/2];
  int block;
  *size = 0;
  if(! v || !v->dev ) 
    return NULL;
  block = v->ss.ino;
  if(!v->ismounted)
    block += 31;
  do_data_transfer(v->dev,block,SECTOR_SIZE,(unsigned char *)buf,READ);

  for (i = 0; i < 16; i++) {
    dp = (dir_entry *) (buf + i*32);
    if (dp->attr == 0) continue; 
    if(dp->attr != 1 && (dp->attr & 1 ) ) {
      /*invalid attr*/
      continue;
    }
    if (dp->name[0] == 0x00 || dp->name[0] == 0x05) continue;
    fname = convname (dp->name);
    if (!(fname[0] >= 'a' && fname[0] <= 'z'&& !(dp->attr&ATTR_SYSTEM))) continue;


    ptr[sz] = (void *) slab_alloc (sizeof(struct dentry), 0);
    strncpy (ptr[sz]->name, fname,sizeof(ptr[sz]->name) -1 );

    sprintf (tbuf, "%s/%s", dir,fname);
    ptr[sz]->v = (struct vnode *) vnode_find(tbuf);
    ptr[sz]->attr = dp->attr;
    sz++;
  }	
  *size = sz;
  return ptr;
}

int __init vfs_fatinit (void)
{
  struct filesystem *fatfs;
  fatfs = slab_cache_alloc(filesystem_cache,0);
  if(! fatfs) {
    panic("Unable to alloc fatfs from filesystem_cache...\n");
  }
  memset(fatfs,0x0,sizeof(struct filesystem));
  strncpy (fatfs->name, FAT12_NAME,sizeof(fatfs->name) - 1 );
  fatfs->read_superblock = read_fatsb;
  fatfs->namestat = vfat_namestat;
  fatfs->read = vfat_read;
  fatfs->getdentry = vfat_getdentry;
  fatfs->lookup_dentry = vfat_lookup_dentry;
  register_fs (fatfs);
  return (0);
}
