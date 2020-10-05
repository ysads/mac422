CC = gcc

# flags:
# -Wall turns on most compiler warnings
CFLAGS = -Wall -std=c99 -pthread -ledit

all: bccsh ep1

bccsh:
	$(CC) $(CFLAGS) bccsh.c -o bccsh

ep1:
	$(CC) $(CFLAGS) ep1.c -o ep1

clean:
	rm bccsh ep1
