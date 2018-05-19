/*   MIR-OS : Scheduler functions
;    Copyright (C) 2003 A.R.Karthick 
;    <a.r.karthick@gmail.com,a_r_karthic@rediffmail.com>
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
;    Completely rewritten from scratch: Check for comments inline
*/
#include <mir/slab.h>
#include <mir/sched.h>
#include <asm/pgtable.h>
#include <mir/timer.h>
#include <asm/system.h>
#include <asm/io.h>
#include <asm/string.h>
#include <asm/irq.h>
#include <asm/irq_stat.h>
#include <asm/setup.h>
#include <mir/init.h>
#include <mir/sem.h>
#include <asm/text.h>
#include <mir/terminal.h>

#define halt()         __asm__ __volatile__ ("hlt"::)

#define ON_RUNQUEUE(task)   ({ (task)->runqueue.next != NULL; })

#define PID_HASH_SIZE (PAGE_SIZE >> 2)

struct task_struct *current = NULL;
struct task_struct *process_used_math = NULL;

extern void ret_from_intr(void);
int max_tasks=0;
short last_pid = 0; /*This should become a bitmap in future*/

struct task_struct init_task = INIT_TASK(init_task);
struct tss_state init_tss_state;
volatile long jiffies = 0;

/*This is required to avoid races with multiple removals of a task*/

static struct task_struct *dying;

/*Index of a list of tasks*/
DECLARE_LIST_HEAD(task_list);
DECLARE_LIST_HEAD(mm_list);
static DECLARE_LIST_HEAD(runqueue);

struct init_stack { 
  unsigned long init_stack[PAGE_SIZE>>1];
}init_stack;

#define SETUP_INIT_STACK(addr)   do { \
__asm__ __volatile__("movl %0,%%esp\n\t" \
                     : :"p"( (addr) ) \
                    );\
}while(0)


#define pid_hash_fn(pid)  ({\
(  ( ( (pid) >> 8) ^ (pid) ) & (PID_HASH_SIZE - 1)  );\
})

slab_cache_t *mm_cache_p,*vm_area_cache_p;

static __inline__ void set_links(struct task_struct *task) { 
  unsigned int hash = pid_hash_fn(task->pid);
  list_add_tail(&task->next,&task_list);
  add_hash(&task->pid_hash,hash);
  SET_LINKS(task);
  ++max_tasks;
}

static __inline__ void remove_links(struct task_struct *task) { 
  list_del(&task->next);
  del_hash(&task->pid_hash);
  REMOVE_LINKS(task);
  /*update max tasks*/
  --max_tasks;
}

/*This should switch pgdir in cr3: */
static __inline__ void switch_mm(struct task_struct *next,struct task_struct *prev) { 
  struct mm_struct sub = {pgd: init_pgdir };
  struct task_struct *rep1 = NULL,*rep2 = NULL;
  if(!next->mm || !prev->mm) { 
    if(!next->mm) {
      rep1 = next;
      next->mm = &sub;
    }
    if(!prev->mm) {
      rep2 = prev;
      prev->mm = &sub;
    }
  }
  __switch_mm(prev->mm,next->mm);
  if(rep1) rep1->mm = NULL;
  if(rep2) rep2->mm = NULL;
}

/*This is now a simple routine to get the pid:
  Will be converted into a bitmap getting free bit nos.
  Scan cyclically for pids
*/

static int get_pid() { 
  short old_pid =++last_pid;
  if(old_pid < 0) {
    old_pid = 1; 
    last_pid = 1;
  }    
  /*check for pid overflows*/
 do {

 do{
    struct list_head *traverse = NULL;
    struct list_head *head = &task_list;
    list_for_each(traverse,head) { 
      struct task_struct *task = list_entry(traverse,struct task_struct,next);
      if(task->pid == last_pid) {
	/*Ok!! someones already using this pid:try another one:*/
        goto next;
      }
    }
  }while(0);
 return last_pid;

 next:
 /*Check for pid overflow*/
 if(++last_pid < 0) {
   last_pid = 1;
 }

 }while( last_pid != old_pid);
 return -1;
}

/*Add to the runqueue of the scheduler*/

void add_runqueue(struct task_struct *next) { 
  if(! ON_RUNQUEUE(next) ) {
    next->status = TASK_RUNNING;
    list_add_tail(&next->runqueue,&runqueue);
  }
}

void del_runqueue(struct task_struct *next) { 
  if(ON_RUNQUEUE(next) ) {
    list_del(&next->runqueue);
  }
}

void wake_up_process(struct task_struct *task) { 
  add_runqueue(task);
}

struct mm_struct *allocate_mm(void) { 
  struct mm_struct *mm = NULL;
  if(! mm_cache_p) 
    panic("MM cache pointer uninitialised:\n");

  if( ( mm = slab_cache_alloc(mm_cache_p,0) ) ) {
      memset(mm,0x0,sizeof(*mm) );
    }
  return mm;
}

void deallocate_mm(struct mm_struct *mm) { 
  if(! mm_cache_p) 
    panic("mm_cache_p UNINITIALISED:\n"); 
  
  slab_cache_free(mm_cache_p,mm);
}


