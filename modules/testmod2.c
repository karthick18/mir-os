/*Author:
 A.R.Karthick for MIR:
 Stress test the MIR kernel by creating many processes and killing them.
*/
#include <mir/kernel.h>
#include <mir/sched.h>
#include <asm/page.h>
#include <mir/slab.h>

struct pcb { 
  char name[MAX_NAMELEN+1];
  int ppid;
};

#define MY_TEST_TASK(function,index) \
static void function_##index(void *arg) { \
  struct pcb *pcb = (struct pcb *)arg;\
  printf("process (%s:ppid %d)\n",pcb->name,pcb->ppid);\
  slab_free(arg);\
}

MY_TEST_TASK(function,1)
MY_TEST_TASK(function,2)
MY_TEST_TASK(function,3)
MY_TEST_TASK(function,4)

MY_TEST_TASK(function,5)
MY_TEST_TASK(function,6)
MY_TEST_TASK(function,7)
MY_TEST_TASK(function,8)

MY_TEST_TASK(function,9)
MY_TEST_TASK(function,10)
MY_TEST_TASK(function,11)
MY_TEST_TASK(function,12)

MY_TEST_TASK(function,13)
MY_TEST_TASK(function,14)
MY_TEST_TASK(function,15)
MY_TEST_TASK(function,16)

MY_TEST_TASK(function,17)
MY_TEST_TASK(function,18)
MY_TEST_TASK(function,19)
MY_TEST_TASK(function,20)

 MY_TEST_TASK(function,21)
 MY_TEST_TASK(function,22)
 MY_TEST_TASK(function,23)
 MY_TEST_TASK(function,24)
 MY_TEST_TASK(function,25)
 MY_TEST_TASK(function,26)
 MY_TEST_TASK(function,27)
 MY_TEST_TASK(function,28)
 MY_TEST_TASK(function,29)
 MY_TEST_TASK(function,30)

#ifdef MAX
#undef MAX
#endif
     
#define MAX 30

typedef void (*funptr) (void *) ;

static funptr functions[] = {
  function_1, function_2,function_3,function_4,function_5,function_6,function_7,function_8,function_9,function_10,function_11,function_12,function_13,function_14,function_15,function_16,function_17,function_18,function_19,function_20,
  function_21,function_22,function_23,function_24,function_25,function_26,
  function_27,function_28,function_29,function_30
};

static int stress_test_mir(void) { 
  int i;
  for(i = 1; i <= MAX; ++i) {
    struct pcb *pcb = NULL;
    char name[MAX_NAMELEN+1];
    sprintf(name,"stress%d",i);
    if( (pcb = slab_alloc(sizeof(*pcb) , 0 ) ) ) {
      strncpy(pcb->name,name,MAX_NAMELEN);
      pcb->ppid = current->pid;
      create_task_name((unsigned long)functions[i-1],pcb->name,(void *)pcb);
    }
    else {
      printf("OOM :Unable to create process %s\n",name);
    }
  }
  return 0;
}

int init_module(int argc,char **argv) { 
  return stress_test_mir();
}

void cleanup_module(int argc,char **argv) {
  printf("Unloading stress_test module..\n");
}
 
