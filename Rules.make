LD = ld -m elf_i386
CFLAGS += $(DEBUG) $(DEFINES)
CC := gcc -m32 -fno-stack-protector

%.s: %.c
	$(CC) $(CFLAGS) -S $< > $@

%.i: %.c
	$(CC) $(CFLAGS) -E $< > $@

%.o: %.s
	$(AS) $(AFLAGS) -o $@ $<

%.o: %.asm
	$(AS) $(AFLAGS) -o $@ $<

%.o: %.c	
	$(CC) $(CFLAGS) -o $@ -c $<

.S.o:
	$(CC) $(CFLAGS) $(ASFLAGS) -c -o $*.o $<

$(O_TARGET): .dep subdirs $(OBJS) 
	$(LD) -r -o $@ $(OBJS)

subdirs:
	@ (\
	for dir in $(SUBDIRS) ;\
	do \
	make -C $${dir};\
	done ;\
	)

.dep dep depend:
	$(CC) $(CFLAGS) -M *.c > $@ 2>/dev/null


ifeq ($(wildcard .dep),.dep)
include .dep
endif

$(O_KERNEL_TARGET): $(KERNEL_OBJS)
	$(LD) --oformat elf32-$(ARCH) -T arch-$(ARCH)/mirkernel.lds -o $@ $^
#	$(LD)  -o $@  -q -S -m elf_$(ARCH) --oformat elf32-$(ARCH)  -Ttext 0x100000 -e _start $^



