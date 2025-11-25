#include "ppu.h"

/* CGB color format conversion */
static uint32_t cgb_color_to_rgb(uint16_t color) {
    uint8_t r = (color & 0x1F) << 3;        /* Red component (5 bits) */
    uint8_t g = ((color >> 5) & 0x1F) << 3; /* Green component (5 bits) */
    uint8_t b = ((color >> 10) & 0x1F) << 3;/* Blue component (5 bits) */
    
    /* Expand from 5 to 8 bits by duplicating highest bits */
    r |= r >> 5;
    g |= g >> 5;
    b |= b >> 5;
    
    return (0xFF << 24) | (r << 16) | (g << 8) | b;
}

/* CGB Background Color Cache */
static struct {
    uint32_t colors[8][4];  /* 8 palettes, 4 colors each */
} bg_color_cache;

/* CGB Sprite Color Cache */
static struct {
    uint32_t colors[8][4];  /* 8 palettes, 4 colors each */
} sprite_color_cache;

void ppu_init_cgb(PPU* ppu) {
    /* Initialize CGB-specific registers */
    memset(ppu->bgpd, 0, sizeof(ppu->bgpd));
    memset(ppu->obpd, 0, sizeof(ppu->obpd));
    ppu->bgpi = 0;
    ppu->obpi = 0;
    ppu->vram_bank = 0;
    
    /* Clear color caches */
    memset(&bg_color_cache, 0, sizeof(bg_color_cache));
    memset(&sprite_color_cache, 0, sizeof(sprite_color_cache));
}

void ppu_write_cgb_registers(PPU* ppu, uint16_t address, uint8_t value) {
    switch (address) {
        case 0xFF68:  /* BGPI - Background Palette Index */
            ppu->bgpi = value;
            break;
            
        case 0xFF69:  /* BGPD - Background Palette Data */
            {
                uint8_t index = ppu->bgpi & 0x3F;
                ppu->bgpd[index] = value;
                
                /* Auto-increment if enabled */
                if (ppu->bgpi & 0x80) {
                    ppu->bgpi = 0x80 | ((index + 1) & 0x3F);
                }
                
                /* Update color cache */
                uint8_t palette = (index >> 3) & 7;
                uint8_t color = (index >> 1) & 3;
                if ((index & 1) == 0) {
                    /* Low byte */
                    bg_color_cache.colors[palette][color] &= 0xFF00;
                    bg_color_cache.colors[palette][color] |= value;
                } else {
                    /* High byte */
                    bg_color_cache.colors[palette][color] &= 0x00FF;
                    bg_color_cache.colors[palette][color] |= (value << 8);
                    /* Convert to RGB32 */
                    bg_color_cache.colors[palette][color] = 
                        cgb_color_to_rgb(bg_color_cache.colors[palette][color]);
                }
            }
            break;
            
        case 0xFF6A:  /* OBPI - Sprite Palette Index */
            ppu->obpi = value;
            break;
            
        case 0xFF6B:  /* OBPD - Sprite Palette Data */
            {
                uint8_t index = ppu->obpi & 0x3F;
                ppu->obpd[index] = value;
                
                /* Auto-increment if enabled */
                if (ppu->obpi & 0x80) {
                    ppu->obpi = 0x80 | ((index + 1) & 0x3F);
                }
                
                /* Update color cache */
                uint8_t palette = (index >> 3) & 7;
                uint8_t color = (index >> 1) & 3;
                if ((index & 1) == 0) {
                    /* Low byte */
                    sprite_color_cache.colors[palette][color] &= 0xFF00;
                    sprite_color_cache.colors[palette][color] |= value;
                } else {
                    /* High byte */
                    sprite_color_cache.colors[palette][color] &= 0x00FF;
                    sprite_color_cache.colors[palette][color] |= (value << 8);
                    /* Convert to RGB32 */
                    sprite_color_cache.colors[palette][color] = 
                        cgb_color_to_rgb(sprite_color_cache.colors[palette][color]);
                }
            }
            break;
            
        case 0xFF4F:  /* VBK - VRAM Bank Select */
            ppu->vram_bank = value & 0x01;
            break;
    }
}

