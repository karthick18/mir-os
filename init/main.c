/*
  ;
  ;    MIR-OS Kernel 0.0.2
  ;       The Kernel, very very messy, lots of old (unused code), please anyone
  ;       try to arrange this file into smaller parts
  ;
  ;    Copyright (C) 2003 Pratap <pratap@starch.tk>
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
  ;    A.R.Karthick         : Massive rearrangement of this file
  A.R.Karthick         : Moved relevant pieces of this file to relevant dirs.                           and changed the code appropriately.
  A.R.Karthick         : Made this file compile with -O2 compiler option.
  A.R.Karthick         : Changed the struct da_stuff to use thread_struct to                            make sense out of pushad and popad calls on ret fro                            m intr.
  A.R.Karthick         : Corrected the __asm__ calls in vesa_scroll.
  Made them call the interfaces in string.h
  A.R.Karthick         : Various other random hacks and additions.
  A.R.Karthick         : VFS cache lookups + cd,ls,cat command implementation
*/

#include <mir/kernel.h>
#include <mir/types.h>
#include <asm/string.h>
#include <mir/sched.h>
#include <mir/timer.h>
#include <mir/mir_utils.h>
#include <asm/bitops.h>
#include <asm/irq.h>
#include <asm/processor.h>
#include <asm/text.h>
#include <mir/terminal.h>
#include <asm/vesa.h>
#include <asm/text.h>
#include <asm/io.h>
#include <asm/page.h>
#include <asm/pgtable.h>
#include <asm/e820.h>
#include <asm/multiboot.h>
#include <asm/reboot.h>
#include <mir/init.h>
#include <mir/sem.h>
#include <asm/time.h>
#include <mir/buffer_head.h>
#include <mir/ide.h>
#include <mir/blkdev.h>
#include <mir/elf.h>
#include <mir/module.h>
#include <mir/vfs/fs.h>
#include <mir/fs/fat.h>
#include <mir/floppy.h>

/*Karthick:Changed the argument to args*/
typedef void CmdFunc(int); /* args*/

/* statics */
static int timer_alreadyrunning =0;
static void printtime(int);
static void printdate(int);
static void print_proc (int);
static void parse_input (char *);
static void timer_start(int);
static void initcmds();
#ifdef VESA
static void task1();
#endif
static void task2();

#define CMDS 0x100

struct {
  char *command;
  CmdFunc *handler; /* function pointer */
  char *comment; /* for 'help' command! */
} cmds[CMDS];


int __init  kmain (mb_info_t *mb) {
  initcmds();
  terminal_vt_init();
#ifdef VESA
  palset (255, 63, 63, 63);
#endif 
  /*set current terminal to console 0*/
  set_current_terminal(0);
  current_term->has_shell = 1; /*for term 0*/
  clrscr();
  printf ("\t\t\t\t\t\t\tMIR OS %s in 32bit!       \n", VER_MIR);
  cpuid_c();
  set_gdt_user(__USER_CS_INDEX,USER_CS_TYPE);
  set_gdt_user(__USER_DS_INDEX,USER_DS_TYPE);
  printf("GDT initialised\n");
  traps_init();
  init_irq();
  init_mm(mb);
  slab_cache_init();
  printf("Initialised memory:\n");
  sti();
  timer_init();
  kbd_init();
  sb_init();
  ipc_init();
  printf("Initialised IPC:\n");
  buffer_head_init();
  get_ksyms(mb);
  modules_init(mb); 
  vfs_init_caches();
  fd_init();
  init_ide(0,NULL);
  vfs_init();
  vfs_mount (FILE_SEPARATOR_STR,FAT12_NAME,MKDEV(FDC_BLKDEV_MAJOR_NR,0) );
  ++timer_alreadyrunning;
  /*Initialise the scheduler: This would setup the timer:
    create the init task,and the shell
  */
  sched_init();
  return (0);
}
#ifdef VESA
static void task1() {
  int w=0, fd, n;
  unsigned char blk[512*109];
  if ( (fd = vfs_open ("logo.sys", 0)) < 0) {
    printf("Unable to open file:%s\n","logo.sys");
    goto out;
  }
  while (1) {
    n = vfs_read (fd, blk+512*w, 512);
    if (n==0)
      break;
    w++;
  }
  bmpread (300, 5, blk);
  out : ;
}
#endif

