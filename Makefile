CFLAGS = -g -MMD -MT $@ -MF $@.d

.PHONY : test clean

all : main

main : $(patsubst %.c,%.o,$(patsubst %.S,%.o,$(wildcard *.[cS])))
	$(CC) $(LDFLAGS) $^ $(LOADLIBES) $(LDLIBS) -o $@

test : all
	./test/run.sh ; echo $$?

clean :
	rm -f *.o main

-include $(patsubst %,%.d,$(OBJ))
