# 🕹️ GBendo Testing Scripts

Comprehensive testing suite for the GBendo emulator, designed to validate performance improvements and ensure compatibility across multiple ROMs.

---

## 🛠️ Available Scripts

### 1. **`test_bulk_roms.sh`** - Comprehensive ROM Testing Suite
**The main testing script with full profiling and reporting capabilities.**

```bash
# Basic usage (run from scripts directory)
cd scripts
./test_bulk_roms.sh ../tests/roms

# Advanced usage with profiling
./test_bulk_roms.sh ../tests/roms --profile --time 15 --verbose

# Custom output directory
./test_bulk_roms.sh /path/to/roms --output my_results --profile
```

**Features:**
- ✅ Tests all ROMs in a directory (.gb, .gbc, .rom files)
- ✅ Configurable test duration per ROM
- ✅ Performance profiling with detailed metrics
- ✅ Comprehensive HTML and CSV reporting
- ✅ Error handling and crash detection
- ✅ Progress tracking and colored output
- ✅ Detailed logs for debugging

**Command Options:**
- `-p, --profile` - Enable performance profiling
- `-t, --time SECONDS` - Test duration per ROM (default: 10s)
- `-v, --verbose` - Enable verbose output with metrics
- `-o, --output DIR` - Output directory for results
- `-h, --help` - Show help message

---

### 2. **`quick_test.sh`** - Fast Compatibility Check
**Simple script for quick ROM compatibility verification.**

```bash
# Test all ROMs in default directory (run from scripts directory)
cd scripts
./quick_test.sh

# Test ROMs in specific directory
./quick_test.sh /path/to/roms
```

**Features:**
- ⚡ Fast 5-second test per ROM
- ✅ Simple pass/fail reporting
- 🎯 Perfect for CI/CD pipelines
- 🚀 Minimal overhead

---

### 3. **`benchmark_performance.sh`** - Performance Benchmarking
**Detailed performance analysis script for optimization validation.**

```bash
# Benchmark with default ROM (run from scripts directory)
cd scripts
./benchmark_performance.sh

# Benchmark specific ROM
./benchmark_performance.sh ../tests/roms/tetris.gb
```

**Features:**
- 📊 30-second intensive benchmark
- 🔍 Detailed performance metrics extraction
- 💾 System information collection
- 📈 Performance analysis and recommendations
- 💯 Comparison-ready output format

**Metrics Collected:**
- Frame rate (FPS)
- CPU usage percentage
- Memory bandwidth (MB/s)
- Instructions per frame
- Total memory accesses
- System resource utilization

---

## 🗺️ Quick Start Guide

### 1. **Build the Optimized Emulator**
```bash
make clean && make -j$(nproc)
```

### 2. **Set Up Test ROMs**
```bash
# Create test directory and add your ROMs
mkdir -p tests/roms
# Copy your .gb/.gbc ROM files to tests/roms/
```

### 3. **Run Compatibility Tests**
```bash
# Navigate to scripts directory
cd scripts

# Quick test
./quick_test.sh ../tests/roms

# Comprehensive test with profiling
./test_bulk_roms.sh ../tests/roms --profile --verbose
```

### 4. **Performance Benchmarking**
```bash
# Benchmark a specific ROM (from scripts directory)
cd scripts
./benchmark_performance.sh ../tests/roms/your_favorite_game.gb
```

---

## 📊 Output Examples

### Test Results Directory Structure
```
test_results/
├── test_log_YYYYMMDD_TIME.txt             # Main log file
├── test_summary.csv                       # CSV data for analysis
├── tetris_test.txt                        # Individual ROM test output
├── tetris_profile.txt                     # Profiling data
├── mario_test.txt                         # Another ROM test
├── benchmark_results_YYYYMMDD_TIME.txt    # Benchmark data
└── temp_benchmark.txt                     # Temporary benchmark output (auto-cleaned)
```

### Sample Performance Report
```
=== Performance Profile Report ===
Function                  Calls      Total (ms)  Avg (µs)  Min (µs)  Max (µs)
--------                  -----      ----------  --------  --------  --------
Frame Render              1800       500.234     278.12    245.67    312.45
CPU Step                  144000     1250.567    8.68      7.23      15.42
Memory Read               576000     234.123     0.41      0.35      1.23
PPU Step                  1800       178.456     99.14     89.34     125.67

=== Performance Counters ===
Frames rendered:     1800
Instructions executed: 7488000
Memory accesses:     2304000
Instructions/frame:  4160
Memory accesses/frame: 1280

=== Current Metrics ===
FPS:                 59.84
CPU usage:           23.4%
Memory bandwidth:    45.67 MB/s
Instructions/sec:    249600
```

