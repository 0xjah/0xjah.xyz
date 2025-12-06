.PHONY: build clean run

CC = gcc
CFLAGS = -O3 -Wall
LDFLAGS = -lcurl -lpthread -lssl -lcrypto

# Detect macOS and add OpenSSL paths
UNAME_S := $(shell uname -s)
ifeq ($(UNAME_S),Darwin)
    OPENSSL_PATH := $(shell brew --prefix openssl 2>/dev/null)
    ifneq ($(OPENSSL_PATH),)
        CFLAGS += -I$(OPENSSL_PATH)/include
        LDFLAGS += -L$(OPENSSL_PATH)/lib
    endif
endif

build:
	$(CC) -o server main.c $(CFLAGS) $(LDFLAGS)

run: build
	./server

clean:
	rm -f server
