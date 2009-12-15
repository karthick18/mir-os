/*Author : A.R.Karthick for MIR OS:
  Task management definitions:
*/

#ifndef _SCHED_H
#ifdef __KERNEL__
#define _SCHED_H

#include <mir/kernel.h>
#include <mir/types.h>
#include <mir/param.h>
#include <mir/timer.h>
#include <asm/thread.h>
#include <asm/processor.h>
#include <asm/page.h>
#include <mir/data_str/dlist.h>
#include <mir/data_str/slist.h>
#include <mir/data_str/myqueue.h>
#include <mir/data_str/myhash.h>
#include <mir/data_str/rbtree.h>
#include <mir/vfs/file.h>

#define USER_MODE(regs)  ( (regs)->cs & 3 )

#define REAL_TIME_TASK(task)  ( (task)->sched_policy == SCHED_FIFO || (task)->sched_policy == SCHED_RR )

/*This exit status would force a scheduler reap*/

#define REAP_ME                     0x100

#define RT_PRIORITY 100

#define MAX_NICE  (-20)
#define MIN_NICE  (19)

#define DEFAULT_PRIORITY  ( MIN_NICE/2 )

#define REAP_TIMEOUT      (100) /*for reaping tasks memory on exits*/

/*Normal processes with a priority of 0*/

#define SCHED_OTHER       0x1000

/*Real time,cpu bound processes*/
#define SCHED_FIFO        0x2000
#define SCHED_RR          0x3000

#define VM_SHARED         0x00010000
#define VM_PRIVATE        0x00020000

/*Define the various possible task states*/

#define TASK_RUNNING              0x0
#define TASK_SLEEPING             0x1
#define TASK_STOPPED              0x2
#define TASK_INTERRUPTIBLE        0x3
#define TASK_UNINTERRUPTIBLE      0x4
#define TASK_ZOMBIE               0x5

#include <mir/wait.h>

#define MAX_NAMELEN          0x10
#define MAX_PID              ( (1<<15) - 1)
/*3GB for paging stuff*/
#define TASK_SIZE            0xC0000000
/*This is the thread size that is governed by the thread stack: a.k.a Memory:
 */
#define THREAD_SIZE          (PAGE_SIZE  << 2 )

/*Total threads governed by the memory. approximately 1/10th of total memory:
 */

#define MAX_THREADS     ( ( max_pfn )/(THREAD_SIZE/PAGE_SIZE)/10 )

/*The task struct structure.
  Keep the mostly accessed fields in this structure,
  cacheline aligned. Note: For persons adding fields in the task
  struct: Be careful with the cacheline alignment:No tradeoffs with performance. If you change these fields ,please note to change the offsets in exceptions.S

*/

struct sem_queue;
struct term;
struct task_struct{       /*field offsets : Keep track of cacheline alignments*/
  byte status;            /*0*/
  struct list_head next;  /*1*/

  /*Okay: now some posix specific chains*/
  /*First the original parent:*/

  struct task_struct *o_pptr;  /*9*/

  /*Next the parent :) */     

  struct task_struct *p_pptr;  /*13*/

  /*What !! I hear you say!! Can the guy become a bastard:)
    Okay : Those are required for GDB or debuggers.When the debugger
    runs,the task which is debugged becomes a forced son of the debugger,
    and the debugger becomes the parent(_not_ original parent)
    for the time being- Rest ptrace.c*/ 
   
  struct task_struct *o_sptr; /* 17 : tasks older sibling  */
  struct task_struct *y_sptr; /* 21 : tasks younger sibling*/  

  struct task_struct *c_ptr ; /*25 : tasks youngest child*/

  /*Ok!! go for some padding for cachline alignment:*/
  unsigned char padding[3]; /*29*/
  
  /*The runqueue for the scheduler*/

  struct list_head runqueue;/*32*/
  int counter; /*40*/   
  int priority; /*44*/
  struct mm_struct *mm;/*48 : mm_struct per process*/

 /*There should be a files struct,but that should happen once VFS is through*/    
  int pid; /*52 :pid of the process*/
  int ppid;/*56 : uid of the process*/
  unsigned long start_time; /*60 : start time of the process:*/
  unsigned long stime;      /*64 Time spent in kernel space*/
  unsigned long utime;      /*68 Time spent in user space*/
  unsigned long cstime;    /* 72: Childs kernel space time*/
  unsigned long cutime;   /*  76: Childs user space time*/

  /*PID HASH LINKAGE*/

  struct hash_struct pid_hash;/*80*/

  /*The wait queue of parents waiting for their children to EXIT/STOP */
  wait_queue_head_t wait_queue_head;/*88*/

  unsigned long timeout; /*96:the task can be scheduled after this timeout*/

  struct thread_struct thread_struct; /*100*/

  unsigned long sched_policy; /*144*/

  char name[MAX_NAMELEN+1];   /*148*/ 

  int exit_status;            /*165*/

  int used_math;             /*169*/
   
  int need_resched;          /*173*/ 

  /*The below is needed for pid hash lookups*/
  int (*hash_search) (struct task_struct *,struct task_struct *);/*177*/

  /*Stack size is now gone :*/

  struct slist undo_process; /*ipc-UNDO*/

  struct term *terminal; /*the terminal for the process*/

  struct files_struct files_struct;

 }__attribute__((packed));

/*Save the FPU state in the tasks thread_struct*/
static __inline__ void save_fpu(struct task_struct *task) { 
   __asm__ __volatile__("fnsave %0\n" : :"m" (task->thread_struct.fsave) );
 }