struct vm_area_struct *allocate_vma(void) { 
  struct vm_area_struct *vma = NULL;
  if(! vm_area_cache_p) {
    panic("vm_area_cache_p UNINITIALISED:\n");
  }
  if( ( vma = slab_cache_alloc(vm_area_cache_p,0) ) ) {
    memset(vma,0x0,sizeof(*vma));
  }
  return vma;
}

void deallocate_vma(struct vm_area_struct *vma) { 
  if(! vm_area_cache_p) {
    panic("vm_area_cache_p UNINITIALISED:\n");
  }
  slab_cache_free(vm_area_cache_p,vma);
}

/*Unlink from the inode chain:
  This should be rewritten when inode structure is through:
  As of now,linking games -
*/
static __inline__ void unlink_vma_inode(struct vm_area_struct *vma) {
  if( (vma->vm_flags & VM_SHARED) ) {
    if(vma->i_mmap_shared.next) {
      vma->i_mmap_shared.next->pprev = vma->i_mmap_shared.pprev;
    }
    *(vma->i_mmap_shared.pprev) = vma->i_mmap_shared.next;
  }
  else { 
    if(vma->i_mmap.next) {
      vma->i_mmap.next->pprev = vma->i_mmap.pprev;
    }
    *(vma->i_mmap.pprev) = vma->i_mmap.next;
  }
}

static __inline__ void unlink_vma(struct vm_area_struct *vma) {
  /*Remove from single chain*/
  slist_del(&vma->vm_root,&vma->mm->vm_root);
  /*Remove from rbtree*/ 
  __rbtree_erase(&vma->mm->vm_rbtree_root,&vma->vm_rbtree_root);
  /*Remove from the inode chain*/
  unlink_vma_inode(vma);
  vma->mm = NULL;
}

/*Zap Page Range: 
  This will come up soon:
  BTW,this would remove the page table entries for this virtual address
  range and free the page:
  How ?? 
  Sample eg:
  do { 
    pgd_t *pgd; pmd_t *pmd; pte_t *pte;
    pgd = pgd_offset(mm,addr);
    if(!pgd_none(*pgd)) {
      pmd = pmd_offset(pgd,addr);
      if(! pmd_none(*pmd) ) {
         pte_t page;
         pte = pte_offset(pmd,addr);
         page = ptep_get_and_clear(pte);
         __free_page((unsigned long)page);
         }
       }
    }
  }while(0);
  Coming up in near future :)
*/
void zap_page_range(int start,int end ) { 
  do { ; } while(0);
 }
   
/*Destroy the vmas: This should be slightly modified to zap the page
 tables when pgd setup is through:
*/
void destroy_vmas(struct mm_struct *mm) {
  struct slist *head  = &mm->vm_root;
  while(! SLIST_EMPTY(head) ) {
    struct slist *next = head->next;
    struct vm_area_struct *vma = slist_entry(next,struct vm_area_struct,vm_root);
    unlink_vma(vma); /*unlink this vma from the relevant chains*/
    /*Now remove the page table entries:Will come up soon:*/
    zap_page_range(vma->vm_start,vma->vm_end);
    deallocate_vma(vma); /*free off the vma itself*/
  } 
}

/*Destroy the mm: */
void destroy_mm(struct task_struct *task) {
  if(task->mm) {
   int users = --task->mm->mm_users;
   if(users <= 0 ) { 
    struct mm_struct *mm = task->mm;
    task->mm = NULL;
    /*There aren any users of this mm apart from us.
      Looks good!! Let it Rip !!
    */
    destroy_vmas(mm); /*first destroy the vmas*/
    list_del(&mm->mm_list); /*unlink from the mm list*/
    deallocate_mm(mm);
   }
  } 
}

/*This should have files_struct ,etc. destruction in future*/
void destroy_task(struct task_struct *task) {
  /*First remove the links of this guy from its parent*/
  destroy_mm(task);
  release_files_struct(&task->files_struct);
  task->timeout = 0;
}

/*Ok!! Define the exit routine for a task:
  If a task has children,then let "init" adopt them,
  as a temporary solution to getting orphaned. 
  Thats what takes place normally.
  But be careful to check for the exit of a parent 
  with a mismatch with the original parent.
  That should signify an exit of some guy like the debugger. 
*/

int exit_task() { 
  int err = -1;
  if(current->pid == 1) {
    printf("Trying to kill init task!! Piss Off Arsehole:\n");
    goto out;  
  }
  else { 
    struct task_struct *task = current;
    err = 0;
    /*Lets first destroy the tasks various chains*/
    destroy_task(current);
    /*undo the semaphore value*/
    sem_exit();
    /*Now check for children and making them adopted by init.
      Check for original parent though:*/
    if(task->c_ptr && task->c_ptr->o_pptr == task)
      { 
        /*This implies,our children are almost becoming orphan,
          but just saved by init.INIT- The saviour!!
	*/
        while(task->c_ptr) { 
          struct task_struct *child = task->c_ptr; 
          struct task_struct *parent = &init_task;
          REMOVE_LINKS(child); /*unlink the child*/
          child->ppid = parent->pid;
          child->o_pptr = parent;
          if(child->p_pptr == task)
           child->p_pptr = parent;
          SET_LINKS(child); /*set up the new links for the child*/ 
	}
      }
    else if(task->c_ptr && task->c_ptr->o_pptr != task && 
            task->c_ptr->p_pptr == task)
      {
	/*This means that someone like the debugger is pissing off.
          Redo the changes made by the bastard and set up the parent and 
          the original parent correctly 
	*/
        struct task_struct *child;
        for(child = task->c_ptr; child ; child = child->o_sptr)
	  child->p_pptr = child->o_pptr;
      }
    /*Okay !! Looks good.Set the status to ZOMBIE and call the
      scheduler(not _always_ though) which should deliver the coup-de-grace in       this context:
      But hey !! Shouldnt this be just kicking off a signal to its parent,
      announcing its exit through SIGCHLD: But that should come after signal 
      handling is through :)
    */
    task->exit_status = 0;  
    if(task->status != TASK_ZOMBIE) {
      task->status = TASK_ZOMBIE;
      __schedule(); 
    }
    goto out;
  }
 out:
  return err ;
}
       
