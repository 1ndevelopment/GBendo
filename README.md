<p align="center">
  <img
    src="https://files.1ndev.com/api/public/dl/6dD5XLg1/images/software/GBendo/logo.png"
    width="42%"
  />
  <br>(guh-ben-doe)</br>
</p>

---

**GBendo** is a cycle-accurate Game Boy (DMG) emulator and Test Suite written in C, featuring optimized performance and comprehensive hardware emulation.

## ✨ Features

- Full Game Boy emulation
- Cycle-accurate CPU (SM83), PPU, and APU
- Support for multiple Memory Bank Controllers (MBC1-7, MMM01, Pocket Camera)
- Save states with version compatibility
- Battery-backed RAM support
- SDL2-based GUI with ROM file picker
- Performance profiling tools
- Comprehensive test suite

## 🚀 Quick Start

### 1. Clone repo

```bash
git clone https://github.com/1ndevelopment/GBendo
cd GBendo
```

### 2. Install packages

```bash
# Debian:
sudo apt install libsdl2-dev libsdl2-image-dev -y

# Fedora:
sudo dnf install SDL2-devel SDL2_image-devel

# Arch:
sudo pacman -S sdl2 sdl2_image
```

### 3. Build project

```bash
# Release build (optimized)
make -j$(nproc)

# Or debug build (with sanitizers)
make debug -j$(nproc)
```

### 4. Run emulator

```bash
# Launch GUI
./gbendo

# Run ROM via command line with verbose output
./gbendo -v tests/roms/tetris.gb

# Enable performance profiling
./gbendo --profile tests/roms/tetris.gb
```

## 📚 Documentation

- [ARCHITECTURE.md](ARCHITECTURE.md) - System architecture and component design
- [CONTRIBUTING.md](CONTRIBUTING.md) - Guidelines for contributors
- [CHANGELOG.md](CHANGELOG.md) - Version history and recent changes
- [PERFORMANCE_UPDATE.md](PERFORMANCE_UPDATE.md) - Performance optimization details
- [TESTING_SCRIPTS.md](scripts/TESTING_SCRIPTS.md) - Testing documentation

## 🧪 Testing

```bash
# Build and run all tests
make build-tests
make test

# Run individual test
./tests/timer_test
```

## 🎯 Compatibility

GBendo strives for high compatibility with commercial Game Boy ROMS. Supported features include:

- All documented SM83 instructions
- Proper interrupt handling and timing
- VRAM/OAM access restrictions
- DMA and HDMA transfers
- RTC support (MBC3)

## 📝 License

See [LICENSE](LICENSE) file for details.

## 🙏 Acknowledgments

- Game Boy community for extensive hardware documentation
- Test ROM authors for validation tools

---

*Made with ❤️ by 1ndevelopment*
