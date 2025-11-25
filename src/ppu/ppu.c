#include "ppu.h"
#include "../memory/memory.h"
#include "ppu_vram.h"
#include "../gbendo.h"
#include "../ui/ui.h"
#include <stdio.h>  /* For printf debug */

/* PPU timing constants */
#define DOTS_PER_LINE     456
#define OAM_SCAN_DOTS     80
#define PIXEL_TRANS_DOTS  172
#define HBLANK_DOTS       204
#define VBLANK_START      144
#define TOTAL_LINES       154  /* VBLANK ends at line 153 */

/* Color Palettes - each palette has 4 colors from lightest to darkest */
static const uint32_t PALETTE_AUTHENTIC[4] = {
    0xFF9BBC0F,  /* Lightest - #9bbc0f */
    0xFF8BAC0F,  /* Light - #8bac0f */
    0xFF306230,  /* Dark - #306230 */
    0xFF0F380F   /* Darkest - #0f380f */
};

static const uint32_t PALETTE_GRAY[4] = {
    0xFFFFFFFF,  /* White */
    0xFFAAAAAA,  /* Light gray */
    0xFF555555,  /* Dark gray */
    0xFF000000   /* Black */
};

static const uint32_t PALETTE_BGB[4] = {
    0xFFE0F8D0,  /* Lightest - #e0f8d0 */
    0xFF88C070,  /* Light - #88c070 */
    0xFF346856,  /* Dark - #346856 */
    0xFF081820   /* Darkest - #081820 */
};

static const uint32_t PALETTE_POCKET[4] = {
    0xFFC4CFA1,  /* Lightest - #c4cfa1 */
    0xFF8B956D,  /* Light - #8b956d */
    0xFF4D533C,  /* Dark - #4d533c */
    0xFF1F1F1F   /* Darkest - #1f1f1f */
};

static const uint32_t PALETTE_LIGHT[4] = {
    0xFF00B581,  /* Lightest - #00b581 */
    0xFF009A71,  /* Light - #009a71 */
    0xFF00694A,  /* Dark - #00694a */
    0xFF004E2C   /* Darkest - #004e2c */
};

/* Palette names for UI display */
static const char* PALETTE_NAMES[] = {
    "Authentic DMG",
    "Grayscale",
    "BGB Emulator",
    "Game Boy Pocket",
    "Game Boy Light"
};

/* Current palette selection */
static int current_palette_index = 0;

/* Get the currently active palette */
static const uint32_t* get_active_palette(void) {
    switch (current_palette_index) {
        case 0: return PALETTE_AUTHENTIC;
        case 1: return PALETTE_GRAY;
        case 2: return PALETTE_BGB;
        case 3: return PALETTE_POCKET;
        case 4: return PALETTE_LIGHT;
        default: return PALETTE_AUTHENTIC;
    }
}

/* Palette management functions */
void ppu_set_palette(int palette_index) {
    if (palette_index >= 0 && palette_index < 5) {
        current_palette_index = palette_index;
    }
}

int ppu_get_palette_index(void) {
    return current_palette_index;
}

const char* ppu_get_palette_name(int index) {
    if (index >= 0 && index < 5) {
        return PALETTE_NAMES[index];
    }
    return NULL;
}

int ppu_get_palette_count(void) {
    return 5;
}

const uint32_t* ppu_get_palette_colors(int index) {
    switch (index) {
        case 0: return PALETTE_AUTHENTIC;
        case 1: return PALETTE_GRAY;
        case 2: return PALETTE_BGB;
        case 3: return PALETTE_POCKET;
        case 4: return PALETTE_LIGHT;
        default: return PALETTE_AUTHENTIC;
    }
}

