/*Author: TABOS Team, A.R.Karthick for MIR
*/
#ifdef __KERNEL__
#ifndef _BLOCK_IDE_H_
#define _BLOCK_IDE_H_

#define read_le16(X)	( * ( ushort * ) ( X ) )

#define MAX_IDE_DEVICES 4
#define ATAPI	1
#define ATA	2

#define ATA_REG_DATA	0	/* data (16-bit) */
#define ATA_REG_COUNT	2	/* sector count */
#define ATA_REG_SECTOR	3	/* sector */
#define ATA_REG_LOCYL	4	/* LSB of cylinder */
#define ATA_REG_HICYL	5	/* MSB of cylinder */
#define ATA_REG_DRVHD	6	/* drive select: head */
#define ATA_REG_STATUS	7	/* read: status and error flags */
#define ATA_REG_CMD	7	/* write: drive command */
#define ATA_REG_DEVCTRL	0x206	/* write: device control */

#define ATA_CMD_PID	0xA1	/* identify ATAPI device */
#define ATA_CMD_ID	0xEC	/* identify ATA device */

/* ATA registers*/
#define ATA_REG_FEAT	1	// write: feature-reg
#define ATA_REG_CNT	2	// sector count
#define ATA_REG_SECT	3	// sect
#define ATA_REG_LOCYL	4	// LSB of cylinder
#define ATA_REG_HICYL	5	// MSB of cylinder
#define ATA_REG_DRVHD	6	// drive select & head
#define ATA_REG_STAT	7	// read status and error flags
#define ATA_REG_CMD	7	// write drive command
#define ATA_REG_DCR	0x206	// device control register
#define ATA_REG_ERROR	ATA_REG_FEAT

/* ATA commands*/

#define ATA_CMD_READ		0x20	// read one sector
#define ATA_CMD_READMUL		0xC4	// read multiple sectors
#define ATA_CMD_WRITE		0x30	// write one sector
#define ATA_CMD_WRITEMUL 	0xC5	// write multiple sectors
#define ATA_CMD_ID		0xEC	// identify
#define ATA_CMD_PKT		0xA0

#define ATAPI_PH_ABORT		0
#define ATAPI_PH_DONE		3
#define ATAPI_PH_DATAOUT	8
#define ATAPI_PH_CMDOUT		9
#define ATAPI_PH_DATAIN		10
#define ATAPI_REG_REASON	2
#define ATAPI_REG_LOCNT		4
#define ATAPI_REG_HICNT		5
#define ATAPI_CMD_READ10	0x28


/*ide identification structure*/

typedef struct ide_id
{
    ushort config;		/* OBSOLETE: config flags	*/
    ushort cyls;		/* OBSOLETE: physical cylinder	*/
    ushort reserved;		/* reserved			*/
    ushort heads;		/* OBSOLETE: physical heads	*/
    ushort track_bytes;		/* unformarted bytes per track	*/
    ushort sector_bytes;	/* unformarted bytes per sector */
    ushort sectors;		/* OBSOLETE: physical sectors	*/
    ushort vendor0;		/* vendor unique		*/
    ushort vendor1;		/* vendor unique		*/
    ushort vendor2;		/* retired vendor unique	*/
    uchar serial[ 20 ];		/* 0 = not specified		*/
    ushort buf_type;		/* retired			*/
    ushort buf_size;		/* retired, 512 byte increments	*/
				/* 0 = not specified		*/
    ushort ecc_bytes;		/* for r/w long cmds; 0 = N/A	*/
    uchar fw_rev[ 8 ];		/* 0 = not specified		*/
    uchar model[ 40 ];		/* 0 = not specified		*/
    uchar max_multsect;		/* 0 = not implemented		*/
    uchar vendor3;		/* vendor unique		*/
    ushort dword_io;		/* 0 = not implemented, 1 = ok	*/
    uchar vendor4;		/* vendor unique		*/
    uchar capability;		/* (upper byte of word 49)	*/
				/* 3: IORDYsup			*/
				/* 2: IORDYsw			*/
				/* 1: LBA			*/
				/* 0: DMA			*/
    ushort reserved50;		/* reserved (word 50)		*/
    uchar vendor5;		/* OBSOLETE: vendor unique	*/
    uchar tPIO;			/* OBSOLETE: 0=slow, 1=med, 2=fast */
    uchar vendor6;		/* OBSOLETE: vendor unique	*/
    uchar tDMA;			/* OBSOLETE: 0=slow, 1=med. 2=fast */
    ushort field_valid;		/* (word 53)			*/
				/* 2: ultra	word 88		*/
				/* 1: eide	words 64-70	*/
				/* 0: cur	words 54-58	*/
    ushort cur_cyls;		/* OBSOLETE: logical cylinders	*/
    ushort cur_heads;		/* OBSOLETE: logical heads	*/
    ushort cur_sectors;		/* OBSOLETE:logical sectors	*/

    ushort cur_capacity0;	/* OBSOLETE: l total sectors	*/
    ushort cur_capacity1;	/* OBSOLETE: 2 words!		*/
    uchar multsect;		/* current multiple sector count*/
    uchar multsect_valid;	/* when (bit0==1) multisect=ok  */
    uint lba_capacity;		/* OBSOLETE: total sector num.	*/
    ushort dma_1word;		/* OBSOLETE: single-word dma inf*/
    ushort dma_mword;		/* multiple-word dma info	*/
    ushort advanced_pio_mode;
    ushort min_dma_cycles;
    ushort recommended_dma_cycles;
    ushort min_noflow_pio_cycles;
    ushort min_iordy_pio_cycles;
    ushort misc12;
    ushort misc13;
    ushort misc14;
    ushort misc15;
    ushort misc16;
    ushort misc17;
    ushort queue_depth;
    ushort misc18;
    ushort misc19;
    ushort misc20;
    ushort misc21;
    ushort major_version;
    ushort minor_version;
    ushort supported_features_1;
    ushort supported_features_2;
    ushort supported_features_3;
    ushort enabled_features_1;
    ushort enabled_features_2;
    ushort enabled_features_3;
    ushort dma_mode_2;
    ushort security_erase_time;
    ushort enhanced_security_earse_time;
    ushort current_advance_power_value;
    ushort master_password_revision_code;
    ushort hardware_reset_result;
    char misc22[ 0xfe - 0xbc ];
    ushort removeable_media_status_features;
    ushort security_status;
    char misc23[ 0x140 - 0x102 ];
    ushort cfa_power_mode_1;
    char mode24[ 0x150 - 0x142 ];
    char mode25[ 0x1fe - 0x150 ];
    ushort integrity;
} ide_id;