static void task2() {
  while (1)  {
    printf ("pvp\n");
  }
}

static void free_mem(int unused) { 
  display_mem_stats();
}

static void test_mem(int unused) {
  test_mm();
}

static void slab_caches_stats(int unused) {
  display_slab_caches_stats();
}
static void test_cache(int unused) {
  test_slab_cache();
}
static void clear_screen(int unused) {
  clrscr();
}

static void e820_display(int unused) {
  display_e820();
}

static void jiffy(int unused) { 
  printf("jiffies=%ld\n",jiffies);
}

static void test_timer(int unused) { 
  setup_timers();
}

static void get_child(int arg) { 
  const char *argv = (const char *)arg;
  int pid = 0;
  const char *name = "init_task";
  if(argv && *argv) {
    /*Now try converting it into an integer to see if it works*/
    pid = atoi(argv);
    if(errno < 0 ) {
      /*didnt work:try name instead:*/
      errno = 0;
      pid =   0;
      while(isspace(*argv) ) ++argv;
      if(*argv) 
	name = argv;
    } 
  }
  get_children(pid,name);

}
/*Kill a task : by pid or name*/
static void kill(int arg) {
  const char *argv = (const char *)arg;
  const char *name = NULL;
  int pid =0;
  if(argv && *argv) { 
    while(isspace(*argv) ) ++argv;
    if(*argv) { 
      pid = atoi(argv); 
      if(errno < 0 ) { 
	/*Didnt work,try name instead*/
        errno = 0;
        pid = 0;
        name = argv;
      }
    }
  }
  kill_task(pid,name);
}

static void spawn_task(int arg) { 
  const char *name = "test_task";
  const char *argv = (const char *)arg;
  if(argv && *argv) { 
    while(isspace(*argv) ) ++argv;
    if(*argv)
      name = argv;
  }
  if(create_task_name((unsigned long)&spawn_test_task,name,NULL) < 0) {
    printf("Error in spawning task (%s)\n",name);
  }
} 

static void do_sleep(int args) { 
  int secs = 0;
  const char *arg = (const char*)args;
  if(! arg) {  
    goto no_arg;
  }
  while(isspace(*arg++) );
  --arg;
  if(! *arg) {
  no_arg:
    printf("Sleep without argument:\n");
    goto out;
  }
  secs = atoi(arg);
  if(errno < 0 ) {
    errno = 0;
    printf("Invalid format for secs\n");
    goto out;
  }
  (void)sleep(secs);
 out:
  return;
}

struct test_sample { 
  int f1; 
  int f2;
};
#define NR 6
static void test_vmalloc(int arg) { 
  int nr = NR;
  unsigned long size = ( (1 << 20UL) );
  static unsigned long loc[NR];
  while(nr--) {
    unsigned char *location = vmalloc(size,0);
    if(location) {
      printf("Vmalloc test successfull:\n"); 
      loc[nr] = (unsigned long)location;
      do
	{
	  struct test_sample*ptr = (struct test_sample *)loc[nr];
	  ptr->f1 = 1 , ptr->f2 = 2;
	  printf("test samples (%d,%d)\n",ptr->f1,ptr->f2);
	  memset(location,0x0,size );
	} while(0);
    }
  }
  nr = NR;
  while(nr--) {
    if(loc[nr]) {
      printf("Freeing location(%#08lx)\n",loc[nr]);  
      vfree((void*)loc[nr]);
    }
    loc[nr] = 0;
  }
}

static void reboot(int arg) { 
  mir_reboot();
}

static void show_irq_stat(int arg) { 
  char *argv = (char *)arg;
  int irq = -1;
  if(argv && *argv) {
    while (isspace(*argv) ) ++argv;
    if(*argv) {
      irq = atoi(argv);
      if(errno < 0 ) {
	/*invalid arg*/
	errno = 0;
	printf("Invalid arg\n");
	return ;
      }
    }
  }
  display_irq_stat(irq);
}

