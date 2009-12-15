CC = gcc
##DEBUG = -DIDE_DEBUG
CFLAGS = -D__KERNEL__ $(DEBUG) $(EXTRA_FLAGS) $(ENVFLAGS) -nostdinc -I$(TOPDIR)/include 
OBJS = idemain.o idehd.o idecd.o ide_partition.o blockops.o
O_TARGET = ide_driver.o
include $(TOPDIR)/Rules.make

clean:
	rm -f *~ *.dep *.o
