#include <mir/sched.h>
#include <mir/elf.h>
#include <mir/module.h>

static int export_symbol = 1;
static int temp = 1;
EXPORT_SYMBOL(export_symbol);
EXPORT_SYMBOL(temp);
int init_module(int argc,char **argv) {
printf ("TESTMOD3 loaded successfully\n");
return 0;
}
void cleanup_module(int argc,char **argv) {
  printf("TESTMOD3 unloaded successfully\n");
 }

MODULE_INFO("A.R.Karthick","0.1.1",karthick,01);
