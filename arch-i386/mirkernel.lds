/*Linker script to make MIR kernel:
 Author: A.R.Karthick 
*/
OUTPUT_FORMAT("elf32-i386","elf32-i386","elf32-i386")
OUTPUT_ARCH(i386)

ENTRY(_start)
SECTIONS 
{
 . = 0x100000;
_text = .;
 .text : { 
           *(.text) 
           *(.fixup)
           *(.gnu.warning)
         }
 _etext = .;

 . = ALIGN(16);
    
 .rodata : {
            *(.rodata)
           }

  . = ALIGN(4096); /*start at a new page for init sections*/

  /*Start of initialising sections which can be ripped off after initialisation is through*/

  _init_start = .;
 
  _init_data_start  = .;
  .initdata : { 
                *(.initdata)
              }
  .  = ALIGN(32); 
  _init_data_end   = .;

  _init_text_start = .;
  .inittext : {
             *(.inittext) 
              }
  _init_text_end = .;
  _init_end = .;
 
 . = ALIGN(4096); 

 _ksymtab_start = .;

 .ksymtab : {
	     *(.ksymtab) 
	    }

 _ksymtab_end   = .;

. = ALIGN(32);

 /*start a new page boundary coz we are going to rip the above one off*/

  _sdata = .;
  .data : {
            *(.data)
            CONSTRUCTORS       
          }
  _edata = .;

  . = ALIGN(32);
  _sbss = .;
  .bss : {
           *(.bss) 
         }
   _end = .;
}
 
  
