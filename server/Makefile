.PHONY: build clean run

CC = gcc
CFLAGS = -O3 -Wall
LDFLAGS = -lcurl -lpthread -lssl -lcrypto -lmd4c-html

# Detect macOS and add library paths
UNAME_S := $(shell uname -s)
ifeq ($(UNAME_S),Darwin)
    OPENSSL_PATH := $(shell brew --prefix openssl 2>/dev/null)
    MD4C_PATH := $(shell brew --prefix md4c 2>/dev/null)
    ifneq ($(OPENSSL_PATH),)
        CFLAGS += -I$(OPENSSL_PATH)/include
        LDFLAGS += -L$(OPENSSL_PATH)/lib
    endif
    ifneq ($(MD4C_PATH),)
        CFLAGS += -I$(MD4C_PATH)/include
        LDFLAGS += -L$(MD4C_PATH)/lib
    endif
endif

build:
	$(CC) -o server main.c $(CFLAGS) $(LDFLAGS)

run: build
	cd .. && ./server/server

clean:
	rm -f server
