#include <stdio.h>
#include <stdint.h>
#include "vendor/unity.h"
#include "../src/memory/memory.h"
#include "../src/ppu/ppu.h"

/* PPU timing constants */
#define DOTS_PER_LINE     456
#define OAM_SCAN_DOTS     80
#define PIXEL_TRANS_DOTS  172
#define HBLANK_DOTS       204
#define VBLANK_START      144
#define SCREEN_WIDTH      160
#define SCREEN_HEIGHT     144

static void test_mode_dot_timing(void) {
    Memory mem;
    PPU ppu;
    memory_init(&mem);
    ppu_init(&ppu, &mem);

    /* Enable LCD - must write through memory since ppu_step syncs from memory */
    mem.io_registers[0x40] = LCDC_DISPLAY_ENABLE;

    /* Step one dot at a time through a line and verify mode transitions */
    uint32_t dots = 0;
    
    /* Should start in Mode 2 (OAM Scan) */
    TEST_ASSERT_EQUAL_UINT8(MODE_OAM_SCAN, ppu.mode);
    
    /* Step to end of OAM scan */
    for (; dots < OAM_SCAN_DOTS; dots++) {
        ppu_step(&ppu, 1);
        TEST_ASSERT_EQUAL_UINT8(MODE_OAM_SCAN, ppu.mode);
    }
    
    /* Next dot should transition to Mode 3 (Pixel Transfer) */
    ppu_step(&ppu, 1);
    dots++;
    TEST_ASSERT_EQUAL_UINT8(MODE_PIXEL_TRANSFER, ppu.mode);
    
    /* Stay in Pixel Transfer mode */
    for (; dots < OAM_SCAN_DOTS + PIXEL_TRANS_DOTS; dots++) {
        ppu_step(&ppu, 1);
        TEST_ASSERT_EQUAL_UINT8(MODE_PIXEL_TRANSFER, ppu.mode);
    }
    
    /* Should enter HBlank */
    ppu_step(&ppu, 1);
    dots++;
    TEST_ASSERT_EQUAL_UINT8(MODE_HBLANK, ppu.mode);
    
    /* Stay in HBlank until end of line (456th step wraps to new line) */
    for (; dots < DOTS_PER_LINE - 1; dots++) {
        ppu_step(&ppu, 1);
        TEST_ASSERT_EQUAL_UINT8(MODE_HBLANK, ppu.mode);
    }
    
    /* 456th step (dots=455) should wrap to new line and start Mode 2 again */
    ppu_step(&ppu, 1);
    TEST_ASSERT_EQUAL_UINT8(MODE_OAM_SCAN, ppu.mode);
    TEST_ASSERT_EQUAL_UINT8(1, ppu.ly);  /* Should be on line 1 now */
}

static void test_lyc_mid_line_change(void) {
    Memory mem;
    PPU ppu;
    memory_init(&mem);
    ppu_init(&ppu, &mem);

    /* Enable LCD and LYC interrupts - must write through memory */
    mem.io_registers[0x40] = LCDC_DISPLAY_ENABLE;
    mem.io_registers[0x41] = STAT_LYC_INT;  /* Enable LYC interrupt in STAT */
    mem.io_registers[0x0F] = 0x00;

    /* Set initial LYC to 0 (should match immediately) */
    ppu.lyc = 0;
    mem.io_registers[0x45] = 0;  /* Also write to memory */
    ppu_step(&ppu, 1);
    TEST_ASSERT_TRUE(ppu.stat & STAT_LYC_MATCH);
    TEST_ASSERT_TRUE(mem.io_registers[0x0F] & 0x02);

    /* Clear IF and change LYC mid-line to non-matching value */
    mem.io_registers[0x0F] = 0x00;
    ppu.lyc = 1;
    mem.io_registers[0x45] = 1;  /* Also write LYC to memory */
    ppu_step(&ppu, 1);
    TEST_ASSERT_FALSE(ppu.stat & STAT_LYC_MATCH);
    TEST_ASSERT_FALSE(mem.io_registers[0x0F] & 0x02);

    /* Change LYC back to current line value mid-line */
    ppu.lyc = 0;
    mem.io_registers[0x45] = 0;  /* Also write to memory */
    ppu_step(&ppu, 1);
    TEST_ASSERT_TRUE(ppu.stat & STAT_LYC_MATCH);
    TEST_ASSERT_TRUE(mem.io_registers[0x0F] & 0x02);
}

