/*Author: A.R.Karthick for MIR
*/

#ifdef __KERNEL__
#ifndef _IPC_H
#define _IPC_H

#define IPC_NOWAIT        0x200
#define IPC_CREAT         0x1000
#define IPC_EXCL          0x2000
#define IPC_PRIVATE       0

#define IPC_GETVAL      0x1 
#define IPC_SETVAL      0x2 
#define IPC_INFO        0x3
#define SEM_INFO        0x4
#define IPC_RMID        0x5

struct ipc_ctrl_block { 
  long c_time;
  long o_time;
  int pid;
  int uid; /*unused*/  
  int gid; /*unused*/
  int perm;/*unused*/
  int key; /*semaphore key*/
};

struct ipc_id {
  struct ipc_ctrl_block *ctrl_blocks;
  int in_use;/*1 if in_use*/
};

/*The main IPC structure */

struct ipc { 
  int max_ipc_ids;
  int max_ipc_per_ids;
  int max_ipc_total;
  int in_use;/*ids in use:*/
  struct ipc_id *ipc_id;
};


struct ipc_info { 
  int max_ipc_ids;
  int max_ipc_per_ids;
  int max_ipc_total;
  int max_ipc_val;
  int max_ipc_undo_val;  
  int ipc_inuse;
};

struct sem_array;

/*semctl operation*/
union semun { 
  int val; /*IPC_GETVAL,IPC_SETVAL*/
  struct sem_array *semid_ds;
  short *array; /*for IPC_SETALL,GET_ALL*/ 
  struct ipc_info *ipc_info; /*IPC_INFO*/
};

extern int ipc_init(void);
#endif
#endif
