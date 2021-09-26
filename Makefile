CFLAGS = -g

all : main

main : $(patsubst %.c,%.o,$(wildcard *.c))

test : main
	$ ./test/run.sh
	echo $$?

clean :
	$(RM) *.o main
