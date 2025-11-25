#!/bin/bash

# Performance Benchmark Script for GBendo
# Compares performance before/after optimizations
# Usage: ./benchmark_performance.sh [ROM_FILE]

set -euo pipefail

ROM_FILE="${1:-../tests/roms/tetris.gb}"
GBENDO="../gbendo"
BENCHMARK_TIME=30  # seconds
OUTPUT_DIR="test_results"
RESULTS_FILE="$OUTPUT_DIR/benchmark_results_$(date +%Y%m%d_%H%M%S).txt"

# Colors
BLUE='\033[0;34m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
PURPLE='\033[0;35m'
RED='\033[0;31m'
NC='\033[0m'

echo -e "${BLUE}╔══════════════════════════════════════════════════════════════════════════════╗${NC}"
echo -e "${BLUE}║                        GBendo Performance Benchmark                          ║${NC}"
echo -e "${BLUE}║                        Refactored Emulator Testing                           ║${NC}"
echo -e "${BLUE}╚══════════════════════════════════════════════════════════════════════════════╝${NC}"

# Check prerequisites
if [[ ! -f "$GBENDO" ]]; then
    echo -e "${RED}Error: GBendo binary not found. Run 'make' first.${NC}"
    exit 1
fi

if [[ ! -f "$ROM_FILE" ]]; then
    echo -e "${RED}Error: ROM file not found: $ROM_FILE${NC}"
    exit 1
fi

# System information
echo -e "${PURPLE}System Information:${NC}"
echo "CPU: $(lscpu | grep 'Model name' | cut -d':' -f2 | xargs)"
echo "Memory: $(free -h | grep '^Mem:' | awk '{print $2}')"
echo "OS: $(uname -sr)"
echo "Compiler: $(gcc --version | head -1)"
echo ""

# Create output directory if it doesn't exist
mkdir -p "$OUTPUT_DIR"

# Start benchmark
echo -e "${YELLOW}Starting ${BENCHMARK_TIME}s benchmark with: $(basename "$ROM_FILE")${NC}"
echo "Results will be saved to: $RESULTS_FILE"
echo ""

# Create results file header
{
    echo "# GBendo Performance Benchmark Results"
    echo "# Date: $(date)"
    echo "# ROM: $ROM_FILE"
    echo "# Duration: ${BENCHMARK_TIME} seconds"
    echo "# System: $(uname -sr)"
    echo "# CPU: $(lscpu | grep 'Model name' | cut -d':' -f2 | xargs)"
    echo ""
} > "$RESULTS_FILE"

# Run benchmark with profiling
echo -e "${GREEN}Running benchmark...${NC}"
timeout "${BENCHMARK_TIME}s" "$GBENDO" --profile --verbose "$ROM_FILE" > "$OUTPUT_DIR/temp_benchmark.txt" 2>&1 || true

# Extract and analyze results
echo -e "${PURPLE}Extracting performance metrics...${NC}"

# Parse results
TEMP_FILE="$OUTPUT_DIR/temp_benchmark.txt"
FRAMES=$(grep "Frames rendered:" "$TEMP_FILE" | tail -1 | awk '{print $3}' || echo "0")
INSTRUCTIONS=$(grep "Instructions executed:" "$TEMP_FILE" | tail -1 | awk '{print $3}' || echo "0")
FPS=$(grep "FPS:" "$TEMP_FILE" | tail -1 | awk '{print $2}' || echo "0.00")
CPU_USAGE=$(grep "CPU usage:" "$TEMP_FILE" | tail -1 | awk '{print $3}' || echo "0.0%")
MEMORY_BW=$(grep "Memory bandwidth:" "$TEMP_FILE" | tail -1 | awk '{print $3}' || echo "0.00")

# Calculate derived metrics
if [[ "$FRAMES" -gt 0 ]]; then
    AVG_INSTRUCTIONS_PER_FRAME=$((INSTRUCTIONS / FRAMES))
else
    AVG_INSTRUCTIONS_PER_FRAME=0
fi

TOTAL_MEMORY_ACCESS=$(grep "Total reads:" "$TEMP_FILE" | tail -1 | awk '{print $3}' || echo "0")

