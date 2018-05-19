/*   MIR-OS : SYS V Semaphore Implementation
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
;   Complete rewrite of Sys V SEMAPHORES for MIR Kernel:
;   It is assumed that the reader knows about the details of semaphores.
;   Simple theory:
;   Semaphores provide sequential access to resources.
;   The operation that can be performed on the semaphore are:
;  a) increment or a non block operation
;  b) decrement operation blocks when semval reaches < 0 
;  c) wait on zero operation on semaphore which blocks the process
  till semval reaches 0
;
*/

#include <mir/kernel.h>
#include <mir/sched.h>
#include <mir/data_str/slist.h>
#include <mir/data_str/myqueue.h>
#include <asm/page.h>
#include <mir/sem.h>
#include <mir/init.h>

struct ipc sem_ipc;

/*Now define the standard ipc alloc operations*/

static int ipc_alloc(struct ipc *ipc,int size ) {
  int err =-1;
  if(! (ipc->ipc_id = slab_alloc(size,0) ) )
    goto out;
  
  memset(ipc->ipc_id,0x0,size);
  err = 0;
 out:
  return err;
}

/*Now initialise the IPC main*/

int __init ipc_init(void) { 
  struct ipc *ipc = &sem_ipc;
  int err =-1;
  memset(ipc,0x0,sizeof(*ipc));
  ipc->max_ipc_ids = MAX_SEM_IDS;
  ipc->max_ipc_per_ids = MAX_SEM_PER_IDS;
  ipc->max_ipc_total = MAX_SEM_TOTAL;
  if(ipc_alloc(ipc,MAX_SEM_IDS * sizeof(struct ipc_id) ) < 0 ) {
    panic("Unable to initialise ipcs.Error in ipc_alloc:\n");
    goto out;/*this will never happen*/
  }
  /*Now return */
  err = 0;
 out:
  return err;
}

static  __inline__ int ipc_get_id(struct ipc *ipc) {
  int i,id = -1;
  for(i = 0; i < ipc->max_ipc_ids; ++i) { 
    struct ipc_id *ptr = ipc->ipc_id + i;
    if(!ptr->in_use) { 
      ptr->in_use = 1;
      id = i;
      break;
    }
  }
  return id;
}

static __inline__ struct sem_array* ipc_get_array(struct ipc *ipc,int id) {
  if(id < 0 || id >= ipc->max_ipc_ids)
    return NULL;
  return (struct sem_array*)(ipc->ipc_id[id].ctrl_blocks);
}

/*Given a key,get the array*/
static __inline__ struct sem_array *ipc_array(struct ipc *ipc,int key) { 
  int i;
  for(i = 0 ;i < ipc->max_ipc_ids; ++i) { 
    struct ipc_ctrl_block *ptr = ipc->ipc_id[i].ctrl_blocks;
    if(ptr && ptr->key == key) 
      return (struct sem_array*) ptr;
  }
  return NULL;
}
/*Free an array*/
static __inline__ struct sem_array *ipc_free_array(struct ipc *ipc,int id) {
  struct sem_array *ptr = NULL;
  if(id < 0 || id >= ipc->max_ipc_ids) 
    return ptr;
  else {
    ptr =  (struct sem_array*)ipc->ipc_id[id].ctrl_blocks;
    ipc->ipc_id[id].ctrl_blocks = NULL;
    ipc->ipc_id[id].in_use = 0;
  }
  return ptr;
}
/*Allocate a semarray with keys,nsems,perm*/

static struct sem_array *ipc_alloc_array(int key,int nsems,int perm) {
  struct ipc *ptr = &sem_ipc;
  struct ipc_ctrl_block *ctrl_block;
  struct sem_array *sem_array = NULL;
  int err = -1;
  if(nsems > ptr->max_ipc_per_ids) 
    goto out;
  if(nsems + ptr->in_use > ptr->max_ipc_total) 
    goto out;
  
  if(! (sem_array = slab_alloc(sizeof(struct sem_array) + nsems * sizeof(struct sem) , 0 ) ) ) 
    goto out;

