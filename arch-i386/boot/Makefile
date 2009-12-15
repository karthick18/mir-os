CC = gcc
ASFLAGS = -D__ASSEMBLY__ -D__KERNEL__ $(ENVFLAGS)
AFLAGS := -f elf 
DEBUG := -DDEBUG
CFLAGS:= -Wall -fomit-frame-pointer -fno-strict-aliasing -I$(TOPDIR)/include $(DEBUG)
OBJS = multi.o page.o
O_TARGET = multiboot.o

include $(TOPDIR)/Rules.make

clean:
	rm -f *.o *~ *.dep dep *.i
