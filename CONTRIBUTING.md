# Contributing to GBendo

Thank you for your interest in contributing to GBendo! This document provides guidelines and information for contributors.

## Code of Conduct

Please be respectful and constructive in all interactions.

## Getting Started

### Prerequisites
- GCC or Clang compiler
- SDL2 and SDL2_image development libraries
- Make build system
- Git for version control

### Building from Source

```bash
# Clone the repository
git clone https://github.com/1ndevelopment/GBendo
cd GBendo

# Install dependencies (Debian/Ubuntu)
sudo apt install libsdl2-dev libsdl2-image-dev

# Build the project
make -j$(n proc)

# Run a test ROM
./gbendo tests/roms/tetris.gb
```

## Code Style and Conventions

### General Guidelines
- Use 4 spaces for indentation (no tabs)
- Maximum line length: 100 characters
- Use descriptive variable and function names
- Add comments for complex logic

### Naming Conventions
- Functions: `snake_case` (e.g., `memory_read`, `ppu_step`)
- Structs/Types: `PascalCase` (e.g., `GBEmulator`, `PPU_Mode`)
- Constants/Macros: `UPPER_CASE` (e.g., `ROM_BANK_SIZE`, `SAVE_FIELD`)
- File names: `snake_case.c` (e.g., `sm83_ops.c`, `ppu_mem.c`)

### File Organization
- Keep subsystems in separate directories (`cpu/`, `memory/`, `ppu/`, `apu/`)
- Header files should have include guards
- Implementation files should include their corresponding header first

### Error Handling
- Use the error handling macros from `error_handling.h`:
  - `VALIDATE_NOT_NULL(ptr)` - Check for null pointers
  - `VALIDATE_RANGE(value, min, max)` - Check value ranges
  - `SAFE_MALLOC(ptr, size)` - Safe memory allocation
  - `SET_ERROR(code, message)` - Report errors

Example:
```c
bool my_function(void* data) {
    VALIDATE_NOT_NULL(data);
    // ... implementation
    return true;
}
```

## Making Changes

### Workflow
1. Fork the repository
2. Create a feature branch (`git checkout -b feature/my-feature`)
3. Make your changes
4. Test thoroughly
5. Commit with descriptive messages
6. Push to your fork
7. Open a Pull Request

### Commit Messages
- Use present tense ("Add feature" not "Added feature")
- Keep first line under 72 characters
- Reference issues when applicable

Example:
```
Add save state compression support

- Implement zlib compression for .state files
- Add backward compatibility for uncompressed states
- Update documentation

Fixes #123
```

## Testing Requirements

### Running Tests
```bash
# Build and run all unit tests
make build-tests
make test

# Run specific test
./tests/timer_test
```

### Adding New Tests
1. Create test file in `tests/` directory
2. Use Unity testing framework
3. Add test to Makefile  
4. Ensure all tests pass before submitting PR

### Test Coverage
- Unit tests for isolated functionality
- Integration tests for subsystem interactions
- Regression tests for bug fixes

## Performance Considerations

### Optimization Guidelines
- Profile before optimizing (`--profile` flag)
- Focus on hot paths (identified via profiling)
- Prefer readability over micro-optimizations
- Document performance-critical sections

### Critical Paths
- CPU instruction dispatch (`sm83_step`)
- Memory read/write operations
- PPU scanline rendering
- APU sample generation

## Documentation

### Code Documentation
- Add comments for non-obvious logic
- Document function parameters and return values
- Explain algorithm choices
- Update architecture docs for major changes

Example:
```c
/**
 * Performs a DMA transfer from source address to OAM.
 * Takes 160 cycles (640 T-cycles) to complete.
 * 
 * @param ppu Pointer to PPU state
 * @param mem Pointer to memory subsystem
 * @param start High byte of source address (actual address = start * 0x100)
 */
void ppu_dma_transfer(PPU* ppu, Memory* mem, uint8_t start);
```

### User-Facing Documentation
- Update README.md for new features
- Add examples for new functionality
- Update help text and command-line options

## Debugging Tips

### Build Debug Version
```bash
# Build with debug symbols and sanitizers (when available)
make DEBUG=1

# Run with gdb
gdb ./gbendo
```

### Common Issues
- **Pointer errors**: Enable AddressSanitizer (`-fsanitize=address`)
- **Memory leaks**: Use Valgrind or sanitizers
- **Timing issues**: Enable verbose logging and profiling
- **Graphics glitches**: Check PPU mode transitions

### Logging
- Use existing debug infrastructure
- Enable debug mode via `gb_enable_debug()`
- Add debug prints sparingly

## Adding New Features

### Before Starting
1. Check if issue exists, if not create one
2. Discuss approach in issue comments
3. Ensure feature aligns with project goals

### Implementation Checklist
- [ ] Code follows style guidelines
- [ ] Added unit tests
- [ ] Updated documentation
- [ ] No compiler warnings
- [ ] Tested with multiple ROMs
- [ ] Performance impact assessed
- [ ] Added to CHANGELOG.md

### Subsystem-Specific Guidelines

#### CPU (SM83)
- Maintain cycle accuracy
- Update jump table for new instructions
- Test with blargg's test ROMs

#### Memory/MBC
- Preserve save RAM correctly
- Handle edge cases (masked bank numbers, etc.)
- Test with games using specific MBC

#### PPU
- Maintain scanline timing accuracy
- Test sprite priority and rendering
- Verify against hardware behavior

#### APU
- Ensure correct sample rate
- Maintain channel synchronization
- Test audio quality

## Review Process

### What Reviewers Look For
- Code correctness and clarity
- Test coverage
- Documentation completeness
- Performance impact
- Adherence to coding standards

### Expected Timeline
- Initial review: 1-3 days
- Follow-up reviews: 1-2 days per round
- Merge after approval from maintainer

## Questions and Support

- **Questions**: Open a discussion on GitHub
- **Bug Reports**: File an issue with details and ROM if applicable
- **Feature Requests**: Open an issue describing use case

## License

By contributing, you agree that your contributions will be licensed under the same license as the project (see LICENSE file).

---

Thank you for contributing to GBendo!
