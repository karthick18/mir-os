CC := gcc
CFLAGS := -D__KERNEL__ $(DEBUG) $(ENVFLAGS) $(EXTRA_FLAGS) -Wall -nostdinc -I$(TOPDIR)/include 
OBJS = tasklet.o elf.o sched.o timer.o write.o module.o buffer_head.o device.o
O_TARGET := kernel_target.o

include $(TOPDIR)/Rules.make

clean:
	rm -f *.o *~ *.dep dep *.i



