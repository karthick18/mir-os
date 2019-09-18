##Makefile for the MIR kernel
CC := gcc -m32 -fno-stack-protector
TOPDIR := $(shell pwd)
ARCH   := i386
SUBDIRS := init arch-$(ARCH)/kernel arch-$(ARCH)/boot drivers/ fs kernel mm ipc modules utils 
BOCHS  := y
ifeq ($(strip $(BOCHS)),y)
 EXTRA_FLAGS := -DBOCHS
endif

TARGET_PATH := 
TARGET_IMAGE := img
O_KERNEL_TARGET := mir.elf
KERNEL_OBJS = arch-$(ARCH)/boot/multiboot.o init/main_target.o arch-$(ARCH)/kernel/arch_target.o \
              drivers/driver_target.o fs/fs_target.o kernel/kernel_target.o \
              mm/mm_target.o ipc/ipc_target.o utils/utils_target.o 

AS := nasm
DEFINES := -fomit-frame-pointer  -fno-common -fno-strict-aliasing
#ENVFLAGS := -DVESA
AFLAGS := $(ENVFLAGS) -felf
BOOTFILES := arch-$(ARCH)/boot/multiboot.o
BUILD_MIR := wrt
export AFLAGS ENVFLAGS DEFINES TOPDIR ARCH EXTRA_FLAGS CC

all: prereq create_symlink mir_all
prereq:
	$(shell ./install-prerequisites.sh)
install: cat
create_symlink:
	@ (\
	if ! test -d $(TOPDIR)/include/asm;\
	then \
	( cd $(TOPDIR)/include ; ln -sf asm-$(ARCH) asm );\
	fi;\
	) 

mir_all: mir_kernel $(O_KERNEL_TARGET)

mir_kernel:
	@ ( \
	for i in $(SUBDIRS);\
	do \
	$(MAKE) -C $${i};\
	done;\
	)

include Rules.make

.PHONY: cat

cat:
ifeq ($(strip $(BOCHS)),y)	
	./st.sh
else
	mcopy mir.elf a:/boot/mir.elf
	mcopy modules/*.o a:/modules
endif
#cat:    $(BOOTFILES) $(BUILD_MIR)
#	cat $(BOOTFILES) $(O_KERNEL_TARGET) > .test1
#	./wrt .test1 $(TARGET_PATH)$(TARGET_IMAGE)
#	rm -f .test1
#	cp img /

$(BUILD_MIR): utils/build.o
	$(CC) -o $@ $<

.PHONY: clean

clean: 
	@ (\
	for i in $(SUBDIRS);\
	do \
	$(Q)$(MAKE) -C $${i} clean;\
	done;\
	)
	rm -f *.o *.bin *~ wrt mir.elf;\
	find . -name ".dep" -exec rm -f {} \;
	find . -name "*~" -exec rm -f {} \;

.PHONY: TAGS

TAGS:
	find . -name "*.[chS]" -print | etags -