  memset(sem_array,0x0,sizeof(struct sem_array) + nsems*sizeof(struct sem));
  /*Initialise the components of the sem_array*/  
  sem_array->sem_base = (struct sem *)&sem_array[1];
  sem_array->nsems = nsems;
  queue_init(sem_array); /*initialise the pending queue for this array*/
  slist_head_init(&sem_array->undo_sem_array); /*initialise the undo queue*/
  ctrl_block = &sem_array->ctrl_block;
  ctrl_block->key = key;
  ctrl_block->perm = perm;
  err = ipc_get_id(ptr);
  if(err < 0 )
    goto out_free;
  sem_array->id = err;
  ptr->ipc_id[err].ctrl_blocks = ctrl_block;
  ptr->in_use += nsems;
  goto out;
 out_free:
  slab_free((void*)sem_array);
  sem_array = NULL;
 out:
  return sem_array;
}

/*The call to create or initialise a semaphore:*/

static int semget_lock(int key,int nsems,int flags) { 
  int err = -1; 
  int perm = flags & 0x1ff;/*truncate to 9 bits*/
  int create;
  struct sem_array *sem_array = NULL ;
  struct ipc_ctrl_block *ctrl_block;
  
  /*First do some sanity checks:*/
  if(key == IPC_PRIVATE && !(flags & IPC_CREAT) ) 
    goto out;
 
  /*try finding the key in the ipc:*/
  if(key != IPC_PRIVATE &&
     (sem_array = ipc_array(&sem_ipc,key) )
     && (flags & IPC_CREAT) 
     )
    goto out;
  else if( (flags & IPC_CREAT)
           &&
           !(sem_array = ipc_alloc_array(key,nsems,perm) ) 
	   )
    goto out;

  if(!sem_array) 
    goto out;
  /*we are here when we have alloced or found the sem_array*/
  if(sem_array->nsems != nsems) 
    goto out;  
  
  ctrl_block = &sem_array->ctrl_block;
  create = (flags & IPC_CREAT) ? 1: 0;
  SEM_UPDATE_CTRL(ctrl_block,create);    
  err = sem_array->id;
 out:
  return err;
}

/*Perform the semop operation:
  undo decides whether the operation has to be undone: 
  This happens on a decrement operation,when each of the guys in the 
  sem_queue tries to perform a decrement operation.
  The first guy in the sem_queue performing the decrement goes through,
  but undoes his changes and tries again when woken up.
*/

static int perform_semop(struct sem_array *sem_array,struct sembuf *sop,int nsops,struct sem_undo *sem_undo,int pid,int undo ) {
  struct sembuf *ptr;
  int err = -1;
  for(ptr = sop; ptr < sop + nsops ; ++ptr) {
    struct sem *sem = sem_array->sem_base + ptr->sem_num;
    sem->pid = (sem->pid << 16 | pid);
    if(!ptr->sem_op && sem->val) 
      goto block;
    sem->val += ptr->sem_op;
    /*Check for the undo flag*/
    if(ptr->sem_flg & SEM_UNDO) {
      sem_undo->undo[ptr->sem_num] -= ptr->sem_op;
      if(sem_undo->undo[ptr->sem_num] < -MAX_SEM_UNDO_VAL - 1 ||
         sem_undo->undo[ptr->sem_num] >=  MAX_SEM_UNDO_VAL)
	goto do_undo;
    }
    if(sem->val < 0) 
      goto block;      
  }
  err = 0; 
  if(undo) {
    --ptr;
    goto do_undo;
  }
  goto out;

 block:
  if(!(ptr->sem_flg & IPC_NOWAIT))
    err = 1;

 do_undo:
  while(ptr >= sop) {
    struct sem *sem = sem_array->sem_base + ptr->sem_num;
    sem->pid >>= 16;
    sem->val -= ptr->sem_op;
    if(ptr->sem_flg & SEM_UNDO) 
      sem_undo->undo[ptr->sem_num] += ptr->sem_op;    
    --ptr;
  }
 out:
  return err;
}

