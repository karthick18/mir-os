/*Author: A.R.Karthick for MIR
*/

#ifdef __KERNEL__
#ifndef _DRIVERS_BLOCK_IDE_IDE_PARTITION_H_
#define _DRIVERS_BLOCK_IDE_IDE_PARTITION_H_

#include "ide.h"

#define PARTITION_OFFSET 0x1BE
#define PARTITION_SIZE   0x10

int get_partitiontable( ulong dev);
int show_partitions(const char *name,ulong dev);
int register_partitions( ulong dev, ulong major );

#endif
#endif
