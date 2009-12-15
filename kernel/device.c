/*   MIR-OS : VFS routines for devices
;    Copyright (C) 2003-2004
;     A.R.Karthick
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
#include <asm/string.h>
#include <mir/vfs/fs.h>
#include <mir/buffer_head.h>
#include <mir/blkdev.h>
#include <mir/init.h>

static DECLARE_LIST_HEAD(devlist);

static const char *const device_map[] = { 
  "Block",
  "Character",
  NULL,
};

/*All the default device_operations get here*/
static struct device_operations *device_operations[] = { 
  &block_device_operations,
  NULL,
};

static __inline__ struct device_operations *get_device_operations(device_type type,struct device_operations *ptr) {
  if(ptr) 
    goto out;
  if(type >= MAX_DEVICES ) 
    goto out;
  ptr = device_operations[type];
 out:
  return ptr;
}

static __inline__ void link_dev(struct device *dev)
{
  list_add_tail(&dev->next,&devlist);
}

void list_dev()
{
  struct list_head *head = &devlist;
  register struct list_head *traverse ;
  printf ("%-17s%-17s%-17s\n","Name","Type","Blksize");
  list_for_each(traverse,head) { 
    struct device *dev = list_entry(traverse,struct device,next);
    printf ("%-17s%-17s%-17d\n", dev->name,dev->type >= MAX_DEVICES ? "Unknown" : device_map[dev->type] ,dev->blksize);
  }
}

void devices(int arg) {
  list_dev();
}

struct device * get_dev_by_name (const char *name)
{
  register struct list_head *traverse;
  struct list_head *head = &devlist;
  list_for_each(traverse,head) {
    struct device *dev = list_entry(traverse,struct device,next);
    if(!strcmp(name,dev->name) ) 
      return dev;
  }
  return NULL;
}      

struct device *get_dev(int dev) { 
  register struct list_head *traverse;
  struct list_head *head = &devlist;
  list_for_each(traverse,head) {
    struct device *device = list_entry(traverse,struct device,next);
    if(device->dev == dev )
      return device;
  }
  return NULL;
}

int register_device(const char *name,int dev,device_type type,int blksize,struct device_operations *device_operations) {
  int err = -1;
  struct device *device ;
  device = slab_cache_alloc(dev_cache,0);
  if(!device) {
    printf("Unable to allocate dev structure in Function %s\n",__FUNCTION__);
    goto out;
  }
  strncpy (device->name,name,sizeof(device->name)-1);
  device->blksize = blksize;
  device->dev = dev;
  device->type = type;
  device->device_operations = get_device_operations(type,device_operations);
  link_dev(device);
  err = 0;
 out:
  return err;
}

/*Data transfer to the device:
  This would/should be used by applications through the VFS:
*/
int do_data_transfer(struct device *device,int block,int size,unsigned char *data,int cmd ) {  
  int err = -1;
  if(! device) {
    goto out;
  }
  if( !device->device_operations) {
    printf("No device operations specified for device (%s).Failed in function %s\n",device->name,__FUNCTION__);
    goto out;
  }
  switch(cmd) { 
 
  case READ:
    if(!device->device_operations->read_data) {
      printf("No read_data function specified for device (%s).Failed in function %s\n",device->name,__FUNCTION__);
      goto out;
    }
    err = device->device_operations->read_data(device->dev,block,size,data);
    break;

  case WRITE:
    if(!device->device_operations->write_data) {
      printf("No write_data function specified for device (%s).Failed in function %s\n",device->name,__FUNCTION__);
      goto out;
    }
    err = device->device_operations->write_data(device->dev,block,size,data);
    break;
  default:
    printf("Invalid Command %d specified in %s\n",cmd,__FUNCTION__);
  }
 out:
  return err;
}

int data_transfer_name(const char *dev_name,int block,int size,unsigned char *data,int cmd) { 
  struct device *device;
  int err = -1;
  device = get_dev_by_name(dev_name);
  if(! device ) {
    printf("device %s not found in Function %s,Line %d\n",dev_name,__FUNCTION__,__LINE__);
    goto out;
  }
  err = do_data_transfer(device,block,size,data,cmd);
 out:
  return err;
}

int data_transfer(int dev,int block,int size,unsigned char *data,int cmd) { 
  struct device *device;
  int err = -1;
  device = get_dev(dev);
  if(! device ) { 
    printf("device %d not found in function %s,line %d\n",dev,__FUNCTION__,__LINE__);
    goto out;
  }
  err = do_data_transfer(device,block,size,data,cmd);
 out:
  return err;
}




