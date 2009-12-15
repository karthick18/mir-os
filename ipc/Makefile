CC := gcc
CFLAGS := -D__KERNEL__ $(ENVFLAGS)  -Wall -nostdinc -I$(TOPDIR)/include 
AS = nasm
OBJS := sem.o
O_TARGET := ipc_target.o

include $(TOPDIR)/Rules.make

clean:
	rm -f *.o *~ *.dep dep *.i

