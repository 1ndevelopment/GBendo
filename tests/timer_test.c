#include <stdint.h>
#include "vendor/unity.h"
#include "../src/memory/memory.h"

static void test_div_increments(void) {
    Memory mem;
    memory_init(&mem);
    memory_timer_init(&mem);
    memory_timer_step(&mem, 256);
    TEST_ASSERT_EQUAL_UINT8(1, mem.io_registers[0x04]);
}

static void test_tima_rates_and_overflow(void) {
    Memory mem;
    memory_init(&mem);

    /* 4096 Hz (TAC 00) -> 1024 cycles */
    memory_timer_init(&mem);
    mem.tima = 0; mem.tma = 0; mem.tac = 0x04 | 0x00; mem.io_registers[0x07] = mem.tac;
    memory_timer_step(&mem, 1024);
    TEST_ASSERT_EQUAL_UINT8(1, mem.io_registers[0x05]);

    /* 262144 Hz (TAC 01) -> 16 cycles */
    memory_timer_init(&mem);
    mem.tima = 0; mem.tac = 0x04 | 0x01; mem.io_registers[0x07] = mem.tac;
    memory_timer_step(&mem, 16);
    TEST_ASSERT_EQUAL_UINT8(1, mem.io_registers[0x05]);

    /* 65536 Hz (TAC 10) -> 64 cycles */
    memory_timer_init(&mem);
    mem.tima = 0; mem.tac = 0x04 | 0x02; mem.io_registers[0x07] = mem.tac;
    memory_timer_step(&mem, 64);
    TEST_ASSERT_EQUAL_UINT8(1, mem.io_registers[0x05]);

    /* 16384 Hz (TAC 11) -> 256 cycles */
    memory_timer_init(&mem);
    mem.tima = 0; mem.tac = 0x04 | 0x03; mem.io_registers[0x07] = mem.tac;
    memory_timer_step(&mem, 256);
    TEST_ASSERT_EQUAL_UINT8(1, mem.io_registers[0x05]);

    /* Overflow reload behavior */
    memory_timer_init(&mem);
    mem.tma = 0x42; mem.tima = 0xFF;
    mem.tac = 0x04 | 0x01; mem.io_registers[0x07] = mem.tac;
    memory_timer_step(&mem, 16); /* cause overflow */
    TEST_ASSERT_EQUAL_UINT8(0x00, mem.io_registers[0x05]); /* during reload delay */
    memory_timer_step(&mem, 4);
    TEST_ASSERT_EQUAL_UINT8(0x42, mem.io_registers[0x05]);
    TEST_ASSERT_TRUE_MESSAGE((mem.io_registers[0x0F] & 0x04) != 0, "Timer IF not set after reload");
}

int main(void) {
    UnityBegin();
    RUN_TEST(test_div_increments);
    RUN_TEST(test_tima_rates_and_overflow);
    return UnityEnd();
}