static void ksyms(int arg) { 
  show_ksyms ();
}

static void ide_cdrom_eject(int arg) { 
  char *argv = (char *)arg;
  int load;
  if(argv && *argv) {
    while (isspace(*argv) ) ++argv;
    if(*argv) {
      load = atoi(argv);
      if(errno < 0 ) {
	/*invalid arg*/
	errno = 0;
	printf("Invalid arg\n");
	goto invalid;
	return ;
      }
      if(cdrom_drives[0] >= 0 ) 
	ide_atapi_eject(cdrom_drives[0],load);
      else
	printf("No CDROMs detected...\n");
    } else goto invalid;
  } else {
  invalid:
    printf("cdrom_eject [1-load 0-unload]\n");
  }
}

#define BLOCK_LIMIT(n) ( (n) << 1 )
#define WRITE_BLOCK_LIMIT BLOCK_LIMIT(5000) 

static void test_ide(int arg,int dev,int cmd) { 
  int block;
  char *argv = (char *)arg;
  int bytes;
  int major = MAJOR(dev);
  unsigned char data[BLOCK_SIZE] ;

  if(argv && *argv ) {
    while (isspace (*argv) ) ++argv;
    if(*argv) {
      block = atoi(argv);
      if(errno < 0 ) {
	errno = 0;
	printf("Invalid arg.\n");
	return ;
      }
      switch(cmd) { 
      case READ:
	bytes = data_transfer(dev,block << 1,sizeof(data),data,READ);
	if(! bytes ) {
	  printf("Error in bread for blkdev dev %d,major %d..\n",dev,major);
	}
	else {
	  printf("read success for sector %d,dev %d,major %d..\n",block << 1,dev,major);
	}
	break;
      case WRITE:
	{
	  if( (block << 1 ) < WRITE_BLOCK_LIMIT ) {
	    printf("Invalid sector #%d, less than limit %d\n",block << 1,WRITE_BLOCK_LIMIT);
	  }else {
	    memset_words(data,0xAA55,sizeof(data) >> 1);
	    data_transfer(dev,block << 1,sizeof(data),data,WRITE);
	    /*verify the read*/
	    data_transfer(dev,block << 1,sizeof(data),data,READ);
	    if( ((unsigned short *)data)[510] == 0xAA55 &&
		((unsigned short *)data)[511] == 0xAA55
		) 
	      printf("write success for sector %d ,dev %d,major %d..\n",block << 1 ,dev,major);
	    else 
	      printf("Error in write for sector %d,dev %d,major %d..\n",block << 1,dev,major);
	  }
	}
	break;
      default:
	printf("Invalid cmd..\n");
      }
    }
  }
}

static void test_ide_read(int arg ) { 
  test_ide(arg,MKDEV(IDE_BLKDEV_MAJOR_NR,1),READ);
}

static void cdrom_eject(int arg) { 
  ide_cdrom_eject(arg);
}

static void test_ide_write(int arg ) { 
  test_ide(arg, MKDEV(IDE_BLKDEV_MAJOR_NR,1),WRITE );
}

static void test_cdrom_read(int arg) { 
  test_ide(arg,MKDEV(IDE_CDROM_MAJOR_NR,cdrom_drives[0]),READ);
}

static void cdrom_features(int arg) { 
  if(cdrom_drives[0] >= 0 )
  ide_atapi_mode_sense(cdrom_drives[0]);
  else {
    printf("No CDROMs detected...\n");
  }
}
extern int test_file_allocation(int);
static void test_file(int arg) { 
  char *str = (char*)arg;
  int nr ;
  if( !str) {
    goto out;
  }
  while(isspace(*str) ) ++str;
  if(! *str) {
    goto out;
  }
  nr = atoi(str);
  if(errno < 0 ) {
    errno = 0;
    printf("Invalid arg: %s\n",str);
    goto out;
  }
  test_file_allocation(nr);
 out:;
}