void ppu_init(PPU* ppu, Memory* mem) {
    ppu->memory = mem;  /* Store memory back-reference */
    if (mem) {
        mem->ppu = ppu;  /* Store PPU reference in Memory for access restrictions */
    }
    ppu->lcdc = 0x00;  /* LCD starts disabled (boot ROM enables it) */
    ppu->stat = 0;
    ppu->scy = 0;
    ppu->scx = 0;
    ppu->ly = 0;
    ppu->lyc = 0;
    ppu->bgp = 0xFC;   /* Default background palette */
    ppu->obp0 = 0xFF;
    ppu->obp1 = 0xFF;
    ppu->wy = 0;
    ppu->wx = 0;
    
    /* Initialize PPU registers in memory */
    if (mem) {
        mem->io_registers[0x40] = ppu->lcdc;
        mem->io_registers[0x41] = ppu->stat;
        mem->io_registers[0x42] = ppu->scy;
        mem->io_registers[0x43] = ppu->scx;
        mem->io_registers[0x44] = ppu->ly;
        mem->io_registers[0x45] = ppu->lyc;
        mem->io_registers[0x47] = ppu->bgp;
        mem->io_registers[0x48] = ppu->obp0;
        mem->io_registers[0x49] = ppu->obp1;
        mem->io_registers[0x4A] = ppu->wy;
        mem->io_registers[0x4B] = ppu->wx;
    }
    
    ppu->mode = MODE_OAM_SCAN;
    ppu->line_cycles = 0;  /* Track cycles per line */
    ppu->clock = 0;
    ppu->frame_ready = false;
    
    /* Clear framebuffer */
    memset(ppu->framebuffer, 0xFF, sizeof(ppu->framebuffer));
    
    /* Clear OAM */
    memset(ppu->oam, 0, sizeof(ppu->oam));
    
    /* Initialize VRAM and CGB fields to safe defaults */
    ppu->cgb_mode = false;
    ppu->vram_bank = 0;
    memset(ppu->vram, 0, sizeof(ppu->vram));
}

void ppu_reset(PPU* ppu) {
    /* Keep current memory reference but reset state */
    Memory* mem = ppu->memory;
    ppu_init(ppu, mem);
}

