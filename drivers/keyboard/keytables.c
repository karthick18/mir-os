/* MIR-OS Keyboard translation tables:
;   a.r.karthick@gmail.com ; a_r_karthic@rediffmail.com
;  
;    Copyright (C) 2003 
;              i) TABOS team.
;	      ii) A.R.Karthick
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
; The table definitions were taken from TABOS and adapted,and modified
; for MIR.
;
*/

#include <mir/kernel.h>
#include <asm/kbd.h>

unsigned short *key_ascii_table,*key_shift_table, *key_control_table,*key_capslock_table;

/* default mapping table for the US keyboard layout */
static unsigned short standard_key_ascii_table[ KEY_MAX ] =
{
    /* start	*/	0,
    /* alphabet	*/	'a', 'b', 'c', 'd', 'e', 'f', 'g', 'h', 'i', 'j', 'k',
			'l', 'm', 'n', 'o', 'p', 'q', 'r', 's', 't', 'u', 'v',
			'w', 'x', 'y', 'z',
    /* numbers	*/	'0', '1', '2', '3', '4', '5', '6', '7', '8', '9',
    /* numpad	*/	'0', '1', '2', '3', '4', '5', '6', '7', '8', '9',
    /* func keys*/	0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF,
			0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF,
    /* misc	*/	27, '`', '-', '=', 8, 9, '[', ']', 10, ';', '\'',
			'\\', '\\', ',', '.', '/', ' ',
    /* controls */	0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF,
			0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF,
    /* numpad	*/	'/', '*', '-', '+', '.', 10,
    /* modifiers*/	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
};

/* capslock mapping table for the US keyboard layout */
static unsigned short standard_key_capslock_table[ KEY_MAX ] =
{
    /* start	*/	0,
    /* alphabet	*/	'A', 'B', 'C', 'D', 'E', 'F', 'G', 'H', 'I', 'J', 'K',
			'L', 'M', 'N', 'O', 'P', 'Q', 'R', 'S', 'T', 'U', 'V',
			'W', 'X', 'Y', 'Z',
    /* numbers	*/	'0', '1', '2', '3', '4', '5', '6', '7', '8', '9',
    /* numpad	*/	'0', '1', '2', '3', '4', '5', '6', '7', '8', '9',
    /* func keys*/	0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF,
			0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF,
    /* misc	*/	27, '`', '-', '=', 8, 9, '[', ']', 10, ';', '\'',
			'\\', '\\', ',', '.', '/', ' ',
    /* controls */	0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF,
			0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF,
    /* numpad	*/	'/', '*', '-', '+', '.', 10,
    /* modifiers*/	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
};

/* shifted mapping table for the US keyboard layout */
static unsigned short standard_key_shift_table[ KEY_MAX ] =
{
    /* start	*/	0,
    /* alphabet	*/	'A', 'B', 'C', 'D', 'E', 'F', 'G', 'H', 'I', 'J', 'K',
			'L', 'M', 'N', 'O', 'P', 'Q', 'R', 'S', 'T', 'U', 'V',
			'W', 'X', 'Y', 'Z',
    /* numbers	*/	')', '!', '@', '#', '$', '%', '^', '&', '*', '(',
    /* numpad	*/	'0', '1', '2', '3', '4', '5', '6', '7', '8', '9',
    /* func keys*/	0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF,
			0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF,
    /* misc	*/	27, '~', '_', '+', 8, 9, '{', '}', 10, ':', '"',
			'|', '|', '<', '>', '?', ' ',
    /* controls */	0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF,
			0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF,
    /* numpad	*/	'/', '*', '-', '+', '.', 10,
    /* modifiers*/	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
};

/* ctrl+key mapping table for the US keyboard layout */
static unsigned short standard_key_control_table[ KEY_MAX ] =
{
    /* start	*/	0,
    /* alphabet	*/	 1,  2,  3,  4,  5,  6,  7,  8,  9, 10, 11, 12, 13,
			14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26,
    /* numbers	*/	2, 2, 2, 2, 2, 2, 2, 2, 2, 2,
    /* numpad	*/	'0', '1', '2', '3', '4', '5', '6', '7', '8', '9',
    /* func keys*/	0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF,
			0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF,
    /* misc	*/	27, 2, 2, 2, 127, 127, 2, 2, 10, 2, 2,
			2, 2, 2, 2, 2, 2,
    /* controls */	0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF,
			0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF,
    /* numpad	*/	2, 2, 2, 2, 2, 10,
    /* modifiers*/	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
};

/* set_standard_keyboard()
 * sets up pointers ready to use the standard US keyboard mapping
 *
*/
void set_standard_keyboard(void)
{
    key_ascii_table	= standard_key_ascii_table;
    key_capslock_table	= standard_key_capslock_table;
    key_shift_table	= standard_key_shift_table;
    key_control_table	= standard_key_control_table;
}

