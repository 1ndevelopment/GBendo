#!/bin/bash

# GBendo ROM Testing Script
# Tests all ROMs in a directory with the refactored emulator
# Usage: ./test_roms.sh [ROM_DIRECTORY] [OPTIONS]

# set -euo pipefail  # Temporarily disabled for debugging

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
PURPLE='\033[0;35m'
CYAN='\033[0;36m'
NC='\033[0m' # No Color

# Default configuration
ROM_DIR="${1:-../tests/roms}"
GBENDO_PATH="../gbendo"
TEST_DURATION=10  # seconds per ROM
ENABLE_PROFILING=false
VERBOSE=false
OUTPUT_DIR="test_results"
LOG_FILE="$OUTPUT_DIR/test_log_$(date +%Y%m%d_%H%M%S).txt"

# Statistics
TOTAL_ROMS=0
SUCCESSFUL_TESTS=0
FAILED_TESTS=0
START_TIME=$(date +%s)

# Signal handling variables
INTERRUPT_COUNT=0
FORCE_EXIT_TIMEOUT=3  # seconds

# Print usage information
print_usage() {
    echo -e "${BLUE}GBendo ROM Testing Script${NC}"
    echo "Usage: $0 [ROM_DIRECTORY] [OPTIONS]"
    echo ""
    echo "Options:"
    echo "  -p, --profile          Enable performance profiling"
    echo "  -t, --time SECONDS     Test duration per ROM (default: 10)"
    echo "  -v, --verbose          Enable verbose output"
    echo "  -o, --output DIR       Output directory for results (default: test_results)"
    echo "  -h, --help             Show this help message"
    echo ""
    echo "Examples:"
    echo "  $0 tests/roms --profile --time 15"
    echo "  $0 /path/to/roms --verbose --output my_results"
    echo ""
    echo "Signal Handling:"
    echo "  Ctrl+C (once)  - Graceful shutdown with report generation"
    echo "  Ctrl+C (twice) - Force immediate exit within 3 seconds"
}

# Parse command line arguments
parse_args() {
    while [[ $# -gt 0 ]]; do
        case $1 in
            -p|--profile)
                ENABLE_PROFILING=true
                shift
                ;;
            -t|--time)
                TEST_DURATION="$2"
                shift 2
                ;;
            -v|--verbose)
                VERBOSE=true
                shift
                ;;
            -o|--output)
                OUTPUT_DIR="$2"
                LOG_FILE="$OUTPUT_DIR/test_log_$(date +%Y%m%d_%H%M%S).txt"
                shift 2
                ;;
            -h|--help)
                print_usage
                exit 0
                ;;
            *)
                if [[ -d "$1" ]]; then
                    ROM_DIR="$1"
                else
                    echo -e "${RED}Error: Unknown option or invalid directory: $1${NC}"
                    print_usage
                    exit 1
                fi
                shift
                ;;
        esac
    done
}

# Setup output directory
setup_output() {
    mkdir -p "$OUTPUT_DIR"
    echo "# GBendo ROM Test Results - $(date)" > "$LOG_FILE"
    echo "# Test Configuration:" >> "$LOG_FILE"
    echo "# ROM Directory: $ROM_DIR" >> "$LOG_FILE"
    echo "# Test Duration: ${TEST_DURATION}s per ROM" >> "$LOG_FILE"
    echo "# Profiling: $ENABLE_PROFILING" >> "$LOG_FILE"
    echo "# Verbose: $VERBOSE" >> "$LOG_FILE"
    echo "" >> "$LOG_FILE"
}

