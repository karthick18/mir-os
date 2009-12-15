/*A.R.Karthick for MIR:
  Made the mostly used string routines in assembly,
  for speed.
*/

#ifndef _STRING_H
#ifdef __KERNEL__
#define _STRING_H

#ifndef MIN
#define MIN(a,b)    ( (a)  < (b) ? (a) : (b) )
#endif

static __inline__ void *memset_words(void *dst,const unsigned short word,const int words) {
  int _d1,_c1;
  __asm__ __volatile__ ("cld;rep;stosw\n\t" 
                        :"=&D"(_d1),"=&c"(_c1) 
                        :"0"( dst ),"a"(word),"1"(words) : "memory"
			);
  return dst;
}

#define strlen  my_strlen
#define strnlen my_strnlen
#define strcpy  my_strcpy
#define strncpy my_strncpy

#define strcat  my_strcat
#define strncat my_strncat
#define strchr  my_strchr
#define strrchr my_strrchr

#define strcmp  my_strcmp
#define strncmp my_strncmp

#define strspn  my_strspn
#define strcspn my_strcspn
#define strpbrk my_strpbrk
#define strstr  my_strstr

#define memcpy  my_memcpy
#define memset  my_memset
#define memchr  my_memchr
#define memrchr my_memrchr
#define memmove my_memmove
#define memcmp  my_memcmp
#define strtok  my_strtok
#define strsep  my_strsep

extern char * ___strtok;
extern char *my_strsep(char **,const char *);
extern char *my_strtok(char *,const char *);

static __inline__ void *my_memcpy(void *dst,const void *src,int bytes) {
  int _s1,_d1,_cx;
  __asm__ __volatile__("cld\n\t"
		       "rep\n\t"
		       "movsl\n\t"
		       "movl %3,%2\n\t"
		       "rep;movsb\n\t"
		       :"=&S"(_s1),"=&D"(_d1),"=&c"(_cx)
		       :"g"(bytes % sizeof(unsigned long) ),"0"(src),"1"(dst),"2"(bytes/sizeof(unsigned long))
		       : "memory"
		       );
  return dst;
}		
 
static __inline__ void *my_memmove(void *dst,const void *src,int bytes) { 
  int _s1,_d1,_cx;
  if(dst > src ) {
    __asm__ __volatile__("std\n\t"
			 "rep\n\t"
			 "movsb\n\t"
			 "cld\n"
			 :"=&S"(_s1),"=&D"(_d1),"=&c"(_cx)
			 :"0"(src + bytes - 1 ),"1"(dst + bytes - 1),"2"(bytes)
			 :"memory"
			 );

  } else {
    __asm__ __volatile__("cld\n\t"
			 "rep\n\t"
			 "movsb\n\t"
			 :"=&S"(_s1),"=&D"(_d1),"=&c"(_cx)
			 :"0"(src),"1"(dst),"2"(bytes)
			 :"memory"
			 );
  }
  return dst;
}

static __inline__ void *my_memset(void *dst,int c, int bytes) {
  int _d1,_cx;
  c *= 0x01010101UL ;
  __asm__ __volatile__("cld\n\t"
		       "rep\n\t"
		       "stosl\n\t"
		       "movl %2,%1\n\t"
		       "rep; stosb\n\t"
		       :"=&D"(_d1),"=&c"(_cx)
		       :"g"(bytes % sizeof(unsigned long)),"a"(c),"0"(dst),"1"(bytes/sizeof(unsigned long))
		       : "memory"
		       );
  return dst;
}

static __inline__ void *my_memchr(void *dst,int c, int bytes) { 
  int _d1,_cx;
  __asm__ __volatile__("cld\n\t"
		       "repne\n\t"
		       "scasb\n\t"
		       "je 1f\n\t"
		       "movl $1,%0\n\t"
		       "1:decl %0\n"
		       :"=&D"(_d1),"=&c"(_cx)
		       :"0"(dst),"1"(bytes),"a"(c)
		       :"memory"
		       );
  return (void *)_d1;
}

