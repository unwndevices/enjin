#!/usr/bin/env bash
set -euo pipefail

# enjin2 developer toolchain setup
# Installs Emscripten 3.1.73 and ESP-IDF v5.5 to XDG-standard paths.
#
# Prerequisites (install separately with your OS package manager — NOT invoked by this script):
#   Required packages: cmake, git, python3, python3-pip
#   Arch Linux: use your system package manager to install them
#
# Usage: bash scripts/setup-dev.sh

ENJIN2_TOOLS="$HOME/.local/share/enjin2"
EMSDK_DIR="$ENJIN2_TOOLS/emsdk"
ESPIDF_DIR="$ENJIN2_TOOLS/esp-idf"

mkdir -p "$ENJIN2_TOOLS"

echo "==> Setting up enjin2 development toolchains"
echo "    Install path: $ENJIN2_TOOLS"
echo ""

# ---------------------------------------------------------------------------
# Emscripten (WASM toolchain)
# ---------------------------------------------------------------------------

echo "==> Emscripten 3.1.73"
if [ -d "$EMSDK_DIR" ]; then
    echo "    already installed at $EMSDK_DIR"
else
    echo "    cloning emsdk..."
    git clone https://github.com/emscripten-core/emsdk.git "$EMSDK_DIR"
fi

cd "$EMSDK_DIR"
./emsdk install 3.1.73
./emsdk activate 3.1.73

# ---------------------------------------------------------------------------
# ESP-IDF v5.5 (ESP32 toolchain)
# ---------------------------------------------------------------------------

echo ""
echo "==> ESP-IDF v5.5"
if [ -d "$ESPIDF_DIR" ]; then
    echo "    already installed at $ESPIDF_DIR"
else
    echo "    cloning esp-idf v5.5 (shallow)..."
    git clone --depth 1 --branch v5.5 https://github.com/espressif/esp-idf.git "$ESPIDF_DIR"
fi

cd "$ESPIDF_DIR"
./install.sh esp32s3

# ---------------------------------------------------------------------------
# Activation instructions
# ---------------------------------------------------------------------------

echo ""
echo "================================================================"
echo " Setup complete. Add the following to your ~/.bashrc or ~/.zshrc:"
echo ""
echo "   source $EMSDK_DIR/emsdk_env.sh"
echo "   source $ESPIDF_DIR/export.sh"
echo ""
echo " Or source them manually before building:"
echo "   source $EMSDK_DIR/emsdk_env.sh  # enables: emcmake, emmake"
echo "   source $ESPIDF_DIR/export.sh    # enables: idf.py"
echo "================================================================"
