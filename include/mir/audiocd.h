/*Authors: TABOS, ported to MIR by A.R.Karthick
*/
#ifdef __KERNEL__
#ifndef _AUDIOCD_H
#define _AUDIOCD_H


#define ATAPI_CMD_START_STOP	0x1B	/* eject/load */
#define ATAPI_CMD_PLAY		0x47	/* play audio */
#define ATAPI_CMD_READTOC	0x43	/* read audio table-of-contents */
#define ATAPI_CMD_PAUSE		0x4B

typedef struct
{
    uchar res1;
    uchar kindof;
    uchar number;
    uchar res2;
    uchar start[ 4 ];
} onetrk __attribute__ ((packed));

typedef struct
{
    ushort len;
    uchar begtrk;
    uchar endtrk;
    onetrk trk[ 50 ];
}toc10 __attribute__ ((packed));

struct play_ioctl
{
    uchar start_m;
    uchar start_s;
    uchar start_f;
    uchar end_m;
    uchar end_s;
    uchar end_f;
};
#endif
#endif