static void initcmds() {

  cmds[0].command = "date";
  cmds[0].handler = printdate;
  cmds[0].comment = "Print today's date";

  cmds[1].command = "help";
  cmds[1].handler = printhelp;
  cmds[1].comment = "This command!";

  cmds[2].command = "timer_start";
  cmds[2].handler = timer_start;
  cmds[2].comment = "Initialize the PIT!";

  cmds[3].command = "time";
  cmds[3].handler = printtime;
  cmds[3].comment = "Print the time";

  cmds[4].command = "ps";
  cmds[4].handler = print_proc;
  cmds[4].comment = "Print current processes";

  cmds[5].command = "halt";
  cmds[5].handler = halt;
  cmds[5].comment = "Halt the system";

  cmds[6].command = "ls";
  cmds[6].handler = list_dir;
  cmds[6].comment = "List directory contents";

  cmds[7].command = "cat";
  cmds[7].handler = cat;
  cmds[7].comment = "Output contents of a file (cat <filename>)";

  cmds[8].command = "free";
  cmds[8].handler = free_mem;
  cmds[8].comment= "Display Memory Info";

  cmds[9].command = "test_mem";
  cmds[9].handler = test_mem;
  cmds[9].comment = "Test MM";

  cmds[10].command = "slab_stats";
  cmds[10].handler =  slab_caches_stats;      
  cmds[10].comment = "Display Slab Cache Stats:";
     
  cmds[11].command = "test_slab_cache";
  cmds[11].handler =  test_cache;
  cmds[11].comment =  "Test slab cache";
          
  cmds[12].command =  "cls";
  cmds[12].handler =  terminal_clear;
  cmds[12].comment =  "Clears the screen";
   
  cmds[13].command = "e820";
  cmds[13].handler = e820_display;
  cmds[13].comment = "Display E820 biosmap";

  cmds[14].command = "jiffy";
  cmds[14].handler = jiffy;
  cmds[14].comment = "Display jiffies count";

  cmds[15].command = "test_timer";
  cmds[15].handler = test_timer;
  cmds[15].comment = "Test timers"; 

  cmds[16].command = "children";
  cmds[16].handler = get_child;
  cmds[16].comment = "Get childs of the task [by name or pid]";
     
  cmds[17].command = "spawn_test_task";
  cmds[17].handler = spawn_task;
  cmds[17].comment = "Simply spawn a predefined test task";

  cmds[18].command = "kill";
  cmds[18].handler = kill;
  cmds[18].comment = "Kill a task by pid or name";

 
  cmds[19].command = "lsmod";
  cmds[19].handler =  lsmod;
  cmds[19].comment = "List all the modules";

  cmds[20].command = "insmod"; 
  cmds[20].handler = insmod;
  cmds[20].comment = "Insert a module(<arg number>)";

  cmds[21].command = "rmmod";
  cmds[21].handler = rmmod;
  cmds[21].comment = "Remove a loaded module(<arg number>)";

  cmds[22].command = "test_vmalloc";
  cmds[22].handler = test_vmalloc;
  cmds[22].comment = "Test the vmalloc interface";

  cmds[23].command = "sleep";
  cmds[23].handler = do_sleep;
  cmds[23].comment = "Sleep for [n] secs";

  cmds[24].command = "reboot"; 
  cmds[24].handler = reboot;
  cmds[24].comment = "Reboot the system";

  cmds[25].command = "cpu_features";
  cmds[25].handler = process_cpu_features;
  cmds[25].comment = "Get CPU Feature Info";

  cmds[26].command = "cpu_type";
  cmds[26].handler = process_cpu_type;
  cmds[26].comment = "Get CPU Type Info";

  cmds[27].command = "cpu_cache";
  cmds[27].handler = process_cpu_cache;
  cmds[27].comment = "Get CPU Cache Info";

  cmds[28].command = "cpu_brand";
  cmds[28].handler = process_cpu_brand;
  cmds[28].comment = "Get CPU brand string";
 
  cmds[29].command = "irq_stat";
  cmds[29].handler = show_irq_stat;
  cmds[29].comment    = "Show IRQ stats/count";

  cmds[30].command = "test_ide_read";
  cmds[30].handler = test_ide_read;
  cmds[30].comment  = "Test IDE read";

  cmds[31].command = "test_ide_write";
  cmds[31].handler = test_ide_write;
  cmds[31].comment = "Test IDE write";

  cmds[32].command = "cdrom_eject";
  cmds[32].handler = cdrom_eject;
  cmds[32].comment = "Eject CDROM";

  cmds[33].command = "test_cdrom_read";
  cmds[33].handler = test_cdrom_read;
  cmds[33].comment = "Test CDROM Read";

  cmds[34].command = "ksyms";
  cmds[34].handler = ksyms;
  cmds[34].comment = "Show Kernel Symbol Table";

  cmds[35].command = "cd";
  cmds[35].handler = cd;
  cmds[35].comment = "Change working directory";

  cmds[36].command = "pwd";
  cmds[36].comment = "Current working directory";
  cmds[36].handler = pwd;

  cmds[37].command = "fstab";
  cmds[37].comment = "Mounted Devices";
  cmds[37].handler =  fstab;
 
  cmds[38].command = "devices";
  cmds[38].comment = "System Devices";
  cmds[38].handler =  devices;
 
  cmds[39].command = "mount";
  cmds[39].comment = "mount a device:eg[mount path [fsname] [devicename] ]";
  cmds[39].handler = mount;

  cmds[40].command = "umount";
  cmds[40].comment = "umount a device:eg[umount mountpoint]";
  cmds[40].handler = umount;

  cmds[41].command = "cdrom_features";
  cmds[41].comment = "Get CDROM features";
  cmds[41].handler = cdrom_features;

  cmds[42].command = "test_file";
  cmds[42].comment = "Test File Management";
  cmds[42].handler = test_file;

  cmds[43].command = NULL;
  cmds[43].handler = NULL;
  cmds[43].comment = NULL;
 
}

