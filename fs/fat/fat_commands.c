/*   MIR-OS : VFS commands for FAT : ls,cat,cd,etc..
     ;    Copyright (C) 2003-2004
     ;                  A.R.Karthick
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
#include <mir/ctype.h>
#include <mir/sched.h>
#include <asm/string.h>
#include <mir/vfs/fs.h>
#include <mir/fs/fat.h>
#include <mir/floppy.h>
#include <mir/blkdev.h>

static int cd_verify_callback(struct dentry *dentry) { 
  return (dentry->attr & ATTR_DIRECTORY) != 0;
}

static int cd_error_callback(struct dentry *dentry,const char *name) { 
  if( !dentry ) { 
    printf("cannot chdir to %s...\n",name);
  } else {
    printf("cannot chdir to %s.Not a directory...\n",name);
  }
  return 0;
}

static int ls_error_callback(struct dentry *dentry, const char *name) { 
  if( !dentry ) { 
    printf("ls error: Invalid path %s\n",name);
  }else {
    printf("ls error: Invalid path %s.Not a directory...\n",name);
  }
  return 0;
}

static int ls_verify_callback(struct dentry *dentry) {
  return ( dentry->attr & ATTR_DIRECTORY ) != 0;
}

int parse_path(const char *path,char *result,struct vnode **ret_vnode,int (*error_callback) (struct dentry *dentry,const char *name), int (*verify_callback)(struct dentry *dentry ) ) { 
  char *args = (char *)path;
  struct vnode *vnode ; /*pointer to the current vnode*/
  struct mountpoint *mp ;
  char output[MAX_PATH+1];
  int err = -1;
  if( ret_vnode )
    *ret_vnode = NULL;

  if( !args ) {
    goto out;
  }

  while (isspace(*args) ) ++args;
  if (! *args) {
    goto out;
  }
  vnode = vnode_lookup(current->files_struct.cwd);
  if(! vnode) { 
    printf("Invalid CURRENT WORKING DIRECTORY %s. vnode not found in Function %s...\n",current->files_struct.cwd,__FUNCTION__);
    goto out;
  }
  trim_file_separator(args,output,sizeof(output)-1);
  {
    char *ptr = output;
    char *ptr1 ;
    char path[MAX_PATH+1], abs_path[MAX_PATH+1]="";
    struct vnode *traverse = vnode;
    struct dentry *dentry ;
    if(output[0] == FILE_SEPARATOR) {
      /*look for a mount point in the path and set the current to that of the mount point*/
      if( ( mp = get_mount(output) ) ) { 
	ptr += strlen(mp->path);
	traverse = mp->v;
      } else {
	error_callback(NULL,args);
	goto out;
      }
    }

    if(strcmp(traverse->name.abs_name,FILE_SEPARATOR_STR) ) 
      strncpy(abs_path,traverse->name.abs_name,sizeof(abs_path)-1);

    while ( ptr && *ptr ) { 
      ptr1 = strchr(ptr,FILE_SEPARATOR);
      if(ptr1)  
	*ptr1++ = 0;
      if(*ptr) { 
	if (!strcmp(ptr,".") ) {
	  /*ignore this guy or current directory references*/
	} else if (!strcmp(ptr,"..") ) { 
	  if(traverse->parent) { 
	    traverse = traverse->parent; /*shift to the parent vnode*/
	    /*overwrite the absolute path*/
	    strncpy(abs_path,traverse->name.abs_name,sizeof(abs_path)-1);
	  } else {
	    error_callback(NULL,args);
	    goto out;
	  }
	} else {
	  sprintf(path,"%s%s",FILE_SEPARATOR_STR,ptr);
	  strncat(abs_path,path,sizeof(abs_path) - strlen(abs_path) - 1);
	  /*no we try to get a dentry corresponding to the current vnode*/
	  dentry = vfs_lookup_dentry(traverse,ptr);
	  if(!dentry) {
	    error_callback(NULL,args);
	    goto out;
	  }
	  if(! verify_callback(dentry) ) {
	    error_callback(dentry,args);
	    goto out;
	  }
	  traverse = vnode_find(abs_path); /*we now get the vnode*/
	  if(! traverse) { 
	    printf("cannot chdir to %s.vnode not found...\n",args);
	    goto out;
	  }
	}
      }
      ptr = ptr1;
    }
    strncpy(result,traverse->name.abs_name,MAX_PATH);
    if(ret_vnode)
      *ret_vnode = traverse;
  }
  err = 0;
 out:
  return err;
}