static __inline__ void link_vma_rbtree(struct rbtree_root *root,struct rbtree *node,struct rbtree *parent,struct rbtree **link) {
  __rbtree_insert(node,parent,link);
  rbtree_insert_colour(root,node);
}

/*Sorted insert into the rbtree*/
static void insert_rbtree(struct rbtree_root *root,struct rbtree *node) {
  struct rbtree **link = &root->root;
  struct rbtree *parent = NULL;
  struct vm_area_struct *reference = NULL;
  reference = rbtree_entry(node,struct vm_area_struct,vm_rbtree_root);
  while(*link) { 
    struct vm_area_struct *traverse = NULL;
    parent = *link; 
    traverse = rbtree_entry(*link,struct vm_area_struct,vm_rbtree_root);
    if(reference->vm_end < traverse->vm_end) {
      link = &parent->left;
    }
    else { 
      link = &parent->right;
    }
  }
  link_vma_rbtree(root,node,parent,link);
}
  
/*Link all the vmas into the rbtree*/
static void link_vmas_rbtree(struct mm_struct *mm) { 
  struct slist *traverse = NULL;
  struct slist *head = &mm->vm_root;
  struct rbtree *parent = NULL;
  struct rbtree **link = &mm->vm_rbtree_root.root;
  slist_for_each(traverse,head) { 
    struct vm_area_struct *vma = slist_entry(traverse,struct vm_area_struct,vm_root);
    link_vma_rbtree(&mm->vm_rbtree_root,&vma->vm_rbtree_root,parent,link);
    parent = *link;
    link = &parent->right;
  }
}

/*Link this guy also into the inodes mapping:
  Shared or private mappings for inode
*/
static void link_vma_inode(struct vm_area_struct *vm_area,struct vm_area_struct *vma) {
  if(! (vm_area->vm_flags & VM_SHARED) ) {
    /*Add it to the inodes private mapping:
      Now that we dont have the inodes private mapping,
      we play some linking games with the vma that we have in hand:
     */
    struct vm_area_struct *temp;
    vm_area->i_mmap.next = &vma->i_mmap;
    vm_area->i_mmap.pprev = vma->i_mmap.pprev;
    *vma->i_mmap.pprev = &vm_area->i_mmap;
    temp = queue_entry(vma->i_mmap.pprev, struct vm_area_struct, i_mmap.next);
    temp->i_mmap.next = &vm_area->i_mmap;
    vma->i_mmap.pprev = &vm_area->i_mmap.next;
  }
  else { 
    /*else add it to the private mapping*/
    struct vm_area_struct *temp;
    vm_area->i_mmap_shared.next = &vma->i_mmap_shared;
    vm_area->i_mmap_shared.pprev = vma->i_mmap_shared.pprev;
    *vma->i_mmap_shared.pprev = &vm_area->i_mmap_shared;
    temp = queue_entry(vma->i_mmap_shared.pprev, struct vm_area_struct,i_mmap_shared.next);
    temp->i_mmap_shared.next = &vm_area->i_mmap_shared;
    vma->i_mmap_shared.pprev  = &vm_area->i_mmap_shared.next;
  }
}

static __inline__ void link_vmas(struct vm_area_struct *vm_area,struct vm_area_struct *vma,struct mm_struct *mm) { 
     link_vma_inode(vm_area,vma);
     slist_add_tail(&vm_area->vm_root,&mm->vm_root);
     /*postpone rbtree linking after all the vmas are linked*/
}


/*Copy each vma from the parent to the child*/
static int copy_vma(struct mm_struct *mm,struct vm_area_struct *vma) {
  int err = -1;
  if(! vma) { 
    goto out;
  }else{ 
    struct vm_area_struct *vm_area = NULL;
    if( ! (vm_area = allocate_vma() ) ) { 
      printf("Unable to allocate vma:\n");
      goto out;
    }
    /*Copy the vma*/
    *vm_area = *vma;
    vm_area->mm = mm;
    link_vmas(vm_area,vma,mm);
  }
  err = 0;
 out:
  return err;
}
      
