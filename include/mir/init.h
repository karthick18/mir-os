/*A.R.Karthick for MIR:
  This one defines the sections(future stuff)
  that can be used for creating disposable
  codes.But what is amazing is that,that
  GCC doesnt give you the linker variables
  _end, or _etext at runtime if the file is compiled
  as an object file.Seems strange !!
  Usage :
  Whenever you want to define a function
  to get inside say an init section,do this:
  void __init myfun()
  For data:
  unsigned char __initdata mydata[] = "MIR";
*/
  
#ifdef __KERNEL__
#ifndef _INIT_H
#define _INIT_H

#define SECTION_NAME(name)   __attribute__ ( (__section__ (#name) ) )
#define __init               SECTION_NAME(.inittext)
#define __initdata           SECTION_NAME(.initdata)

/*This tells GCC to find its arguments in the stack*/

#define asmlinkage           __attribute__ ((regparm(0)))

#endif
#endif