void ppu_step(PPU* ppu, uint32_t cycles) {
    /* Sync PPU registers from memory (CPU may have written to them) */
    if (ppu->memory) {
        ppu->lcdc = ppu->memory->io_registers[0x40];
        ppu->stat = (ppu->stat & 0x07) | (ppu->memory->io_registers[0x41] & 0xF8); /* Keep mode bits */
        ppu->scy = ppu->memory->io_registers[0x42];
        ppu->scx = ppu->memory->io_registers[0x43];
        ppu->lyc = ppu->memory->io_registers[0x45];
        ppu->bgp = ppu->memory->io_registers[0x47];
        ppu->obp0 = ppu->memory->io_registers[0x48];
        ppu->obp1 = ppu->memory->io_registers[0x49];
        ppu->wy = ppu->memory->io_registers[0x4A];
        ppu->wx = ppu->memory->io_registers[0x4B];
        /* Always sync LY to memory so reads get current value */
        ppu->memory->io_registers[0x44] = ppu->ly;
    }
    
    /* Even when LCD is disabled, we should still step the PPU to advance LY
     * (though we don't render or change modes) */
    if (!(ppu->lcdc & LCDC_DISPLAY_ENABLE)) {
        /* When LCD is disabled, still advance LY but don't render or change modes */
        ppu->line_cycles += cycles;
        while (ppu->line_cycles >= DOTS_PER_LINE) {
            ppu->line_cycles -= DOTS_PER_LINE;
            ppu->ly++;
            if (ppu->ly >= TOTAL_LINES) {
                ppu->ly = 0;
            }
            /* Sync LY to memory */
            if (ppu->memory) {
                ppu->memory->io_registers[0x44] = ppu->ly;
            }
        }
        return;
    }
    
    ppu->line_cycles += cycles;
    ppu->clock += cycles;
    
    /* Each line has fixed timing: 
       Mode 2 (OAM) -> Mode 3 (PIXEL) -> Mode 0 (HBlank) -> Next line */
    while (ppu->line_cycles >= DOTS_PER_LINE) {
        /* Begin new line */
        ppu->line_cycles -= DOTS_PER_LINE;
        ppu->ly++;
        ppu->clock = 0;  /* Reset clock at line start */

        /* Check for VBlank */
        if (ppu->ly >= VBLANK_START) {
            /* Enter VBlank period */
            if (ppu->mode != MODE_VBLANK) {  /* Only trigger once at start of VBlank */
                GBEmulator* gb = gb_get_debug_gb();
                if (gb) {
                    ui_debug_log(UI_DEBUG_PPU, "[PPU] VBlank started - Frame ready (LY=%u, Cycles=%u)", ppu->ly, gb->cycles);
                }
                ppu->mode = MODE_VBLANK;
                ppu->stat = (ppu->stat & 0xFC) | MODE_VBLANK;
                if (ppu->memory) ppu->memory->io_registers[0x41] = ppu->stat; /* Sync back to memory */
                ppu->frame_ready = true;
                /* Request VBlank interrupt (bit 0) */
                if (ppu->memory) {
                    ppu->memory->io_registers[0x0F] |= 0x01;  /* VBlank interrupt */
                    /* Request LCD STAT interrupt if MODE 1 interrupts enabled */
                    if (ppu->stat & STAT_MODE1_INT) {
                        ppu->memory->io_registers[0x0F] |= 0x02;  /* LCD STAT interrupt */
                    }
                }
            }
        } else {
            /* Reset to Mode 2 (OAM scan) at start of line */
            ppu->mode = MODE_OAM_SCAN;
            ppu->stat = (ppu->stat & 0xFC) | MODE_OAM_SCAN;
            if (ppu->memory) {
                ppu->memory->io_registers[0x44] = ppu->ly; /* Sync LY */
                ppu->memory->io_registers[0x41] = ppu->stat; /* Sync back to memory */
            }
            if (ppu->stat & STAT_MODE2_INT) {
                /* Request LCD STAT interrupt for Mode 2 */
                if (ppu->memory) {
                    ppu->memory->io_registers[0x0F] |= 0x02;
                }
            }
        }

        /* End of VBlank (frame) */
        if (ppu->ly >= TOTAL_LINES) {
            ppu->ly = 0;
            ppu->mode = MODE_OAM_SCAN;
            ppu->stat = (ppu->stat & 0xFC) | MODE_OAM_SCAN;
            ppu->frame_ready = false; /* Reset frame ready flag */
            if (ppu->memory) {
                ppu->memory->io_registers[0x44] = ppu->ly; /* Sync LY */
                ppu->memory->io_registers[0x41] = ppu->stat; /* Sync STAT */
            }
        }
    }

    /* Handle mode transitions within the line */
    if (ppu->ly < VBLANK_START) {  /* Skip during VBlank */
        uint32_t line_pos = ppu->line_cycles;
        /* Mode boundaries: cycles 1-80 = Mode 2, cycles 81-252 = Mode 3, cycles 253-456 = Mode 0 */
        if (line_pos <= OAM_SCAN_DOTS) {
            /* Mode 2 - OAM Scan (cycles 1-80) */
            if (ppu->mode != MODE_OAM_SCAN) {
                ppu->mode = MODE_OAM_SCAN;
                ppu->stat = (ppu->stat & 0xFC) | MODE_OAM_SCAN;
                if (ppu->memory) ppu->memory->io_registers[0x41] = ppu->stat; /* Sync back to memory */
                if (ppu->stat & STAT_MODE2_INT) {
                    if (ppu->memory) ppu->memory->io_registers[0x0F] |= 0x02;
                }
            }
        } else if (line_pos <= (OAM_SCAN_DOTS + PIXEL_TRANS_DOTS)) {
            /* Mode 3 - Pixel Transfer (cycles 81-252) */
            if (ppu->mode != MODE_PIXEL_TRANSFER) {
                ppu->mode = MODE_PIXEL_TRANSFER;
                ppu->stat = (ppu->stat & 0xFC) | MODE_PIXEL_TRANSFER;
                if (ppu->memory) ppu->memory->io_registers[0x41] = ppu->stat; /* Sync back to memory */
            }
        } else {
            /* Mode 0 - HBlank */
            if (ppu->mode != MODE_HBLANK) {
                /* Choose renderer based on mode */
                if (ppu->cgb_mode) {
                    ppu_render_scanline_cgb(ppu);
                } else {
                    ppu_render_scanline(ppu);  /* Render at start of HBlank */
                }
                /* If HDMA in H-Blank mode is active, perform one HDMA block now */
                if (ppu->hdma_active && ppu->hdma_hblank && ppu->memory) {
                    ppu_hdma_step(ppu, ppu->memory);
                }
                ppu->mode = MODE_HBLANK;
                ppu->stat = (ppu->stat & 0xFC) | MODE_HBLANK;
                if (ppu->memory) ppu->memory->io_registers[0x41] = ppu->stat; /* Sync back to memory */
                if (ppu->stat & STAT_MODE0_INT) {
                    if (ppu->memory) ppu->memory->io_registers[0x0F] |= 0x02;
                }
            }
        }
        
        /* Sync LY back to memory */
        if (ppu->memory) {
            ppu->memory->io_registers[0x44] = ppu->ly;
        }
    }
    
    /* Check LYC comparison on every step (not just line transitions) */
    if (ppu->ly == ppu->lyc) {
        ppu->stat |= STAT_LYC_MATCH;
        if (ppu->memory) ppu->memory->io_registers[0x41] = ppu->stat;
        if (ppu->stat & STAT_LYC_INT) {
            /* Request LCD STAT interrupt for LYC match */
            if (ppu->memory) {
                ppu->memory->io_registers[0x0F] |= 0x02;
            }
        }
    } else {
        ppu->stat &= ~STAT_LYC_MATCH;
        if (ppu->memory) ppu->memory->io_registers[0x41] = ppu->stat;
    }
    }

