CFLAGS = -g -MMD -MT $@ -MF $@.d

all : main

main : $(patsubst %.c,%.o,$(patsubst %.S,%.o,$(wildcard *.[cS])))
	$(CC) $(LDFLAGS) $^ $(LOADLIBES) $(LDLIBS) -o $@

test : main
	$ ./test/run.sh
	echo $$?

try : main
	$ ./main

clean :
	$(RM) *.o main

-include $(patsubst %,%.d,$(OBJ))
