.PHONY: run build clean

CC = gcc
CFLAGS = -O3 -Wall
LDFLAGS = -lcurl -lpthread -lssl -lcrypto
OPENSSL = $(shell brew --prefix openssl 2>/dev/null)

ifneq ($(OPENSSL),)
	CFLAGS += -I$(OPENSSL)/include
	LDFLAGS += -L$(OPENSSL)/lib
endif

run: build
	./server

build:
	$(CC) -o server main.c $(CFLAGS) $(LDFLAGS)

clean:
	rm -f server