uint8_t ppu_read_cgb_registers(PPU* ppu, uint16_t address) {
    switch (address) {
        case 0xFF68:  /* BGPI */
            return ppu->bgpi;
            
        case 0xFF69:  /* BGPD */
            return ppu->bgpd[ppu->bgpi & 0x3F];
            
        case 0xFF6A:  /* OBPI */
            return ppu->obpi;
            
        case 0xFF6B:  /* OBPD */
            return ppu->obpd[ppu->obpi & 0x3F];
            
        case 0xFF4F:  /* VBK */
            return ppu->vram_bank;
            
        default:
            return 0xFF;
    }
}

void ppu_render_scanline_cgb(PPU* ppu) {
    uint8_t scanline[SCREEN_WIDTH];
    uint8_t attributes[SCREEN_WIDTH];
    uint8_t priorities[SCREEN_WIDTH];
    
    /* Clear scanline buffers */
    memset(scanline, 0, SCREEN_WIDTH);
    memset(attributes, 0, SCREEN_WIDTH);
    memset(priorities, 0, SCREEN_WIDTH);
    
    /* Render background if enabled */
    if (ppu->lcdc & LCDC_BG_ENABLE) {
        ppu_render_background_cgb(ppu, scanline, attributes, priorities);
    }
    
    /* Render window if enabled */
    if ((ppu->lcdc & LCDC_WINDOW_ENABLE) && ppu->ly >= ppu->wy) {
        ppu_render_window_cgb(ppu, scanline, attributes, priorities);
    }
    
    /* Render sprites if enabled */
    if (ppu->lcdc & LCDC_OBJ_ENABLE) {
        ppu_render_sprites_cgb(ppu, scanline, attributes, priorities);
    }
    
    /* Apply colors and copy to framebuffer */
    uint32_t* fb = &ppu->framebuffer[ppu->ly * SCREEN_WIDTH];
    for (int x = 0; x < SCREEN_WIDTH; x++) {
        uint8_t color_idx = scanline[x];
        uint8_t attrs = attributes[x];
        uint8_t palette = (attrs >> 2) & 7;
        
        /* Get final color based on the layer (BG or Sprite) */
        if (attrs & 0x80) {  /* Sprite pixel */
            fb[x] = sprite_color_cache.colors[palette][color_idx];
        } else {  /* Background pixel */
            fb[x] = bg_color_cache.colors[palette][color_idx];
        }
    }
}

/* CGB-specific tile rendering with attributes */
/* Render window layer with CGB attributes */
void ppu_render_window_cgb(PPU* ppu, uint8_t* scanline, uint8_t* attributes, uint8_t* priorities) {
    static uint8_t window_line = 0;
    
    /* Only render window if within visible area */
    if (ppu->ly < ppu->wy || ppu->wx > 166) return;
    
    uint16_t tile_map = (ppu->lcdc & LCDC_WINDOW_MAP) ? 0x9C00 : 0x9800;
    uint16_t tile_data = (ppu->lcdc & LCDC_TILE_SELECT) ? 0x8000 : 0x9000;
    uint8_t win_x_start = ppu->wx - 7;
    
    uint8_t tile_y = window_line / 8;
    uint8_t fine_y = window_line % 8;
    
    for (int x = win_x_start; x < SCREEN_WIDTH; x++) {
        uint8_t tile_x = (x - win_x_start) / 8;
        uint8_t fine_x = (x - win_x_start) % 8;
        
        uint16_t map_addr = tile_map + (tile_y * 32) + tile_x;
        uint8_t tile_index = ppu->vram[0][map_addr & 0x1FFF];
        uint8_t tile_attrs = ppu->vram[1][map_addr & 0x1FFF];
        
        uint8_t palette = (tile_attrs >> 2) & 7;
        bool tile_bank = (tile_attrs & 0x08) != 0;
        bool x_flip = (tile_attrs & 0x20) != 0;
        bool y_flip = (tile_attrs & 0x40) != 0;
        bool priority = (tile_attrs & 0x80) != 0;
        
        if (y_flip) {
            fine_y = 7 - fine_y;
        }
        if (x_flip) {
            fine_x = 7 - fine_x;
        }
        
        uint16_t tile_addr;
        if (tile_data == 0x8000) {
            tile_addr = tile_data + (tile_index * 16);
        } else {
            tile_addr = tile_data + ((int8_t)tile_index * 16);
        }
        
        const uint8_t* tile = &ppu->vram[tile_bank ? 1 : 0][tile_addr & 0x1FFF];
        
        uint8_t color = get_tile_pixel(tile, fine_x, fine_y);
        
        scanline[x] = color;
        /* Store BG palette consistently in bits 2..4 like window/sprite path */
        attributes[x] = (palette << 2) | (tile_attrs & 0x80);
        priorities[x] = priority;
    }
    window_line++;
}