static void poll_queue(struct sem_array *sem_array) {
  struct queue_struct *traverse,*next;
  for(traverse = sem_array->head ; traverse ; traverse = next) {
    struct sem_queue *sem_queue = queue_entry(traverse,struct sem_queue,queue_struct);
    next = traverse->next;
#ifdef DEBUG
    printf("polling queue for task (%d)\n",sem_queue->task->pid);
#endif
    if(sem_queue->status == 1) 
      continue;
    sem_queue->status = perform_semop(sem_queue->sem_array,sem_queue->sop,sem_queue->nsops,sem_queue->sem_undo,sem_queue->task->pid,sem_queue->alter);

    if(sem_queue->status <= 0 ) {
      wake_up_process(sem_queue->task);
      if(sem_queue->status == 0 && sem_queue->alter) {
	/*try again for decrements.:*/
	return;
      }
      queue_del(traverse,sem_array);
    }
  }
}
/*Free the array identified by id:*/
static int free_array(int id) { 
  struct sem_array *sem_array = NULL;
  register struct queue_struct *traverse,*next;
  struct slist *head,*temp;
  int err = -1;
  if(! (sem_array = ipc_free_array(&sem_ipc,id) ) ) 
    goto out;
  for(traverse = sem_array->head ; traverse ; traverse = next) { 
    struct sem_queue *queue = queue_entry(traverse,struct sem_queue,queue_struct);
    next = traverse->next;
    queue->status = -1;
    /*zero off the links*/
    traverse->pprev = NULL;
    traverse->next  = NULL;
    wake_up_process(queue->task);
  }
  /*now invalidate the undo ids:*/
  head = &sem_array->undo_sem_array;
  slist_for_each(temp,head) { 
    struct sem_undo *undo = slist_entry(temp,struct sem_undo,undo_sem_array);
    undo->id = -1;
  }

  sem_ipc.in_use -= sem_array->nsems;
  if(sem_ipc.in_use < 0 ) /*shouldnt happen*/
    sem_ipc.in_use = 0;

  slab_free((void*)sem_array);
  err = 0;
 out:
  return err;
}  

/*Perform semctl operations:*/

static int semctl_lock(int id,int sem_num,int cmd,union semun *arg) {
  struct sem_array *sem_array;
  struct ipc_ctrl_block *ctrl_block;
  int err = -1;
  if(! (sem_array = ipc_get_array(&sem_ipc,id) ) ) 
    goto out;
  if(sem_num >= sem_array->nsems) 
    goto out;

  ctrl_block = &sem_array->ctrl_block;

  SEM_UPDATE_CTRL(ctrl_block,0);

  switch(cmd) { 
 
  case IPC_GETVAL:
    /*get the value of sem_numth sem.*/
    err = sem_array->sem_base[sem_num].val;
    goto out;

  case IPC_SETVAL:
    /*set the value of the sem_numth sem.*/
    
    if(!arg || arg->val > MAX_SEM_VAL) 
      goto out;
    
    sem_array->sem_base[sem_num].val = arg->val;
    /*Someone might be waiting for this update on the sem.*/
    poll_queue(sem_array);
    break;

  case IPC_INFO:
  case SEM_INFO:
    {
    struct ipc_info *info = arg->ipc_info;
    memset(info,0x0,sizeof(struct ipc_info));
    info->max_ipc_ids = MAX_SEM_IDS;
    info->max_ipc_per_ids = MAX_SEM_PER_IDS;
    info->max_ipc_total = MAX_SEM_TOTAL;
    info->max_ipc_val = MAX_SEM_VAL;
    info->max_ipc_undo_val = MAX_SEM_UNDO_VAL; 
    if(cmd == SEM_INFO)
      info->ipc_inuse = sem_ipc.in_use;
    }
    break;

  case IPC_RMID:
    /*remove the semaphore:*/
    err = free_array(id);
    goto out;

  default:
    printf("Invalid cmd (%d)\n",cmd);
    goto out;
  }
  err = 0;
 out:
  return err;
}

/*Allocate an undo structure and setup the process and array links*/
static int allocate_undo(struct sem_array *sem_array,struct sem_undo **sem_undo) {
  struct sem_undo *undo = NULL;
  int err = -1;
  *sem_undo = NULL;

  if(! ( undo = slab_alloc(sizeof(struct sem_undo) + sem_array->nsems * sizeof(short) , 0 ) ) ) 
    goto out;
  *sem_undo = undo; 
  memset(undo,0x0,sizeof(struct sem_undo) + sem_array->nsems * sizeof(short) );
  undo->undo = (short *)&undo[1]; 
  undo->id = sem_array->id;
  /*add this undo structure to the current process*/
  slist_add(&undo->undo_process,&current->undo_process);
  /*Add this to the sem_array*/
  slist_add(&undo->undo_sem_array,&sem_array->undo_sem_array);
  err = 0;
 out:
  return err;
}

