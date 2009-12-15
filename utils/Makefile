CC := gcc
CFLAGS := -D__KERNEL__ -I$(TOPDIR)/include -Wall
AS = nasm
OBJS := rbtree.o vsprintf.o ctype.o utils.o
O_TARGET := utils_target.o

include $(TOPDIR)/Rules.make

clean:
	rm -f *.o *~ *.dep dep *.i

