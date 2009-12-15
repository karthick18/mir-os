/*Author: A.R.Karthick for MIR
*/
#ifdef __KERNEL__
#ifndef _MIR_UTILS_H
#define _MIR_UTILS_H

#include <mir/ctype.h>
extern int atoi(const char *);
extern double atof(const char *);
extern int errno;
typedef int compare_function(const void *,const void *);
void qsort (void *base, int nr, int size, compare_function *function);

#endif
#endif
