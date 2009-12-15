/*Author: A.R.Karthick for MIR.
  A Reimplementation of SYS V Semaphores for MIR Kernel.
  This ones a complete rewrite of the standard implementation of semaphores
  that can be seen in the *NIX Kernels.
*/
#ifdef __KERNEL__
#ifndef _SEM_H
#define _SEM_H

#include <mir/data_str/slist.h>
#include <mir/data_str/myqueue.h>
#include <asm/page.h>
#include <asm/irq.h>
#include <mir/ipc.h>

/*Define the standard limits for the Semaphores*/

#define MAX_SEM_IDS      0x100 /*total sems ids.possible in the system*/
 
#define MAX_SEM_PER_IDS  0x100 /*Total sem sets per semaphore desc.*/

#define MAX_SEM_TOTAL  (MAX_SEM_IDS * MAX_SEM_PER_IDS)

#define MAX_SEM_VAL    0x7fff  /*max possible sem. val for semaphore operation*/

#define MAX_SEM_UNDO_VAL  0x7fff /*max. val of sem. undo*/

/*Now define the semaphore ctrl. operations or semctl operations possible  
  by the user. 
*/

#define SEM_UNDO          0x100

/*This is just jiffies right now*/
#define SEM_TIME(val)       (val) 

/*Update the semaphore ctrl.block:
  This one updates the creation,operation time.
*/
#define SEM_UPDATE_CTRL(ctrl,create)  do { \
 if(create) {\
 (ctrl)->c_time = SEM_TIME(jiffies);\
 }\
 (ctrl)->o_time = SEM_TIME(jiffies);\
 (ctrl)->pid    = current->pid;\
}while(0)

/*Now comes the real semaphore structure*/

struct sem_array { 
  struct ipc_ctrl_block ctrl_block;
  struct queue_struct *head; 
  struct queue_struct **tail; 
  struct slist undo_sem_array;
  struct sem *sem_base; /*sem base*/
  int nsems; /*nr. of sems:*/
  int id;
};

extern struct ipc sem_ipc; /*semaphore ipc*/

/*sem struct*/
struct sem { 
  int val;
  int pid;
};

/*semop structure*/
struct sembuf { 
  int sem_num;  
  int sem_op;
  int sem_flg;
};

/*semundo structure*/
struct sem_undo { 
  struct slist undo_process; /*undo link for the process:*/
  struct slist undo_sem_array; /*undo link per array*/
  int id; /*the array id*/
  short *undo; /*undo counts per sem.*/
};

/*The per sem_array queue of waiting processes:*/
struct sem_array;

struct sem_queue { 
  struct queue_struct queue_struct; /*the index into the next,pprev queue*/
  struct task_struct *task; /*task waiting in the queue*/
  struct sem_array *sem_array ; /*pointer to the sem_array on which its waiting*/
  struct sembuf *sop; /*semaphore operation on which its sleeping*/
  struct sem_undo *sem_undo; /*undo structure on which its sleeping:*/
  int nsops; /*nr. of semaphore operations*/
  int alter; /*whether the operation would cause a sem decrement.*/
  int status; /*the status for which we have to poll to take the correct action:*/
};     

extern int semget(int,int,int);
extern int semctl(int,int,int,union semun *);
extern int semop (int,struct sembuf *,int);
extern int sem_exit(void);

#define SEM_KEY 0x1234567
extern int p(int);
extern int v(int);
extern int create_sem(int,int);
extern int remove_sem(int);

static __inline__ void sem_lock() { 
  cli();
 }

static __inline__ void sem_unlock() {
  sti();
}

#endif
#endif    


    
  
  




