#!/bin/bash
set -eu
cd "$(dirname "$0")"

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

# --- Check for required binary name argument ---------------------------------
if [ $# -lt 1 ]; then
    echo "Usage: $0 <binary_name> [run] [debug|release] [clang|gcc]"
    echo "       $0 lint              # Show all warnings"
    echo "       $0 lint-fix          # Auto-format code"
    echo "Example: $0 dbui"
    echo "Example: $0 tansaku release"
    echo "Example: $0 tansaku run       # Build and run"
    exit 1
fi

BINARY_NAME="$1"
shift  # Remove first argument so the rest can be processed

# --- Unpack Arguments --------------------------------------------------------
for arg in "$@"; do declare $arg='1'; done

# Default settings
clang=1
debug=1
compiler="${CC:-clang}"

# Parse arguments
if [[ "${gcc:-0}" == "1" ]]; then
    clang=0
    gcc=1
    compiler="${CC:-gcc}"
fi

if [[ "${release:-0}" == "1" ]]; then
    debug=0
    release=1
fi

if [[ "${debug:-0}" == "1" ]]; then echo "[debug mode]"; fi
if [[ "${release:-0}" == "1" ]]; then echo "[release mode]"; fi
if [[ "${clang:-0}" == "1" ]]; then echo "[clang compile]"; fi
if [[ "${gcc:-0}" == "1" ]]; then echo "[gcc compile]"; fi

# --- Get Current Git Commit Id -----------------------------------------------
git_hash=$(git describe --always --dirty 2>/dev/null || echo "nogit")
git_hash_full=$(git rev-parse HEAD 2>/dev/null || echo "nogit")

# --- Detect Operating System -------------------------------------------------
OS_NAME=$(uname -s)
echo "[Detected OS: $OS_NAME]"

# --- Compile/Link Line Definitions -------------------------------------------
common="-Isrc/ -g -DBUILD_GIT_HASH=\"$git_hash\" -DBUILD_GIT_HASH_FULL=\"$git_hash_full\" -Wall -Wno-unused-function -Wno-unused-variable -D_GNU_SOURCE -DGL_SILENCE_DEPRECATION"

if [ "$OS_NAME" = "Darwin" ]; then
    # macOS specific settings
    echo "[Building for macOS with Metal renderer]"
    link_flags="-framework Cocoa -framework Metal -framework MetalKit -framework QuartzCore -framework IOKit -framework CoreVideo -ldl -lm"
    objc_flags="-x objective-c"
    debug_flags="$compiler -O0 -DBUILD_DEBUG=1 -DUSE_METAL=1 ${common}"
    release_flags="$compiler -O3 -DBUILD_DEBUG=0 -DUSE_METAL=1 ${common}"
elif [ "$OS_NAME" = "Linux" ]; then
    # Linux specific settings
    echo "[Building for Linux with Vulkan renderer]"
    common="$common -D_POSIX_C_SOURCE=200809L"
    link_flags="-lvulkan -lX11 -ldl -lm"
    debug_flags="$compiler -O0 -DBUILD_DEBUG=1 -DUSE_VULKAN=1 ${common}"
    release_flags="$compiler -O3 -DBUILD_DEBUG=0 -DUSE_VULKAN=1 ${common}"
else
    echo "Unsupported OS: $OS_NAME"
    exit 1
fi

# --- Build Settings ----------------------------------------------------------
if [[ "${debug:-0}" == "1" ]]; then compile_line="$debug_flags"; fi
if [[ "${release:-0}" == "1" ]]; then compile_line="$release_flags"; fi

# --- Meta Build ---------------------------------------------------------------
mkdir -p build src/generated
echo "Building meta tool..."
if [ "$OS_NAME" = "Linux" ]; then
    echo "Compiling meta tool for code generation..."
    $compile_line -o build/meta src/meta/meta.c -lm
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
fi

# --- Build --------------------------------------------------------------------
echo "Building ${BINARY_NAME}..."

# Check if source file exists
if [ ! -f "src/${BINARY_NAME}/main.c" ]; then
    echo "Error: Source file src/${BINARY_NAME}/main.c does not exist!"
    exit 1
fi

if [ "$OS_NAME" = "Darwin" ]; then
    echo "Unity build for macOS with Metal renderer"
    $compile_line -x objective-c -o build/${BINARY_NAME} src/${BINARY_NAME}/main.c $link_flags
else
    echo "Unity build for Linux with Vulkan renderer"
    $compile_line -o build/${BINARY_NAME} src/${BINARY_NAME}/main.c src/generated/vulkan_shaders.c $link_flags
fi

echo "Build complete: build/${BINARY_NAME}"

# --- Run if requested --------------------------------------------------------
if [[ "${run:-0}" == "1" ]]; then
    echo "Running ${BINARY_NAME}..."
    ./build/${BINARY_NAME}
fi