# Log message to both console and file
log_message() {
    local level="$1"
    local message="$2"
    local timestamp=$(date '+%H:%M:%S')
    
    case $level in
        "INFO")
            echo -e "${CYAN}[$timestamp] INFO:${NC} $message" | tee -a "$LOG_FILE"
            ;;
        "SUCCESS")
            echo -e "${GREEN}[$timestamp] SUCCESS:${NC} $message" | tee -a "$LOG_FILE"
            ;;
        "ERROR")
            echo -e "${RED}[$timestamp] ERROR:${NC} $message" | tee -a "$LOG_FILE"
            ;;
        "WARNING")
            echo -e "${YELLOW}[$timestamp] WARNING:${NC} $message" | tee -a "$LOG_FILE"
            ;;
        *)
            echo "[$timestamp] $message" | tee -a "$LOG_FILE"
            ;;
    esac
}

# Check if GBendo binary exists and is executable
check_gbendo() {
    if [[ ! -f "$GBENDO_PATH" ]]; then
        log_message "ERROR" "GBendo binary not found at: $GBENDO_PATH"
        echo -e "${YELLOW}Tip: Run 'make clean && make -j\$(nproc)' to build GBendo${NC}"
        exit 1
    fi
    
    if [[ ! -x "$GBENDO_PATH" ]]; then
        log_message "ERROR" "GBendo binary is not executable: $GBENDO_PATH"
        exit 1
    fi
    
    log_message "INFO" "Found GBendo binary: $GBENDO_PATH"
}

# Check ROM directory
check_rom_directory() {
    if [[ ! -d "$ROM_DIR" ]]; then
        log_message "ERROR" "ROM directory not found: $ROM_DIR"
        exit 1
    fi
    
    # Count ROM files
    TOTAL_ROMS=$(find "$ROM_DIR" -type f \( -iname "*.gb" -o -iname "*.gbc" -o -iname "*.rom" \) | wc -l)
    
    if [[ $TOTAL_ROMS -eq 0 ]]; then
        log_message "WARNING" "No ROM files found in directory: $ROM_DIR"
        echo -e "${YELLOW}Looking for files with extensions: .gb, .gbc, .rom${NC}"
        exit 1
    fi
    
    log_message "INFO" "Found $TOTAL_ROMS ROM files in: $ROM_DIR"
}

# Test a single ROM
test_rom() {
    local rom_path="$1"
    local rom_name=$(basename "$rom_path")
    local test_output_file="$OUTPUT_DIR/${rom_name%.gb*}_test.txt"
    local profile_output_file="$OUTPUT_DIR/${rom_name%.gb*}_profile.txt"
    
    log_message "INFO" "Testing ROM: $rom_name"
    
    # Build command
    local cmd="timeout ${TEST_DURATION}s $GBENDO_PATH"
    if [[ "$ENABLE_PROFILING" == "true" ]]; then
        cmd="$cmd --profile"
    fi
    if [[ "$VERBOSE" == "true" ]]; then
        cmd="$cmd --verbose"
    fi
    cmd="$cmd \"$rom_path\""
    
    # Execute test
    local start_test_time=$(date +%s)
    if eval "$cmd" > "$test_output_file" 2>&1; then
        local end_test_time=$(date +%s)
        local test_duration=$((end_test_time - start_test_time))
        
        # Extract profiling data if available
        if [[ "$ENABLE_PROFILING" == "true" ]]; then
            grep -A 20 "Performance Profile Report" "$test_output_file" > "$profile_output_file" 2>/dev/null || true
        fi
        
        log_message "SUCCESS" "$rom_name completed successfully (${test_duration}s)"
        ((SUCCESSFUL_TESTS++))
        
        # Extract key metrics if verbose
        if [[ "$VERBOSE" == "true" ]] && [[ "$ENABLE_PROFILING" == "true" ]]; then
            local fps=$(grep "FPS:" "$test_output_file" | tail -1 | awk '{print $2}' || echo "N/A")
            local cpu_usage=$(grep "CPU usage:" "$test_output_file" | tail -1 | awk '{print $3}' || echo "N/A")
            echo -e "  ${PURPLE}├─ FPS: $fps${NC}"
            echo -e "  ${PURPLE}└─ CPU Usage: $cpu_usage${NC}"
        fi
        
        return 0
    else
        local exit_code=$?
        log_message "ERROR" "$rom_name failed with exit code $exit_code"
        ((FAILED_TESTS++))
        
        # Show error details if verbose
        if [[ "$VERBOSE" == "true" ]]; then
            echo -e "  ${RED}Error details:${NC}"
            tail -5 "$test_output_file" | sed 's/^/  │ /'
        fi
        
        return $exit_code
    fi
}

