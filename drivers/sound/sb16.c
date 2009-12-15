/*
;
;    MIR-OS Sound Blaster 16 Generic driver
;       SB-16 code + Mixer + .WAV player  (TODO: mp3 :)), not even 20% complete :(
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
;    A.R.Karthick: Made sb_init get inside .inittext  
;                  Changed the IRQ entry to entry.S,and setidt interface a bit.
*/
#include<mir/kernel.h>
#include <asm/irq.h>
#include <asm/io.h>
#include <mir/init.h>

static int iobase, irq, dma, dma16, MixerAddr, MixerData;
static long tbaddr = 0x90000;

#define LoByte(x)(short)(x & 0x00FF)
#define HiByte(x)(short)((x&0xFF00)>>8)


typedef struct  /* struct to store ALL mixer values */
{unsigned char masterleft,masterright; /* starts at 0x30 */
 unsigned char vocleft,vocright;
 unsigned char midileft,midiright;
 unsigned char cdleft,cdright;
 unsigned char lineleft,lineright;
 unsigned char micvolume;
/* all the above are 5 bit values */
 unsigned char pcspeaker;
 unsigned char outputswitches;
 unsigned char inputswitchesleft;
 unsigned char inputswitchesright;
 unsigned char inputgainleft,inputgainright;
 unsigned char outputgainleft,outputgainright;
 unsigned char agc;
/* all the below are 4 bit values */
 unsigned char trebleleft,trebleright;
 unsigned char bassleft,bassright;
} mixer_values __attribute__ ((packed));

typedef struct {
unsigned char riff[4];
unsigned long riff_len;
unsigned char wave[4];

unsigned char fmt[4];
unsigned long fmt_len;
unsigned short type; // mono or stereo = 0:1
unsigned short no_chans; //no of channels
unsigned long sample_rate; // sample rate in Hz
unsigned long bytes_sec; 
unsigned short bytes_sample; // 1 = 8 bit mono
unsigned short bites_sample; 

unsigned char data[4];
unsigned long data_len;
} wav_header __attribute__ ((packed));

/* prototypes */
unsigned char get_mixer_val(unsigned char src);
void get_mixer (mixer_values *val);
void print_mixer (mixer_values *val);
unsigned char set_mixer_val(unsigned char src, unsigned char val);
void set_mixer (mixer_values val);
extern void sb16_handler();

mixer_values old_mix, new_mix;


void dspwrite ( unsigned char c )
{
    while(inb(iobase+0xC)&0x80); 
    outb(iobase+0xC,c);
}

unsigned char dspread ( void )
{
    while(!(inb(iobase+0xE)&0x80)) continue; //wait for data to be available
    return(inb(iobase+0xA));
}


static int sb16_interrupt(int irq,void *device,struct thread_regs *regs) {
	printf ("SB16 Handler!\n");
	return 0;
}

