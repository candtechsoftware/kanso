#!/bin/bash
set -eu
cd "$(dirname "$0")"

# --- Unpack Arguments --------------------------------------------------------
for arg in "$@"; do declare $arg='1'; done
if [ ! -v gcc ];     then clang=1; fi
if [ ! -v release ]; then debug=1; fi
if [ -v debug ];     then echo "[debug mode]"; fi
if [ -v release ];   then echo "[release mode]"; fi
if [ -v clang ];     then compiler="${CC:-clang}"; echo "[clang compile]"; fi
if [ -v gcc ];       then compiler="${CC:-gcc}"; echo "[gcc compile]"; fi

# --- Get Current Git Commit Id -----------------------------------------------
git_hash=$(git describe --always --dirty 2>/dev/null || echo "nogit")
git_hash_full=$(git rev-parse HEAD 2>/dev/null || echo "nogit")

# --- Compile/Link Line Definitions -------------------------------------------
common="-Isrc/ -g -DBUILD_GIT_HASH=\"$git_hash\" -DBUILD_GIT_HASH_FULL=\"$git_hash_full\" -Wall -Wno-unused-function -Wno-unused-variable -D_GNU_SOURCE -D_POSIX_C_SOURCE=200809L"
debug_flags="$compiler -O0 -DBUILD_DEBUG=1 ${common}"
release_flags="$compiler -O3 -DBUILD_DEBUG=0 ${common}"
link_flags="-lpq -lX11 -lGL -ldl -lm"

# --- Build Settings ----------------------------------------------------------
if [ -v debug ];   then compile_line="$debug_flags"; fi
if [ -v release ]; then compile_line="$release_flags"; fi

# --- Build --------------------------------------------------------------------
mkdir -p build
echo "Building dbui..."
$compile_line -o build/dbui src/dbui/main.c $link_flags

echo "Build complete: build/dbui"