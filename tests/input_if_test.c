#include <stdint.h>
#include "vendor/unity.h"
#include "../src/memory/memory.h"
#include "../src/input/input.h"

static void test_input_sets_if_when_visible(void) {
    Memory mem;
    memory_init(&mem);
    input_init(&mem);

    /* Clear IF */
    mem.io_registers[0x0F] = 0x00;

    /* Select buttons (P15 low) */
    mem.io_registers[0x00] = (mem.io_registers[0x00] & 0xCF) | (0 << 5) | (1 << 4);
    printf("JOYP after select write = 0x%02X\n", mem.io_registers[0x00]);
    uint8_t oldv = mem.io_registers[0x00] & 0x0F;
    input_press(&mem, JP_A);
    uint8_t nowv = mem.io_registers[0x00] & 0x0F;
    uint8_t fell = (oldv & ~nowv) & 0x0F;
    printf("old=0x%X now=0x%X fell=0x%X JOYP=0x%02X IF=0x%02X\n", oldv, nowv, fell, mem.io_registers[0x00], mem.io_registers[0x0F]);
    TEST_ASSERT_TRUE_MESSAGE((mem.io_registers[0x0F] & 0x10) == 0x10, "IF joypad bit should be set when A pressed (buttons selected)");

    /* Clear IF */
    mem.io_registers[0x0F] = 0x00;
    input_release(&mem, JP_A);

    /* Select directions (P14 low) - pressing A now is not visible, should NOT set IF */
    mem.io_registers[0x00] = (mem.io_registers[0x00] & 0xCF) | (1 << 5) | (0 << 4);
    input_press(&mem, JP_A);
    TEST_ASSERT_FALSE_MESSAGE((mem.io_registers[0x0F] & 0x10) == 0x10, "IF joypad bit should NOT be set when A pressed (directions selected)");

    /* Now press Right with directions selected - should set IF */
    mem.io_registers[0x0F] = 0x00;
    input_press(&mem, JP_RIGHT);
    TEST_ASSERT_TRUE_MESSAGE((mem.io_registers[0x0F] & 0x10) == 0x10, "IF joypad bit should be set when Right pressed (directions selected)");
}

int main(void) {
    UnityBegin();
    RUN_TEST(test_input_sets_if_when_visible);
    return UnityEnd();
}
