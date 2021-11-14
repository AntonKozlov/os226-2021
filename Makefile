CFLAGS = -g -MMD -MT $@ -MF $@.d
ASFLAGS = $(CFLAGS)

KERNEL_START := 0xf00000000
USERSPACE_START := 0x400000

CFLAGS += -DIKERNEL_START=$(KERNEL_START) -DIUSERSPACE_START=$(USERSPACE_START)

all : main

main : $(patsubst %.c,%.o,$(patsubst %.S,%.o,$(filter-out %.app.c,$(wildcard *.[cS]))))
	$(CC) $(LDFLAGS) $^ $(LOADLIBES) $(LDLIBS) -o $@

APPS = $(patsubst %.app.c,%.app,$(wildcard *.app.c))

$(APPS) : %.app : %.app.c
	$(CC) -fno-pic -Wl,-Ttext-segment=$(USERSPACE_START) -nostdlib -e main -static -x c $< -o $@

all : $(APPS)

clean :
	rm -f *.o main

-include $(patsubst %,%.d,$(OBJ))