uint8_t ppu_read_register(PPU* ppu, uint16_t address) {
    switch (address) {
        case 0xFF40: return ppu->lcdc;
        case 0xFF41: return ppu->stat;
        case 0xFF42: return ppu->scy;
        case 0xFF43: return ppu->scx;
        case 0xFF44: return ppu->ly;
        case 0xFF45: return ppu->lyc;
        case 0xFF47: return ppu->bgp;
        case 0xFF48: return ppu->obp0;
        case 0xFF49: return ppu->obp1;
        case 0xFF4A: return ppu->wy;
        case 0xFF4B: return ppu->wx;
        case 0xFF4F: return (ppu->vram_bank & 0x01) | 0xFE; /* VBK */
        case 0xFF68: return ppu->bgpi;                     /* BGPI */
        case 0xFF69: return ppu->bgpd[ppu->bgpi & 0x3F];   /* BGPD */
        case 0xFF6A: return ppu->obpi;                     /* OBPI */
        case 0xFF6B: return ppu->obpd[ppu->obpi & 0x3F];   /* OBPD */
        default: return 0xFF;
    }
}

void ppu_write_register(PPU* ppu, uint16_t address, uint8_t value) {
    switch (address) {
        case 0xFF40:
            if (!(value & LCDC_DISPLAY_ENABLE) && (ppu->lcdc & LCDC_DISPLAY_ENABLE)) {
                /* LCD turned off - reset PPU */
                ui_debug_log(UI_DEBUG_PPU, "[PPU] LCD disabled (LCDC: 0x%02X -> 0x%02X)", ppu->lcdc, value);
                ppu->ly = 0;
                ppu->mode = MODE_HBLANK;
                ppu->stat &= ~STAT_MODE;
                if (ppu->memory) {
                    ppu->memory->io_registers[0x44] = 0; /* LY = 0 */
                    ppu->memory->io_registers[0x41] = ppu->stat;
                }
            } else if ((value & LCDC_DISPLAY_ENABLE) && !(ppu->lcdc & LCDC_DISPLAY_ENABLE)) {
                /* LCD turned on - reset PPU state */
                ui_debug_log(UI_DEBUG_PPU, "[PPU] LCD enabled (LCDC: 0x%02X -> 0x%02X), BGP=0x%02X", ppu->lcdc, value, ppu->bgp);
                /* If BGP is still 0x00 (uninitialized), set a default palette */
                if (ppu->bgp == 0x00) {
                    ppu->bgp = 0xFC;  /* Default: white, black, black, black */
                    if (ppu->memory) {
                        ppu->memory->io_registers[0x47] = ppu->bgp;
                    }
                    ui_debug_log(UI_DEBUG_PPU, "[PPU] BGP was 0x00, setting default 0xFC");
                }
                ppu->ly = 0;
                ppu->line_cycles = 0;
                ppu->clock = 0;
                ppu->mode = MODE_OAM_SCAN;
                ppu->stat = (ppu->stat & 0xF8) | MODE_OAM_SCAN;
                if (ppu->memory) {
                    ppu->memory->io_registers[0x44] = 0;
                    ppu->memory->io_registers[0x41] = ppu->stat;
                }
            }
            ppu->lcdc = value;
            if (ppu->memory) {
                ppu->memory->io_registers[0x40] = value;
            }
            break;
            
        case 0xFF41:
            /* Only bits 3-6 are writable */
            ppu->stat = (value & 0x78) | (ppu->stat & 0x87);
            if (ppu->memory) {
                ppu->memory->io_registers[0x41] = ppu->stat;
            }
            break;
            
        case 0xFF42:
            ppu->scy = value;
            if (ppu->memory) ppu->memory->io_registers[0x42] = value;
            break;
        case 0xFF43:
            ppu->scx = value;
            if (ppu->memory) ppu->memory->io_registers[0x43] = value;
            break;
        case 0xFF45:
            ppu->lyc = value;
            if (ppu->memory) ppu->memory->io_registers[0x45] = value;
            break;
        case 0xFF47: 
            ppu->bgp = value;
            if (ppu->memory) {
                ppu->memory->io_registers[0x47] = value;
            }
            ui_debug_log(UI_DEBUG_PPU, "[PPU] BGP written: 0x%02X", value);
            break;
        case 0xFF48:
            ppu->obp0 = value;
            if (ppu->memory) ppu->memory->io_registers[0x48] = value;
            break;
        case 0xFF49:
            ppu->obp1 = value;
            if (ppu->memory) ppu->memory->io_registers[0x49] = value;
            break;
        case 0xFF4A:
            ppu->wy = value;
            if (ppu->memory) ppu->memory->io_registers[0x4A] = value;
            break;
        case 0xFF4B:
            ppu->wx = value;
            if (ppu->memory) ppu->memory->io_registers[0x4B] = value;
            break;
        case 0xFF4F: /* VBK */
            ppu->vram_bank = value & 0x01;
            if (ppu->memory) ppu->memory->io_registers[0x4F] = (ppu->vram_bank & 1) | 0xFE;
            break;
        case 0xFF68: /* BGPI */
        case 0xFF69: /* BGPD */
        case 0xFF6A: /* OBPI */
        case 0xFF6B: /* OBPD */
            ppu_write_cgb_registers(ppu, address, value);
            break;
    }
}

