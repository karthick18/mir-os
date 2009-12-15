/*
;
;    MIR-OS Variable Arguments Header
;       Handles the macros needed for variable arguments
;
;    Copyright (C) 2003 Pratap <pratap@starch.tk>
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
*/

#ifndef _STDARG_H
#define _STDARG_H
typedef unsigned char *va_list;
#define	VA_SIZE(TYPE)					\
	((sizeof(TYPE) + sizeof(int) - 1)	\
		& ~(sizeof(int) - 1))
#define	va_start(AP, LASTARG)	\
	(AP=((va_list)&(LASTARG) + VA_SIZE(LASTARG)))
#define va_end(AP)
#define va_arg(AP, TYPE)	\
	(AP += VA_SIZE(TYPE), *((TYPE *)(AP - VA_SIZE(TYPE))))

#endif
