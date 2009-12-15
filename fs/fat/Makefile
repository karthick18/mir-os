CC = gcc
CFLAGS = -D__KERNEL__ $(DEBUG) $(EXTRA_FLAGS) $(ENVFLAGS) -nostdinc -I$(TOPDIR)/include 
OBJS  := fatvfs.o fat_commands.o
O_TARGET = fat_target.o
include $(TOPDIR)/Rules.make

clean:
	rm -f *~ *.dep *.o
