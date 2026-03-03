#!/usr/bin/env bash
set -euo pipefail

TARGET="sdl3"
CLEAN=false
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

# Argument parsing
while [[ $# -gt 0 ]]; do
    case "$1" in
        --target) TARGET="$2"; shift 2 ;;
        --clean)  CLEAN=true; shift ;;
        *) echo "Error: Unknown flag: $1"; echo "Usage: build.sh [--target sdl3|wasm|esp32] [--clean]"; exit 1 ;;
    esac
done

# ---------------------------------------------------------------------------
# Toolchain checks
# ---------------------------------------------------------------------------

check_emsdk() {
    if [ -z "${EMSDK:-}" ]; then
        echo "Error: Emscripten toolchain not activated."
        echo ""
        echo "To fix:"
        echo "  1. Run:    bash scripts/setup-dev.sh"
        echo "  2. Activate: source ~/.local/share/enjin2/emsdk/emsdk_env.sh"
        echo "  3. Retry:  bash build.sh --target wasm"
        exit 1
    fi
}

check_espidf() {
    if [ -z "${IDF_PATH:-}" ]; then
        echo "Error: ESP-IDF toolchain not activated."
        echo ""
        echo "To fix:"
        echo "  1. Run:    bash scripts/setup-dev.sh"
        echo "  2. Activate: source ~/.local/share/enjin2/esp-idf/export.sh"
        echo "  3. Retry:  bash build.sh --target esp32"
        exit 1
    fi
}

# ---------------------------------------------------------------------------
# Build functions
# ---------------------------------------------------------------------------

build_sdl3() {
    local OUT="$SCRIPT_DIR/build/sdl3"
    if [ "$CLEAN" = true ]; then
        echo "Cleaning $OUT..."
        rm -rf "$OUT"
    fi
    mkdir -p "$OUT"
    cmake -B "$OUT" \
        -DENJIN2_BUILD_SDL=ON \
        -DENJIN2_BUILD_LUA=ON \
        -DENJIN2_BUILD_TESTS=OFF \
        -DENJIN2_BUILD_EXAMPLES=OFF \
        -DCMAKE_BUILD_TYPE=Release \
        -DENJIN2_CANVAS_WIDTH="${ENJIN2_CANVAS_WIDTH:-128}" \
        -DENJIN2_CANVAS_HEIGHT="${ENJIN2_CANVAS_HEIGHT:-128}" \
        "$SCRIPT_DIR"
    cmake --build "$OUT" --target enjin2_sdl -j"$(nproc)"
    echo ""
    echo "SDL3 build complete: $OUT/enjin2_sdl"
}

build_wasm() {
    local OUT="$SCRIPT_DIR/build/wasm"
    if [ "$CLEAN" = true ]; then
        echo "Cleaning $OUT..."
        rm -rf "$OUT"
    fi
    mkdir -p "$OUT"
    cd "$OUT"
    emcmake cmake \
        -DENJIN2_BUILD_WASM=ON \
        -DENJIN2_BUILD_LUA=ON \
        -DENJIN2_BUILD_TESTS=OFF \
        -DENJIN2_BUILD_EXAMPLES=OFF \
        -DCMAKE_BUILD_TYPE=Release \
        -DENJIN2_CANVAS_WIDTH="${ENJIN2_CANVAS_WIDTH:-128}" \
        -DENJIN2_CANVAS_HEIGHT="${ENJIN2_CANVAS_HEIGHT:-128}" \
        "$SCRIPT_DIR"
    emmake make -j"$(nproc)" enjin2_wasm
    echo ""
    echo "WebAssembly build complete!"
    echo "Output files:"
    ls -la "$OUT"/enjin2.*
    # Copy to DROP if available
    local DROP_PUBLIC_DIR="/home/unwn/dev/DROP/public"
    if [ -d "$DROP_PUBLIC_DIR" ]; then
        echo "Copying WebAssembly files to DROP..."
        cp "$OUT/enjin2.js"   "$DROP_PUBLIC_DIR/"
        cp "$OUT/enjin2.wasm" "$DROP_PUBLIC_DIR/"
        echo "Files copied to $DROP_PUBLIC_DIR"
    fi
}

build_esp32() {
    local OUT="$SCRIPT_DIR/build/esp32"
    if [ "$CLEAN" = true ]; then
        echo "Cleaning $OUT..."
        rm -rf "$OUT"
    fi
    mkdir -p "$OUT"
    cd "$SCRIPT_DIR/examples/esp32_idf_example"
    idf.py -B "$OUT" build
    echo ""
    echo "ESP32 build complete: $OUT"
}

# ---------------------------------------------------------------------------
# Dispatch
# ---------------------------------------------------------------------------

echo "Building enjin2 for target: $TARGET"
case "$TARGET" in
    sdl3)  build_sdl3 ;;
    wasm)  check_emsdk; build_wasm ;;
    esp32) check_espidf; build_esp32 ;;
    *)
        echo "Error: Unknown target: $TARGET"
        echo "Valid targets: sdl3, wasm, esp32"
        exit 1
        ;;
esac
