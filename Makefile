.PHONY: run build clean docker docker-run test

# Development
run:
	go run main.go

# Build
build:
	go build -o main .

# Clean
clean:
	rm -f main

# Docker
docker:
	docker build -t 0xjah-site .

docker-run:
	docker run -p 3000:3000 0xjah-site

# Docker Compose
up:
	docker-compose up

down:
	docker-compose down

# Test
test:
	go test -v ./...

# Format code
fmt:
	go fmt ./...

# Help
help:
	@echo "Available commands:"
	@echo "  run        - Run the application locally"
	@echo "  build      - Build the application"
	@echo "  clean      - Clean build artifacts"
	@echo "  docker     - Build Docker image"
	@echo "  docker-run - Run Docker container"
	@echo "  up         - Start with docker-compose"
	@echo "  down       - Stop docker-compose"
	@echo "  test       - Run tests"
	@echo "  fmt        - Format code"
