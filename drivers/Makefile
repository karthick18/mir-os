CC := gcc
CFLAGS := -D__KERNEL__  $(ENVFLAGS) -Wall -nostdinc -I$(TOPDIR)/include
AS = nasm
OBJS := keyboard/keyboard_driver.o fdc1.o geom.o tty_raw.o tty_vesa.o sound/sb16_driver.o mouse/ps2_driver.o terminal.o block/ide_driver.o
SUBDIRS := keyboard sound mouse block
O_TARGET := driver_target.o

include $(TOPDIR)/Rules.make

clean:
	rm -f *.o *.dep *~ *.i
	@ (\
	for dir in $(SUBDIRS);\
	do \
	$(MAKE) -C $${dir} clean;\
	done;\
	)



