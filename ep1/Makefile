CC = gcc

# flags:
# -Wall turns on most compiler warnings
CFLAGS = -Wall -std=c99

all: bccsh ep1

bccsh:
	$(CC) $(CFLAGS) bccsh.c -o bccsh.o

ep1:
	$(CC) $(CFLAGS) ep1.c -o ep1.o

clean:
	rm *.o