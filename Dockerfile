# Build stage
FROM debian:bookworm-slim AS builder

RUN apt-get update && apt-get install -y \
    gcc \
    make \
    libcurl4-openssl-dev \
    libssl-dev \
    libmd4c-dev \
    libmd4c-html0-dev \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /app
COPY server/main.c server/
COPY server/Makefile server/

# Build for Linux
RUN cd server && gcc -o server main.c -O3 -Wall -lcurl -lpthread -lssl -lcrypto -lmd4c-html

# Runtime stage
FROM debian:bookworm-slim

RUN apt-get update && apt-get install -y \
    libcurl4 \
    libssl3 \
    libmd4c-html0 \
    ca-certificates \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /app

# Copy the built binary
COPY --from=builder /app/server/server ./server/server

# Copy static files and content
COPY public/ ./public/
COPY content/ ./content/

# Railway injects PORT env var
ENV PORT=3000
ENV CONTENT_DIR=content

EXPOSE 3000

# Run from the app root directory (server expects to be run from parent)
CMD ["./server/server"]