### CSV Output for Analysis
```csv
ROM_Name,Status,Duration,FPS,CPU_Usage
tetris,SUCCESS,10,59.84,23.4%
mario,SUCCESS,10,58.92,25.1%
zelda,FAILED,10,N/A,N/A
pokemon,SUCCESS,10,60.00,22.8%
```

---

## ⚙️ Advanced Usage

### Automated Testing Pipeline
```bash
#!/bin/bash
# Continuous integration script

echo "Building optimized emulator..."
make clean && make -j$(nproc)

echo "Running compatibility tests..."
cd scripts
./test_bulk_roms.sh ../tests/roms --profile --output ci_results

echo "Running performance benchmark..."
./benchmark_performance.sh ../tests/roms/test_rom.gb

echo "Analyzing results..."
if [[ -f "ci_results/test_summary.csv" ]]; then
    PASS_RATE=$(awk -F',' 'NR>1 && $2=="SUCCESS" {success++} NR>1 {total++} END {print (success/total)*100}' ci_results/test_summary.csv)
    echo "Pass rate: ${PASS_RATE}%"
    
    if (( $(echo "$PASS_RATE >= 90" | bc -l) )); then
        echo "✅ Quality gate passed!"
        exit 0
    else
        echo "❌ Quality gate failed!"
        exit 1
    fi
fi
```

### Performance Regression Testing
```bash
# Compare performance between versions
cd scripts
./benchmark_performance.sh ../tests/roms/benchmark_rom.gb
mv benchmark_results_*.txt baseline_performance.txt

# After making changes...
cd ..
make clean && make -j$(nproc)
cd scripts
./benchmark_performance.sh ../tests/roms/benchmark_rom.gb

# Compare results manually or with automated tools
```

### Mass ROM Testing
```bash
# Test multiple ROM directories (from scripts directory)
cd scripts
for rom_dir in ../roms_gameboy ../roms_gameboy_color ../roms_homebrew; do
    echo "Testing $rom_dir..."
    ./test_bulk_roms.sh "$rom_dir" --profile --output "results_$(basename $rom_dir)"
done

# Generate combined report
cat results_*/test_summary.csv | head -1 > combined_results.csv
tail -n +2 -q results_*/test_summary.csv >> combined_results.csv
```

---

## 📈 Validation Checklist

Use these scripts to validate your optimizations:

### ✅ **Compatibility Testing**
- [ ] All ROMs load without crashing
- [ ] No regression in ROM compatibility  
- [ ] Error messages are informative

### ✅ **Performance Validation**
- [ ] Frame rate is stable (>= 59 FPS)
- [ ] CPU usage is reasonable (<= 30%)
- [ ] Memory bandwidth is within limits
- [ ] No performance regressions

### ✅ **Stability Testing**  
- [ ] Long-running tests complete successfully
- [ ] No memory leaks detected
- [ ] Clean exit without crashes
- [ ] Consistent performance over time

---

## 🔍 Troubleshooting

### Common Issues

**"GBendo binary not found"**
```bash
# Solution: Build the emulator first
make clean && make -j$(nproc)
```

**"No ROM files found"**
```bash
# Solution: Check ROM directory and file extensions
ls -la tests/roms/
# Ensure files have .gb, .gbc, or .rom extensions
```

**"Permission denied"**
```bash
# Solution: Make scripts executable
chmod +x *.sh
```

**"Timeout errors"**
```bash
# Solution: Increase test duration
./test_bulk_roms.sh tests/roms --time 30  # 30 seconds per ROM
```

---

## 💡 Tips for Best Results

1. **Use Consistent Test ROMs** - Test with the same ROM set for reliable comparisons
2. **Close Background Apps** - Ensure consistent system resources during benchmarking
3. **Multiple Runs** - Run benchmarks multiple times and average results
4. **Document Changes** - Keep notes on what optimizations were tested
5. **Archive Results** - Save benchmark results for historical comparison

---
