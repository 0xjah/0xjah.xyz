#!/bin/bash
# Build script for 0xjah.xyz C server

set -e

CC="${CC:-gcc}"
CFLAGS="-O3 -Wall -Wextra -Wpedantic"
LIBS="-lcurl -lpthread -lssl -lcrypto -lmd4c-html"

# Detect OS and set paths
if [[ "$OSTYPE" == "darwin"* ]]; then
    # macOS with Homebrew
    if [[ -d "/opt/homebrew" ]]; then
        CFLAGS="$CFLAGS -I/opt/homebrew/include"
        LDFLAGS="-L/opt/homebrew/lib"
    elif [[ -d "/usr/local" ]]; then
        CFLAGS="$CFLAGS -I/usr/local/include"
        LDFLAGS="-L/usr/local/lib"
    fi
fi

echo "Building 0xjah.xyz server..."
echo "  CC: $CC"
echo "  CFLAGS: $CFLAGS"

$CC $CFLAGS -o server main.c $LDFLAGS $LIBS

echo ""
echo "Build successful!"
echo "Run with: ./server"
