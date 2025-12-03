.PHONY: build clean run

CC = gcc
CFLAGS = -O3 -Wall
LDFLAGS = -lcurl -lpthread -lssl -lcrypto

build:
	$(CC) -o server main.c $(CFLAGS) $(LDFLAGS)

run: build
	./server

clean:
	rm -f server
