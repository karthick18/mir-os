/*   MIR-OS : General utility functions
;    Copyright (C) 2003 A.R.Karthick 
;    <a_r_karthic@rediffmail.com,karthick_r@infosys.com>
;
;    This program is free software; you can redistribute it and/or modify
;    it under the terms of the GNU General Public License as published by
;    the Free Software Foundation; either version 2 of the License, or
;    (at your option) any later version.
;
;    This program is distributed in the hope that it will be useful,
;    but WITHOUT ANY WARRANTY; without even the implied warranty of
;    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
;    GNU General Public License for more details.
;
;    You should have received a copy of the GNU General Public License
;    along with this program; if not, write to the Free Software
;    Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
;
;
;    
*/

#include <mir/kernel.h>
#include <mir/mir_utils.h>
#include <asm/string.h>

char * ___strtok;
int errno;

/*String to float*/
double atof(const char *buffer) { 
  double result = 0,power = 1,sign = 1;
  const char *str = buffer;
  int base = 10; /*only base 10 allowed*/
  errno = -1;
  while(isspace(*str) ) ++str;
  sign = (*str == '-') ? -1 : 1;
  if(sign < 0) 
    ++str;
  for(result = 0; *str && isdigit(*str) ; ++str) { 
    int val = *str - '0';
    if(val >= base) { 
      goto out;
    }
    result = result * base + val;
  }
  if(*str == '.') {
    ++str;
    for(; *str  && isdigit(*str) ; ++str) {
      int val = *str - '0';
      if(val >= base) {
        goto out;
      }
      result = result * base + val;
      power *= 10;
    }
  }
 if(*str) {
    goto out;
  }
 errno = 0;
 out: 
 return ( (result * sign)/power );
}

/*Generic definition of atoi:
  Handles octal and hexa and binary base entries:
*/
    
int atoi(const char *buffer) { 
  int result = 0,sign = 1;
  int base = 10; /*default base*/
  const char *str = buffer;
  errno = -1;
  while(isspace(*str) ) ++str;
  sign = (*str == '-') ? -1:1;
  if(sign < 0 )  
    ++str;
  if(str[0] == '0' ) {
    base = 8; /*take it as octal equivalent*/
    ++str;
    if(tolower(str[0]) == 'x' ) {
      base = 16;
      ++str;  
    }else if(tolower(str[0]) == 'b') {
      base = 2;
      ++str;
    }
  }
  for(result = 0; *str && isxdigit(*str) ; ++str) {
    int val = (isalpha(*str) ? ( isupper(*str) ? tolower(*str) : *str) - 'a' + 10 : *str - '0' );
    if(val >= base) {
      goto out;
    }
    result = result * base + val; 
  }
  if(*str) { 
    goto out;
  }
  errno = 0;
 out:
  return result*sign;
}


/*Qsort:
 Generic QuickSort routine
*/

void qsort (void *base, int nr,int size, compare_function *function) { 
  register int i,j,k;
  for(i = nr/2; i > 0 ; i/=2 ) { 
    for(j = i ; j < nr ; ++j ) { 
      for(k = j - i ; k >= 0; k -= i ) { 
	unsigned char *start = (unsigned char *) base;
	unsigned char *a  =  start + k * size;
	unsigned char *b  =  start + (k + i ) *size;
	if( function ( (const void *)a,(const void *) b ) > 0 ) {
	  /*swap objects*/
	  register int c;
	  unsigned char temp;
	  for( c = 0; c < size ; ++c ) { 
	    temp = a[c];
	    a[c] = b[c];
	    b[c] = temp;
	  }
	}
      }
    }
  }
}

char * my_strsep(char **s, const char *ct)
{
        char *sbegin = *s, *end;

        if (sbegin == NULL)
                return NULL;

        end = strpbrk(sbegin, ct);
        if (end)
                *end++ = '\0';
        *s = end;

        return sbegin;
}


char * my_strtok(char * s,const char * ct)
{
        char *sbegin, *send;

        sbegin  = s ? s : ___strtok;
        if (!sbegin) {
                return NULL;
        }
        sbegin += strspn(sbegin,ct);
        if (*sbegin == '\0') {
                ___strtok = NULL;
                return( NULL );
        }
        send = strpbrk( sbegin, ct);
        if (send && *send != '\0')
                *send++ = '\0';
        ___strtok = send;
        return (sbegin);
}