static int copy_vmarea(struct mm_struct *next,struct mm_struct *mm) {
  int err = -1; 
  if(! mm) {
    err =0;
    goto out;
  }else { 
    struct slist *head = &mm->vm_root;
    struct slist *traverse = NULL;
    slist_for_each(traverse,head) { 
      struct vm_area_struct *vma = slist_entry(traverse,struct vm_area_struct,vm_root);
      if(copy_vma(next,vma) < 0 ) {
        goto out_free;
      }       
    }
  }
  /*now link the vmas into the rbtree:*/
  link_vmas_rbtree(next);
  err = 0;
  goto out;
 out_free:
  {
    /*deallocate all the vmas allocated*/
    struct slist *head = &next->vm_root;
    while (! SLIST_EMPTY(head) ) { 
      struct slist *next = head->next;
      struct vm_area_struct *vma = slist_entry(next,struct vm_area_struct,vm_root);
      slist_del(next,head);
      deallocate_vma(vma);
    }
  }
 out:
  return err;
}
   
static int copy_mm(struct task_struct *next,struct task_struct *current) {
  struct mm_struct *mm = NULL;
  int err = -1;
  next->mm = NULL;
  mm = allocate_mm();
  if(! mm ) {
    printf("Unable to allocate mm_struct for task (%s)\n",next->name);
    goto out;
  }
  if(current->mm)
   *mm = *current->mm;

  mm->pgd = init_pgdir; /*use init pgdir as of now*/
  mm->mm_users = 1;
  slist_head_init(&mm->vm_root);
  list_head_init(&mm->mm_list);
  mm->vm_rbtree_root.root = NULL;
  /*Now copy the vm_area:*/
  if(copy_vmarea(mm,current->mm)  < 0) {  
    printf("Unable to copy vmarea for task(%s)\n",next->name);
    goto out_free;
  }
  next->mm = mm;
  err =0;  
  goto out;
 out_free:
  deallocate_mm(mm);
 out:
  return err;
}

/*Timeout schedule handler*/

static unsigned long timeout_handler(void *arg) { 
  struct task_struct *task = (struct task_struct *)arg;

#ifdef WAIT_QUEUE_TEST
  wait_queue_head_t *head = &task->wait_queue_head; 
  if(WAIT_QUEUE_ACTIVE(head) ) { 
    wake_up_interruptible(head);
  }
#else
  add_runqueue(task);
#endif
  return 0UL;
}

/*Timeout with schedule:
  We start a timer with the timeout value, 
  and schedule out the guy calling us.
  The guy would be woken up by the timer handler:
*/

unsigned long schedule_timeout(unsigned long timeout) { 
  if(timeout > MAX_TIMEOUT) {
    return 0UL;
  }
  else { 
    struct timer_list timer_list;
    init_timer(&timer_list);

#ifdef WAIT_QUEUE_TEST
    init_waitqueue_head(&current->wait_queue_head);
#endif

    if(setup_timer(&timer_list,timeout,(timer_handler)timeout_handler,(void*)current )  < 0) {
      printf("Unable to setup the timer (%s:%s) :\n",__FILE__,__FUNCTION__);
      return 0UL;
    }

#ifdef WAIT_QUEUE_TEST
    interruptible_sleep_on(&current->wait_queue_head);
#else
    current->status = TASK_INTERRUPTIBLE;
    __schedule();
#endif
    if(current->exit_status == REAP_ME) 
      {
	/*Task is just going to die:so delete the timer 
          if there is one:  
	*/
        if(timer_list.timer_head.next) {
	  del_timer(&timer_list);
        }
      }

  }
  return (timeout - jiffies > 0UL ) ? timeout - jiffies : 0UL;
}
/*Sleep for n secs*/
unsigned long sleep(int secs) {
  unsigned long timeout = jiffies + HZ*secs;
  return schedule_timeout(timeout); 
}

int sleep_ticks(int ticks) { 
  int err = -1;
  if(ticks < 0 || ticks  > INT_MAX) 
    goto out;
  current->status = TASK_INTERRUPTIBLE;
  current->timeout = jiffies + ticks;
  __schedule();
  current->status = TASK_RUNNING;
  err = 0;
 out:
  return err;
}

/*Wait queue wakeups and sleeps*/

/*Add to the waitqueue*/
void __add_waitqueue(wait_queue_t *wait_queue,wait_queue_head_t *wait_queue_head) { 
  del_runqueue(wait_queue->task);
}

void __interruptible_sleep(wait_queue_head_t *wait_queue_head,byte status) {
  DECLARE_WAIT_QUEUE(wait_queue,current);
  add_waitqueue(&wait_queue,wait_queue_head);
  current->status = status;
  __schedule(); 
  remove_waitqueue(&wait_queue);
  current->status = TASK_RUNNING;
  return ;
}

/*loop through the wait_queue and wakeup tasks: 
  Wake up tasks that are interruptible or wake_up all
  if uninterruptible
*/

void  __wake_up(wait_queue_head_t *wait_queue_head,byte status) { 
  struct list_head *head = &wait_queue_head->wait_queue_head;
  struct list_head *traverse = NULL;
  list_for_each(traverse,head) { 
    wait_queue_t *wait_queue = list_entry(traverse,wait_queue_t,wait_queue_head);
    if(status == TASK_UNINTERRUPTIBLE) {
      add_runqueue(wait_queue->task);
    } else if(status == wait_queue->task->status) {
      add_runqueue(wait_queue->task);
    } 
  }
}

