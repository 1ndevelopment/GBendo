#include <stdint.h>
#include "vendor/unity.h"
#include "../src/memory/memory.h"

static void test_div_write_during_reload(void) {
    Memory mem;
    memory_init(&mem);
    memory_timer_init(&mem);

    /* Prepare: set TIMA to overflow on next tick */
    mem.tima = 0xFF;
    mem.tma = 0x55;
    mem.tac = 0x04 | 0x01; /* enabled, fast */
    mem.io_registers[0x07] = mem.tac;

    /* Cause overflow by stepping 16 cycles */
    memory_timer_step(&mem, 16);
    TEST_ASSERT_EQUAL_UINT8(0x00, mem.io_registers[0x05]);

    /* Now write DIV (reset) during reload delay */
    memory_write(&mem, 0xFF04, 0);
    /* Advance 4 cycles to finish reload */
    memory_timer_step(&mem, 4);
    TEST_ASSERT_EQUAL_UINT8(0x55, mem.io_registers[0x05]);
    TEST_ASSERT_TRUE_MESSAGE((mem.io_registers[0x0F] & 0x04) != 0, "IF not set after reload");
}

static void test_tac_change_during_reload(void) {
    Memory mem;
    memory_init(&mem);
    memory_timer_init(&mem);

    mem.tima = 0xFF;
    mem.tma = 0x77;
    mem.tac = 0x04 | 0x01; /* enabled */
    mem.io_registers[0x07] = mem.tac;

    memory_timer_step(&mem, 16); /* overflow */
    TEST_ASSERT_EQUAL_UINT8(0x00, mem.io_registers[0x05]);

    /* Change TAC to a different input clock while reload pending */
    memory_write(&mem, 0xFF07, 0x04 | 0x02); /* select different clock */
    /* Finish reload */
    memory_timer_step(&mem, 4);
    TEST_ASSERT_EQUAL_UINT8(0x77, mem.io_registers[0x05]);
    TEST_ASSERT_TRUE_MESSAGE((mem.io_registers[0x0F] & 0x04) != 0, "IF not set after reload with TAC change");
}

int main(void) {
    UnityBegin();
    RUN_TEST(test_div_write_during_reload);
    RUN_TEST(test_tac_change_during_reload);
    return UnityEnd();
}