# Display results
echo ""
echo -e "${BLUE}╔══════════════════════════════════════════════════════════════════════════════╗${NC}"
echo -e "${BLUE}║                           BENCHMARK RESULTS                                  ║${NC}"
echo -e "${BLUE}╠══════════════════════════════════════════════════════════════════════════════╣${NC}"
echo -e "${BLUE}║${NC} Performance Metrics (${BENCHMARK_TIME}s test):"
echo -e "${BLUE}║${NC}"
echo -e "${BLUE}║${NC} 🎯 Frames Rendered:      ${GREEN}${FRAMES}${NC} frames"
echo -e "${BLUE}║${NC} ⚡ Frame Rate:          ${GREEN}${FPS}${NC} FPS"
echo -e "${BLUE}║${NC} 🔧 Instructions:        ${YELLOW}${INSTRUCTIONS}${NC} total"  
echo -e "${BLUE}║${NC} 📊 Instructions/Frame:   ${YELLOW}${AVG_INSTRUCTIONS_PER_FRAME}${NC} avg"
echo -e "${BLUE}║${NC} 💾 Memory Accesses:     ${PURPLE}${TOTAL_MEMORY_ACCESS}${NC} reads"
echo -e "${BLUE}║${NC} 🖥️  CPU Usage:          ${PURPLE}${CPU_USAGE}${NC}"
echo -e "${BLUE}║${NC} 📈 Memory Bandwidth:    ${PURPLE}${MEMORY_BW}${NC} MB/s"
echo -e "${BLUE}╚══════════════════════════════════════════════════════════════════════════════╝${NC}"

# Performance analysis
echo ""
echo -e "${YELLOW}Performance Analysis:${NC}"

if (( $(echo "$FPS >= 59.0" | bc -l) )); then
    echo -e "✅ Frame Rate: ${GREEN}EXCELLENT${NC} - Achieving near-perfect 60 FPS"
elif (( $(echo "$FPS >= 50.0" | bc -l) )); then
    echo -e "✅ Frame Rate: ${GREEN}GOOD${NC} - Stable performance"
elif (( $(echo "$FPS >= 30.0" | bc -l) )); then
    echo -e "⚠️  Frame Rate: ${YELLOW}ACCEPTABLE${NC} - Playable but could be better"
else
    echo -e "❌ Frame Rate: ${RED}POOR${NC} - Performance issues detected"
fi

CPU_NUM=$(echo "$CPU_USAGE" | sed 's/%//' | bc -l 2>/dev/null || echo 0)
if (( $(echo "$CPU_NUM <= 25.0" | bc -l) )); then
    echo -e "✅ CPU Usage: ${GREEN}EFFICIENT${NC} - Low CPU overhead"
elif (( $(echo "$CPU_NUM <= 50.0" | bc -l) )); then
    echo -e "✅ CPU Usage: ${GREEN}MODERATE${NC} - Reasonable resource usage"
elif (( $(echo "$CPU_NUM <= 75.0" | bc -l) )); then
    echo -e "⚠️  CPU Usage: ${YELLOW}HIGH${NC} - Consider optimization"
else
    echo -e "❌ CPU Usage: ${RED}VERY HIGH${NC} - Performance bottleneck"
fi

# Save detailed results
{
    echo "=== BENCHMARK SUMMARY ==="
    echo "Frames Rendered: $FRAMES"
    echo "Frame Rate: $FPS FPS" 
    echo "Instructions Executed: $INSTRUCTIONS"
    echo "Average Instructions per Frame: $AVG_INSTRUCTIONS_PER_FRAME"
    echo "Memory Accesses: $TOTAL_MEMORY_ACCESS"
    echo "CPU Usage: $CPU_USAGE"
    echo "Memory Bandwidth: $MEMORY_BW MB/s"
    echo ""
    echo "=== RAW PROFILER OUTPUT ==="
} >> "$RESULTS_FILE"

cat "$TEMP_FILE" >> "$RESULTS_FILE"

# Cleanup
rm -f "$TEMP_FILE"

echo ""
echo -e "${GREEN}✅ Benchmark complete! Results saved to: $RESULTS_FILE${NC}"
echo -e "${BLUE}💡 Use this data to compare with future optimizations${NC}"
