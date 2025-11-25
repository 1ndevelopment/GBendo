#include <stdint.h>
#include "vendor/unity.h"
#include "../src/memory/memory.h"
#include "../src/input/input.h"

static void test_input_press_release(void) {
    Memory mem;
    memory_init(&mem);
    input_init(&mem);

    /* Select buttons (P15 low) */
    mem.io_registers[0x00] = (mem.io_registers[0x00] & 0xCF) | (0 << 5) | (1 << 4);
    input_press(&mem, JP_A);
    TEST_ASSERT_EQUAL_UINT8(0x00, mem.io_registers[0x00] & 0x01);

    input_release(&mem, JP_A);
    TEST_ASSERT_EQUAL_UINT8(0x01, mem.io_registers[0x00] & 0x01);

    /* Select directions (P14 low) */
    mem.io_registers[0x00] = (mem.io_registers[0x00] & 0xCF) | (1 << 5) | (0 << 4);
    input_press(&mem, JP_RIGHT);
    TEST_ASSERT_EQUAL_UINT8(0x00, mem.io_registers[0x00] & 0x01);

    input_release(&mem, JP_RIGHT);
    TEST_ASSERT_EQUAL_UINT8(0x01, mem.io_registers[0x00] & 0x01);
}

int main(void) {
    UnityBegin();
    RUN_TEST(test_input_press_release);
    return UnityEnd();
}