/* CGB-specific tile rendering with attributes */
void ppu_render_background_cgb(PPU* ppu, uint8_t* scanline, uint8_t* attributes, uint8_t* priorities) {
    uint16_t tile_map;
    uint16_t tile_data;
    uint8_t y, tile_y, fine_y;
    int x;
    uint8_t tile_x, fine_x;
    uint16_t map_addr;
    uint8_t tile_index;
    uint8_t old_bank;
    uint8_t tile_attrs;
    uint8_t palette;
    int tile_bank;
    int x_flip;
    int y_flip;
    int priority;
    uint16_t tile_addr;
    uint8_t pixel;
    uint8_t *vram_ptr;

    /* Calculate base addresses */
    tile_map = (ppu->lcdc & LCDC_BG_MAP) ? 0x9C00 : 0x9800;
    tile_data = (ppu->lcdc & LCDC_TILE_SELECT) ? 0x8000 : 0x9000;
    
    /* Calculate tile Y position */
    y = ppu->ly + ppu->scy;
    tile_y = y / 8;
    fine_y = y % 8;
    
    /* Render each pixel in the scanline */
    for (x = 0; x < SCREEN_WIDTH; x++) {
        /* Calculate tile X position */
        tile_x = ((x + ppu->scx) / 8) & 0x1F;
        fine_x = (x + ppu->scx) % 8;
        
        /* Get tile index and attributes */
        map_addr = tile_map + (tile_y * 32) + tile_x;
        vram_ptr = (uint8_t *)&ppu->vram[0][map_addr - VRAM_START];
        tile_index = *vram_ptr;

        /* Get tile attributes from bank 1 */
        old_bank = ppu->vram_bank;
        ppu->vram_bank = 1;
        tile_attrs = ppu->vram[1][map_addr - VRAM_START];
        ppu->vram_bank = old_bank;
        
        /* Extract attributes */
        palette = (tile_attrs >> 2) & 7;
        tile_bank = (tile_attrs & 0x08) != 0;
        x_flip = (tile_attrs & 0x20) != 0;
        y_flip = (tile_attrs & 0x40) != 0;
        priority = (tile_attrs & 0x80) != 0;
        
        /* Handle flips */
        if (y_flip) {
            fine_y = 7 - fine_y;
        }
        if (x_flip) {
            fine_x = 7 - fine_x;
        }

        /* Get tile data */
        ppu->vram_bank = tile_bank;
        tile_addr = (tile_data + (tile_index * 16) + (fine_y * 2)) & 0x1FFF;
        vram_ptr = &ppu->vram[tile_bank][tile_addr];
        /* Bit 7 is the leftmost pixel */
        pixel = (((vram_ptr[1] >> (7 - fine_x)) & 1) << 1) | ((vram_ptr[0] >> (7 - fine_x)) & 1);
        
        /* Store pixel data */
        scanline[x] = pixel;
        /* Store BG palette consistently in bits 2..4 */
        attributes[x] = (palette << 2) | (priority ? 0x80 : 0x00);
        priorities[x] = priority;
    }

    /* Restore original bank */
    ppu->vram_bank = old_bank;
}

