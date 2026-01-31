#!/bin/bash

# Shadow Mode Execution Script
# Runs both enjin1 and enjin2 backends, compares outputs, and reports results

set -e  # Exit on error (but we'll handle errors in functions)

# Configuration
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
# Script is in .planning/phases/04-validation/, so go up 3 levels to reach repo root
REPO_ROOT="$(cd "$SCRIPT_DIR/../../../" && pwd)"
RESULTS_DIR="$REPO_ROOT/test-results-shadow-$(date +%Y%m%d-%H%M%S)"
ENJIN1_BMP="output-enjin1.bmp"
ENJIN2_BMP="output-enjin2.bmp"
TOLERANCE_PERCENT=3.0
TIMING_PERCENT_WARNING=20  # Warn if timing gap > 20%
TIMING_ABS_WARNING=50     # Warn if timing gap > 50ms absolute

# Initialize results directory
init_results() {
    mkdir -p "$RESULTS_DIR"
    echo "Shadow Mode Test Results: $(date)" > "$RESULTS_DIR/summary.txt"
    echo "======================================" >> "$RESULTS_DIR/summary.txt"
    echo "" >> "$RESULTS_DIR/summary.txt"
}

# Build both backends
build_backends() {
    echo "Building shadow mode backends..."
    echo ""

    # Build enjin1 backend
    echo "[1/2] Building enjin1 backend..."
    cd "$REPO_ROOT/enjin2/build"
    cmake -DUSE_ENJIN1=ON .. > /dev/null 2>&1
    if make shadow_mode_test > /dev/null 2>&1; then
        echo "  ✓ enjin1 backend built successfully"
        ENJIN1_BUILD_SUCCESS=true
    else
        echo "  ✗ enjin1 backend build failed"
        ENJIN1_BUILD_SUCCESS=false
    fi
    cd "$REPO_ROOT"

    # Build enjin2 backend
    echo "[2/2] Building enjin2 backend..."
    cd "$REPO_ROOT/enjin2/build"
    cmake -DUSE_ENJIN1=OFF .. > /dev/null 2>&1
    if make shadow_mode_test > /dev/null 2>&1; then
        echo "  ✓ enjin2 backend built successfully"
        ENJIN2_BUILD_SUCCESS=true
    else
        echo "  ✗ enjin2 backend build failed"
        ENJIN2_BUILD_SUCCESS=false
    fi
    cd "$REPO_ROOT"


    echo ""

    # Check if both builds succeeded
    if [ "$ENJIN1_BUILD_SUCCESS" = false ] || [ "$ENJIN2_BUILD_SUCCESS" = false ]; then
        echo "ERROR: Build failures detected. Cannot continue."
        return 1
    fi

    return 0
}

# Run shadow tests
run_shadow_tests() {
    echo "Running shadow mode tests..."
    echo ""

    # Run enjin1 test
    echo "[1/2] Running enjin1 test..."
    cd "$REPO_ROOT/enjin2/build/tests"
    ENJIN1_OUTPUT=$(./shadow_mode_test enjin1 2>&1)
    ENJIN1_TIME=$(echo "$ENJIN1_OUTPUT" | grep "Execution time:" | sed 's/.*: //' | sed 's/ ms//')
    echo "  Execution time: ${ENJIN1_TIME} ms"

    # Check if enjin1 BMP was created
    if [ -f "$ENJIN1_BMP" ]; then
        echo "  ✓ Output exported: $ENJIN1_BMP"
        mv "$ENJIN1_BMP" "$RESULTS_DIR/"
    else
        echo "  ✗ Failed to export BMP"
        ENJIN1_RUN_SUCCESS=false
    fi

    # Run enjin2 test
    echo ""
    echo "[2/2] Running enjin2 test..."
    ENJIN2_OUTPUT=$(./shadow_mode_test enjin2 2>&1)
    ENJIN2_TIME=$(echo "$ENJIN2_OUTPUT" | grep "Execution time:" | sed 's/.*: //' | sed 's/ ms//')
    echo "  Execution time: ${ENJIN2_TIME} ms"

    # Check if enjin2 BMP was created
    if [ -f "$ENJIN2_BMP" ]; then
        echo "  ✓ Output exported: $ENJIN2_BMP"
        mv "$ENJIN2_BMP" "$RESULTS_DIR/"
    else
        echo "  ✗ Failed to export BMP"
        ENJIN2_RUN_SUCCESS=false
    fi

    cd "$REPO_ROOT"

    echo ""
    echo "Tests complete. Artifacts saved to: $RESULTS_DIR"
    echo ""

    return 0
}

