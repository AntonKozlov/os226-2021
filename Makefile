CFLAGS = -g

all : main

main : main.o cvector.o
	gcc main.o cvector.o -o main

main.o : main.c
	gcc -c main.c

cvector.o : cvector.c
	gcc -c cvector.c

clean :
	rm -f *.o main