void ppu_render_sprites_cgb(PPU* ppu, uint8_t* scanline, uint8_t* attributes, uint8_t* priorities) {
    struct visible_sprite {
        uint8_t y;
        uint8_t x;
        uint8_t tile;
        uint8_t flags;
    };
    struct visible_sprite visible_sprites[10];
    uint8_t sprite_height;
    int num_sprites;
    int i, j, screen_pos, sprite_y, screen_x;
    uint8_t bank, palette, priority;
    uint16_t tile_addr;
    uint8_t data_low, data_high;
    int bit;
    uint8_t color;
    uint8_t adjusted_y;
    uint8_t old_bank;

    /* Initialize sprite rendering */
    sprite_height = (ppu->lcdc & LCDC_OBJ_SIZE) ? 16 : 8;
    num_sprites = 0;
    
    /* Find visible sprites (maximum 10 per scanline) */
    for (i = 0; i < 40 && num_sprites < 10; i++) {
        /* Skip sprites that are off screen */
        if (ppu->oam[i].y == 0 || ppu->oam[i].y >= 160) {
            continue;
        }

        /* Adjust Y position */
        adjusted_y = ppu->oam[i].y - 16;

        /* Check if sprite is on current scanline */
        if (ppu->ly >= adjusted_y && ppu->ly < adjusted_y + sprite_height) {
            visible_sprites[num_sprites].y = ppu->oam[i].y;
            visible_sprites[num_sprites].x = ppu->oam[i].x;
            visible_sprites[num_sprites].tile = ppu->oam[i].tile;
            visible_sprites[num_sprites].flags = ppu->oam[i].flags;
            num_sprites++;
        }
    }

    /* Save current bank */
    old_bank = ppu->vram_bank;

    /* Render visible sprites honoring BG priority and OBJ priority rules */
    for (i = num_sprites - 1; i >= 0; i--) {
        bank = (visible_sprites[i].flags & 0x08) ? 1 : 0;
        palette = visible_sprites[i].flags & 0x07;
        priority = visible_sprites[i].flags & 0x80;
        sprite_y = ppu->ly - (visible_sprites[i].y - 16);
        screen_x = visible_sprites[i].x - 8;

        /* Apply Y-flip if needed */
        if (visible_sprites[i].flags & 0x40) {
            sprite_y = (sprite_height - 1) - sprite_y;
        }

        /* Get tile data */
        ppu->vram_bank = bank;
        tile_addr = ((visible_sprites[i].tile * 16) + (sprite_y * 2)) & 0x1FFF;
        data_low = ppu->vram[bank][tile_addr];
        data_high = ppu->vram[bank][tile_addr + 1];

        /* Render sprite line */
        for (j = 0; j < 8; j++) {
            screen_pos = screen_x + j;
            if (screen_pos < 0 || screen_pos >= SCREEN_WIDTH) {
                continue;
            }

            /* Get pixel from tile data */
            bit = (visible_sprites[i].flags & 0x20) ? j : 7 - j;
            color = ((data_high >> bit) & 1) << 1 | ((data_low >> bit) & 1);

            /* Skip transparent pixels */
            if (color == 0) {
                continue;
            }

            /* Priority rules (CGB):
             * - If BG tile priority bit is set and BG color != 0, BG wins.
             * - Else if BG priority bit is clear:
             *     - If OBJ priority bit is set, BG nonzero blocks; BG color 0 allows sprite.
             *     - If OBJ priority bit is clear, sprite wins regardless of BG color.
             */
            if (priorities[screen_pos]) {
                /* BG priority attribute is set */
                if (scanline[screen_pos] != 0) {
                    continue; /* BG non-zero blocks sprite */
                }
                /* BG color 0: sprite allowed regardless of OBJ priority */
            } else {
                if (priority && scanline[screen_pos] != 0) {
                    continue; /* OBJ behind BG and BG non-zero */
                }
            }

            /* Draw sprite pixel */
            scanline[screen_pos] = color;
            attributes[screen_pos] = (palette << 2) | 0x80;
            priorities[screen_pos] = priority;
        }
    }

    /* Restore original bank */
    ppu->vram_bank = old_bank;
}