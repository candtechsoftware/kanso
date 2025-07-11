#!/bin/bash

# Unity build script for OpenGL GUI application

# Compiler
CC="clang++"

# Source file
SRC="src/main.cpp"

# Parse build type (default to debug)
BUILD_TYPE="${1:-debug}"

# Set build-specific flags and output directory
if [ "$BUILD_TYPE" == "release" ]; then
    BUILD_DIR="build/release"
    CFLAGS="-std=c++11 -Wall -O3 -DNDEBUG"
else
    BUILD_DIR="build/debug"
    CFLAGS="-std=c++11 -Wall -g -O0 -DDEBUG"
fi

# Output binary
OUTPUT="$BUILD_DIR/gui"

# Create build directory
mkdir -p "$BUILD_DIR"

# Platform detection
if [[ "$OSTYPE" == "darwin"* ]]; then
    # macOS
    PLATFORM_FLAGS="-framework OpenGL -framework Cocoa -framework IOKit -framework CoreVideo"
    INCLUDES="-I/opt/homebrew/include -I/usr/local/include"
    LIBS="-L/opt/homebrew/lib -L/usr/local/lib -lglfw"
elif [[ "$OSTYPE" == "linux-gnu"* ]]; then
    # Linux
    PLATFORM_FLAGS=""
    LIBS="-lGL -lglfw -lX11 -lpthread -lXrandr -lXi -ldl"
else
    echo "Unsupported platform: $OSTYPE"
    exit 1
fi

# Build command
echo "Building $BUILD_TYPE for $OSTYPE..."
$CC $CFLAGS $INCLUDES $SRC -o $OUTPUT $PLATFORM_FLAGS $LIBS

if [ $? -eq 0 ]; then
    echo "Build successful! Output: $OUTPUT"
    echo "Usage: ./build.sh [debug|release]"
else
    echo "Build failed!"
    exit 1
fi