/*Hash lookups of task pid*/
int hash_search(struct task_struct *task,struct task_struct *ref) { 
  return (ref->pid == task->pid);
}

 /*setup the args for the process.
   for kernel threads as this one, the return address would go
   in regs->esp, and argument regs->ss, as thats the way it would be popped.
 */
static __inline__ void setup_args(struct thread_regs *regs,unsigned long arg) {  regs->esp = (unsigned long)&exit_task;
  regs->ss  = arg;
}

static void setup_terminal(struct task_struct *task) { 
  task->terminal = current_term;
}
    
/*Setup the points for save and restore on context switches*/
void copy_process (struct task_struct *task,unsigned long eip,void *arg) {
struct thread_regs *regs = NULL;
#ifdef DEBUG
printf ("Spawning Task \"%s\" Pid(%d),PPid(%d) \n",task->name,task->pid,task->ppid);
#endif

 regs = (struct thread_regs *)( (unsigned long)task + THREAD_SIZE ) - 1;
 /*when fork is through,(almost similar to the one that is implemented here),
   we just copy all the registers from the parent to the child:
   But now,we dont _yet_ (almost like this except the syscall interface)have fork like the one above as a syscall.
 */
 memset(regs,0x0,sizeof(*regs));
 regs->eip = (long)eip;
 regs->cs  = __KERNEL_CS;
 regs->eflags = __KERNEL_EFLAGS;
 regs->ds  = __KERNEL_DS;
 regs->es  = __KERNEL_DS;
 task->thread_struct.esp = (unsigned long)regs;
 task->thread_struct.esp0 =(unsigned long) (regs + 1) ;
 setup_args(regs,(unsigned long)arg);
 task->thread_struct.eip  = (unsigned long)&ret_from_intr;
 memset(task->thread_struct.debugreg,0x0,sizeof(task->thread_struct.debugreg));

}

/*We should have proper error codes from now on*/

int create_task_name(unsigned long eip,const char *name,void *arg) {
  struct task_struct *next  = NULL; /*the next task*/
  int err = -1,len = 0; 

  if(max_tasks >= MAX_THREADS) {
    printf("Too many threads(%d) forked.Allowed (%lu):\n",max_tasks,MAX_THREADS);
    goto out;
  }

  /*Try allocating memory for the new task*/
  if( !( next = (struct task_struct *)__get_free_pages(get_order(THREAD_SIZE),0) ) ) { 
    printf("Unable to allocate memory for task %s\n",name);
    goto out;
  }
  /*Okay : Now do a structure copy*/
  *next = *current ; 
  next->hash_search = hash_search;
  /*Now initialise the various fields*/
  next->o_pptr = current;
  next->p_pptr = current;
  next->counter = 1; /*give it a single timeslice:This is dynamic*/
  next->timeout = 0;
  next->sched_policy = SCHED_OTHER;
  next->start_time = jiffies;
  next->pid_hash.next = NULL;
  next->pid_hash.pprev = NULL;
  list_head_init(&next->next);
  init_waitqueue_head(&next->wait_queue_head);
  next->runqueue.next = next->runqueue.prev = NULL;
  slist_head_init(&next->undo_process);  
  init_files_struct(&next->files_struct);
  strncpy(next->files_struct.cwd,current->files_struct.cwd,sizeof(next->files_struct.cwd)-1);
  strncpy(next->name,name,MAX_NAMELEN);
  len = strlen(name); 
  next->name[MIN(len,MAX_NAMELEN)] = 0; 
  next->ppid = current->pid; 
  if((next->pid = get_pid() ) < 0) {
    printf("Unable to get the pid:\n");
    goto out_free;
  }
  copy_process(next,eip,arg); 
  if(copy_mm(next,current) < 0) {
    printf("Error in copying the mm part of the process:\n");
    goto out_free;
  }
  /*setup the terminal*/
  setup_terminal(next);
  /*link this mm in the mm list*/
  list_add_tail(&next->mm->mm_list,&mm_list);

  /*Now if all goes well,set the links for this guy*/
  set_links(next);  
  /*Now :Add this guy to the scheduler run queue*/ 
  add_runqueue(next);
  
  err = 0;
  goto out;
 out_free:
  __free_pages((unsigned long)next,1);
 out:
  return err;
} 
static int g_v;
static void child1(void *unused) { 
  /*cause a divide_error*/
  int a = 4/0;
  (void)a;
  for(;;) sleep(3);
}

static void child2(void *unused) { 
  int id = -1; 
  int count = 0;
  if( (id = create_sem(SEM_KEY,1) ) < 0 ) {
    if((id = create_sem(SEM_KEY,0) ) < 0 ) {
#ifdef DEBUG
      printf("Error in creating sem:\n");
#endif
      goto out;
    }
  }
  while(count++ < 10 ) {
    p(id);     /*grab the sem:*/
    ++g_v;
#ifdef DEBUG 
    printf("value of global g_v for function %s=%d\n",__FUNCTION__,g_v); 
#endif
    v(id);
  }
 out:
  return ;
}
static void child3(void *unused) { 
  int id = -1;
  int count = 0;
  if( (id = create_sem(SEM_KEY,1) ) < 0 ) {
    if((id = create_sem(SEM_KEY,0) ) < 0 ) {
      printf("Error in creating sem:\n");
      goto out;
    }
  }
  while(count++ < 20) {
    p(id);
    ++g_v;
#ifdef DEBUG
    printf("value of global v for function %s=%d\n",__FUNCTION__,g_v);
#endif
    v(id);
  }
 out:
  return ;
}

