/*
;
;    MIR-OS graphics geometric figure plotting
;       plot lines, box's. TODO: Circles, crooked lines
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
;   A.R.Karthick        : Some minor changes and reorganisation.
*/

#ifdef VESA
#include <asm/vesa.h>

void plothline(int x , int y, int length,int color)
{
int i;
        for(i = 0; i < length; i++)
        {
                video_pointer[(y*1024) + x] = color;
                x++;
        }
}
                                                                                
void plotvline(int x , int y , int length , int color )
{
int i;
        for(i = 0; i < length; i++)
        {
                video_pointer[(y*1024) + x] = color;
                y++;
        }
}

void plotrect(int x , int y , int width , int height , int border , int color )
{
int cur;
        //Top...
        for(cur = 0; cur < border; cur++)
                plothline(x, y + cur, width, color);
        //Left...
        for(cur = 0; cur < border; cur++)
                plotvline(x + cur, y, height, color);
        //Right...
        for(cur = 0; cur < border; cur++)
                plotvline(x + width - border + cur, y, height, color);
        //Bottom...
        for(cur = 0; cur < border; cur++)
                plothline(x, y + height - border + cur, width, color);
}
#endif
