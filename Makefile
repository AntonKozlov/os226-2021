CFLAGS = -g -MMD -MT $@ -MF $@.d

all : main

main : $(patsubst %.c,%.o,$(patsubst %.S,%.o,$(wildcard *.[cS])))
	$(CC) $(LDFLAGS) $^ $(LOADLIBES) $(LDLIBS) -o $@

clean :
	rm -f *.o main

-include $(patsubst %,%.d,$(OBJ))

