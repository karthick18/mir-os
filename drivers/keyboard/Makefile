CC = gcc
CFLAGS = -D__KERNEL__ $(EXTRA_FLAGS) $(DEBUG) -nostdinc -I$(TOPDIR)/include 
OBJS = keyboard.o keytables.o keyio.o
O_TARGET = keyboard_driver.o
include $(TOPDIR)/Rules.make

clean:
	rm -f *~ *.dep *.o