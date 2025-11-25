#include <stdint.h>
#include "vendor/unity.h"
#include "../src/memory/memory.h"
#include "../src/ppu/ppu.h"

/* Helpers to advance PPU to specific mode boundaries */
#define DOTS_PER_LINE     456
#define OAM_SCAN_DOTS     80
#define PIXEL_TRANS_DOTS  172

static void setup(Memory* mem, PPU* ppu) {
    memory_init(mem);
    ppu_init(ppu, mem);
}

static void step_to_mode(PPU* ppu, uint8_t mode) {
    /* Ensure LCD on - must write through memory since ppu_step syncs from memory */
    if (ppu->memory) {
        ppu->memory->io_registers[0x40] = LCDC_DISPLAY_ENABLE;
    }
    ppu->lcdc = LCDC_DISPLAY_ENABLE;
    
    /* If we're not at the start of a line, advance to the next line first */
    if (ppu->line_cycles > 0) {
        uint32_t to_next_line = DOTS_PER_LINE - ppu->line_cycles;
        ppu_step(ppu, to_next_line + 1);
    }
    
    /* Now advance within the current line to reach the desired mode */
    switch (mode) {
        case MODE_OAM_SCAN:
            ppu_step(ppu, 1);
            break;
        case MODE_PIXEL_TRANSFER:
            ppu_step(ppu, OAM_SCAN_DOTS + 1);
            break;
        case MODE_HBLANK:
            ppu_step(ppu, OAM_SCAN_DOTS + PIXEL_TRANS_DOTS + 1);
            break;
        default:
            break;
    }
}

static void test_vram_inaccessible_in_mode3(void) {
    Memory mem; PPU ppu; setup(&mem, &ppu);
    /* LCD on - write to memory so ppu_step will see it */
    mem.io_registers[0x40] = LCDC_DISPLAY_ENABLE;

    /* Write a known value into VRAM before mode 3 */
    memory_write(&mem, 0x8000, 0x12);
    TEST_ASSERT_EQUAL_UINT8(0x12, memory_read(&mem, 0x8000));

    /* Step to Mode 3 */
    step_to_mode(&ppu, MODE_PIXEL_TRANSFER);
    TEST_ASSERT_EQUAL_UINT8(MODE_PIXEL_TRANSFER, ppu.mode);

    /* CPU reads during Mode 3 should return 0xFF */
    uint8_t r = memory_read(&mem, 0x8001);
    TEST_ASSERT_EQUAL_UINT8(0xFF, r);

    /* CPU writes during Mode 3 should be ignored */
    memory_write(&mem, 0x8001, 0x34);
    /* After leaving Mode 3, content should remain unchanged (default 0x00) */
    step_to_mode(&ppu, MODE_HBLANK);
    uint8_t after = memory_read(&mem, 0x8001);
    TEST_ASSERT_EQUAL_UINT8(0x00, after);
}

static void test_oam_inaccessible_in_modes2_and_3(void) {
    Memory mem; PPU ppu; setup(&mem, &ppu);
    mem.io_registers[0x40] = LCDC_DISPLAY_ENABLE;

    /* Mode 2 */
    step_to_mode(&ppu, MODE_OAM_SCAN);
    TEST_ASSERT_EQUAL_UINT8(MODE_OAM_SCAN, ppu.mode);
    TEST_ASSERT_EQUAL_UINT8(0xFF, memory_read(&mem, 0xFE00));
    memory_write(&mem, 0xFE00, 0x77);
    step_to_mode(&ppu, MODE_HBLANK);
    TEST_ASSERT_EQUAL_UINT8(0x00, memory_read(&mem, 0xFE00));

    /* Mode 3 */
    step_to_mode(&ppu, MODE_PIXEL_TRANSFER);
    TEST_ASSERT_EQUAL_UINT8(MODE_PIXEL_TRANSFER, ppu.mode);
    TEST_ASSERT_EQUAL_UINT8(0xFF, memory_read(&mem, 0xFE01));
    memory_write(&mem, 0xFE01, 0x66);
    step_to_mode(&ppu, MODE_HBLANK);
    TEST_ASSERT_EQUAL_UINT8(0x00, memory_read(&mem, 0xFE01));
}

static void test_vram_oam_access_when_lcd_off(void) {
    Memory mem; PPU ppu; setup(&mem, &ppu);
    /* LCD off by default */
    memory_write(&mem, 0x8002, 0xAB);
    TEST_ASSERT_EQUAL_UINT8(0xAB, memory_read(&mem, 0x8002));
    memory_write(&mem, 0xFE10, 0xCD);
    TEST_ASSERT_EQUAL_UINT8(0xCD, memory_read(&mem, 0xFE10));
}

int main(void) {
    UnityBegin();
    RUN_TEST(test_vram_inaccessible_in_mode3);
    RUN_TEST(test_oam_inaccessible_in_modes2_and_3);
    RUN_TEST(test_vram_oam_access_when_lcd_off);
    return UnityEnd();
}
