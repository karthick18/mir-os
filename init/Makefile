CC := gcc
SRCS := main.c
CFLAGS := -D__KERNEL__ $(ENVFLAGS) -I$(TOPDIR)/include
AS = nasm
OBJS := main.o
O_TARGET := main_target.o

include $(TOPDIR)/Rules.make

clean:
	rm -f *.o *~ *.dep dep *.i


