CFLAGS = -g

.PHONY : test clean

all : main

main : $(patsubst %.c,%.o,$(wildcard *.c))
	$(CC) $(LDFLAGS) $^ $(LOADLIBES) $(LDLIBS) -o $@

test :
	./test/run.sh ; echo $$?

clean :
	rm -f *.o main