void cd(int i ) { 
  char *args = (char *)i;
  if(args){
    parse_path(args,current->files_struct.cwd,NULL,cd_error_callback,cd_verify_callback);
  }
}

void pwd(int i) { 
  printf("%s\n",current->files_struct.cwd);
}

void fstab(int i) {
  list_mp();
}

/*Mount a FS*/
void mount(int i) { 
  char *args = ( char *)i;
  struct device *device ;
  int nr = 0;
  char output[MAX_PATH+1],*ptr,*str;
  char pathname[MAX_PATH+1];
  int dev_number=-1;
  /*max 3 args*/
  char *argv[3] = { NULL,FAT12_NAME,NULL };

  if(! args ) {
  out_usage:
        printf("usage: mount mountpoint FS DEVICE\n");
	goto out;
  }
  /*get the args: mountpoint,FS,devicename*/
  strncpy(output,args,sizeof(output)-1);
  ptr = output;
  while(ptr && *ptr) { 
    /*skip leading spaces*/
    while(*ptr && isspace(*ptr) ) ++ptr;
    if( ! *ptr )
      break;

    if( nr >= 3 ) {
      goto out_usage;
    }
    str = strchr(ptr,' ');
    if(str) {
      *str++ = 0;
    }
    /*store the arg*/
    argv[nr++] = ptr; 
    ptr = str;
  }
  if(!argv[0] ) { 
    printf("No mount point specified...\n");
    goto out_usage;
  }
  if(! argv[2] ) { 
    dev_number = MKDEV(FDC_BLKDEV_MAJOR_NR,0);
  } else {
    device = get_dev_by_name(argv[2]);
    if(!device) { 
      printf("Device %s not found...\n",argv[2]);
      goto out;
    }
    dev_number = device->dev;
  }
  trim_file_separator(argv[0],pathname,sizeof(pathname)-1);
  /*Special check for MOUNTS of / via shell.Not allowed*/
  if(!strcmp(pathname,FILE_SEPARATOR_STR) ) {
    printf("cannot mount already mounted \"%s\", again as it cannot be unmounted...\n",FILE_SEPARATOR_STR);
    goto out;
  }
  vfs_mount(pathname,argv[1],dev_number);
 out:;
}

/*Unmount a device*/
void umount(int i) { 
  char *args = (char *)i;
  char output[MAX_PATH+1];
  if(! args) { 
    goto out_usage;
  }
  while(*args && isspace(*args) ) ++args;
  if(! *args) {
  out_usage:
    printf("usage: umount mountpoint\n");
    goto out;
  }
  trim_file_separator(args,output,sizeof(output)-1);
  vfs_umount(output);
 out:;
}
	
static __inline__ void display_dentries(struct dentry **ptr,int size) {
  register int j;  
  printf ("%-20s%-20s%-20s\n","Name", "Type", "Size");
  for (j = 0; j < size; j++)
    printf ("%-20s%-20s%-10d\n",ptr[j]->name, attr_read(ptr[j]->attr), ptr[j]->v ? ptr[j]->v->ss.size : 0);
}

