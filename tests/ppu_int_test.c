#include <stdint.h>
#include "vendor/unity.h"
#include "../src/memory/memory.h"
#include "../src/ppu/ppu.h"

/* PPU timing constants from ppu.c */
#define DOTS_PER_LINE     456
#define OAM_SCAN_DOTS     80
#define PIXEL_TRANS_DOTS  172
#define HBLANK_DOTS       204
#define VBLANK_START      144

static void test_ppu_vblank_interrupt(void) {
    Memory mem;
    PPU ppu;
    memory_init(&mem);
    ppu_init(&ppu, &mem);

    /* Clear IF */
    mem.io_registers[0x0F] = 0x00;

    /* Enable LCD via IO register (ppu_step syncs from memory) */
    mem.io_registers[0x40] |= LCDC_DISPLAY_ENABLE;

    /* Run PPU up to VBlank (144 lines, 456 dots each = 65664 dots) */
    uint32_t cycles_per_step = 80;  /* Run in smaller chunks to see progress */
    uint32_t remaining = VBLANK_START * DOTS_PER_LINE;
    while (remaining > 0) {
        uint32_t step = remaining > cycles_per_step ? cycles_per_step : remaining;
        ppu_step(&ppu, step);
        remaining -= step;
    }
    TEST_ASSERT_TRUE_MESSAGE((mem.io_registers[0x0F] & 0x01) == 0x01, "VBlank IF not set at start of VBlank");

    /* Run rest of frame, VBlank bit should stay set until serviced */
    uint32_t cycles_in_vblank = ((TOTAL_LINES - VBLANK_START) * DOTS_PER_LINE);
    ppu_step(&ppu, cycles_in_vblank);
    TEST_ASSERT_TRUE_MESSAGE((mem.io_registers[0x0F] & 0x01) == 0x01, "VBlank IF cleared unexpectedly");
}

static void test_ppu_lcd_stat_interrupts(void) {
    Memory mem;
    PPU ppu;
    memory_init(&mem);
    ppu_init(&ppu, &mem);

    /* Clear IF */
    mem.io_registers[0x0F] = 0x00;

    /* Enable STAT HBlank interrupts via IO register */
    mem.io_registers[0x41] |= STAT_MODE0_INT;

    /* Enable LCD */
    mem.io_registers[0x40] |= LCDC_DISPLAY_ENABLE;

    /* Run PPU to first HBlank (need to go past Mode 3) */
    uint32_t cycles_to_hblank = OAM_SCAN_DOTS + PIXEL_TRANS_DOTS + 1;
    ppu_step(&ppu, cycles_to_hblank);
    TEST_ASSERT_TRUE_MESSAGE((mem.io_registers[0x0F] & 0x02) == 0x02, "LCD STAT HBlank IF not set");

    /* Clear IF, enable LYC interrupts, set LYC=1 via IO registers */
    mem.io_registers[0x0F] = 0x00;
    mem.io_registers[0x41] |= STAT_LYC_INT;
    mem.io_registers[0x45] = 1;

    /* Run PPU to line 1 */
    ppu_step(&ppu, HBLANK_DOTS);  /* finish line 0 */
    TEST_ASSERT_TRUE_MESSAGE((mem.io_registers[0x0F] & 0x02) == 0x02, "LCD STAT LYC IF not set");
}

int main(void) {
    UnityBegin();
    RUN_TEST(test_ppu_vblank_interrupt);
    RUN_TEST(test_ppu_lcd_stat_interrupts);
    return UnityEnd();
}