#ifndef _KERNEL_H
#ifdef __KERNEL__
#define _KERNEL_H
#ifndef NULL
#define NULL (void *)0
#endif
#define INT_MAX ((int)(~0U>>1) )
#define INT_MIN (-INT_MAX - 1)
#define UINT_MAX    (~0U)
#define ULONG_MAX (~0UL)

#define VER_MIR "0.0.4"
#define printk(arg1) printstr(arg1, 0)

#ifndef MIN
#define MIN(a,b)    ( (a) < (b) ? (a) : (b) )
#endif

#ifndef MAX
#define MAX(a,b)   ( (a) > (b) ? (a) : (b) ) 
#endif

#define PARAMETER_NOT_USED(param) do { (void)(param); } while(0)

#define ALIGN_VAL(val,boundary) (( (val) + (boundary) - 1 ) & ~((boundary) - 1))
typedef unsigned int size_t;

#include <mir/stdarg.h>

#define printf mir_printf
extern int mir_printf(const char *fmt,...);
extern int getstr(char *);
extern int vsprintf(char *,const char *fmt,va_list);
extern char *sprintf(char *,const char *fmt,...);
extern int vsnprintf(char *,int,const char *,va_list);
extern int snprintf(char *,int , const char *,...);
extern int panic(const char *fmt,...);
extern void print_keybuff(void);
extern void printhelp(int);
extern void halt(int);
extern void clrscr(void);
#ifdef VESA
extern void print_vesa(char *);
extern void clrscr_vesa(void);
#else
extern void printstr(char *,unsigned char );
extern void printraw(int ,int , char *);
extern void printf_raw(int ,int, const char *,...);
extern void clrscr_raw(void);
#endif
extern void fd_init();

#endif
#endif
