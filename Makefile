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

# Prettier formatting
format:
	npm run format

format-check:
	npm run format:check

format-html:
	npm run format:html

format-css:
	npm run format:css

format-js:
	npm run format:js

# Format all (Go + Prettier)
format-all: fmt format

# Setup development environment
setup:
	npm install
	git config core.hooksPath .githooks

# Install git hooks
install-hooks:
	git config core.hooksPath .githooks

# Help
help:
	@echo "Available commands:"
	@echo "  run         - Run the application locally"
	@echo "  build       - Build the application"
	@echo "  clean       - Clean build artifacts"
	@echo "  docker      - Build Docker image"
	@echo "  docker-run  - Run Docker container"
	@echo "  up          - Start with docker-compose"
	@echo "  down        - Stop docker-compose"
	@echo "  test        - Run tests"
	@echo "  fmt         - Format Go code"
	@echo "  format      - Format all files with Prettier"
	@echo "  format-check - Check Prettier formatting"
	@echo "  format-html - Format HTML files only"
	@echo "  format-css  - Format CSS files only"
	@echo "  format-js   - Format JS files only"
	@echo "  format-all  - Format both Go and frontend files"
	@echo "  setup       - Setup development environment"
	@echo "  install-hooks - Install git pre-commit hooks"
