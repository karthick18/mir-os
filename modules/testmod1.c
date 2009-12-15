
extern int export_symbol;
extern int temp;
int init_module(int argc,char **argv) {
++export_symbol;
++temp;
printf("export symbol val = %d\n",export_symbol);
printf ("TESTMOD4 loaded successfully\n");
 return 1;
}

void cleanup_module(int argc,char **argv) {
  printf("TESTMOD4 unloaded successfully\n");
}

