# Kanso UI 

A lightweight OpenGL-based GUI renderer with modern C++ architecture.

## Build Requirements

### macOS
- CMake 3.16 or higher
- C++20 compatible compiler (Clang recommended)
- GLFW 3.3 or higher (install via Homebrew: `brew install glfw`)
- OpenGL 4.1 or higher

### Linux
- CMake 3.16 or higher
- C++20 compatible compiler (GCC or Clang)
- GLFW 3.3 or higher
- OpenGL 3.3 or higher
- X11 development libraries

## Building

### Using Make (Recommended)

```bash
# Build debug version (default)
make

# Build release version
make release

# Clean build files
make clean

# Build and run
make run

# Build and run release version
make run-release

# Check code formatting
make lint

# Auto-format code
make format

# Show all available commands
make help
```

### Using CMake Directly

```bash
# Create build directory
mkdir build && cd build

# Configure for debug
cmake -DCMAKE_BUILD_TYPE=Debug ..

# Configure for release
cmake -DCMAKE_BUILD_TYPE=Release ..

# Build
make

# Run
./kanso
```

## Dependencies

The project depends on the following libraries:

- **GLFW**: Window creation and input handling
- **OpenGL**: Graphics rendering
- **System libraries**: Platform-specific (Cocoa, IOKit, CoreVideo on macOS; X11 on Linux)

All dependencies except GLFW are typically available by default on supported platforms.