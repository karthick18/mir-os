/*Author: Pratap
         A.R.Karthick : Reorganised this file.
                        Some minor function additions.
*/
#include <mir/kernel.h>
#include <asm/text.h>
#include <mir/terminal.h>
#include <asm/string.h>
#include <asm/irq.h>
static __inline__ void print(char *buff,int unused) {
  terminal_write (buff, strlen(buff));
}
static __inline__ void clear_screen(void) {
  terminal_clear();
}

char *sprintf (char *buf, const char *fmt, ...) {
va_list args;
va_start (args, fmt);
vsprintf (buf, fmt, args);
va_end(args);
return buf;
}


int printf (const char *fmt, ...) {
/* from linux 0.0.1 */
char buf[1024+1];
va_list args;
int i;
va_start (args, fmt);
i = vsprintf (buf, fmt, args);
va_end(args);
print(buf,0);
return i;
}

int panic(const char *fmt,...) {
  va_list ptr;
  char buffer[1025];
  char *str = buffer;
  strcpy(str,"Kernel Panic:");
  str += strlen(buffer);
  va_start(ptr,fmt);
  vsprintf(str,fmt,ptr);
  va_end(ptr);
  print(buffer,0);
  /*Halt the System*/
  cli();  for(;;);
  /*Should never get here*/
  return 0;
}



void clrscr(void) {
  terminal_clear();
}
