CC := gcc -m32 -fno-stack-protector
CFLAGS := -D__KERNEL__ -D__MODULE__ -nostdinc  -I$(TOPDIR)/include  
SOURCE := $(shell ls *.c)

all:
	@ ( \
        for i in $(SOURCE);\
        do \
        $(CC) -c $(CFLAGS) $${i};\
        done;\
        )


clean:
	rm -f *~ *.o *.dep dep *.i
