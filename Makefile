# Allow overriding compiler and flags from environment
CC ?= gcc

# Build mode: DEBUG=1 for debug build, DEBUG=0 (default) for release
DEBUG ?= 0

# Use pkg-config for SDL2 (more portable)
SDL2_CFLAGS := $(shell pkg-config --cflags sdl2 SDL2_image)
SDL2_LIBS := $(shell pkg-config --libs sdl2 SDL2_image)

ifeq ($(DEBUG),1)
    # Debug build: symbols, sanitizers, no optimization
    CFLAGS ?= -Wall -Wextra -Werror -O0 -g3 -fsanitize=address -fsanitize=undefined \
              -fno-omit-frame-pointer -DDEBUG
    BASE_LDFLAGS = -lm -lz $(SDL2_LIBS) -fsanitize=address -fsanitize=undefined
    $(info Building in DEBUG mode with sanitizers...)
else
    # Release build: aggressive optimization flags for performance
    CFLAGS ?= -Wall -Wextra -O3 -g -march=native -mtune=native -flto -ffast-math \
              -funroll-loops -finline-functions -fomit-frame-pointer \
              -fstrict-aliasing -fno-stack-protector
    BASE_LDFLAGS = -lm -lz $(SDL2_LIBS) -flto
    $(info Building in RELEASE mode with optimizations...)
endif

LDFLAGS ?= $(BASE_LDFLAGS)
CFLAGS += $(SDL2_CFLAGS)

# Support for verbose output
V ?= 0
ifeq ($(V),1)
  Q =
  E = @true
else
  Q = @
  E = @echo
endif

SRC_DIR = src
OBJ_DIR = obj