/*Test task that runs for around 15 secs approx:*/
void spawn_test_task(void *unused) { 
    printf("Test task started and spawning 3 child processes:\n");
    /*Create 3 childs of this guy:*/
    create_task_name((unsigned long)&child1,"child1", NULL );
    create_task_name((unsigned long)&child2,"child2", NULL );  
    create_task_name((unsigned long)&child3,"child3", NULL );   
    sleep(15);
}

/*Lookup in the hash table for a task with a pid: 
  Fast lookup*/
struct task_struct *lookup_pid(int pid) { 
  struct task_struct ref = { pid: pid } , *ptr = &ref;
  struct task_struct *task = NULL;
  int hash = pid_hash_fn(pid);
  if(! (task = search_hash(hash,ptr,struct task_struct,pid_hash) ) ) {
    printf("Task with Pid (%d) not found in the hash table:\n",pid);
  }
  return task;
}  
/*Slow lookup:
  Lookup by task name:
*/
struct task_struct *lookup_name(const char *name) { 
  struct list_head *head = &task_list;
  struct list_head *traverse = NULL;
  struct task_struct *found = NULL; 
  list_for_each(traverse,head) {
    struct task_struct *task = list_entry(traverse,struct task_struct,next);
    if(!strcmp(task->name,name) ) { 
      found = task;
      break;
    }
  }
  return found;
}

/*
  Initialise the TSS for the INIT TASK :
  We have a single tss_state structure and
  we modify esp0 on context switches in arch-i386/kernel/system.c

*/

static __inline__ void initialise_init_tss(unsigned long stack) { 
  init_tss_state.esp0 = stack;
  init_tss_state.ss0_low  = __KERNEL_DS;
  init_tss_state.esp1 = (unsigned long)&init_tss_state + sizeof(init_tss_state);
  init_tss_state.ss1_low  = __KERNEL_CS;
  /*now setup the GDT for the TSS*/
  set_gdt_tss( __TSS_INDEX, (unsigned long)&init_tss_state,sizeof(init_tss_state) - 1);
  /*setup the task register with the TSS SELECTOR*/
  load_ltr(__TSS_SELECTOR);
}
  
/*Initialise the init task: 
  Setup the timer and let the next guy be scheduled
*/
static void initialise_init(void) { 
  unsigned long stack = (unsigned long)&init_stack.init_stack[PAGE_SIZE >> 1] - sizeof(unsigned long);
  disable_irq(0);
  current = &init_task; /*initialise the current pointer*/
  add_runqueue(&init_task);
  set_links(&init_task);
  init_task.hash_search = hash_search;
  init_task.start_time = jiffies;
  init_task.stime      = jiffies;
  init_files_struct(&init_task.files_struct);
  strncpy(init_task.files_struct.cwd,FILE_SEPARATOR_STR,sizeof(init_task.files_struct.cwd) - 1);
  last_pid = init_task.pid;
  /*Now create tasks that are childs of the init task*/
  create_task_name((unsigned long)&shell,"shell", NULL );
  init_task.thread_struct.esp = stack;
  init_task.thread_struct.esp0 = stack + sizeof(unsigned long);
  /*Let us at this point,use a different stack for INIT: safe baby*/
  SETUP_INIT_STACK(stack);
  initialise_init_tss(stack);
  /*Rip off the codes in the initialisation section:*/
  reclaim_init();
  enable_irq(0);
  /*timer_init();  initialise the timer*/
  /*loop forever and wait for the timer to strike*/
  for(;;) ;
}
  
void __init sched_init(void) { 
  mm_cache_p = slab_cache_create("mm_cache",sizeof(struct mm_struct),SLAB_HWCACHE_ALIGN,0);
  vm_area_cache_p = slab_cache_create("vm_cache",sizeof(struct vm_area_struct),SLAB_HWCACHE_ALIGN,0);
  if(!mm_cache_p || !vm_area_cache_p) {
    panic("Unable to create MM/VM caches:\n");
  }
  if(init_hash(PID_HASH_SIZE) < 0) {
    panic("Unable to initialise the pid hash table:\n");
  }
  /*Now initialise the init task:
    Note that timer_init would be called just before   
    init task goes for a sleep: 
  */
  initialise_init();
}

/*Compute the goodness of a task with respect to
  the current task:
  Its called the preemption goodness:
*/
int preemption_goodness(struct task_struct *next) { 
  int goodness = 0;
  if(REAL_TIME_TASK(next) ) {
    goodness = RT_PRIORITY;
    goto out;
  }  
  /*We should do some computation here based on other factors in future*/
  goodness = next->counter;
 out:
  return goodness;
}


/*Reaper for task exits:
  Rip off tasks memory:
*/
struct reap_control {
  struct timer_list *timer;
  struct task_struct *task;
};

static unsigned long task_reaper(void *arg) {
  struct reap_control *reap_control = (struct reap_control*)arg;
  struct task_struct *task = (struct task_struct *) reap_control->task;
#ifdef DEBUG
  printf("Ripping off task (%s,Pid (%d) ) memory:\n",task->name,task->pid);
#endif
  __free_pages((unsigned long)task,1);
  slab_free((void*) reap_control->timer);
  slab_free(arg); /*free the reap control structure itself*/
  dying = NULL;
  return 0UL;
}