# Generate final report
generate_report() {
    local end_time=$(date +%s)
    local total_duration=$((end_time - START_TIME))
    local minutes=$((total_duration / 60))
    local seconds=$((total_duration % 60))
    
    echo "" | tee -a "$LOG_FILE"
    echo -e "${BLUE}╔══════════════════════════════════════════════════════════════════════════════╗${NC}"
    echo -e "${BLUE}║                            FINAL TEST REPORT                                 ║${NC}"
    echo -e "${BLUE}╠══════════════════════════════════════════════════════════════════════════════╣${NC}"
    echo -e "${BLUE}║${NC} Total ROMs Tested: ${CYAN}$TOTAL_ROMS${NC}"
    echo -e "${BLUE}║${NC} Successful Tests:  ${GREEN}$SUCCESSFUL_TESTS${NC}"
    echo -e "${BLUE}║${NC} Failed Tests:      ${RED}$FAILED_TESTS${NC}"
    echo -e "${BLUE}║${NC} Success Rate:      ${CYAN}$(( (SUCCESSFUL_TESTS * 100) / (TOTAL_ROMS == 0 ? 1 : TOTAL_ROMS) ))%${NC}"
    echo -e "${BLUE}║${NC} Total Duration:    ${YELLOW}${minutes}m ${seconds}s${NC}"
    echo -e "${BLUE}║${NC} Results Saved To:  ${PURPLE}$OUTPUT_DIR${NC}"
    echo -e "${BLUE}╚══════════════════════════════════════════════════════════════════════════════╝${NC}"
    
    # Add to log file
    {
        echo ""
        echo "=== FINAL TEST REPORT ==="
        echo "Total ROMs Tested: $TOTAL_ROMS"
        echo "Successful Tests: $SUCCESSFUL_TESTS"
        echo "Failed Tests: $FAILED_TESTS"
        echo "Success Rate: $(( (SUCCESSFUL_TESTS * 100) / (TOTAL_ROMS == 0 ? 1 : TOTAL_ROMS) ))%"
        echo "Total Duration: ${minutes}m ${seconds}s"
        echo "Results Directory: $OUTPUT_DIR"
    } >> "$LOG_FILE"
    
    # Generate CSV report for analysis
    local csv_file="$OUTPUT_DIR/test_summary.csv"
    echo "ROM_Name,Status,Duration,FPS,CPU_Usage" > "$csv_file"
    
    # Process individual test results
    for result_file in "$OUTPUT_DIR"/*_test.txt; do
        if [[ -f "$result_file" ]]; then
            local rom_name=$(basename "$result_file" "_test.txt")
            local status="SUCCESS"
            local fps="N/A"
            local cpu_usage="N/A"
            
            if grep -q "ERROR" "$result_file"; then
                status="FAILED"
            fi
            
            if [[ "$ENABLE_PROFILING" == "true" ]]; then
                fps=$(grep "FPS:" "$result_file" | tail -1 | awk '{print $2}' 2>/dev/null || echo "N/A")
                cpu_usage=$(grep "CPU usage:" "$result_file" | tail -1 | awk '{print $3}' 2>/dev/null || echo "N/A")
            fi
            
            echo "$rom_name,$status,$TEST_DURATION,$fps,$cpu_usage" >> "$csv_file"
        fi
    done
    
    log_message "INFO" "CSV report generated: $csv_file"
}

# Cleanup function
cleanup() {
    log_message "INFO" "Cleaning up..."
    # Kill any remaining GBendo processes
    pkill -f "$GBENDO_PATH" 2>/dev/null || true
}

# Interrupt handler for graceful shutdown with force exit option
handle_interrupt() {
    ((INTERRUPT_COUNT++))
    
    if [[ $INTERRUPT_COUNT -eq 1 ]]; then
        echo ""
        log_message "WARNING" "Interrupt received! Attempting graceful shutdown..."
        echo -e "${YELLOW}Press Ctrl+C again within ${FORCE_EXIT_TIMEOUT} seconds to force exit${NC}"
        
        # Start background timer for force exit option
        (
            sleep "$FORCE_EXIT_TIMEOUT"
            # Reset interrupt count after timeout
            INTERRUPT_COUNT=0
        ) &
        TIMER_PID=$!
        
        # Attempt graceful shutdown
        cleanup
        generate_report
        log_message "INFO" "Graceful shutdown complete"
        exit 1
        
    elif [[ $INTERRUPT_COUNT -eq 2 ]]; then
        echo ""
        log_message "ERROR" "Force exit requested! Terminating immediately..."
        
        # Kill timer process if still running
        kill "$TIMER_PID" 2>/dev/null || true
        
        # Force kill any remaining processes
        pkill -9 -f "$GBENDO_PATH" 2>/dev/null || true
        
        echo -e "${RED}✗ Tests forcefully terminated${NC}"
        exit 130  # Standard exit code for Ctrl+C
    fi
}

# Signal handlers
trap cleanup EXIT
trap handle_interrupt INT
trap 'log_message "WARNING" "Test terminated"; exit 1' TERM

# Main execution
main() {
    echo -e "${BLUE}╔══════════════════════════════════════════════════════════════════════════════╗${NC}"
    echo -e "${BLUE}║                           GBendo ROM Test Suite                              ║${NC}"
    echo -e "${BLUE}║                            Validation Emulator                               ║${NC}"
    echo -e "${BLUE}╚══════════════════════════════════════════════════════════════════════════════╝${NC}"
    echo ""
    
    parse_args "$@"
    setup_output
    check_gbendo
    check_rom_directory
    
    log_message "INFO" "Starting ROM compatibility test suite"
    log_message "INFO" "Configuration: Duration=${TEST_DURATION}s, Profiling=$ENABLE_PROFILING, Verbose=$VERBOSE"
    
    log_message "INFO" "Building ROM file list..."
    
    # Process each ROM file - using array to avoid subshell issues
    local rom_files=()
    local rom_count=0
    
    # Build array of ROM files
    log_message "INFO" "Running find command..."
    while IFS= read -r -d '' file; do
        log_message "INFO" "Found ROM: $file"
        rom_files+=("$file")
    done < <(find "$ROM_DIR" -type f \( -iname "*.gb" -o -iname "*.gbc" -o -iname "*.rom" \) -print0 | sort -z)
    
    log_message "INFO" "Found ${#rom_files[@]} ROM files to test"
    
    if [[ ${#rom_files[@]} -eq 0 ]]; then
        log_message "ERROR" "No ROM files found in $ROM_DIR"
        exit 1
    fi
    
    # Process each ROM file
    for rom_file in "${rom_files[@]}"; do
        ((rom_count++))
        echo -e "\n${CYAN}[${rom_count}/${TOTAL_ROMS}]${NC} Testing: $(basename "$rom_file")"
        test_rom "$rom_file"
        
        # Progress indicator
        local progress=$((rom_count * 100 / TOTAL_ROMS))
        echo -ne "${BLUE}Progress: [$progress%] $rom_count/$TOTAL_ROMS ROMs tested${NC}\r"
    done
    
    echo "" # New line after progress indicator
    generate_report
    
    # Exit with error if any tests failed
    if [[ $FAILED_TESTS -gt 0 ]]; then
        log_message "WARNING" "Some tests failed. Check individual test files in $OUTPUT_DIR"
        exit 1
    else
        log_message "SUCCESS" "All ROMs tested successfully!"
        exit 0
    fi
}

# Run main function with all arguments
main "$@"