void halt (int num) {
  printf ("MIR Halted..!!");
  cli();
  triple_fault();
}

static void print_proc (int num) {
  display_processes();
}

static void timer_start (int ind) {
  if (timer_alreadyrunning) {
    printf ("PIT already initialized\n");
    return;
  }
  timer_alreadyrunning ++;
  timer_init();
}


static int cmos_read (unsigned char addr) {
  outb (0x70, 0x80|addr);
  return (BCD2BIN(inb(0x71)));
}

void printhelp (int index) {
  int i;
  printf ("---------- MIR OS ------------\n");
  printf ("Currently available (built-in) commands - \n");
  for (i =0; cmds[i].command!=NULL; i++) {
    printf ("%s - %s\n", cmds[i].command, cmds[i].comment);
  }
  printf ("------------------------------\n");
  //sendbyte (0x0f);
  //for (i=0; i<7; i++) 
  //	printf ("REGISTER DUMP %d - %d\n", i, getbyte());

}


static void printdate (int index) {
  printf ("DATE: %02d/%02d/%02d\n", cmos_read(7), cmos_read(8), 2000+cmos_read(9) );
}

static void printtime(int arg) {
  printf ("%02d:%02d:%02d\n", cmos_read(4), cmos_read(2), cmos_read(0));

}

static void parse_input (char *input) {
  int i=0;
  char *ptr = (char *)0;

  ptr = strchr (input, ' ');
  if(ptr)
    *ptr++ = 0;

  for (i=0; cmds[i].command != NULL; i++) {
    if (strcmp (input, cmds[i].command)==0) {
      cmds[i].handler((int)ptr);
      return;
    }
  }
  if (strlen(input) == 0) 
    return;
  printf("%s: Bad command or file name\n", input);

}

void shell (void *arg) {
  /* lame "shell" :)*/
  char inp[PAGE_SIZE];
  printf ("\n");
  while (1) {
    printf ("[mir: %s] ", current->files_struct.cwd);
    getstr(inp);
    parse_input (inp);
  }
  exit_task();
}