# Source files including optimized implementations
SRCS = $(wildcard $(SRC_DIR)/*.c) \
       $(wildcard $(SRC_DIR)/cpu/*.c) \
       $(wildcard $(SRC_DIR)/memory/*.c) \
	$(wildcard $(SRC_DIR)/ppu/*.c) \
	$(wildcard $(SRC_DIR)/apu/*.c) \
	$(wildcard $(SRC_DIR)/input/*.c) \
	$(wildcard $(SRC_DIR)/ui/*.c)

# Object files
OBJS = $(SRCS:$(SRC_DIR)/%.c=$(OBJ_DIR)/%.o)

# Executable name
TARGET = gbendo

# Optional upstream Unity source (if present)
UNITY_SRC := $(wildcard tests/unity/unity.c)

# Ensure the object directory structure exists
$(shell mkdir -p $(OBJ_DIR) $(OBJ_DIR)/cpu $(OBJ_DIR)/memory $(OBJ_DIR)/ppu $(OBJ_DIR)/apu $(OBJ_DIR)/input $(OBJ_DIR)/ui)

.PHONY: all clean test build-tests debug release

all: $(TARGET)

# Convenience targets for different build modes
debug:
	$(MAKE) DEBUG=1

release:
	$(MAKE) DEBUG=0


# List of all test binaries
TEST_BINARIES = tests/timer_test \
                tests/timer_edgecases \
                tests/input_test \
                tests/input_if_test \
                tests/ppu_int_test \
                tests/ppu_stat_test \
                tests/ppu_mode_timing_test \
                tests/ppu_cpu_integration_test \
                tests/ppu_access_test \
                tests/sprite_priority_test

# Build all test binaries
build-tests: $(TEST_BINARIES)

# Run all tests
test: $(TEST_BINARIES)
	@echo "=== Running all tests ==="
	@FAILED=0; \
	for test in $(TEST_BINARIES); do \
		echo "Running $$test..."; \
		if ./$$test; then \
			echo "✓ $$test PASSED"; \
		else \
			echo "✗ $$test FAILED"; \
			FAILED=1; \
		fi; \
		echo ""; \
	done; \
	if [ $$FAILED -eq 1 ]; then \
		echo "Some tests failed!"; \
		exit 1; \
	else \
		echo "All tests passed!"; \
	fi

$(TARGET): $(OBJS)
	$(CC) $(OBJS) -o $@ $(LDFLAGS)

$(OBJ_DIR)/%.o: $(SRC_DIR)/%.c
	$(E) "  CC      $<"
	$(Q)$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -rf $(OBJ_DIR) $(TARGET)

tests/timer_test: tests/timer_test.c $(SRC_DIR)/memory/memory.c $(SRC_DIR)/memory/timer.c $(SRC_DIR)/ppu/ppu.c $(SRC_DIR)/ppu/ppu_mem.c $(SRC_DIR)/ppu/ppu_cgb.c $(SRC_DIR)/apu/apu.c tests/stubs/gb_debug_stubs.c $(UNITY_SRC)
	$(CC) $(CFLAGS) tests/timer_test.c $(SRC_DIR)/memory/memory.c $(SRC_DIR)/memory/timer.c $(SRC_DIR)/ppu/ppu.c $(SRC_DIR)/ppu/ppu_mem.c $(SRC_DIR)/ppu/ppu_cgb.c $(SRC_DIR)/apu/apu.c tests/stubs/gb_debug_stubs.c $(UNITY_SRC) tests/unity/test_support.c -o tests/timer_test $(LDFLAGS)

tests/timer_edgecases: tests/timer_edgecases.c $(SRC_DIR)/memory/memory.c $(SRC_DIR)/memory/timer.c $(SRC_DIR)/ppu/ppu.c $(SRC_DIR)/ppu/ppu_mem.c $(SRC_DIR)/ppu/ppu_cgb.c $(SRC_DIR)/apu/apu.c tests/stubs/gb_debug_stubs.c $(UNITY_SRC)
	$(CC) $(CFLAGS) tests/timer_edgecases.c $(SRC_DIR)/memory/memory.c $(SRC_DIR)/memory/timer.c $(SRC_DIR)/ppu/ppu.c $(SRC_DIR)/ppu/ppu_mem.c $(SRC_DIR)/ppu/ppu_cgb.c $(SRC_DIR)/apu/apu.c tests/stubs/gb_debug_stubs.c $(UNITY_SRC) tests/unity/test_support.c -o tests/timer_edgecases $(LDFLAGS)

tests/input_test: tests/input_test.c $(SRC_DIR)/memory/memory.c $(SRC_DIR)/memory/timer.c $(SRC_DIR)/input/input.c $(SRC_DIR)/ppu/ppu.c $(SRC_DIR)/ppu/ppu_mem.c $(SRC_DIR)/ppu/ppu_cgb.c $(SRC_DIR)/apu/apu.c tests/stubs/gb_debug_stubs.c $(UNITY_SRC)
	$(CC) $(CFLAGS) tests/input_test.c $(SRC_DIR)/memory/memory.c $(SRC_DIR)/memory/timer.c $(SRC_DIR)/input/input.c $(SRC_DIR)/ppu/ppu.c $(SRC_DIR)/ppu/ppu_mem.c $(SRC_DIR)/ppu/ppu_cgb.c $(SRC_DIR)/apu/apu.c tests/stubs/gb_debug_stubs.c $(UNITY_SRC) tests/unity/test_support.c -o tests/input_test $(LDFLAGS)

tests/input_if_test: tests/input_if_test.c $(SRC_DIR)/memory/memory.c $(SRC_DIR)/memory/timer.c $(SRC_DIR)/input/input.c $(SRC_DIR)/ppu/ppu.c $(SRC_DIR)/ppu/ppu_mem.c $(SRC_DIR)/ppu/ppu_cgb.c $(SRC_DIR)/apu/apu.c tests/stubs/gb_debug_stubs.c $(UNITY_SRC)
	$(CC) $(CFLAGS) tests/input_if_test.c $(SRC_DIR)/memory/memory.c $(SRC_DIR)/memory/timer.c $(SRC_DIR)/input/input.c $(SRC_DIR)/ppu/ppu.c $(SRC_DIR)/ppu/ppu_mem.c $(SRC_DIR)/ppu/ppu_cgb.c $(SRC_DIR)/apu/apu.c tests/stubs/gb_debug_stubs.c $(UNITY_SRC) tests/unity/test_support.c -o tests/input_if_test $(LDFLAGS)

tests/ppu_int_test: tests/ppu_int_test.c $(SRC_DIR)/memory/memory.c $(SRC_DIR)/memory/timer.c $(SRC_DIR)/ppu/ppu.c $(SRC_DIR)/ppu/ppu_mem.c $(SRC_DIR)/ppu/ppu_cgb.c $(SRC_DIR)/apu/apu.c tests/stubs/gb_debug_stubs.c $(UNITY_SRC)
	$(CC) $(CFLAGS) tests/ppu_int_test.c $(SRC_DIR)/memory/memory.c $(SRC_DIR)/memory/timer.c $(SRC_DIR)/ppu/ppu.c $(SRC_DIR)/ppu/ppu_mem.c $(SRC_DIR)/ppu/ppu_cgb.c $(SRC_DIR)/apu/apu.c tests/stubs/gb_debug_stubs.c $(UNITY_SRC) tests/unity/test_support.c -o tests/ppu_int_test $(LDFLAGS)

tests/ppu_stat_test: tests/ppu_stat_test.c $(SRC_DIR)/memory/memory.c $(SRC_DIR)/memory/timer.c $(SRC_DIR)/ppu/ppu.c $(SRC_DIR)/ppu/ppu_mem.c $(SRC_DIR)/ppu/ppu_cgb.c $(SRC_DIR)/apu/apu.c tests/stubs/gb_debug_stubs.c $(UNITY_SRC)
	$(CC) $(CFLAGS) tests/ppu_stat_test.c $(SRC_DIR)/memory/memory.c $(SRC_DIR)/memory/timer.c $(SRC_DIR)/ppu/ppu.c $(SRC_DIR)/ppu/ppu_mem.c $(SRC_DIR)/ppu/ppu_cgb.c $(SRC_DIR)/apu/apu.c tests/stubs/gb_debug_stubs.c $(UNITY_SRC) tests/unity/test_support.c -o tests/ppu_stat_test $(LDFLAGS)

tests/ppu_mode_timing_test: tests/ppu_mode_timing_test.c $(SRC_DIR)/memory/memory.c $(SRC_DIR)/memory/timer.c $(SRC_DIR)/ppu/ppu.c $(SRC_DIR)/ppu/ppu_mem.c $(SRC_DIR)/ppu/ppu_cgb.c $(SRC_DIR)/apu/apu.c tests/stubs/gb_debug_stubs.c $(UNITY_SRC)
	$(CC) $(CFLAGS) tests/ppu_mode_timing_test.c $(SRC_DIR)/memory/memory.c $(SRC_DIR)/memory/timer.c $(SRC_DIR)/ppu/ppu.c $(SRC_DIR)/ppu/ppu_mem.c $(SRC_DIR)/ppu/ppu_cgb.c $(SRC_DIR)/apu/apu.c tests/stubs/gb_debug_stubs.c $(UNITY_SRC) tests/unity/test_support.c -o tests/ppu_mode_timing_test $(LDFLAGS)

tests/ppu_cpu_integration_test: tests/ppu_cpu_integration_test.c $(SRC_DIR)/memory/memory.c $(SRC_DIR)/memory/timer.c $(SRC_DIR)/ppu/ppu.c $(SRC_DIR)/ppu/ppu_mem.c $(SRC_DIR)/ppu/ppu_cgb.c $(SRC_DIR)/apu/apu.c $(SRC_DIR)/cpu/sm83.c $(SRC_DIR)/cpu/sm83_ops.c $(SRC_DIR)/cpu/sm83_ops_alu.c $(SRC_DIR)/cpu/sm83_ops_bits.c $(SRC_DIR)/cpu/sm83_ops_ctrl.c tests/stubs/gb_debug_stubs.c $(UNITY_SRC)
	$(CC) $(CFLAGS) tests/ppu_cpu_integration_test.c $(SRC_DIR)/memory/memory.c $(SRC_DIR)/memory/timer.c $(SRC_DIR)/ppu/ppu.c $(SRC_DIR)/ppu/ppu_mem.c $(SRC_DIR)/ppu/ppu_cgb.c $(SRC_DIR)/apu/apu.c $(SRC_DIR)/cpu/sm83.c $(SRC_DIR)/cpu/sm83_ops.c $(SRC_DIR)/cpu/sm83_ops_alu.c $(SRC_DIR)/cpu/sm83_ops_bits.c $(SRC_DIR)/cpu/sm83_ops_ctrl.c tests/stubs/gb_debug_stubs.c $(UNITY_SRC) tests/unity/test_support.c -o tests/ppu_cpu_integration_test $(LDFLAGS)

tests/ppu_access_test: tests/ppu_access_test.c $(SRC_DIR)/memory/memory.c $(SRC_DIR)/memory/timer.c $(SRC_DIR)/ppu/ppu.c $(SRC_DIR)/ppu/ppu_mem.c $(SRC_DIR)/ppu/ppu_cgb.c $(SRC_DIR)/apu/apu.c tests/stubs/gb_debug_stubs.c $(UNITY_SRC)
	$(CC) $(CFLAGS) tests/ppu_access_test.c $(SRC_DIR)/memory/memory.c $(SRC_DIR)/memory/timer.c $(SRC_DIR)/ppu/ppu.c $(SRC_DIR)/ppu/ppu_mem.c $(SRC_DIR)/ppu/ppu_cgb.c $(SRC_DIR)/apu/apu.c tests/stubs/gb_debug_stubs.c $(UNITY_SRC) tests/unity/test_support.c -o tests/ppu_access_test $(LDFLAGS)

tests/sprite_priority_test: tests/sprite_priority_test.c $(SRC_DIR)/memory/memory.c $(SRC_DIR)/memory/timer.c $(SRC_DIR)/ppu/ppu.c $(SRC_DIR)/ppu/ppu_mem.c $(SRC_DIR)/ppu/ppu_cgb.c $(SRC_DIR)/apu/apu.c tests/stubs/gb_debug_stubs.c $(UNITY_SRC)
	$(CC) $(CFLAGS) tests/sprite_priority_test.c $(SRC_DIR)/memory/memory.c $(SRC_DIR)/memory/timer.c $(SRC_DIR)/ppu/ppu.c $(SRC_DIR)/ppu/ppu_mem.c $(SRC_DIR)/ppu/ppu_cgb.c $(SRC_DIR)/apu/apu.c tests/stubs/gb_debug_stubs.c $(UNITY_SRC) tests/unity/test_support.c -o tests/sprite_priority_test $(LDFLAGS)