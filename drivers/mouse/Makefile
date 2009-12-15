CC = gcc
CFLAGS = -D__KERNEL__ $(EXTRA_FLAGS) $(DEBUG) -nostdinc -I$(TOPDIR)/include 
OBJS = ps2.o
O_TARGET = ps2_driver.o
include $(TOPDIR)/Rules.make

clean:
	rm -f *~ *.dep *.o
