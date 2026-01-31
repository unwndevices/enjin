#!/bin/bash
#
# Test Results Formatter for enjin2
# Phase: 04-Validation
#
# This script reads manual and shadow mode test results
# and displays them in a clean, chronological terminal output format.
#

# Color output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
CYAN='\033[0;36m'
NC='\033[0m' # No Color

# Configuration
MANUAL_RESULTS_PATTERN="test-results-manual-*"
SHADOW_RESULTS_PATTERN="test-results-shadow-*"

# Global counters
TOTAL_MANUAL_PASSED=0
TOTAL_MANUAL_FAILED=0
TOTAL_SHADOW_WITHIN_TOLERANCE=0
TOTAL_SHADOW_EXCEEDED=0
TOTAL_SHADOW_WARNINGS=0
FAILED_TESTS=()

# Format manual test results
format_manual_results() {
    echo -e "${BLUE}========================================${NC}"
    echo -e "${BLUE}Manual Test Results${NC}"
    echo -e "${BLUE}========================================${NC}"
    echo ""

    # Find all manual test result directories sorted by timestamp (newest first)
    local dirs=()
    while IFS= read -r -d '' dir; do
        dirs+=("$dir")
    done < <(find . -maxdepth 1 -type d -name "test-results-manual-*" -print0 | sort -z -r)

    if [ ${#dirs[@]} -eq 0 ]; then
        echo -e "${YELLOW}No manual test results found.${NC}"
        echo ""
        return
    fi

    # Process each directory
    for dir in "${dirs[@]}"; do
        local summary_file="${dir}/summary.md"
        local dir_name=$(basename "$dir")

        # Extract timestamp from directory name
        local timestamp=$(echo "$dir_name" | sed 's/test-results-manual-//')

        if [ ! -f "$summary_file" ]; then
            continue
        fi

        # Parse the summary.md file
        echo -e "${CYAN}Manual Test Results (${timestamp})${NC}"
        echo ""

        local passed=0
        local failed=0
        local skipped=0

        # Extract test results from tables
        while IFS= read -r line; do
            # Look for table rows with test results
            if [[ $line == \|\ Test* ]]; then
                # Extract test name, status, and output file
                local test_name=$(echo "$line" | awk -F'|' '{print $2}' | xargs)
                local status=$(echo "$line" | awk -F'|' '{print $3}' | xargs)
                local output=$(echo "$line" | awk -F'|' '{print $4}' | xargs)

                if [ -z "$test_name" ] || [ "$test_name" == "Test" ]; then
                    continue
                fi

                # Format output based on status
                case "$status" in
                    PASSED|✓)
                        echo -e "  ${GREEN}✓${NC} ${test_name}"
                        passed=$((passed + 1))
                        TOTAL_MANUAL_PASSED=$((TOTAL_MANUAL_PASSED + 1))
                        ;;
                    FAILED|✗)
                        echo -e "  ${RED}✗${NC} ${test_name} (see ${output})"
                        failed=$((failed + 1))
                        TOTAL_MANUAL_FAILED=$((TOTAL_MANUAL_FAILED + 1))
                        FAILED_TESTS+=("Manual: ${test_name} (${dir_name}/${output})")
                        ;;
                    TBD|SKIPPED|?)
                        echo -e "  ${YELLOW}?${NC} ${test_name} (status: ${status})"
                        skipped=$((skipped + 1))
                        ;;
                    *)
                        # Try to parse the output file to see if it indicates failure
                        if [[ $output == *.bmp ]]; then
                            # If output is BMP, treat as pending/unknown
                            echo -e "  ${YELLOW}?${NC} ${test_name} (status pending, see ${output})"
                            skipped=$((skipped + 1))
                        fi
                        ;;
                esac
            fi
        done < "$summary_file"

        # Print summary for this run
        echo ""
        echo -e "  ${CYAN}Summary: ${passed}/${passed} passed${NC}"
        if [ $failed -gt 0 ]; then
            echo -e "  ${RED}Summary: ${failed} failed${NC}"
        fi
        if [ $skipped -gt 0 ]; then
            echo -e "  ${YELLOW}Summary: ${skipped} skipped${NC}"
        fi
        echo ""
    done
}

