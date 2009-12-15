/*Author: A.R.Karthick for MIR:
  Moved the definitions from fatvfs.c
*/
#ifdef __KERNEL__
#ifndef _FAT_H
#define _FAT_H

typedef struct {
unsigned char jmp[3];
unsigned char oem_name[8];
unsigned short bytes_sec;
unsigned char sec_clu;
unsigned short rsvd_sec;
unsigned char no_fats;
unsigned short no_root;
unsigned short tot_sector;
unsigned char media;
unsigned short fat_sectors;
unsigned short sec_track;
unsigned short no_heads;
unsigned int hidd_sectors;
unsigned int totsec32;

/* BS */
unsigned char drvnum;
unsigned char res;
unsigned char bootsig;
unsigned long vol_id;
unsigned char vol_label[11];
unsigned char fsystype[8];
} __attribute__((packed)) bpb;

typedef struct {
unsigned char name[11];
unsigned char attr;
#define ATTR_READ_ONLY 0x01
#define ATTR_HIDDEN 0x02
#define ATTR_SYSTEM 0x04
#define ATTR_VOLUME_ID 0x08
#define ATTR_DIRECTORY 0x10
#define ATTR_ARCHIVE 0x20
unsigned char res;
unsigned char time_tenth;
unsigned short time;
unsigned short date;
unsigned short last_access_date;
unsigned short res1; /* res for fat12/16 */
unsigned short wrt_time;
unsigned short wrt_date;
unsigned short cluster;
unsigned long size;
} __attribute__((packed)) dir_entry;

#define FAT12_NAME  "fat12"
#define TOT_FSEC 512*9

extern int vfs_fatinit(void);
extern char *attr_read(unsigned char attr);

extern void list_dir(int arg);
extern void cat(int arg);
extern void cd(int arg);
extern void pwd(int arg);
extern void mount(int arg);
extern void umount(int arg);
#endif
#endif