/*Restore FPU state from task struct*/

static __inline__ void restore_fpu(struct task_struct *task) {
  __asm__ __volatile__("frstor %0\n" : : "m"(task->thread_struct.fsave) );
 }

static __inline__ void init_fpu(void) { 
  __asm__ __volatile__ ("fninit" :: );
  }

#define INIT_THREAD_STRUCT { \
eip:0,\
esp:0,\
esp0:0,\
debugreg:{ [0 ... 7] = 0 },\
}
/*Initialise the fields for INIT_TASK*/
#define INIT_TASK(init_task) {\
pid: 1, \
ppid:1,\
status:TASK_RUNNING,\
next : INIT_LIST_HEAD(init_task.next),\
o_pptr: NULL,\
p_pptr: NULL,\
y_sptr: NULL,\
o_sptr: NULL,\
c_ptr : NULL,\
runqueue: { NULL,NULL},\
priority:DEFAULT_PRIORITY,\
counter:0,\
name: "init_task",\
mm: NULL,\
wait_queue_head: __INIT_WAIT_QUEUE_HEAD(init_task.wait_queue_head),\
timeout:0,\
stime: 0,\
utime: 0,\
cstime: 0,\
cutime: 0,\
start_time: 0,\
pid_hash : { NULL,NULL},\
sched_policy: SCHED_OTHER,\
thread_struct: INIT_THREAD_STRUCT,\
exit_status: 0,\
used_math: 0,\
undo_process: INIT_SLIST_HEAD(init_task.undo_process),\
terminal : NULL,\
need_resched:0,\
}

/*Set up the links for the tasks:  
  Please setup the parent before calling this guy:
 */
#define SET_LINKS(task) ({\
 if((task)->o_pptr) {\
  (task)->y_sptr = (task)->o_sptr = (task)->c_ptr = NULL;\
 if( ( (task)->o_sptr = (task)->o_pptr->c_ptr) ) {\
    (task)->o_pptr->c_ptr->y_sptr = (task);\
    }\
  (task)->o_pptr->c_ptr = (task);\
}\
 0;\
})

#define REMOVE_LINKS(task) ({\
 if((task)->o_pptr) {\
 if( (task)->o_pptr->c_ptr == (task) ) { \
  (task)->o_pptr->c_ptr = (task)->o_sptr;\
}\
if( (task)->o_sptr ) {\
  (task)->o_sptr->y_sptr = (task)->y_sptr;\
}\
if( (task)->y_sptr ) {\
  (task)->y_sptr->o_sptr = (task)->o_sptr;\
}\
(task)->o_sptr = (task)->y_sptr = (task)->c_ptr = NULL;\
}\
0;\
})


/*pgd pointer or the page directory pointer should be coming up soon*/

struct mm_struct { 
  struct list_head mm_list;  /*All the mm_structs should be getting inside this guy: Why ? Easy for page replacement */
  /*Per process vm_area_root*/
  struct slist vm_root; /*This is a single chain of vm_area_structs*/
  /*This is a chain of rbtrees for the VMAS*/
  struct rbtree_root vm_rbtree_root; 
  pgd_t *pgd;
  int total_vm; /*total vmarea consumed by the task*/
  int rss_size; /*total resident set size of the task: minus the swap*/
  int mm_users; /*nr. users of the mm_struct*/
  int nr_vmas; /* nr. of vmas in this mm_struct*/
  /*The below should be fetched from the elf/aout/bin header*/
  unsigned long text_start,text_end;
  unsigned long data_start,data_end;
  unsigned long bss_start,bss_end;
  unsigned long arg_start,arg_end;
};
  
/*This is the linkage to the vm_root and vm_rbtree_root in the mm_struct*/
struct vm_area_struct {
  struct mm_struct *mm; /*back point to the mm_struct owning this guy*/
  struct slist vm_root; 
  struct rbtree vm_rbtree_root; /*rbtree linkage*/
  /*inode mmap- shared/private mappings linkage to the inodes*/
  struct queue_struct i_mmap_shared; 
  struct queue_struct i_mmap;
  int vm_start,vm_end; 
  int vm_flags;
  int vm_offset;
  struct vm_operations_struct *vm_op; /*operations possible on a VMA*/
};

struct vm_operations_struct { 
  void (*open) (struct vm_area_struct *);
  void (*close)(struct vm_area_struct *);
  /*Page fault or no page method on the vma*/
  unsigned long (*nopage) (struct vm_area_struct *,unsigned long address,int unused);
};


extern volatile long jiffies;
extern struct task_struct *current; /*current task*/
extern struct task_struct *process_used_math;/*task used math*/
extern int current_task,max_tasks;
extern void c_sched();
extern void shell(void *arg);
extern volatile void schedule();
extern unsigned long schedule_timeout(unsigned long timeout);
extern unsigned long sleep(int);
extern int sleep_ticks(int);
extern int exit_task();
extern int create_task_name(unsigned long eip,const char *name,void *arg);
extern void display_processes();
extern int get_children(int,const char *);
extern void spawn_test_task(void *arg);
extern int kill_task(int pid,const char *name);
extern void wake_up_process(struct task_struct *);
extern void add_runqueue(struct task_struct *);
extern void del_runqueue(struct task_struct *);
extern void do_timer(struct thread_regs *);
extern void __schedule(void);
extern void delay(int msecs);
#endif
#endif
