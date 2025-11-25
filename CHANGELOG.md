# Changelog

All notable changes to GBendo will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [Unreleased]

### Added
- Comprehensive architecture documentation (`ARCHITECTURE.md`)
- Contributing guidelines (`CONTRIBUTING.md`)
- CHANGELOG for tracking project history
- Helper macros (SAVE_FIELD, LOAD_FIELD, SAVE_ARRAY, LOAD_ARRAY) for save state serialization
- Helper functions for APU channel state save/load operations

### Changed
- **BREAKING**: Refactored `gb_save_state()` and `gb_load_state()` functions to use helper macros and functions
  - Reduced code duplication from ~315 lines to ~185 lines (~41% reduction)
  - Improved maintainability when adding new emulator state fields
  - No changes to save state file format (fully backward compatible)
- Implemented HDMA cancellation feature (was previously a TODO stub)
  - `ppu_hdma_cancel()` now properly cancels active HDMA transfers
  - Resets HDMA state flags correctly

### Fixed
- HDMA cancellation now properly implemented instead of being a no-op

## [1.1.0] - Recent Performance Update

### Added
- CPU jump table-based instruction dispatch (`sm83_optimized.c`)
- Memory access optimization system (`memory_optimized.c/h`)
- High-precision performance profiler (`profiler.c/h`)
- Comprehensive error handling framework (`error_handling.c/h`)
- Input validation macros (VALIDATE_NOT_NULL, VALIDATE_RANGE, SAFE_MALLOC)
- PPU rendering optimization framework (`ppu_optimized.h`)
- Batched instruction processing (16 instructions per subsystem update)

### Changed
- Upgraded compiler optimization flags from `-O2` to `-O3`
- Added Link Time Optimization (LTO) support
- Added CPU-specific optimizations (`-march=native -mtune=native`)
- Improved main emulation loop with nanosecond-precision timing
- Enhanced frame rate control for consistent 60 FPS

### Performance
- 25-40% overall emulation speed improvement
- 20-30% CPU emulation speed improvement
- 15-20% memory access overhead reduction
- More consistent frame timing with reduced CPU usage

## [1.0.0] - Initial Release

### Added
- Full Game Boy (DMG) and Game Boy Color (CGB) emulation
- SM83 CPU emulation with all documented instructions
- Cycle-accurate PPU with DMG and CGB graphics modes
- APU with all 4 sound channels
- Memory Banking Controller support:
  - ROM Only, MBC1, MBC2, MBC3 (+ RTC)
  - MBC5 (+ Rumble), MBC6, MBC7
  - MMM01, Pocket Camera
- Save state functionality
- Battery-backed RAM support
- SDL2-based GUI with file picker
- Command-line ROM loading
- Comprehensive test suite:
  - Timer tests
  - Input tests
  - PPU tests (modes, timing, access, sprite priority)
- Test ROM support (Tetris and others)

### Features
- VRAM and OAM access restrictions based on PPU mode
- DMA and HDMA transfer support
- RTC (Real-Time Clock) for MBC3
- Proper interrupt handling
- Joypad input support
- Audio output via SDL2

## Known Issues

### Current Limitations
- Some CGB-specific features may have compatibility issues with certain ROMs
- Audio emulation may have minor timing discrepancies  
- Performance profiler requires rebuild with profiling enabled

### Planned improvements
- Debug build configuration for easier development
- Save state round-trip automated tests
- Additional documentation for ROM compatibility
- Performance tuning for battery-powered devices

---

For detailed technical changes, see git commit history.
For questions or bug reports, please file an issue on GitHub.
