##Makefile for  mm
CC := gcc
EXTRA_FLAGS = 
CFLAGS := -D__KERNEL__ $(ENVFLAGS) $(EXTRA_FLAGS)  -I$(TOPDIR)/include 
AS = nasm
OBJS := page_alloc.o slab.o
O_TARGET := mm_target.o

include $(TOPDIR)/Rules.make

clean:
	rm -f *.o *~ *.dep dep *.i