static struct slist *free_undo(struct slist *head,struct sem_undo *undo) {
  struct slist *next ;
  next = undo->undo_process.next;
  slist_del(&undo->undo_process,head);
  slab_free((void*)undo);
  return next;
}

/*Perform a semaphore operation:
  This is the main routine that handles the semaphore
  operation:
*/
static int semop_lock(int id,struct sembuf *sop,int nsops) {   
  struct sem_array *sem_array;
  struct sembuf *ptr ;
  struct ipc_ctrl_block *ctrl_block;
  struct sem_undo *sem_undo = NULL;
  int undo = 0,alter = 0,decrement = 0;
  int err = -1;
  
  if(! (sem_array = ipc_get_array(&sem_ipc,id) ) ) 
    goto out;

  /*Do some sanity checks on the semop*/
  for(ptr = sop; ptr < sop + nsops; ++ptr) {
    if(ptr->sem_num >= sem_array->nsems) 
      goto out;
    if(ptr->sem_op > MAX_SEM_VAL || ptr->sem_op < -MAX_SEM_VAL - 1)
      goto out;
    if(ptr->sem_op > 0 && !alter) alter = 1;
    if(ptr->sem_op < 0 && !decrement) { 
      decrement = 1;
      if(!alter) alter = 1;
    }
    if((ptr->sem_flg & SEM_UNDO) && !undo) undo = 1;
  }
  ctrl_block = &sem_array->ctrl_block;
  SEM_UPDATE_CTRL(ctrl_block,0);
  /*now check if an undo structure exists or needs to be allocated:*/
  if(undo) { 
    /*first check though if an undo structure already exists
      for this process,and for this id:
    */
    struct slist *head = &current->undo_process;
    struct slist *traverse,*next;
    for(traverse = head->next ; traverse != head; traverse = next) {
      struct sem_undo *temp = slist_entry(traverse,struct sem_undo,undo_process);
      if(temp->id == id) {
	sem_undo = temp;
        break;
      }
      if(temp->id == -1) {
	/*this means we have an undo structure of an array thats been ripped off:free this guy from our queue*/
        next = free_undo(head,temp);
        continue;
      }
      next = traverse->next;
    }
    if(!sem_undo) 
      if(allocate_undo(sem_array,&sem_undo) < 0 ) 
        goto out;
  }
  /*now perform the operation to see if it works:*/
  err = perform_semop(sem_array,sop,nsops,sem_undo,current->pid,0);
  if(err <= 0 ) 
    goto out; /*either sucessful or failure:*/
  else {
    /*Okay!! we are here when we are blocking:*/
    struct sem_queue queue;/*the queue on which to sleep if unsuccessful*/
    struct queue_struct *p_queue= &queue.queue_struct;
    queue.sem_array = sem_array;
    queue.sop = sop;
    queue.nsops = nsops;
    queue.alter = decrement; /*true if going to decrement:*/
    queue.sem_undo = sem_undo;
    queue.task = current;
    p_queue->next = NULL;
    p_queue->pprev = NULL;
    queue_add_tail(p_queue,sem_array); /*link this queue in this array:*/      
    for(;;) { 
      queue.status = -1;
      current->status = TASK_INTERRUPTIBLE;
      /*now unlock the sem and schedule:unlock is a STI*/
      /*sem_unlock();*/
      __schedule();
      /*We are here when we are woken up again:*/
      sem_lock();
      err = queue.status;

      /* we can be woken up when the sem array can be ripped off :*/
      if( !(sem_array = ipc_get_array(&sem_ipc,id) ) ) {
        goto out;
      }

      /*A Non Racy check for process exits that can screw up the sem.queue above through sem_exits*/
      if(current->exit_status == REAP_ME) {
        if(p_queue->pprev) {
	  queue_del(p_queue,sem_array);
	}
	break;
      }
      /*check if we have to retry:
	we retry for 0 status wakeups,which will ask us to retry again 
	to take the sem.:
      */
      if(queue.status == 0 ) {
        if( (err = perform_semop(sem_array,sop,nsops,sem_undo,current->pid,0) ) <= 0 ) 
	  {
            queue_del(p_queue,sem_array);
            break;
	  }
      } else if( queue.status < 0 ) {
	/*we can break safely:*/
	break;
      }
    }
  }

  /*now check if we have got to poll the queue again due to a change:*/
  if(alter) 
    poll_queue(sem_array);
 out:
  return err;
}    

