CFLAGS = -g

all : main

main : $(patsubst %.c,%.o,$(wildcard *.c))

clean :
	rm -f *.o main
