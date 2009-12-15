/*   MIR-OS : FILE Management routines
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
#include <mir/sched.h>
#include <mir/vfs/fs.h>
#include <asm/page.h>
#include <asm/bitops.h>

#define KMALLOC_LIMIT ( 8 * 1024 )

static void link_file(struct files_struct *files_struct,struct file *fptr) { 
  list_add_tail(&fptr->file_head,&files_struct->file_head);
}

static void unlink_file(struct file *fptr) { 
  list_del(&fptr->file_head);
}

void destroy_file(struct file *fptr) { 
  if(fptr) { 
    slab_cache_free(file_cache,fptr);
  }
}

static struct file **alloc_file(int size) { 
  struct file **ptr;
  if( size > KMALLOC_LIMIT ) { 
    ptr = (struct file **)vmalloc(size,0);
  } else {
    ptr = (struct file **)slab_alloc(size,0);
  }
  return ptr;
}

static void free_file(struct file **ptr,int size) { 
  if(size > KMALLOC_LIMIT) {
    vfree((void*)ptr);
  } else {
    slab_free((void*)ptr);
  }
}    

int init_files_struct(struct files_struct *files_struct) { 
  list_head_init(&files_struct->file_head);
  files_struct->free_fd = 3;
  files_struct->curr_fd = 3;
  files_struct->max_fd  = MAX_FDS;
  files_struct->file_p   = files_struct->file;
  files_struct->fd_map_p = files_struct->fd_map;
  files_struct->fd_size  = sizeof(files_struct->fd_map) * 8;
  memset(files_struct->fd_map,0xff,sizeof(files_struct->fd_map) );
  memset(files_struct->file_p,0x0,sizeof(files_struct->file) );
  
  return 0;
}

/*grow the file bitmap set*/
static int grow_file_map(struct files_struct *files_struct) { 
  int fd_size = files_struct->fd_size;
  int new_size = fd_size + 32;
  int err = -1;
  unsigned char *map;

  if(new_size > MAX_OPEN_FDS) {
    goto out;
  }
  map = slab_alloc(new_size/8,0);
  if(! map ) {
    goto out;
  }
  memcpy(map,files_struct->fd_map_p,fd_size/8);
  memset(map + fd_size/8 ,0xff,4);
  if(files_struct->fd_map_p != files_struct->fd_map ) { 
    slab_free((void*)files_struct->fd_map_p);
  }
  files_struct->fd_map_p = map;
  err = 0;
 out:
  return err;
}

/*grow the fd_set*/

static int grow_fds(struct files_struct *files_struct) { 
  int fd_size = files_struct->fd_size;
  int new_size;
  int err = -1;
  struct file **ptr ;
  new_size = fd_size + 32;
  if(new_size > MAX_OPEN_FDS) { 
    goto out;
  }
  ptr =  alloc_file(new_size * sizeof(struct file*) );
  if(! ptr ) {
    printf("Unable to alloc file table..\n");
    goto out;
  }
  /*copy the old fd table*/
  memcpy(ptr,files_struct->file_p,fd_size * sizeof(struct file *));
  memset(ptr + fd_size, 0x0, 32 * sizeof(struct file *) );
  /*grow the bitmap*/
  if(grow_file_map(files_struct) < 0 ) { 
    goto out_free;
  }
  if(files_struct->file_p != files_struct->file )
    free_file(files_struct->file_p,fd_size);
  files_struct->file_p = ptr;
  files_struct->fd_size = new_size;
  err = 0;
  goto out;
 out_free:
  free_file(ptr,new_size);
 out:
  return err;
}

