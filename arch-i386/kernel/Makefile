CC := gcc
CPP := cpp
ASFLAGS:= -D__ASSEMBLY__
CFLAGS := -D__KERNEL__ $(EXTRA_FLAGS)  -nostdinc -I$(TOPDIR)/include 
CPPFLAGS = -P
AS = as
OBJS := timer.o irq.o setup.o system.o vmalloc.o reboot.o exceptions.o traps.o processor.o 8259a.o

O_TARGET := arch_target.o

include $(TOPDIR)/Rules.make

clean:
	rm -f *.o *~ *.dep dep *.i
