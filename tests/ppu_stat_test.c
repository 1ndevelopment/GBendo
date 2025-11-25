#include <stdint.h>
#include "vendor/unity.h"
#include "../src/memory/memory.h"
#include "../src/ppu/ppu.h"

/* Timing constants used by PPU */
#define DOTS_PER_LINE     456
#define OAM_SCAN_DOTS     80
#define PIXEL_TRANS_DOTS  172

static void test_ppu_mode2_stat_request(void) {
    Memory mem;
    PPU ppu;
    memory_init(&mem);
    ppu_init(&ppu, &mem);

    /* Clear IF */
    mem.io_registers[0x0F] = 0x00;

    /* Enable STAT Mode 2 interrupt and LCD via IO registers */
    mem.io_registers[0x41] |= STAT_MODE2_INT;
    mem.io_registers[0x40] |= LCDC_DISPLAY_ENABLE;

    /* Advance one full line so the PPU will re-enter Mode 2 at the next line start */
    ppu_step(&ppu, DOTS_PER_LINE);
    TEST_ASSERT_TRUE_MESSAGE((mem.io_registers[0x0F] & 0x02) == 0x02, "STAT Mode2 IF not set");
}

static void test_ppu_mode0_stat_request(void) {
    Memory mem;
    PPU ppu;
    memory_init(&mem);
    ppu_init(&ppu, &mem);

    mem.io_registers[0x0F] = 0x00;

    /* Enable STAT Mode 0 (HBlank) interrupt and LCD via IO registers */
    mem.io_registers[0x41] |= STAT_MODE0_INT;
    mem.io_registers[0x40] |= LCDC_DISPLAY_ENABLE;

    /* Run until HBlank within the first visible line (need to go past Mode 3) */
    uint32_t cycles_to_hblank = OAM_SCAN_DOTS + PIXEL_TRANS_DOTS + 1;
    ppu_step(&ppu, cycles_to_hblank);

    TEST_ASSERT_TRUE_MESSAGE((mem.io_registers[0x0F] & 0x02) == 0x02, "STAT Mode0 (HBlank) IF not set");
}

static void test_ppu_lyc_match_request(void) {
    Memory mem;
    PPU ppu;
    memory_init(&mem);
    ppu_init(&ppu, &mem);

    mem.io_registers[0x0F] = 0x00;

    /* Set LYC to 1 and enable LYC interrupts via IO registers. The PPU will increment LY at line start. */
    mem.io_registers[0x45] = 1;
    mem.io_registers[0x41] |= STAT_LYC_INT;
    mem.io_registers[0x40] |= LCDC_DISPLAY_ENABLE;

    /* Advance one full line so LY becomes 1 and LYC check runs */
    ppu_step(&ppu, DOTS_PER_LINE);
    TEST_ASSERT_TRUE_MESSAGE((mem.io_registers[0x0F] & 0x02) == 0x02, "STAT LYC IF not set on LYC match");
}

int main(void) {
    UnityBegin();
    RUN_TEST(test_ppu_mode2_stat_request);
    RUN_TEST(test_ppu_mode0_stat_request);
    RUN_TEST(test_ppu_lyc_match_request);
    return UnityEnd();
}