void ppu_oam_write(PPU* ppu, uint8_t index, uint8_t value) {
    if (index < sizeof(ppu->oam)) {
        ((uint8_t*)ppu->oam)[index] = value;
    }
}

uint8_t ppu_oam_read(PPU* ppu, uint8_t index) {
    if (index < sizeof(ppu->oam)) {
        return ((uint8_t*)ppu->oam)[index];
    }
    return 0xFF;
}

void ppu_render_scanline(PPU* ppu) {
    if (ppu->ly >= SCREEN_HEIGHT) return;

    uint8_t scanline[SCREEN_WIDTH];
    uint8_t sprite_scanline[SCREEN_WIDTH];  /* Track sprite pixels separately */
    uint8_t sprite_palette[SCREEN_WIDTH];   /* Track which palette to use for sprite pixels */
    for (int i = 0; i < SCREEN_WIDTH; i++) {
        scanline[i] = 0;
        sprite_scanline[i] = 0;
        sprite_palette[i] = 0;
    }

    /* Background */
    if (ppu->lcdc & LCDC_BG_ENABLE) {
        ppu_render_background(ppu, scanline);
    }

    /* Window */
    if ((ppu->lcdc & LCDC_WINDOW_ENABLE) && (ppu->ly >= ppu->wy)) {
        ppu_render_window(ppu, scanline);
    }

    /* Sprites - render to separate array */
    if (ppu->lcdc & LCDC_OBJ_ENABLE) {
        ppu_render_sprites(ppu, scanline, sprite_scanline, sprite_palette);
    }

    /* Map scanline palette indices to framebuffer colors */
    uint32_t* fb = &ppu->framebuffer[ppu->ly * SCREEN_WIDTH];
    
    for (int x = 0; x < SCREEN_WIDTH; x++) {
        uint8_t palette_index;
        /* Sprite layer is already composited with priority in ppu_render_sprites()
         * If sprite_scanline[x] is non-zero, sprite wins (already checked priority)
         * Otherwise, use background/window layer */
        if (sprite_scanline[x] != 0) {
            /* Sprite pixel - use sprite palette */
            uint8_t palette_reg = sprite_palette[x] ? ppu->obp1 : ppu->obp0;
            palette_index = (palette_reg >> (sprite_scanline[x] * 2)) & 0x03;
        } else {
            /* Background/window pixel - use background palette */
            palette_index = (ppu->bgp >> (scanline[x] * 2)) & 0x03;
        }
        fb[x] = get_active_palette()[palette_index];
    }
}