int test_reschedule(void) { 
  return current && current->need_resched == 1;
}

/*
  This should become more robust and principled in future.
  A.R.Karthick:
  Rewrote this routine:
  We dont want "asm" to clutter up the scheduler.
  Instead we let the scheduler,pick up the next process.
  If the scheduler,by chance picks up the same process,
  then we actually skip the save and restore part,
  and simply "iret".
  But if the scheduler happens to pick up the next process, 
  then we save and restore in another routine:
  switch_to:
  Thumb Rule:
  Never call the scheduler in interrupt context
  as it was done before. It can lead to nested things,
  if one isnt careful,which can crash the system badly.
  Interrupt handler should be fast,and in this case should
  just increment jiffies and send EOI to interrupt controller.
*/

volatile void schedule(void) { 
  struct task_struct *next = NULL;
  struct task_struct *prev = current;
  struct list_head   *head = &runqueue;
  struct list_head *traverse = NULL;
  int max, counter;
  if(in_interrupt() ) {
    panic("Scheduler called from inside an interrupt\n");
  }
  /*check for multiple exceptions for an instruction*/
  if(current == dying) {
#ifdef DEBUG
    printf("dying and current same:\n");
#endif
    goto out;
  }
  current->need_resched = 0; /*reset need_resched*/

  /*Check for reaps*/
  if(current->exit_status == REAP_ME) {
    current->status = TASK_ZOMBIE;
  }

  /* Now take decision whether to remove the current from the runqueue or not
   */
  switch(current->status) { 

  case TASK_RUNNING:
    break;

  case TASK_ZOMBIE:
    /*Here we catch exits*/
    if(current->exit_status == REAP_ME) {
      /*This implies that we havent yet reaped ourself*/ 
      if(exit_task() < 0 ) {
	/*Kill failed: Reset*/
	current->status = TASK_RUNNING;
	current->exit_status = 0;
	current->sched_policy = SCHED_OTHER;
	break;
      } 
    }
    dying = current;
    remove_links(current); /*remove all the links of this guy*/
   
    /*I am sorry if anyones waiting in this guys waitqueue:
      Highly unlikely that it should happen,albeit it should crash:  

    */
    init_waitqueue_head(&current->wait_queue_head);

    /*Ok!! This is a bit tricky coz of no signal handling:
      We cant just release the memory of this guy as its our stack right
      now:So we start a timer,and give the stack destruction  
      a timeout of REAP_TIMEOUT :So use it for REAP_TIMEOUT.
      (sufficient for the context switch and then rip it off:)
    */
    do
      {
	struct reap_control *reap_control;
	struct timer_list *timer;
	if(! (reap_control = (struct reap_control*) slab_alloc(sizeof(*reap_control), 0 ) ) || (! (timer= (struct timer_list *)slab_alloc(sizeof(*timer), 0 ) )  ) ) {
	  panic("Unable to allocate memory for reap_control in routine %s,line %d\n",__FUNCTION__,__LINE__);
	}
	reap_control->timer = timer;
	reap_control->task = current;
	init_timer(timer);
	setup_timer(timer,jiffies +  REAP_TIMEOUT,(timer_handler)task_reaper,(void *)reap_control);
      }while(0);

    /*Fall Through*/

  default:
    list_del(&current->runqueue); /*remove from runqueue*/

  }

  /*Now loop through the task list to check for expired timeouts for tasks*/

  do { 
    struct list_head *head = &task_list;
    struct list_head *traverse = NULL;  
    list_for_each(traverse,head) { 
      struct task_struct *task = list_entry(traverse,struct task_struct,next);
      if(task->timeout && task->timeout <= jiffies) { 
        task->timeout = 0;
	add_runqueue(task); /*add this guy into the runqueue*/
      }
    }
  }while(0);

 rescan: 
  counter = 0;  
  max = -1;
  next = &init_task;
  list_for_each(traverse,head) { 
    struct task_struct *task = list_entry(traverse,struct task_struct,runqueue);
    counter = preemption_goodness(task); 
    if(counter > max) {
      max = counter,next = task;
    }
  }
  if(counter == 0) { 
    /*This implies that all of them have expired timeslices:
      Recalculate counters*/
    goto recalculate;
  }
 
  /*Okay !! Now we have the next task to be switched to:
    switch to the next task:
  */
  current = next;
  if(prev->status == TASK_RUNNING)
    prev->status = TASK_SLEEPING;
  current->status = TASK_RUNNING;

  /*switch_mm would setup the page directory in cr3 to point to the process
    switched
  */
  if(next != prev) {
    switch_mm(next,prev);
    /*This does the actual context switch:*/
    switch_to(next,prev);  
  }

  /*When we are are,it implies that we are back from the last process
    or prev, that was scheduled out:
    for NON smps,this guy just returns
  */
  goto out;

 recalculate:
  /*Ok!! This is a goto optimisation */
  do { 
    struct list_head *traverse = NULL;  
    struct list_head *head = &task_list;
    list_for_each(traverse,head) { 
      struct task_struct *task = list_entry(traverse,struct task_struct,next);
      task->counter = (task->counter >> 1 ) + task->priority;
    }
    goto rescan;
  }while(0);

 out: ;
}

