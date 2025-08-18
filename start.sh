#!/bin/bash

# Exit on any error
set -e

echo "Starting Ollama and Go application..."

# Start Ollama in background
echo "Starting Ollama server..."
ollama serve &

# Wait for Ollama to be ready
echo "Waiting for Ollama to start..."
sleep 10

# Check if Ollama is running
while ! curl -f http://localhost:11434/api/version >/dev/null 2>&1; do
    echo "Waiting for Ollama server to be ready..."
    sleep 5
done

echo "Ollama server is ready!"

# Pull the required model (try a few times in case of network issues)
echo "Pulling Llama model..."
for i in {1..3}; do
    if ollama pull ${OLLAMA_MODEL:-llama3.2:1b}; then
        echo "Model pulled successfully"
        break
    else
        echo "Failed to pull model, attempt $i/3"
        if [ $i -eq 3 ]; then
            echo "Warning: Could not pull model, application will use fallback responses"
        fi
        sleep 10
    fi
done

echo "Starting Go application..."
exec ./main
