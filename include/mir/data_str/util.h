/*Author: A.R.Karthick for MIR
 */

#ifndef _UTIL_H
#define _UTIL_H

#include <mir/kernel.h>
#include <asm/string.h>
#include <mir/stdarg.h>
#include <mir/slab.h>

#define BUFFER 1024

#define malloc(bytes)        slab_alloc(bytes,0)
#define free(addr)           slab_free(addr)

/*Generic malloc interface.
 @ptr: type of object to allocate.
 @number:number of objects to allocate.
 @extra: extra count to allocate per object
*/

#define ALLOC_MEM(ptr,number,extra) ({\
__typeof__(ptr) __temp = NULL;\
int __bytes = 0;\
__temp = (__typeof__(__temp) )malloc(__bytes = ((number) * ( sizeof(*(__temp) ) + (extra) ) ));\
if(__temp) {\
 memset(__temp,__bytes,0x0);\
}\
__temp;\
})
#endif