/* Note: get_tile_pixel is provided by the inline helper in ppu_vram.h */

/* Read a single pixel from VRAM tile data - PPU internal read bypasses mode check */
static uint8_t ppu_get_tile_pixel(PPU* ppu, Memory* mem, uint16_t tile_addr, int x, int y) {
    /* tile_addr points to the start of the 16-byte tile in VRAM (0x8000-0x9FFF) */
    if (!ppu || !mem) return 0;
    uint16_t base = tile_addr;
    uint16_t off = (y & 7) * 2;
    /* PPU internal read - access VRAM directly, bypassing mode check */
    uint8_t byte1 = mem->vram[base + off - 0x8000];
    uint8_t byte2 = mem->vram[base + off + 1 - 0x8000];
    int bit = 7 - x;
    return ((byte1 >> bit) & 1) | (((byte2 >> bit) & 1) << 1);
}

void ppu_render_background(PPU* ppu, uint8_t* scanline) {
    if (!ppu->memory) return;

    uint16_t tile_map_base = (ppu->lcdc & LCDC_BG_MAP) ? 0x9C00 : 0x9800;
    bool tile_data_select = (ppu->lcdc & LCDC_TILE_SELECT) != 0; /* 1 = 0x8000, 0 = 0x9000(signed) */

    uint16_t bg_y = (uint16_t)ppu->ly + (uint16_t)ppu->scy;
    uint8_t fine_y = bg_y & 0x07;
    uint16_t tile_row = (bg_y / 8) & 0x1F;

    for (int x = 0; x < SCREEN_WIDTH; x++) {
        uint16_t bg_x = (uint16_t)x + (uint16_t)ppu->scx;
        uint8_t fine_x = bg_x & 0x07;
        uint16_t tile_col = (bg_x / 8) & 0x1F;

        uint16_t map_addr = tile_map_base + tile_row * 32 + tile_col;
        /* PPU internal read - access VRAM directly, bypassing mode check */
        uint8_t tile_index = ppu->memory->vram[map_addr - 0x8000];

        /* Get tile data using helper */
        uint16_t tile_addr = vram_get_tile_addr(tile_index, tile_data_select);
        uint16_t row_addr = vram_get_tile_row_addr(tile_addr, fine_y);
        
        /* Validate row_addr and row_addr+1 are both within tile data region */
        if (IS_TILE_DATA_ADDR(row_addr) && IS_TILE_DATA_ADDR(row_addr + 1)) {
            /* PPU internal read - access VRAM directly, bypassing mode check */
            uint8_t byte1 = ppu->memory->vram[row_addr - 0x8000];
            uint8_t byte2 = ppu->memory->vram[row_addr + 1 - 0x8000];
            scanline[x] = vram_get_tile_pixel(byte1, byte2, fine_x);
        } else {
            scanline[x] = 0;
        }
    }
}