static __inline__ void *my_memrchr(const void *dst,int c,int bytes) { 
  int _d1,_cx,_ax,_dx;
  __asm__ __volatile__("cld\n\t"
		       "1:\n\t"
		       "decl %2\n\t"
		       "js 3f\n\t"
		       "scasb\n\t"
		       "je 2f\n\t"
		       "jmp 1b\n\t"
		       "2:movl %0,%3\n\t"
		       "jmp 1b\n\t"
		       "3:decl %3\n"
		       :"=&D"(_d1),"=&a"(_ax),"=&c"(_cx),"=&d"(_dx)
		       :"0"(dst),"1"(c),"2"(bytes),"3"(1)
		       :"memory"
		       );
  return (void *)_dx;
}
static __inline__ int my_memcmp(const void *a,const void *b,int bytes) { 
  int _s1,_d1,_cx,_ax;
  __asm__ __volatile__("cld\n\t"
		       "repe\n\t"
		       "cmpsb\n\t"
		       "je 2f\n\t"
		       "movl $1,%%eax\n\t"
		       "jl 2f\n\t"
		       "negl %%eax\n\t"
		       "2:\n"
		       :"=&a"(_ax),"=&S"(_s1),"=&D"(_d1),"=&c"(_cx)
		       :"0"(0),"1"(b),"2"(a),"3"(bytes)
		       :"memory"
		       );

  return _ax;
}

static __inline__ char *my_strstr(const char *haystack,const char *needle) { 
  int _s1,_d1,_cx,_ax;
  __asm__ __volatile__("cld\n\t"
		       "repne\n\t"
		       "scasb\n\t"
		       "notl %3\n\t"
		       "decl %3\n\t"
		       "movl %%ecx,%%edx\n\t"
		       "1:\n\t"
		       "movl %1,%0\n\t"
		       "movl %4,%%edi\n\t"
		       "movl %%edx,%%ecx\n\t"
		       "repe\n\t"
		       "cmpsb\n\t"
		       "je 3f\n\t"
		       "xchgl %%eax,%%esi\n\t"
       		       "cmpb $0,-1(%%eax)\n\t"
		       "je 2f\n\t"
		       "incl %1\n\t"
		       "jmp 1b\n\t"
		       "2: xorl %%eax,%%eax\n\t"
		       "3:\n"
		       :"=&a"(_ax),"=&S"(_s1),"=&D"(_d1),"=&c"(_cx)
		       :"g"(needle),"0"(0),"1"(haystack),"2"(needle),"3"(0xffffffff)
		       :"memory","dx"
		       );
  return (char *)_ax;
}
		       

/*length of initial segment of src consisting of characters from accept
*/
static __inline__ int my_strspn(const char *src,const char *accept) { 
  int _s1,_d1,_cx;
  __asm__ __volatile__("cld\n\t"
		       "repne\n\t"
		       "scasb\n\t"
		       "notl %2\n\t"
		       "decl %2\n\t"
       		       "movl %%ecx,%%edx\n\t"
		       "1:\n\t"
		       "lodsb\n\t"
		       "testb %%al,%%al\n\t"
		       "je 2f\n\t"
		       "movl %%edx,%%ecx\n\t"
		       "movl %3,%%edi\n\t"
		       "repne\n\t"
		       "scasb\n\t"
		       "je 1b\n\t"
		       "2:\n\t"
		       "decl %0\n"
		       :"=&S"(_s1),"=&D"(_d1),"=&c"(_cx)
		       :"g"(accept),"a"(0),"0"(src),"1"(accept),"2"(0xffffffff)
		       :"memory","dx"
		       );
  return (const char *)_s1 - src;
}