void __init
sb_init () {
int ra, rb, fd, i;
unsigned char buf[512];
wav_header *wav_h;


// chk if sb16 present by brute force reseting
printf ("SB16: Sound Blaster 16 initializing\n");
    for(iobase=0x220;iobase<=0x280;iobase+=0x20)
    {
        if(inb(iobase)==0xFF) continue;
        outb(iobase+0x6,0x01);  // Reset Sound Blaster
        for(ra=0;ra<65000;ra++) continue; //At least 3ms delay
        outb(iobase+0x6,0x00);
        for(ra=0;ra<65000;ra++) if(dspread()==0xAA) break;
        if(ra<65000) break;
    }
    if(iobase>0x280)
    {
        printf("\tSound Blaster not detected\n");
	return; //bad luck
    }
	else 
		printf ("\tSB16 detected: IOBASE = 0x%x, ", iobase);

// get dsp version.
	dspwrite (0xE1);
	ra = dspread();
	rb = dspread();
	printf ("DSP Version = %d.%d, ", ra, rb);

// set mixer addy and data
	MixerAddr= iobase + 0x0004;
	MixerData= iobase + 0x0005;


// chk for irq, dma 8bit and dma 16bit.
    outb(iobase+0x4,0x80);
    switch(inb(iobase+0x5)&0xF)
    {
        case 0x00:
            printf("\n\tPANIC: IRQ Line not set\n");
		return;
        case 0x01: irq=2; break;
        case 0x02: irq=5; break;
        case 0x04: irq=7; break;
        case 0x08: irq=10; break;
    }
    outb(iobase+0x4,0x81);
    switch(inb(iobase+0x5)&0x0B)
    {
        case 0x00:
            printf("\n\tPANIC: DMA Channel not set\n");
		return;
        case 0x01: dma=0; break;
        case 0x02: dma=1; break;
        case 0x08: dma=3; break;
    }
    switch(inb(iobase+0x5)&0xE0)
    {
        case 0x00: dma16=dma; break;
        case 0x20: dma16=5; break;
        case 0x40: dma16=6; break;
        case 0x80: dma16=7; break;
    }

    printf("IRQ = %d, ",irq);
    printf("DMA = %d, ",dma);
    printf("DMA16 = %d\n",dma16);

//mess arnd wid mixer, put volume to max
printf ("\tMIXER: Adjusting the values\n");
old_mix.masterleft = 32;
old_mix.masterright = 32;
set_mixer (old_mix);
//print_mixer (&old_mix);

//setup irqh n pic
request_irq(irq,&sb16_interrupt,"SB16",INTERRUPT_IRQ,NULL);

/*
The below is test code to play 8bit, 11025Hz .wav file

on Bochs i got something like a ring, but it wasent proper
pls chk if u get the proper sound.


*/



/*
fd = mir_open ("ringin.wav", 0);
mir_read (fd, buf, 0);
memcpy (tbaddr, buf+44, 512-44);

tbaddr += 512-44;
for (i = 0; i< 20; i++) {
	mir_read (fd, buf, 0);
	memcpy (tbaddr, buf, 512);
	tbaddr+=512;
}
outb (iobase+0xc, 0xd1);
#define LEN 20*512+512-44-1
dma_xfer (dma, 0x90000, LEN, 1);

//freq
dspwrite (0x41);
#define FREQ 11025
dspwrite (HiByte(FREQ));
dspwrite (LoByte(FREQ));
dspwrite (0xc0);
dspwrite (0x00);
dspwrite (LoByte(LEN));
dspwrite (HiByte(LEN));

*/
}



unsigned char get_mixer_val(unsigned char src)
{unsigned char temp;
 outb(MixerAddr,src);
 temp=inb(MixerData);

 // The bit shifts are to align the bits to the lower end of the byte appropriatly

 if(src >=0x30 && src <=0x3A)
  	return temp>>3;
  
 else if(src >=0x3F && src <=0x42)
   return temp>>6;
   
 else if(src >=0x44 && src <=0x47)
  return temp>>4;
}

void get_mixer (mixer_values *val) {
unsigned char *ptr=&(val->masterleft), index;

for(index=0x30;index<=0x47;index++) {
*ptr=get_mixer_val(index);
 ptr++;
}
}

unsigned char set_mixer_val(unsigned char src, unsigned char val) {
// The bit shifts are to align the bits to the lower end of the byte appropriatly

 if(src >=0x30 && src <=0x3A) {
	outb (MixerAddr,src);
	outb (MixerData,val << 3);
 }
  
 else if(src >=0x3F && src <=0x42) {
	outb (MixerAddr,src);
	outb (MixerData,val << 6);
 }
   
 else if(src >=0x44 && src <=0x47) {
	outb (MixerAddr,src);
	outb (MixerData,val << 4);
 }
}

void set_mixer (mixer_values val) {
unsigned char *ptr=&(val.masterleft), index;

for(index=0x30;index<=0x47;index++) {
set_mixer_val(index, *ptr);
ptr++;
}

}


void print_mixer (mixer_values *val) {
printf ("MASTER LEFT: %d \t MASTER RIGHT : %d\n", val->masterleft, val->masterright);
}


