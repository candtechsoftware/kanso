#!/bin/bash
set -eu
cd "$(dirname "$0")"

# --- Usage Function ----------------------------------------------------------
show_usage() {
    echo "Usage: $0 [<binary_name>] [debug|release] [profile] [run [-- args...]]"
    echo "       $0 lint              # Show all warnings"
    echo "       $0 lint-fix          # Auto-format code"
    echo ""
    echo "Options:"
    echo "  <binary_name>  Binary to build (default: dbui)"
    echo "  debug          Build in debug mode (default)"
    echo "  release        Build in release mode"
    echo "  profile        Enable profiling"
    echo "  run            Run after building"
    echo "  -- args...     Arguments to pass to the program (only with 'run')"
    echo ""
    echo "Examples:"
    echo "  $0                       # Build dbui in debug mode"
    echo "  $0 release               # Build dbui in release mode"
    echo "  $0 release profile       # Build dbui in release mode with profiling"
    echo "  $0 run                   # Build and run dbui"
    echo "  $0 run -- --help         # Build and run dbui with --help argument"
    echo "  $0 dbui release run      # Build and run dbui in release mode"
}

# --- Handle lint and lint-fix commands ---------------------------------------
if [ $# -ge 1 ] && [ "$1" = "lint" ]; then
    echo "[Linting mode - showing all warnings]"
    echo "Checking all .c and .h files in src/ (excluding third_party and generated)..."

    # Find all C files excluding third_party and generated directories
    files=$(find src -type f \( -name "*.c" -o -name "*.h" \) | grep -v third_party | grep -v generated | sort)

    total_warnings=0
    for file in $files; do
        # Compile with all warnings enabled, just to check syntax
        output=$(clang -Wall -Wextra -Wpedantic -Wno-unused-function -Wno-unused-variable -Wno-unused-parameter \
                 -Wno-gnu-zero-variadic-macro-arguments \
                 -Wformat=2 -Wconversion -Wshadow -Wcast-qual -Wwrite-strings \
                 -fsyntax-only -fno-color-diagnostics "$file" 2>&1 || true)

        if [ -n "$output" ]; then
            echo "=== $file ==="
            echo "$output"
            echo ""
            warning_count=$(echo "$output" | grep -c "warning:" || true)
            total_warnings=$((total_warnings + warning_count))
        fi
    done

    echo "============================="
    echo "Total warnings found: $total_warnings"
    exit 0
fi

if [ $# -ge 1 ] && [ "$1" = "lint-fix" ]; then
    echo "[Lint-fix mode - formatting code]"
    echo "Formatting all .c and .h files in src/ (excluding third_party and generated)..."

    # Find all C files excluding third_party and generated directories
    files=$(find src -type f \( -name "*.c" -o -name "*.h" \) | grep -v third_party | grep -v generated | sort)

    # Check if clang-format is available
    if ! command -v clang-format &> /dev/null; then
        echo "Error: clang-format is not installed. Please install it first."
        echo "  macOS: brew install clang-format"
        echo "  Linux: sudo apt-get install clang-format"
        exit 1
    fi

    # Create a .clang-format file if it doesn't exist
    if [ ! -f .clang-format ]; then
        echo "Creating .clang-format configuration..."
        cat > .clang-format << 'EOF'
BasedOnStyle: LLVM
IndentWidth: 4
TabWidth: 4
UseTab: Never
BreakBeforeBraces: Allman
AllowShortIfStatementsOnASingleLine: false
AllowShortLoopsOnASingleLine: false
AllowShortFunctionsOnASingleLine: false
AlignConsecutiveDeclarations: true
AlignConsecutiveAssignments: true
PointerAlignment: Right
ColumnLimit: 120
AlignAfterOpenBracket: Align
AlignTrailingComments: true
SpaceBeforeParens: ControlStatements
EOF
    fi

    formatted_count=0
    for file in $files; do
        echo "Formatting: $file"
        clang-format -i "$file"
        formatted_count=$((formatted_count + 1))
    done

    echo "============================="
    echo "Formatted $formatted_count files"
    echo "Run 'git diff' to see the changes"
    exit 0
fi

# --- Parse Arguments ---------------------------------------------------------
BINARY_NAME="dbui"  # Default binary
BUILD_MODE="debug"  # Default build mode
ENABLE_PROFILE=0
RUN_AFTER_BUILD=0
RUN_ARGS=()

# Parse arguments
while [[ $# -gt 0 ]]; do
    case "$1" in
        debug|release)
            BUILD_MODE="$1"
            shift
            ;;
        profile)
            ENABLE_PROFILE=1
            shift
            ;;
        run)
            RUN_AFTER_BUILD=1
            shift
            # Check if there are program arguments after --
            if [[ $# -gt 0 ]] && [[ "$1" == "--" ]]; then
                shift  # Skip the --
                # Collect all remaining arguments for the program
                while [[ $# -gt 0 ]]; do
                    RUN_ARGS+=("$1")
                    shift
                done
            fi
            ;;
        --)
            # This shouldn't happen without 'run' but handle it anyway
            shift
            break
            ;;
        -h|--help)
            show_usage
            exit 0
            ;;
        *)
            # Check if it's a binary name (directory exists in src/)
            if [ -d "src/$1" ] && [ -f "src/$1/main.c" ]; then
                BINARY_NAME="$1"
            else
                echo "Error: Unknown option '$1'"
                echo ""
                show_usage
                exit 1
            fi
            shift
            ;;
    esac
done

# --- Display Build Configuration ---------------------------------------------
echo "[Building: $BINARY_NAME]"
if [[ "$BUILD_MODE" == "debug" ]]; then
    echo "[debug mode]"
else
    echo "[release mode]"
fi

if [[ $ENABLE_PROFILE -eq 1 ]]; then
    echo "[profiling enabled]"
fi

# Default compiler
compiler="${CC:-gcc}"
echo "[compiler: $compiler]"

# --- Get Current Git Commit Id -----------------------------------------------
git_hash=$(git describe --always --dirty 2>/dev/null || echo "nogit")
git_hash_full=$(git rev-parse HEAD 2>/dev/null || echo "nogit")

# --- Detect Operating System -------------------------------------------------
OS_NAME=$(uname -s)
echo "[Detected OS: $OS_NAME]"

# --- Compile/Link Line Definitions -------------------------------------------
common="-std=c99 -Isrc/ -g -DBUILD_GIT_HASH=\"$git_hash\" -DBUILD_GIT_HASH_FULL=\"$git_hash_full\" -Wall -Wno-unused-function -Wno-unused-variable -Wno-gnu-zero-variadic-macro-arguments -D_GNU_SOURCE -DGL_SILENCE_DEPRECATION"

# Add profiling flags if requested
profile_flags=""
if [[ $ENABLE_PROFILE -eq 1 ]]; then
    profile_flags="-DENABLE_PROFILE=1"
    # Add Tracy profiling support if available
    if [ -d "src/third_party/tracy" ]; then
        profile_flags="$profile_flags -DTRACY_ENABLE"
    fi
fi

if [ "$OS_NAME" = "Darwin" ]; then
    # macOS specific settings
    echo "[Building for macOS with Metal renderer]"

    # Add FreeType support
    freetype_cflags=$(pkg-config --cflags freetype2 2>/dev/null || echo "-I/opt/homebrew/opt/freetype/include/freetype2")
    freetype_libs=$(pkg-config --libs freetype2 2>/dev/null || echo "-L/opt/homebrew/opt/freetype/lib -lfreetype")


    postgresql_cflags=$(pkg-config --cflags libpq 2>/dev/null || echo "-I/opt/homebrew/opt/libpq/include")
    postgresql_libs=$(pkg-config --libs libpq 2>/dev/null   || echo "-L/opt/homebrew/opt/libpq/lib -lpq")

    link_flags="-framework Cocoa -framework Metal -framework MetalKit -framework QuartzCore -framework IOKit -framework CoreVideo -ldl -lm $freetype_libs $postgresql_libs"

    if [[ "$BUILD_MODE" == "debug" ]]; then
        compile_line="$compiler -O0 -DBUILD_DEBUG=1 -DUSE_METAL=1 ${common} ${profile_flags} ${freetype_cflags} ${postgresql_cflags}"
    else
        compile_line="$compiler -O3 -DBUILD_DEBUG=0 -DUSE_METAL=1 ${common} ${profile_flags} ${freetype_cflags} ${postgresql_cflags}"
    fi
elif [ "$OS_NAME" = "Linux" ]; then
    # Linux specific settings
    echo "[Building for Linux with Vulkan renderer]"

    # Add FreeType support
    freetype_cflags=$(pkg-config --cflags freetype2 2>/dev/null || echo "-I/usr/include/freetype2")
    freetype_libs=$(pkg-config --libs freetype2 2>/dev/null || echo "-lfreetype")

    common="$common -D_POSIX_C_SOURCE=200809L"
    link_flags="-lvulkan -lX11 -lXext -lXcursor -lXi -ldl -lm $freetype_libs"

    if [[ "$BUILD_MODE" == "debug" ]]; then
        compile_line="$compiler -O0 -DBUILD_DEBUG=1 -DUSE_VULKAN=1 ${common} ${profile_flags} ${freetype_cflags}"
    else
        compile_line="$compiler -O3 -DBUILD_DEBUG=0 -DUSE_VULKAN=1 ${common} ${profile_flags} ${freetype_cflags}"
    fi
else
    echo "Unsupported OS: $OS_NAME"
    exit 1
fi

# --- Meta Build (Linux Vulkan only) ------------------------------------------
mkdir -p build src/generated
if [ "$OS_NAME" = "Linux" ]; then
    echo "Building meta tool..."
    echo "Compiling meta tool for code generation..."
    $compile_line -o build/meta src/meta/meta.c $link_flags
    if [ $? -ne 0 ]; then
        echo "Meta tool compilation failed!"
        exit 1
    fi

    echo "Running meta tool to generate shader sources..."
    ./build/meta
    if [ $? -ne 0 ]; then
        echo "Meta tool execution failed!"
        exit 1
    fi
    echo "Meta build complete!"
fi

# --- Build Binary -------------------------------------------------------------
echo "Building ${BINARY_NAME}..."

# Check if source file exists
if [ ! -f "src/${BINARY_NAME}/main.c" ]; then
    echo "Error: Source file src/${BINARY_NAME}/main.c does not exist!"
    echo "Available binaries:"
    for dir in src/*/; do
        if [ -f "${dir}main.c" ]; then
            echo "  - $(basename $dir)"
        fi
    done
    exit 1
fi

if [ "$OS_NAME" = "Darwin" ]; then
    echo "Unity build for macOS with Metal renderer"
    $compile_line -x objective-c -o build/${BINARY_NAME} src/${BINARY_NAME}/main.c $link_flags
else
    echo "Unity build for Linux with Vulkan renderer"
    $compile_line -o build/${BINARY_NAME} src/${BINARY_NAME}/main.c src/generated/vulkan_shaders.c $link_flags
fi

if [ $? -eq 0 ]; then
    echo "Build complete: build/${BINARY_NAME}"
else
    echo "Build failed!"
    exit 1
fi

# --- Run if requested --------------------------------------------------------
if [[ $RUN_AFTER_BUILD -eq 1 ]]; then
    echo "Running ${BINARY_NAME}..."
    if [ ${#RUN_ARGS[@]} -gt 0 ]; then
        echo "With arguments: ${RUN_ARGS[@]}"
        ./build/${BINARY_NAME} "${RUN_ARGS[@]}"
    else
        ./build/${BINARY_NAME}
    fi
fi