static __inline__ const char *get_task_status(const struct task_struct *task) { 
  const char *status = NULL;
  switch(task->status) {
  case TASK_RUNNING:
    status = "RUNNING";
    break;
  case TASK_SLEEPING:
    status = "SLEEPING";
    break;
  case TASK_STOPPED:
    status = "STOPPED";
    break;
  case TASK_ZOMBIE: 
    status = "ZOMBIE";
    break;
  case TASK_INTERRUPTIBLE:
    status = "INTERRUPTIBLE";
    break;
  case TASK_UNINTERRUPTIBLE:
    status = "UNINTERRUPTIBLE";
    break;
  default: 
    status = "UNKNOWN";
  }
  return status;
}

void display_processes(void) {
 struct list_head *head = &task_list; 
 struct list_head *traverse = NULL;
 printf ("PID\tPPID\tNAME\n");
 list_for_each(traverse,head) {
   struct task_struct *task = list_entry(traverse,struct task_struct,next);
   printf ("%d  \t%d  \t%s [%s]\n", task->pid,task->ppid,task->name,get_task_status(task) );
 }
}

static unsigned long timer_expiry(void *arg) { 
  struct timer_list *ptr = (struct timer_list *)arg;
  int interval = ptr->timeout - jiffies;
  printf("\nTimer handler called for timeout(%ld)\n",ptr->timeout);
  slab_free(ptr);
  return (interval < 0) ? 0UL : interval;
}
 
int setup_timers(void) { 
  int timers = 3; /*setup 3 timers to test*/
  int i,err = -1;
  struct timer_list *timer_list[3] = { NULL,NULL,NULL }; 
  for( i = 0; i < timers; ++i) { 
    if(! (timer_list[i] = slab_alloc(sizeof(struct timer_list),0) ) ) {
      printf("Unable to alloc. memory for timers:\n");
      goto out_free;
    }
  }
  /*Now add the timers*/
  for(i = 0; i < timers;++i) { 
    init_timer(timer_list[i]); 
    /*setup the timers*/
    timer_list[i]->timeout = jiffies + HZ*(i+1);
    timer_list[i]->handler = timer_expiry;
    timer_list[i]->arg     = (void*)timer_list[i]; 
    if(add_timer(timer_list[i]) < 0) {
      printf("Error in adding the timer %d\n",i);
    }
  }
  current->status = TASK_INTERRUPTIBLE;
  current->timeout = jiffies + HZ*6;
  __schedule();
  current->status = TASK_RUNNING;
  err = 0;
  goto out;
 out_free:
  do {
    int j;
    for(j = i-1; j >= 0; j--) {
      struct timer_list *ptr = timer_list[j];
      if(ptr)
       slab_free(ptr);
    }
  }while(0);
 out:
  return err;
}
 
int get_children(int pid,const char *name) {
  struct task_struct *task = NULL;
  int err = -1;
  /*First find the task in the table*/ 
  if(pid) { 
    task = lookup_pid(pid);
   }
  else if(name) 
   {
    task = lookup_name(name);
   }
  if(!task) {
    if(pid) {
      printf("Task lookup failed for Pid (%d):\n",pid) ;
    }
    else {
      printf("Task lookup failed for Name (%s):\n",name == NULL ? "Unknown" : name);
    }
    goto out;
  }
  /*Now get the children associated with the task and print them*/
  printf("\nDisplaying Childs of Task (Pid:%d,Name:%s)\n",task->pid,task->name);
  do { 
    struct task_struct *child = NULL;
    printf("%-10s%-10s%-10s\n","Pid","PPID","NAME");
    for(child = task->c_ptr; child ; child = child->o_sptr) {
      printf("%-10d%-10d%-10s\n",child->pid,child->ppid,child->name);
    }
  }while(0);
  err = 0;
 out:
  return err;
}

/*Kill a task:
  This ones again a bit tricky:
  But we can play some scheduler games out here.
  We increase the priority of this guy to RT_PRIORITY,
  so that this guy gets picked at the next clock tick,almost certainly.
  Give him one more chance to run.
  That is to say that if we are trying to kill a task
  that is running by itself,then it makes it simpler,and 
  restricts the exit to REAP_TIMEOUT:
*/
int kill_task(int pid,const char *name) {
  struct task_struct *task = NULL;
  int err =  -1;
  if(pid) { 
    task = lookup_pid(pid);
  }
  else if(name) {
    task = lookup_name(name);
  } 
  if(!task || task->pid == 1) {
    if(pid)
      printf("Unable to kill task with Pid(%d)\n",pid);
    else {
      printf("Unable to kill task (%s)\n",name == NULL ? "Unknown" : name);
    }
    goto out; 
  }
  printf("Killing task (%s:%d)\n",task->name,task->pid);
  task->sched_policy = SCHED_FIFO; /*Make it real time,with higher priority*/
  /*This will force the scheduler to perform cleanups*/
  task->exit_status = REAP_ME;
  if(task->status != TASK_RUNNING) {
    task->status = TASK_RUNNING;
    add_runqueue(task);
  }
  err = 0;
 out:
  return err;
}
          
