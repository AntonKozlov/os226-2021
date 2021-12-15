CFLAGS = -g -MMD -MT $@ -MF $@.d
ASFLAGS = $(CFLAGS)

KERNEL_START := 0xf00000000
USERSPACE_START := 0x400000

CFLAGS += -DIKERNEL_START=$(KERNEL_START) -DIUSERSPACE_START=$(USERSPACE_START)

all : main rootfs.cpio

main : $(patsubst %.c,%.o,$(patsubst %.S,%.o,$(filter-out %.app.c,$(wildcard *.[cS]))))
	$(CC) -pthread $(LDFLAGS) $^ $(LOADLIBES) $(LDLIBS) -o $@

APPS = $(patsubst %.app.c,%.app,$(wildcard *.app.c))

$(APPS) : %.app : %.app.c
	$(CC) -fno-stack-protector -fno-pic -Wl,-Ttext-segment=$(USERSPACE_START) -nostdlib -e main -static -x c $< -o $@

rootfs.cpio : $(APPS)
	ls -1 $^ | cpio -o -H bin > $@

clean :
	rm -f *.[od] main rootfs.cpio *.app

-include $(wildcard *.d)