/*This is the routine that is called on process exits:
  This undoes all the changes, the process would have done
  before terminating. Hence it keeps track of forever blocks
  if a process acquires a sem,and exits before releasing it.
  This happens, provided the process has specified the SEM_UNDO flag
  while performing the semop on a sem_array:
*/

static int sem_exit_lock(void) { 
  struct slist *head =&current->undo_process; /*head of undo structures*/

  while(!SLIST_EMPTY(head) ) {
    struct slist *next = head->next;
    struct sem_undo *undo = slist_entry(next,struct sem_undo,undo_process);
    struct sem_array *sem_array;
    struct ipc_ctrl_block *ctrl_block;
    int i;
    slist_del(next,head);
    if(undo->id == -1) {
      /*implies that the array has been ripped off:*/
      goto next;
    }
    if(! (sem_array = ipc_get_array(&sem_ipc,undo->id) ) ) {
      printf("Not found sem_array for sem id(%d)\n",undo->id);
      goto next;
    }    
    /*delete this guy from this sem_array*/
    slist_del(&undo->undo_sem_array,&sem_array->undo_sem_array);
    /*now undo the changes made to this sem_array*/
    for(i = 0; i < sem_array->nsems; ++i ) { 
      struct sem *sem = sem_array->sem_base + i;
      sem->val += undo->undo[i];
      if(sem->val < 0 ) {
	/*shouldnt happen though:*/
        printf("incorrect adjustment during undo:\n");
        sem->val = 0;
      }
      sem->pid = (sem->pid << 16) | current->pid;
    }
    ctrl_block = &sem_array->ctrl_block;
    SEM_UPDATE_CTRL(ctrl_block,0);
    poll_queue(sem_array); /*update the queue for wakeups*/
  next:
    slab_free((void*)undo);
  }
  return 0;
}        
         
/*All these versions called the locked versions:*/

int semget(int key,int nsems,int flags) { 
  int err = -1;
  if(current->exit_status == REAP_ME) {
    /*This guy is going to die soon.Why do went want to take the useles
      overhead and cause spurious, crazy but rare crashes:
    */
    goto out;
  }
  sem_lock();
  err = semget_lock(key,nsems,flags);
  sem_unlock();
 out:
  return err;
}

int semctl(int id,int num,int cmd,union semun *arg) {
  int err = -1;
  if(current->exit_status == REAP_ME) {
    /*This guy is going to die soon.Why do went want to take the useles
      overhead and cause spurious, crazy but rare crashes:
    */
    goto out;
  }
  sem_lock();
  err = semctl_lock(id,num,cmd,arg);
  sem_unlock();
 out:
  return err;
}

int semop(int id,struct sembuf *sop,int nsops) {
  int err = -1;
  if(current->exit_status == REAP_ME) {
    /*This guy is going to die soon.Why do we want to take the useles
      overhead and cause spurious, crazy but rare crashes:
    */
    goto out;
  }
  sem_lock();
  err = semop_lock(id,sop,nsops);
  sem_unlock();
 out:
  return err;
}

int sem_exit(void) {
  int err;
  sem_lock();
  err = sem_exit_lock();
  sem_unlock();
  return err;
}  
      
static struct sembuf op_lock[2] = { 
  { 0,0,SEM_UNDO},
  { 0,1,SEM_UNDO},
};

static struct sembuf op_unlock[1] = {
  {0,-1,SEM_UNDO},
};

/*p operation of dijkstra:*/

int p(int id) { 
  if(semop(id,op_lock,2) < 0 ) {
#ifdef DEBUG
    printf("Error in semop:\n");
#endif
  }
  return 0;
}
/*the v or release operation of dijsktra:*/
int v(int id) {
  if(semop(id,op_unlock,1) < 0 ) {
#ifdef DEBUG
    printf("Error in semop:\n");
#endif
  }
  return 0;
}
int create_sem(int key,int create) { 
  return semget(key,1,create ? IPC_CREAT : 0);
}
int remove_sem(int id) {
  return semctl(id,0,IPC_RMID,NULL);
}
  
