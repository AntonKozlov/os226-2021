CFLAGS = -g

all : main

main : $(patsubst %.c,%.o,$(wildcard *.c))
	$(CC) $(LDFLAGS) $^ $(LOADLIBES) $(LDLIBS) -o $@

test : main
	$ ./test/run.sh
	echo $$?

try : main
	$ ./main

clean :
	$(RM) *.o main
