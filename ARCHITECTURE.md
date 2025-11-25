# GBendo Architecture

## Overview

GBendo is a cycle-accurate Game Boy (DMG) and Game Boy Color (CGB) emulator written in C. The architecture follows a modular design with clear separation between the CPU, memory, PPU (Picture Processing Unit), and APU (Audio Processing Unit) subsystems.

## System Components

### CPU: SM83 (Sharp LR35902)
- Location: `src/cpu/`
- The CPU is a hybrid between the Intel 8080 and Zilog Z80
- Implements all documented and undocumented instructions
- Cycle-accurate timing with proper interrupt handling
- Optimized dispatch system using jump tables (`sm83_optimized.c`)

**Key Files:**
- `sm83.c` - Main CPU emulation loop and state management
- `sm83_ops.c` - Instruction implementations
- `sm83_ops_alu.c` - Arithmetic and logic unit operations
- `sm83_ops_bits.c` - Bit manipulation instructions  
- `sm83_ops_ctrl.c` - Control flow instructions
- `sm83_optimized.c` - Jump table-based instruction dispatch

### Memory Subsystem
- Location: `src/memory/`
- Handles all memory banking controllers (MBCs)
- Manages memory mapping and access restrictions
- Implements DMA and HDMA transfers
- Support for save states and battery-backed RAM

**Memory Map:**
- `0x0000-0x3FFF` - ROM Bank 0 (16KB, fixed)
- `0x4000-0x7FFF` - ROM Bank N (16KB, switchable)
- `0x8000-0x9FFF` - Video RAM (8KB)
- `0xA000-0xBFFF` - External RAM (8KB)
- `0xC000-0xDFFF` - Work RAM (8KB)
- `0xFE00-0xFE9F` - OAM (Object Attribute Memory)
- `0xFF00-0xFF7F` - I/O Registers
- `0xFF80-0xFFFE` - High RAM (HRAM)
- `0xFFFF` - Interrupt Enable Register

**Supported MBCs:**
- ROM Only
- MBC1, MBC2, MBC3 (+ RTC), MBC5 (+ Rumble)
- MBC6, MBC7, MMM01, Pocket Camera

**Key Files:**
- `memory.c` - Core memory access and banking
- `mbc.c` - MBC1/MBC2/MBC3 implementations
- `mbc_ext.c` - Extended MBC implementations (MBC5/MBC6/MBC7)
- `memory_state.c` - Save state serialization
- `timer.c` - DIV, TIMA, TMA, TAC timer registers

### PPU (Picture Processing Unit)
- Location: `src/ppu/`
- Cycle-accurate rendering with proper timing
- Supports both DMG and CGB graphics modes
- Implements all PPU modes: OAM scan, pixel transfer, H-Blank, V-Blank
- VRAM banking for CGB mode
- HDMA (HBlank DMA) support for CGB

**PPU Modes:**
1. **Mode 2 (OAM Scan)** - 80 cycles - Scans OAM for sprites
2. **Mode 3 (Pixel Transfer)** - 172-289 cycles - Renders scanline
3. **Mode 0 (H-Blank)** - Variable length - Horizontal blank period
4. **Mode 1 (V-Blank)** - ~4560 cycles (10 scanlines) - Vertical blank period

**Key Files:**
- `ppu.c` - Main PPU logic and rendering
- `ppu_mem.c` - VRAM/OAM access with timing restrictions
- `ppu_cgb.c` - Color Game Boy specific features

### APU (Audio Processing Unit)
- Location: `src/apu/`
- Implements all 4 Game Boy sound channels
-  Channel 1: Square wave with sweep
- Channel 2: Square wave
- Channel 3: Programmable wave
- Channel 4: Noise generator

**Key Files:**
- `apu.c` - Full APU implementation

### UI and Platform Layer
- Location: `src/ui/`
- SDL2-based windowing and input
- ROM file selection dialog
- Real-time display and audio output

**Key Files:**
- `window.c` - SDL2 window management
- `ui.c` - UI rendering and menu system

### Error Handling
- Location: `src/error_handling.c/h`
- Comprehensive error code system
- Input validation macros (VALIDATE_NOT_NULL, VALIDATE_RANGE)
- ROM header and checksum validation
- Safe memory allocation wrappers

### Performance Profiling
- Location: `src/profiler.c/h`
- High-precision nanosecond timing
- Real-time FPS, cycle count, and memory bandwidth tracking
- Enabled via `--profile` command-line flag

## Save State Format

Save states consist of two files:
1. Main state file (`.state`) - Contains CPU/PPU/APU state
2. Memory state file (`.state.mem`) - Contains full memory dump

**Version Compatibility:**
- Version number stored in header (`GB_SAVE_STATE_VERSION`)
- Forward compatibility checked on load
- Pointer cross-references carefully preserved during serialization

**Critical Implementation Notes:**
- CPU and PPU pointers (`cpu.mem`, `ppu.memory`) must be preserved
- Memory subsystem pointers (`memory.ppu`, `memory.apu`) must be restored
- Helper macros (SAVE_FIELD, LOAD_FIELD) ensure consistency

## Performance Optimizations

### Compiler Optimizations
- `-O3` with LTO (Link Time Optimization)
- `-march=native` for CPU-specific optimizations
- `-ffast-math`, `-funroll-loops`, `-finline-functions`

### Runtime Optimizations
1. **Jump Table Dispatch** - CPU instruction dispatch via function pointers
2. **Batched Execution** - Process 16 instructions before updating PPU/APU
3. **Inline Memory Access** - Hot path memory reads/writes inlined
4. **Minimal Branching** - Reduced conditional logic in critical paths

See `PERFORMANCE_UPDATE.md` for detailed performance analysis.

## Build System

The project uses a Makefile with the following targets:
- `make` or `make all` - Build optimized release binary
- `make debug` - Build with debug symbols and sanitizers (coming soon)
- `make test` - Run all unit tests
- `make clean` - Remove build artifacts

## Testing Infrastructure

Tests are located in `tests/` directory:
- Timer tests (`timer_test.c`, `timer_edgecases.c`)
- Input tests (`input_test.c`, `input_if_test.c`)
- PPU tests (`ppu_*_test.c`)
- Sprite priority tests (`sprite_priority_test.c`)

Tests use the Unity testing framework (`tests/unity/`).

## Code Organization Principles

1. **Modularity** - Each subsystem is self-contained
2. **Clear Interfaces** - Well-defined APIs between components  
3. **Minimal Dependencies** - Subsystems depend only on what they need
4. **Pointer Safety** - Careful management of cross-references
5. **Error Checking** - Comprehensive validation at API boundaries

## Future Enhancement Opportunities

1. **SIMD Optimizations** - Vectorize PPU color blending
2. **Multi-threading** - Separate audio/video threads
3. **JIT Compilation** - Dynamic recompilation of hot code paths
4. **GPU Rendering** - Hardware-accelerated PPU via shaders
5. **Debugger Integration** - Full GDB-style debugging interface