/*get a free FD */
static int get_free_fd(struct files_struct *files_struct) { 
  int fd ;
  fd = find_first_zero_bit((void*)files_struct->fd_map_p,files_struct->fd_size);
  if(fd >= files_struct->fd_size) {
    fd  = -1;
  }
  /*nothing in the bitmap area*/
  if(fd < 0 ) { 
    if(files_struct->free_fd >= MAX_OPEN_FDS) { 
      printf("No free FDs...\n");
      goto out;
    }
    if( files_struct->free_fd && ( !(files_struct->free_fd % 32 ) ) ) { 
      if(grow_fds(files_struct)  < 0 ) {
	fd = -1;
	goto out;
      }
    }
    fd = files_struct->free_fd++;
  } else {
    unsigned char *map;
    int nr = fd % 32;
    map = files_struct->fd_map_p + (fd / 32 * 4);
    set_bit(nr,map);
  }
  files_struct->curr_fd = fd;
#ifdef DEBUG
  printf("giving fd %d\n",fd);
#endif
 out:
  return fd;
}

/*get a free fd*/    
int get_file(struct files_struct *files_struct) { 
  int fd ;
  int err = -1;
  struct file *fptr;
  if(! files_struct || !files_struct->file_p ) {
    goto out;
  }
  fd = get_free_fd(files_struct);
  if(fd < 0 ) {
    goto out;
  }
  if(files_struct->file_p[fd] ) { 
    printf("%s :There seems to be a file pointer already existing.Freeing it.\n",__FUNCTION__);
    slab_cache_free(file_cache,files_struct->file_p[fd]);
    files_struct->file_p[fd] = NULL;
  }

  fptr = slab_cache_alloc(file_cache,0);
  if(! fptr) { 
    goto out;
  }
  memset(fptr,0x0,sizeof(struct file));
  files_struct->file_p[fd] = fptr;
  link_file(files_struct,fptr);
  err = fd;
 out:
  return fd;
}

int release_file(struct files_struct *files_struct,int fd) { 
  int err = -1;
  unsigned char *map;
  int nr ;
  struct file *fptr;
  if(! files_struct) {
    goto out;
  }
#ifdef DEBUG
  printf("releasing fd %d\n",fd);
#endif
  if(fd < 0  || fd >= files_struct->free_fd ) { 
    goto out;
  }
  if(! files_struct->file_p[fd] ) { 
    /*seems to be already free*/
    goto out;
  }
  map = files_struct->fd_map_p + (fd/32 * 4);
  nr = fd % 32;
  if(! test_and_clear_bit( nr, map )  ) { 
    printf("FD seems to be already closed.\n");
  }
  fptr = files_struct->file_p[fd];
  files_struct->file_p[fd] = NULL;
  unlink_file(fptr);
  slab_cache_free(file_cache,fptr);
  err = 0;
 out:
  return err;
}

int release_files_struct(struct files_struct *files_struct) { 
  int last_fd;
  int err = -1;
  int i;
  if(! files_struct) {
    goto out;
  }
  last_fd = files_struct->free_fd;
  for(i = 0; i < last_fd ; ++i ) {
    if(files_struct->file_p[i] ) {
      destroy_file(files_struct->file_p[i]);
      files_struct->file_p[i] = NULL;
    }
  }
  if(files_struct->file_p != files_struct->file)
    free_file(files_struct->file_p,files_struct->fd_size);
  files_struct->file_p = NULL;
  if(files_struct->fd_map_p != files_struct->fd_map) 
    slab_free((void*)files_struct->fd_map_p);
  
  files_struct->fd_map_p = NULL;
  list_head_init(&files_struct->file_head);
  err = 0;
 out:
  return err;
}
  
int test_file_allocation(int nr) { 
  int *fds;
  int i;
  if(! nr ) {
    goto out;
  }
  fds = slab_alloc(nr *sizeof(int),0);
  if(! fds ) {
    printf("error in allcating fds..\n");
    goto out;
  }
  memset(fds,0x0,nr * sizeof(int));
  for(i = 0; i < nr ; ++i) { 
    fds[i] = vfs_open("/boot/mir.elf",READ_MODE);
    printf("fd allocated = %d\n",fds[i]);
  }
  for(i = 0;i < nr;++i) {
    if(fds[i] > 0 ) {
      vfs_close(fds[i]);
    }
  }
  slab_free((void*)fds);
 out:
  return 0;
}
    