void list_dir (int i) {
  char *args = (char *) i;
  char input[MAX_PATH+1],output[MAX_PATH+1];
  struct dentry **ptr;
  int size = 0; 
  struct vnode *vnode = vnode_lookup(current->files_struct.cwd);
  char *str;
  int relative = 0;
  if(! vnode) { 
    printf("current working directory vnode not found..\n");
    goto out;
  }
  strncpy(output,vnode->name.abs_name,sizeof(output)-1);
  if( ! args ) { 
    goto get_dentry;
  }
  while( isspace (*args) ) ++args;
  if(! *args) { 
    goto get_dentry;
  } 
  trim_file_separator(args,input,sizeof(input)-1);
  strncpy(output,input,sizeof(output)-1);
  /*find the pointer to the last SEPARATOR*/
  str = strrchr(input,FILE_SEPARATOR);
  if(str) { 
    char  result[MAX_PATH+1];
    if( strcmp(str+1,".") &&
	strcmp(str+1,"..")
	) {
      relative = 1;
      *str++ = 0;
    }
    if(input[0]) { 
      if (parse_path(input,result,&vnode,ls_error_callback,ls_verify_callback) ) {
	goto out;
      }
      if(relative) 
	strncpy(output,str,sizeof(output)-1);
      else {
	strncpy(output,result,sizeof(output)-1);
      }
    } else { 
      struct mountpoint *mp;
      str = output;
      if ( ! (mp = get_mount(output) ) ) {
	printf("ls error: Invalid path %s\n",args);
	goto out;
      }
      vnode = mp->v;
      str += strlen(mp->path); 
      if( *str ) {
	strncpy(output,str,sizeof(output)-1);
      } else {
	relative = 0;
      }
    }
  } else if (! strcmp(input,".") ) {
    strncpy(output,vnode->name.abs_name,sizeof(output)-1);
  } else if (! strcmp(input,"..") ) { 
    vnode = vnode->parent;
    if(vnode) {
      strncpy(output,vnode->name.abs_name,sizeof(output)-1);
    } else {
      printf("ls error: Invalid path %s\n",args);
      goto out;
    }
  } else {
    relative = 1;
  }
  if(! relative) {
    /*get the dentry straight for non relative paths*/
  get_dentry:
    ptr = vfs_getdentry (output, &size);
  } else { 
    /*first try getting the dentry*/
    struct dentry *dentry = vfs_lookup_dentry(vnode,output);
    if( !dentry) {
      printf("ls error: Invalid relative path %s\n", output);
      goto out;
    }
    if (! (dentry->attr & ATTR_DIRECTORY ) ) { 
      /*display the single entry:*/
      ptr = &dentry;
      size = 1;
    } else { 
      /*its a directory: fetch the Dentry*/
      sprintf(input,"%s%s%s",vnode->name.abs_name,FILE_SEPARATOR_STR,output);
      strncpy(output,input,sizeof(output)-1);
      ptr = vfs_getdentry(output,&size);
    }
  }
  if(!ptr) {
    printf("ls error: vfs_getdentry error for path %s\n",output);
    goto out;
  }
  display_dentries(ptr,size);
 out:;
}

static int do_cat (const char *path) {
  int n,err = -1;
  int fd;
  char buf[SECTOR_SIZE+1], buffer[MAX_PATH+1];
  if( ! path) { 
    goto out;
  }
  strncpy(buffer,path,sizeof(buffer)-1);
  fd = vfs_open (buffer,READ_MODE);
  if(fd < 0 ) {
    printf ("open error:\n");
    goto out;
  }
  while ( (n = vfs_read(fd,buf,sizeof(buf)-1 ) ) > 0 ) {
    buf[n] = 0;
    printf("%s",buf);
  }
  vfs_close(fd);
  err = 0;
 out:
  return err;
}

void cat (int i) {
  char *args = (char *) i;
  if( ! args ) { 
    goto no_arg;
  }
  while( isspace (*args) ) ++args;
  if(! *args) {
  no_arg: 
    printf("no argument specified for cat...\n");
    goto out;
  }
  do_cat(args);
 out:;
}