# Compare outputs
compare_outputs() {
    echo "Comparing outputs..."
    echo ""

        # Run image comparison
    cd "$REPO_ROOT/enjin2/build/tests"
    COMPARISON_OUTPUT=$(./image_comparison_test "$RESULTS_DIR/$ENJIN1_BMP" "$RESULTS_DIR/$ENJIN2_BMP" 2>&1)
    DIFF_PERCENT=$(echo "$COMPARISON_OUTPUT" | grep "Pixel difference:" | sed 's/.*: //' | sed 's/%//')
    RESULT=$(echo "$COMPARISON_OUTPUT" | grep "Result:" | sed 's/.*: //')
    cd "$REPO_ROOT"

    echo "  Pixel difference: ${DIFF_PERCENT}%"
    echo "  Result: $RESULT"

    # Check if difference exceeds tolerance
    if awk "BEGIN {exit !($DIFF_PERCENT > $TOLERANCE_PERCENT)}"; then
        echo "  ✗ EXCEEDS TOLERANCE (max ${TOLERANCE_PERCENT}%)"
        DIFFERENCES_DETECTED=true
    else
        echo "  ✓ Within tolerance"
        DIFFERENCES_DETECTED=false
    fi

    echo ""

    # Check timing gap
    TIMING_GAP_ABS=$(awk "BEGIN {print $ENJIN1_TIME - $ENJIN2_TIME}")
    TIMING_GAP_ABS=$(echo "$TIMING_GAP_ABS" | sed 's/^-//')  # Absolute value
    TIMING_GAP_PERCENT=0

    if [ "$ENJIN2_TIME" -gt 0 ]; then
        TIMING_GAP_PERCENT=$(awk "BEGIN {print ($TIMING_GAP_ABS / $ENJIN2_TIME) * 100}")
    fi

    echo "Timing Analysis:"
    echo "  enjin1: ${ENJIN1_TIME} ms"
    echo "  enjin2: ${ENJIN2_TIME} ms"
    echo "  Gap: ${TIMING_GAP_ABS} ms (${TIMING_GAP_PERCENT}%)"

    # Check for timing warnings
    TIMING_WARNINGS=0
    if awk "BEGIN {exit !($TIMING_GAP_PERCENT > $TIMING_PERCENT_WARNING)}"; then
        echo "  ⚠ WARNING: Timing gap exceeds ${TIMING_PERCENT_WARNING}% threshold"
        TIMING_WARNINGS=$((TIMING_WARNINGS + 1))
    fi

    if awk "BEGIN {exit !($TIMING_GAP_ABS > $TIMING_ABS_WARNING)}"; then
        echo "  ⚠ WARNING: Timing gap exceeds ${TIMING_ABS_WARNING}ms threshold"
        TIMING_WARNINGS=$((TIMING_WARNINGS + 1))
    fi

    if [ $TIMING_WARNINGS -eq 0 ]; then
        echo "  ✓ Timing within acceptable range"
    fi

    echo ""

    return 0
}

# Report results
report_results() {
    echo "============================================"
    echo "SHADOW MODE SUMMARY"
    echo "============================================"
    echo ""

    # Build status
    echo "Build Status:"
    if [ "$ENJIN1_BUILD_SUCCESS" = true ]; then
        echo "  ✓ enjin1 backend: BUILD OK"
    else
        echo "  ✗ enjin1 backend: BUILD FAILED"
    fi

    if [ "$ENJIN2_BUILD_SUCCESS" = true ]; then
        echo "  ✓ enjin2 backend: BUILD OK"
    else
        echo "  ✗ enjin2 backend: BUILD FAILED"
    fi

    echo ""

    # Pixel difference status
    echo "Pixel Difference:"
    echo "  Difference: ${DIFF_PERCENT}%"
    echo "  Tolerance: ${TOLERANCE_PERCENT}%"

    if [ "$DIFFERENCES_DETECTED" = true ]; then
        echo "  ✗ EXCEEDS TOLERANCE"
        echo "  BMP files: $RESULTS_DIR/$ENJIN1_BMP, $RESULTS_DIR/$ENJIN2_BMP"
    else
        echo "  ✓ WITHIN TOLERANCE"
    fi

    echo ""

    # Timing warnings
    echo "Timing Warnings: $TIMING_WARNINGS"
    if [ $TIMING_WARNINGS -gt 0 ]; then
        echo "  enjin1: ${ENJIN1_TIME} ms"
        echo "  enjin2: ${ENJIN2_TIME} ms"
        echo "  Gap: ${TIMING_GAP_ABS} ms (${TIMING_GAP_PERCENT}%)"
    fi

    echo ""

    # Overall result
    echo "Overall Result:"
    if [ "$DIFFERENCES_DETECTED" = false ] && [ $TIMING_WARNINGS -eq 0 ]; then
        echo "  ✓ ALL TESTS PASSED"
        OVERALL_RESULT="PASS"
    elif [ "$DIFFERENCES_DETECTED" = true ]; then
        echo "  ✗ PIXEL DIFFERENCES DETECTED"
        OVERALL_RESULT="FAIL"
    else
        echo "  ⚠ TIMING WARNINGS ONLY (output matches)"
        OVERALL_RESULT="WARN"
    fi

    echo ""

    # Write summary to file
    cat >> "$RESULTS_DIR/summary.txt" << EOF
BUILD STATUS
============
enjin1 backend: $([ "$ENJIN1_BUILD_SUCCESS" = true ] && echo "OK" || echo "FAILED")
enjin2 backend: $([ "$ENJIN2_BUILD_SUCCESS" = true ] && echo "OK" || echo "FAILED")

PIXEL DIFFERENCE
================
Difference: ${DIFF_PERCENT}%
Tolerance: ${TOLERANCE_PERCENT}%
Result: $RESULT

TIMING ANALYSIS
===============
enjin1: ${ENJIN1_TIME} ms
enjin2: ${ENJIN2_TIME} ms
Gap: ${TIMING_GAP_ABS} ms (${TIMING_GAP_PERCENT}%)
Warnings: $TIMING_WARNINGS

OVERALL RESULT: $OVERALL_RESULT
EOF

    echo "Full report saved to: $RESULTS_DIR/summary.txt"
    echo ""

    return 0
}

# Main execution
main() {
    echo "============================================"
    echo "SHADOW MODE EXECUTION"
    echo "============================================"
    echo ""

    init_results
    build_backends
    run_shadow_tests
    compare_outputs
    report_results

    # Return non-zero if there were failures
    if [ "$OVERALL_RESULT" = "FAIL" ]; then
        exit 1
    fi

    exit 0
}

# Run main
main "$@"