# Format shadow mode results
format_shadow_results() {
    echo -e "${BLUE}========================================${NC}"
    echo -e "${BLUE}Shadow Mode Results${NC}"
    echo -e "${BLUE}========================================${NC}"
    echo ""

    # Find all shadow test result directories sorted by timestamp (newest first)
    local dirs=()
    while IFS= read -r -d '' dir; do
        dirs+=("$dir")
    done < <(find . -maxdepth 1 -type d -name "test-results-shadow-*" -print0 | sort -z -r)

    if [ ${#dirs[@]} -eq 0 ]; then
        echo -e "${YELLOW}No shadow mode results found.${NC}"
        echo ""
        return
    fi

    # Process each directory
    for dir in "${dirs[@]}"; do
        local summary_file="${dir}/summary.txt"
        local dir_name=$(basename "$dir")

        # Extract timestamp from directory name
        local timestamp=$(echo "$dir_name" | sed 's/test-results-shadow-//')

        if [ ! -f "$summary_file" ]; then
            continue
        fi

        echo -e "${CYAN}Shadow Mode Results (${timestamp})${NC}"
        echo ""

        # Parse sections from summary.txt
        local in_pixel_diff=false
        local in_timing=false
        local diff_percent="N/A"
        local result="N/A"
        local enjin1_time="N/A"
        local enjin2_time="N/A"
        local timing_gap="N/A"
        local timing_gap_percent="N/A"
        local warnings=0

        while IFS= read -r line; do
            # Parse pixel difference section
            if [[ $line == *"PIXEL DIFFERENCE"* ]]; then
                in_pixel_diff=true
                in_timing=false
                continue
            elif [[ $line == *"TIMING ANALYSIS"* ]]; then
                in_timing=true
                in_pixel_diff=false
                continue
            fi

            if [ "$in_pixel_diff" = true ]; then
                if [[ $line == *"Difference:"* ]]; then
                    diff_percent=$(echo "$line" | awk '{print $2}')
                elif [[ $line == *"Result:"* ]]; then
                    result=$(echo "$line" | awk '{print $2}')
                fi
            elif [ "$in_timing" = true ]; then
            if [[ $line == *"enjin1:"* ]]; then
                enjin1_time=$(echo "$line" | awk '{print $2}')
            elif [[ $line == *"enjin2:"* ]]; then
                enjin2_time=$(echo "$line" | awk '{print $2}')
            elif [[ $line == *"Gap:"* ]]; then
                timing_gap=$(echo "$line" | awk '{print $2}')
                timing_gap_percent=$(echo "$line" | awk '{print $4}' | tr -d '()')
            elif [[ $line == *"Warnings:"* ]]; then
                warnings=$(echo "$line" | awk '{print $2}')
            fi
            fi
        done < "$summary_file"

        # Format output
        local status_symbol="✓"
        local within_tolerance=false

        # Check if within tolerance (result is PASS or difference <= 3%)
        # Remove % symbol from diff_percent for comparison
        local diff_numeric=$(echo "$diff_percent" | sed 's/%//')

        if [ "$diff_numeric" == "N/A" ]; then
            status_symbol="?"
            within_tolerance=false
            TOTAL_SHADOW_EXCEEDED=$((TOTAL_SHADOW_EXCEEDED + 1))
            FAILED_TESTS+=("Shadow: Pixel diff N/A (${dir_name}/output-enjin1.bmp, output-enjin2.bmp)")
        elif [[ "$result" == "PASS" ]] || awk "BEGIN {exit !(${diff_numeric} <= 3.0)}"; then
            status_symbol="✓"
            within_tolerance=true
            TOTAL_SHADOW_WITHIN_TOLERANCE=$((TOTAL_SHADOW_WITHIN_TOLERANCE + 1))
        else
            status_symbol="✗"
            within_tolerance=false
            TOTAL_SHADOW_EXCEEDED=$((TOTAL_SHADOW_EXCEEDED + 1))

            # Add to failed tests list with BMP references
            FAILED_TESTS+=("Shadow: Pixel diff ${diff_numeric}% (${dir_name}/output-enjin1.bmp, output-enjin2.bmp)")
        fi

        # Calculate timing gap percentage
        local timing_gap_display=""
        if [ "$timing_gap_percent" != "N/A" ] && [ ! -z "$timing_gap_percent" ]; then
            # Remove % symbol and trailing ms
            timing_gap_numeric=$(echo "$timing_gap_percent" | sed 's/%//' | sed 's/ms//')
            timing_gap_display="(${timing_gap_numeric}%)"
        fi

        # Display main result
        local diff_display="${diff_percent}"
        # Remove double % symbols if present
        diff_display=$(echo "$diff_display" | sed 's/%%/%/')

        if [ "$within_tolerance" = true ]; then
            echo -e "  ${GREEN}${status_symbol}${NC} Pixel difference: ${diff_display} (within 3% tolerance)"
        else
            echo -e "  ${RED}${status_symbol}${NC} Pixel difference: ${diff_display} (${RED}⚠️ EXCEEDS 3% tolerance${NC})"
        fi

        # Display timing info (add "ms" units for readability)
        echo "    Timing: enjin1=${enjin1_time}ms, enjin2=${enjin2_time}ms (gap: ${timing_gap}ms ${timing_gap_display})"

        # Check for timing warnings
        local timing_warning=false
        if [ "$warnings" -gt 0 ]; then
            timing_warning=true
            TOTAL_SHADOW_WARNINGS=$((TOTAL_SHADOW_WARNINGS + 1))
            echo -e "    ${YELLOW}⚠️ WARNING${NC}: Timing gap exceeds threshold (${warnings} warning(s))"
        fi

        # Show BMP references for failures or warnings
        if [ "$within_tolerance" = false ] || [ "$timing_warning" = true ]; then
            echo "    See: ${dir_name}/output-enjin1.bmp, ${dir_name}/output-enjin2.bmp"
        fi

        echo ""
    done
}

# Print overall summary
print_summary() {
    echo -e "${BLUE}========================================${NC}"
    echo -e "${BLUE}Overall Summary${NC}"
    echo -e "${BLUE}========================================${NC}"
    echo ""

    # Manual test summary
    local manual_total=$((TOTAL_MANUAL_PASSED + TOTAL_MANUAL_FAILED))
    if [ $manual_total -gt 0 ]; then
        echo -e "Total Manual: ${GREEN}${TOTAL_MANUAL_PASSED}/${manual_total} passed${NC}"
        if [ $TOTAL_MANUAL_FAILED -gt 0 ]; then
            echo -e "              ${RED}${TOTAL_MANUAL_FAILED} failed${NC}"
        fi
    else
        echo -e "Total Manual: ${YELLOW}No results${NC}"
    fi

    # Shadow test summary
    local shadow_total=$((TOTAL_SHADOW_WITHIN_TOLERANCE + TOTAL_SHADOW_EXCEEDED))
    if [ $shadow_total -gt 0 ]; then
        echo "Total Shadow: ${TOTAL_SHADOW_WITHIN_TOLERANCE}/${shadow_total} within tolerance"
        if [ $TOTAL_SHADOW_WARNINGS -gt 0 ]; then
            echo -e "             ${YELLOW}${TOTAL_SHADOW_WARNINGS} timing warning(s)${NC}"
        fi
        if [ $TOTAL_SHADOW_EXCEEDED -gt 0 ]; then
            echo -e "             ${RED}${TOTAL_SHADOW_EXCEEDED} exceeded tolerance${NC}"
        fi
    else
        echo -e "Total Shadow: ${YELLOW}No results${NC}"
    fi

    echo ""

    # List failed tests
    if [ ${#FAILED_TESTS[@]} -gt 0 ]; then
        echo -e "${RED}Failed Tests:${NC}"
        for test in "${FAILED_TESTS[@]}"; do
            echo -e "  ${RED}✗${NC} ${test}"
        done
        echo ""
    fi
}

# Main execution
main() {
    echo ""
    format_manual_results
    format_shadow_results
    print_summary
    echo ""
}

# Run main
main