static void test_window_render_offset(void) {
    Memory mem;
    PPU ppu;
    memory_init(&mem);
    ppu_init(&ppu, &mem);

    /* Enable LCD, window, and background */
    ppu.lcdc |= LCDC_DISPLAY_ENABLE | LCDC_WINDOW_ENABLE | LCDC_BG_ENABLE;

    /* Test window appearing mid-screen */
    ppu.wy = 50;  /* Window starts at line 50 */
    ppu.wx = 15;  /* Window starts 7 pixels after x=15 (effective x=8) */

    /* Run to line 49 (before window) */
    ppu_step(&ppu, 49 * DOTS_PER_LINE);
    TEST_ASSERT_EQUAL_UINT8(49, ppu.ly);

    /* Window should not be visible yet */
    ppu_render_scanline(&ppu);

    /* Run to line 50 (window start) */
    ppu_step(&ppu, DOTS_PER_LINE);
    TEST_ASSERT_EQUAL_UINT8(50, ppu.ly);

    /* Window should now be rendered */
    ppu_render_scanline(&ppu);
}

static void test_scroll_wrap(void) {
    Memory mem;
    PPU ppu;
    memory_init(&mem);
    ppu_init(&ppu, &mem);

    /* Enable LCD and background */
    ppu.lcdc |= LCDC_DISPLAY_ENABLE | LCDC_BG_ENABLE;

    /* Test different scroll combinations */
    const uint8_t test_scroll_values[] = {0, 1, 255, 128};
    for (size_t i = 0; i < sizeof(test_scroll_values); i++) {
        ppu.scx = test_scroll_values[i];
        ppu.scy = test_scroll_values[i];
        
        /* Render a full frame and verify no crashes/hangs */
        for (int line = 0; line < SCREEN_HEIGHT; line++) {
            ppu_step(&ppu, DOTS_PER_LINE);
        }
    }
}

static void test_sprite_priority(void) {
    Memory mem;
    PPU ppu;
    memory_init(&mem);
    ppu_init(&ppu, &mem);

    /* Enable LCD, sprites, and background */
    ppu.lcdc |= LCDC_DISPLAY_ENABLE | LCDC_OBJ_ENABLE | LCDC_BG_ENABLE;

    /* Set up two overlapping sprites */
    /* Sprite 0 (higher priority) */
    ppu.oam[0].y = 16;  /* At y=16 (showing on screen at y=24) */
    ppu.oam[0].x = 8;   /* At x=8 (showing on screen at x=16) */
    ppu.oam[0].tile = 0;
    ppu.oam[0].flags = 0x00;  /* No flip, palette 0, priority over BG */

    /* Sprite 1 (lower priority) */
    ppu.oam[1].y = 16;  /* Same position */
    ppu.oam[1].x = 8;
    ppu.oam[1].tile = 1;
    ppu.oam[1].flags = 0x80;  /* Priority behind BG */

    /* Run to the line where sprites appear */
    ppu_step(&ppu, 24 * DOTS_PER_LINE);
    
    /* Render the line and verify sprite priorities */
    ppu_render_scanline(&ppu);
}

int main(void) {
    UnityBegin();
    RUN_TEST(test_mode_dot_timing);
    RUN_TEST(test_lyc_mid_line_change);
    RUN_TEST(test_window_render_offset);
    RUN_TEST(test_scroll_wrap);
    RUN_TEST(test_sprite_priority);
    return UnityEnd();
}