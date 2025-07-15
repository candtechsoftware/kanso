# Kanso

A lightweight, high-performance renderer built with Zig, featuring modern graphics capabilities and a clean API.

## Build Requirements

- Zig 0.15.0 or later
- GLFW 3.3 or higher
- OpenGL 4.1 or higher (macOS) / 3.3 or higher (Linux)

### Platform-specific requirements

#### macOS
- Xcode Command Line Tools
- GLFW (install via Homebrew: `brew install glfw`)

#### Linux
- X11 development libraries
- GLFW development package

## Building

```bash
# Build the project
zig build

# Build and run
zig build run

# Build in release mode
zig build -Doptimize=ReleaseFast

# Build and run in release mode
zig build run -Doptimize=ReleaseFast
```

