#!/bin/bash
# Build script for optimizing the project

echo "🔨 Building and optimizing 0xjah.me..."

# Create build directory
mkdir -p build

# Copy public directory
cp -r public build/

# Copy go files
cp main.go go.mod build/

# Optimize images (if imagemagick is installed)
if command -v convert &> /dev/null; then
    echo "📸 Optimizing images..."
    find build/public/static/img -name "*.png" -exec convert {} -strip -quality 85 {} \;
    find build/public/static/img -name "*.jpg" -exec convert {} -strip -quality 85 {} \;
fi

# Build Go binary
echo "⚙️ Building Go binary..."
cd build
go build -ldflags="-s -w" -o server main.go

echo "✅ Build complete! Run './build/server' to start the optimized server."
