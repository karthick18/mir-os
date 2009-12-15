CC = gcc
CFLAGS = -D__KERNEL__ $(EXTRA_FLAGS) $(DEBUG) -nostdinc -I$(TOPDIR)/include 
OBJS = sb16.o
O_TARGET = sb16_driver.o
include $(TOPDIR)/Rules.make

clean:
	rm -f *~ *.dep *.o