void ppu_render_window(PPU* ppu, uint8_t* scanline) {
    if (ppu->wx >= SCREEN_WIDTH + 7) return;
    if (!ppu->memory) return;

    uint16_t tile_map_base = (ppu->lcdc & LCDC_WINDOW_MAP) ? 0x9C00 : 0x9800;
    bool tile_data_select = (ppu->lcdc & LCDC_TILE_SELECT) != 0;

    uint8_t window_y = ppu->ly - ppu->wy;
    uint8_t fine_y = window_y & 0x07;
    uint16_t tile_row = (window_y / 8) & 0x1F;

    /* Window X position: WX=7 means window starts at screen X=0 */
    int win_x_start = (int)ppu->wx - 7;
    
    /* Only render window if it's visible on screen */
    if (win_x_start >= SCREEN_WIDTH) return;
    
    /* Calculate actual screen X range for window */
    int screen_x_start = (win_x_start < 0) ? 0 : win_x_start;
    
    for (int screen_x = screen_x_start; screen_x < SCREEN_WIDTH; screen_x++) {
        int window_x = screen_x - win_x_start;
        uint8_t fine_x = window_x & 0x07;
        uint16_t tile_col = (window_x / 8) & 0x1F;

        uint16_t map_addr = tile_map_base + tile_row * 32 + tile_col;
        /* PPU internal read - access VRAM directly, bypassing mode check */
        uint8_t tile_index = ppu->memory->vram[map_addr - 0x8000];

        uint16_t tile_addr = vram_get_tile_addr(tile_index, tile_data_select);
        uint16_t row_addr = vram_get_tile_row_addr(tile_addr, fine_y);

        /* Validate row_addr and row_addr+1 are both within tile data region */
        if (IS_TILE_DATA_ADDR(row_addr) && IS_TILE_DATA_ADDR(row_addr + 1)) {
            scanline[screen_x] = ppu_get_tile_pixel(ppu, ppu->memory, tile_addr, fine_x, fine_y);
        }
    }
}

