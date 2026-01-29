#!/bin/bash
set -e

echo "Building enjin2 for WebAssembly..."

# Set up Emscripten environment
EMSDK_DIR="$(pwd)/../emsdk"
if [ ! -d "$EMSDK_DIR" ]; then
    echo "Error: Emscripten SDK not found at $EMSDK_DIR"
    exit 1
fi

# Source the Emscripten environment
export PATH="$EMSDK_DIR:$EMSDK_DIR/upstream/emscripten:$PATH"
export EMSDK="$EMSDK_DIR"
export EM_CONFIG="$EMSDK_DIR/.emscripten"

# Check if emcc is available
if ! command -v emcc &> /dev/null; then
    echo "Error: emcc not found in PATH"
    echo "PATH: $PATH"
    exit 1
fi

echo "Using Emscripten at: $(which emcc)"
emcc --version

# Create build directory
mkdir -p build_wasm
cd build_wasm

# Configure with Emscripten
echo "Configuring with Emscripten..."
emcmake cmake \
    -DENJIN2_BUILD_WASM=ON \
    -DENJIN2_BUILD_LUA=ON \
    -DENJIN2_BUILD_TESTS=OFF \
    -DENJIN2_BUILD_EXAMPLES=OFF \
    -DCMAKE_BUILD_TYPE=Release \
    ..

# Build
echo "Building..."
emmake make -j$(nproc) enjin2_wasm

echo "WebAssembly build complete!"
echo "Output files:"
ls -la enjin2.*

# Copy to DROP public directory if it exists
DROP_PUBLIC_DIR="/home/unwn/dev/DROP/public"
if [ -d "$DROP_PUBLIC_DIR" ]; then
    echo "Copying WebAssembly files to DROP..."
    cp enjin2.js "$DROP_PUBLIC_DIR/"
    cp enjin2.wasm "$DROP_PUBLIC_DIR/"
    echo "Files copied to $DROP_PUBLIC_DIR"
else
    echo "DROP public directory not found at $DROP_PUBLIC_DIR"
    echo "Manual copy required:"
    echo "  cp $(pwd)/enjin2.js <DROP_PROJECT>/public/"
    echo "  cp $(pwd)/enjin2.wasm <DROP_PROJECT>/public/"
fi