typedef struct partition
{
    uchar active;
    uchar system;

    uint start_track;
    uint start_head;
    uint start_sector;

    uint end_track;
    uint end_head;
    uint end_sector;
    uint offset;
    uint size;
} part __attribute__ ((packed));

struct ide_devices
{
    uchar detect;

#define MAX_IDE_SERIAL_NUMBER 20
#define MAX_IDE_MODEL_NUMBER  40
#define MAX_IDE_FIRMWARE      8

    uchar serial_number[ MAX_IDE_SERIAL_NUMBER ];
    uchar model_number[ MAX_IDE_MODEL_NUMBER ];
    uchar firmware[ MAX_IDE_FIRMWARE ];
    uchar flags;
    ushort base;
    uchar connection;
    ushort io_irq;
    ushort bytes_per_block;
    ushort multi_count;
    ushort type;
    ushort lba;
    ushort dma;
    uint cylinders;
    uint heads;
    ulong sectors;
    
    ushort start;
    
    part *partitions;  /*total PARTITION_SHIFT possible*/
};

extern struct ide_devices *ide_device[];
extern void ide_sleep( uint msecs);
extern int ide_select( ushort port, uchar sel );
extern void ide_insw( ushort port, ushort *data, uint count );
extern void ide_outsw( ushort port, ushort *data, uint count );
extern void init_atadevice( uchar drive );
extern void init_atapidevice(uchar drive);
extern ushort ide_atapi_read_discard(ushort ,uchar *,ushort );
extern void ide_atapi_mode_sense(ulong);
extern int ide_atapi_eject(ulong , int );
extern int ide_atapi_pause(ulong , int );
extern int ide_atapi_play(ulong, int);
extern int ide_atapi_toc(uchar , uchar *);

struct play_ioctl;
struct buffer_head;
extern int ide_atapi_play_msf(uchar , struct play_ioctl *);

extern uchar ide_wait_irq( ushort port, ushort irq_mask, uchar status_mask,
    uchar status_bits, uint timeout );
extern ushort ide_wait_for_interrupt( ushort irq_mask, uint timeout );
extern uchar ide_wait( int port, uchar status_mask, uchar status_bits, uint timeout );
extern int init_ide(int argc,char **argv);
extern int ide_hd_rw( struct buffer_head *,int size,int cmd);
extern int cdrom_drives[MAX_IDE_DEVICES];
static __inline__ ushort bswap16( ushort arg )
{
    return ( ( arg << 8 ) & 0xFF00 ) | ( ( arg >> 8 ) & 0x00FF );
}

#define read_be16( X ) bswap16( * ( ushort * ) (X ) )

#endif /*_BLOCK_IDE_H*/
#endif/* __KERNEL__ */