void ppu_render_sprites(PPU* ppu, uint8_t* scanline, uint8_t* sprite_scanline, uint8_t* sprite_palette) {
    bool tall_sprites = ppu->lcdc & LCDC_OBJ_SIZE;
    uint8_t sprite_height = tall_sprites ? 16 : 8;
    
    /* Find visible sprites on this scanline */
    struct {
        const uint8_t* sprite;
        uint8_t x;
        uint8_t flags;
        uint8_t oam_index;
    } visible_sprites[10];
    int num_sprites = 0;
    
    /* Read OAM entries directly - PPU internal rendering bypasses access restrictions */
    for (int i = 0; i < 40 && num_sprites < 10; i++) {
        /* Read directly from OAM array, not through ppu_read_oam which blocks during pixel transfer */
        uint8_t y = ((uint8_t*)ppu->oam)[i * 4 + 0];
        uint8_t x = ((uint8_t*)ppu->oam)[i * 4 + 1];
        uint8_t tile = ((uint8_t*)ppu->oam)[i * 4 + 2];
        uint8_t flags = ((uint8_t*)ppu->oam)[i * 4 + 3];

        /* Sprite Y position: 0 = off-screen above, 16 = top of screen, 160+ = off-screen below */
        if (y == 0 || y >= 160 + 16) continue;  /* Sprite is completely hidden */
        
        /* Calculate sprite's on-screen Y position (can be negative if partially off-top) */
        int sprite_top = (int)y - 16;
        int sprite_bottom = sprite_top + sprite_height;
        
        /* Check if sprite is visible on this scanline */
        if (ppu->ly >= sprite_top && ppu->ly < sprite_bottom) {
            visible_sprites[num_sprites].sprite = NULL; /* not used */
            visible_sprites[num_sprites].x = x;
            visible_sprites[num_sprites].flags = flags;
            visible_sprites[num_sprites].oam_index = (uint8_t)i;
            /* Pack tile & sprite_top into local storage by reusing sprite pointer cast */
            visible_sprites[num_sprites].sprite = (const uint8_t*)(uintptr_t)((tile << 8) | (sprite_top & 0xFF));
            num_sprites++;
        }
    }
    
    /* Sort by X ascending, then OAM index ascending (priority = smaller X, then earlier OAM) */
    for (int a = 0; a < num_sprites; a++) {
        for (int b = a + 1; b < num_sprites; b++) {
            if (visible_sprites[b].x < visible_sprites[a].x ||
               (visible_sprites[b].x == visible_sprites[a].x && visible_sprites[b].oam_index < visible_sprites[a].oam_index)) {
                typeof(visible_sprites[0]) tmp = visible_sprites[a];
                visible_sprites[a] = visible_sprites[b];
                visible_sprites[b] = tmp;
            }
        }
    }

    /* Render visible sprites in sorted order (later writes win) */
    for (int i = 0; i < num_sprites; i++) {
        const uint8_t* sprite_packed = visible_sprites[i].sprite;
        int x = (int)visible_sprites[i].x - 8;  /* Adjust for sprite X offset (can be negative) */
        uint8_t flags = visible_sprites[i].flags;

        uint16_t packed = (uint16_t)(uintptr_t)sprite_packed;
        uint8_t tile = (packed >> 8) & 0xFF;
        uint8_t adj_sprite_y = (uint8_t)(packed & 0xFF);

        /* Calculate row within the sprite (0-7 or 0-15) */
        /* adj_sprite_y can be negative (wrapped as uint8_t), so use signed arithmetic */
        int sprite_row = (int)ppu->ly - (int8_t)adj_sprite_y;
        
        /* Validate sprite_row is within bounds */
        if (sprite_row < 0 || sprite_row >= sprite_height) continue;

        if (flags & 0x40) {  /* Y-flip */
            sprite_row = (sprite_height - 1) - sprite_row;
        }

        /* Palette selection */
        uint8_t use_obp1 = (flags & 0x10) != 0;
        
        /* For 8x16 sprites, select the correct tile based on row */
        uint8_t actual_tile = tile;
        int tile_row = sprite_row;
        if (tall_sprites) {
            if (sprite_row >= 8) {
                /* Bottom half - use next tile */
                actual_tile = tile | 0x01;
                tile_row = sprite_row - 8;
            } else {
                /* Top half - use even tile */
                actual_tile = tile & 0xFE;
                tile_row = sprite_row;
            }
        }

        /* For each pixel in sprite */
        for (int px = 0; px < 8; px++) {
            int dst_x = x + px;
            if (dst_x < 0 || dst_x >= SCREEN_WIDTH) continue;

            uint8_t sprite_x = px;
            if (flags & 0x20) {  /* X-flip */
                sprite_x = 7 - px;
            }

            /* Get pixel from tile via VRAM read helper */
            uint16_t tile_addr = 0x8000 + ((uint16_t)actual_tile * 16);
            uint8_t color = ppu_get_tile_pixel(ppu, ppu->memory, tile_addr, sprite_x, tile_row);
            if (color == 0) continue; /* Transparent */

            /* Priority: if OBJ-to-BG priority set, sprite is behind non-zero BG pixels */
            if (flags & 0x80) {
                if (scanline[dst_x] != 0) continue; /* BG non-zero, skip */
            }

            /* Draw sprite pixel - store raw color index, not palette-mapped */
            sprite_scanline[dst_x] = color;
            sprite_palette[dst_x] = use_obp1;
        }
    }
}