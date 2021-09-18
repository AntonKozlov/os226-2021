CFLAGS = -g

all : main

main : $(patsubst %.c,%.o,$(wildcard *.c))

clean :
	$(RM) *.o main
