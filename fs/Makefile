CC := gcc
CFLAGS := -D__KERNEL__ $(ENVFLAGS) -I$(TOPDIR)/include
AS = nasm
OBJS :=     vfs.o file.o fat/fat_target.o
O_TARGET := fs_target.o
SUBDIRS  := fat

include $(TOPDIR)/Rules.make

clean:
	@ (\
	for dir in $(SUBDIRS) ;\
	do \
	make -C $${dir} clean;\
	done;\
	)
	rm -f *.o *~ *.dep dep *.i

