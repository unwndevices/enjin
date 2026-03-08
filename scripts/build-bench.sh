#!/usr/bin/env bash
# build-bench.sh — build and run all enjin2 benchmark binaries
# Writes JSON results to bench-results/ in the project root.
# Uses a separate build-bench/ directory to avoid clobbering the dev build.
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "${SCRIPT_DIR}/.." && pwd)"
BUILD_DIR="${PROJECT_ROOT}/build-bench"

echo "=== enjin2 benchmark suite ==="
echo "Project: ${PROJECT_ROOT}"
echo "Build:   ${BUILD_DIR}"

# Configure — benchmarks ON, tests/examples/SDL OFF, Release mode
cmake -DENJIN2_BUILD_BENCHMARKS=ON \
      -DENJIN2_BUILD_LUA=ON \
      -DENJIN2_BUILD_TESTS=OFF \
      -DENJIN2_BUILD_EXAMPLES=OFF \
      -DENJIN2_BUILD_SDL=OFF \
      -DCMAKE_BUILD_TYPE=Release \
      -B "${BUILD_DIR}" \
      "${PROJECT_ROOT}"

# Build all benchmark targets including allocation verification
cmake --build "${BUILD_DIR}" --target bench_canvas bench_ecs bench_lua bench_alloc -- -j"$(nproc)"

# Ensure output directory exists (binaries also call mkdir but this guarantees it)
mkdir -p "${PROJECT_ROOT}/bench-results"

# Run each binary — they write JSON to bench-results/ relative to CWD
cd "${PROJECT_ROOT}"

echo ""
echo "--- running bench_canvas ---"
"${BUILD_DIR}/benchmarks/bench_canvas"

echo ""
echo "--- running bench_ecs ---"
"${BUILD_DIR}/benchmarks/bench_ecs"

echo ""
echo "--- running bench_lua ---"
"${BUILD_DIR}/benchmarks/bench_lua"

echo ""
echo "--- running bench_alloc (allocation verification) ---"
"${BUILD_DIR}/benchmarks/bench_alloc"

echo ""
echo "=== results written to bench-results/ ==="
ls -lh "${PROJECT_ROOT}/bench-results/"
