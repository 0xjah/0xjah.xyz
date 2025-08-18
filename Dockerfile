# Build stage
FROM golang:1.22-alpine AS builder

WORKDIR /app

# Install build dependencies
RUN apk add --no-cache git ca-certificates tzdata

# Copy go mod files
COPY go.mod go.sum ./
RUN go mod download

# Copy source code
COPY . .

# Build the application
RUN CGO_ENABLED=0 GOOS=linux go build -a -installsuffix cgo -o main .

# Final stage
FROM alpine:latest

# Install ca-certificates, curl, and bash for Ollama
RUN apk --no-cache add ca-certificates tzdata curl bash

WORKDIR /root/

# Install Ollama
RUN curl -fsSL https://ollama.com/install.sh | sh

# Copy the binary from builder stage
COPY --from=builder /app/main .

# Copy public directory
COPY --from=builder /app/public ./public

# Create startup script
COPY start.sh ./
RUN chmod +x start.sh

# Expose port
EXPOSE 3000

# Environment variables
ENV PORT=3000
ENV HOST=0.0.0.0  
ENV ENV=production
ENV OLLAMA_HOST=http://localhost:11434
ENV OLLAMA_MODEL=llama3.2:1b

# Run the startup script
CMD ["./start.sh"]

# Health check
HEALTHCHECK --interval=30s --timeout=10s --start-period=5s --retries=3 \
  CMD wget --no-verbose --tries=1 --spider http://localhost:3000/ || exit 1