static __inline__ int my_strcspn(const char *src,const char *reject) { 
  int _s1,_d1,_cx;
  __asm__ __volatile__("cld\n\t"
		       "repne\n\t"
		       "scasb\n\t"
		       "notl %2\n\t"
		       "decl %2\n\t"
       		       "movl %%ecx,%%edx\n\t"
		       "1:\n\t"
		       "lodsb\n\t"
		       "testb %%al,%%al\n\t"
		       "je 2f\n\t"
		       "movl %%edx,%%ecx\n\t"
		       "movl %3,%%edi\n\t"
		       "repne\n\t"
		       "scasb\n\t"
		       "jne 1b\n\t"
		       "2:\n\t"
		       "decl %0\n"
		       :"=&S"(_s1),"=&D"(_d1),"=&c"(_cx)
		       :"g"(reject),"a"(0),"0"(src),"1"(reject),"2"(0xffffffff)
		       :"memory","dx"
		       );
  return (const char *)_s1 - src;
}

/*Returns a pointer to the string src, which matches any of the characters in accept
 */
static __inline__ char *my_strpbrk(const char *src,const char *accept) { 
  int _s1,_d1,_cx;
  __asm__ __volatile__("cld\n\t"
		       "repne\n\t"
		       "scasb\n\t"
		       "notl %2\n\t"
		       "decl %2\n\t"
       		       "movl %%ecx,%%edx\n\t"
		       "1:\n\t"
       		       "movl %%edx,%%ecx\n\t"
		       "movl %4,%%edi\n\t"
		       "lodsb\n\t"
		       "testb %%al,%%al\n\t"
		       "je 2f\n\t"
		       "repne\n\t"
		       "scasb\n\t"
		       "jne 1b\n\t"
		       "decl %0\n\t"
		       "jmp 3f\n\t"
		       "2:xorl %0,%0\n\t"
		       "3:\n"
		       :"=&S"(_s1),"=&D"(_d1),"=&c"(_cx)
		       :"a"(0),"g"(accept),"0"(src),"1"(accept),"2"(0xffffffff)
		       :"memory","dx"
		       );
  return (char *)_s1;
}

static __inline__ void my_strcat(char *dst,const char *src) { 
  int _s1,_d1;
  __asm__ __volatile__("cld\n\t"
		       "repne\n\t"
		       "scasb\n\t"
		       "decl %1\n\t"
		       "1:\t\n"
		       "lodsb\t\n"
		       "stosb\n\t"
		       "testb %%al,%%al\n\t"
		       "jne 1b\n\t"
		       "2:\n"
		       :"=&S"(_s1),"=&D"(_d1)
		       :"a"(0),"c"(0xffffffff),"0"(src),"1"(dst)
		       : "memory"
		       );
}

static __inline__ void my_strncat(char *dst,const char *src,int n) {
  int _s1,_d1,_cx;
  __asm__ __volatile__("cld\n\t"
		       "repne\n\t"
		       "scasb\n\t"
		       "decl %1\n\t"
		       "movl %4,%2\n\t"
		       "1:\n\t"
		       "decl %2\n\t"
		       "js 2f\n\t"
		       "lodsb\n\t"
		       "stosb\n\t"
		       "testb %%al,%%al\n\t"
		       "jne 1b\n\t"
		       "jmp 3f\n\t"
		       "2:\n\t"
		       "xorl %5,%5\n\t"
		       "stosb\n\t"
		       "3:\n"
		       :"=&S"(_s1),"=&D"(_d1),"=&c"(_cx)
		       :"2"(0xffffffff),"g"(n),"a"(0),"0"(src),"1"(dst)
		       :"memory"
		       );
		       
}

/*Define the string copy,compare routines*/
static __inline__ void my_strcpy(char *dst,const char *src) {
  int _a1,_s1,_d1;
  __asm__ __volatile__ ("cld\n\t" 
			"1:\n\t" 
			"lodsb\n\t" 
			"stosb\n\t" 
			"testb %%al,%%al\n\t" 
			"jne 1b\n\t" 
			:"=&S"(_s1),"=&D"(_d1),"=&a"(_a1)
			: "0"(src),"1"(dst) : "memory" );
}

static __inline__ void my_strncpy(char *dst,const char *src,int n) {
  int _a1,_c1,_s1,_d1;
  __asm__ __volatile__ ("cld\n\t" 
			"1:decl %2\n\t" 
			"js 2f\n\t" 
			"lodsb\n\t" 
			"stosb\n\t" 
			"testb %%al,%%al\n\t" 
			"jne 1b\n\t" 
			"rep;stosb\n\t" 
			"2:" 
			:"=&S"(_s1),"=&D"(_d1),"=&c"(_c1),"=&a"(_a1)
			:"0"(src),"1"(dst),"2"(n) : "memory"
			);
}

