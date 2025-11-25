# 🧪 GBendo Testing Scripts

This directory contains comprehensive testing and benchmarking tools for the refactored GBendo Game Boy emulator.

## 🚀 Quick Start

```bash
# Navigate to scripts directory
cd scripts

# Quick ROM compatibility test
./quick_test.sh ../tests/roms

# Comprehensive testing with profiling
./test_bulk_roms.sh ../tests/roms --profile --verbose

# Performance benchmarking
./benchmark_performance.sh ../tests/roms/tetris.gb
```

## 📋 Available Scripts

| Script | Purpose | Usage |
|--------|---------|-------|
| `test_bulk_roms.sh` | Comprehensive ROM testing suite | `./test_bulk_roms.sh ../tests/roms --profile` |
| `quick_test.sh` | Fast compatibility check | `./quick_test.sh ../tests/roms` |
| `benchmark_performance.sh` | Performance analysis | `./benchmark_performance.sh ../tests/roms/game.gb` |

## 📚 Documentation

See [TESTING_SCRIPTS.md](TESTING_SCRIPTS.md) for complete documentation including:
- Detailed usage instructions
- Command-line options
- Output examples
- Advanced usage patterns
- CI/CD integration examples

## 🎯 Prerequisites

1. **Build GBendo first**: `cd .. && make clean && make -j$(nproc)`
2. **Prepare test ROMs**: Place .gb/.gbc files in `../tests/roms/`
3. **Run from scripts directory**: `cd scripts`

## ⚡ Features

- **Performance Profiling** - Real-time metrics and analysis
- **Batch Testing** - Test multiple ROMs automatically  
- **Detailed Reporting** - CSV exports and comprehensive logs saved to `test_results/`
- **Error Detection** - Crash and compatibility issue identification
- **Benchmarking** - Performance validation and regression testing with organized output
- **Centralized Output** - All test results saved to `test_results/` directory for easy analysis

Perfect for validating the **25-40% performance improvements** delivered by the GBendo refactoring!