static __inline__ int my_strlen(const char *str) { 
  int i;
  int _d1;
  __asm__ __volatile__ ("cld\n\t"
                        "repne\n\t"
                        "scasb\n\t"
                        "notl %0\n\t"
                        "decl %0\n\t"
			:"=c"(i),"=&D"(_d1)
                        : "0"(0xffffffff),"1"(str),"a"(0) : "memory" );
  return i;
}

/*Returns the minimum of length and bytes*/
static __inline__ int my_strnlen(const char *s,int bytes) {
  int len = strlen(s);
  return MIN(len,bytes); 
}

static __inline__ int my_strcmp(const char *str1,const char *str2) {
  register int __status __asm__("ax");
  int _s1,_d1;
  __asm__ __volatile__("cld\n\t" 
		       "1:\n\t" 
		       "lodsb\n\t" 
		       "scasb\n\t" 
		       "jne 3f\n\t" 
		       "testb %%al,%%al\n\t" 
		       "jne 1b\n\t" 
		       "xorl %%eax,%%eax\n\t" 
		       "jmp 4f\n\t" 
		       "3: movl $1,%%eax\n\t" 
		       "jl 4f\n\t" 
		       "negl %%eax\n\t" 
		       "4:" 
		       :"=a"(__status),"=&S"(_s1),"=&D"(_d1)
		       : "1"(str2),"2"(str1) : "memory" );
  return __status;
}

static __inline__ int  my_strncmp(const char *str1,const char *str2,int n) {
  register int __status __asm__("ax");
  int _c1,_s1,_d1;
  __asm__ __volatile__ ("cld\n\t" 
			"1:decl %3\n\t" 
			"js 2f\n\t" 
			"lodsb\n\t" 
			"scasb\n\t" 
			"jne 3f\n\t" 
			"testb %%al,%%al\n\t" 
			"jne 1b\n\t" 
			"2:xorl %%eax,%%eax\n\t" 
			"jmp 4f\n\t" 
			"3:movl $1,%%eax\n\t" 
			"jl 4f\n\t" 
			"negl %%eax\n\t" 
			"4:\n\t" 
			:"=a"(__status),"=&S"(_s1),"=&D"(_d1),"=&c"(_c1)
			: "1"(str2),"2"(str1),"3"(n) : "memory" );
  return __status;
}

static __inline__ char *my_strchr(const char *str1,int c) {
  char *__status;
  int _s1;
  __asm__ __volatile__("movb %%al,%%ah\n\t" 
		       "cld\n\t" 
		       "1:lodsb\n\t" 
		       "cmpb %%al,%%ah\n\t" 
		       "je 3f\n\t" 
		       "testb %%al,%%al\n\t" 
		       "jne 1b\n\t" 
		       "movl $1,%1\n\t" 
		       "3:movl %1,%0\n\t" 
		       "decl %0\n\t" 
		       :"=a"(__status),"=&S"(_s1)
		       : "1"(str1),"0"(c) : "memory"
		       );
  return  __status;
}

/*last occurence of character c*/
static __inline__ char *my_strrchr(const char *src,int c) {
  int _s1,_cx,_ax;
  __asm__ __volatile__("movb %%al,%%ah\n\t"
		       "1:\n\t"
		       "lodsb\n\t"
		       "testb %%al,%%al\n\t"
		       "je 3f\n\t"
		       "cmpb %%al,%%ah\n\t"
		       "je 2f\n\t"
		       "jmp 1b\n\t"
		       "2:movl %0,%2\n\t"
		       "jmp 1b\n\t"
		       "3:decl %2\n"
		       :"=&S"(_s1),"=&a"(_ax),"=&c"(_cx)
		       :"0"(src),"1"(c),"2"(1)
		       :"memory"
		       );
  return (char *)_cx;
}

#endif
#endif
