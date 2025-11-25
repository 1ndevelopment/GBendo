#ifndef PPU_OPTIMIZED_H
#define PPU_OPTIMIZED_H

#include "ppu.h"
#include <stdint.h>
#ifdef __SSE2__
#include <immintrin.h>  /* For SIMD instructions if available */
#endif

/* Optimized PPU rendering with scanline batching */

/* SIMD-optimized color blending */
#ifdef __SSE2__
#define USE_SIMD_RENDERING 1
#include <emmintrin.h>
#else
#define USE_SIMD_RENDERING 0
#endif

/* Scanline rendering optimization */
typedef struct {
    uint32_t line_pixels[160];  /* Pre-calculated pixel data for current scanline */
    bool line_dirty;            /* Whether scanline needs re-rendering */
    uint8_t tile_cache[384];    /* Cache for frequently accessed tiles */
    uint32_t tile_cache_tags[384]; /* Tags for tile cache validity */
} ScanlineCache;

/* Optimized sprite rendering */
typedef struct {
    uint8_t x;
    uint8_t y; 
    uint8_t tile;
    uint8_t flags;
    uint8_t priority;
} OptimizedSprite;

/* PPU optimization state */
typedef struct {
    ScanlineCache scanline_cache;
    OptimizedSprite sprite_buffer[40];  /* Sorted sprite buffer */
    uint8_t visible_sprites;            /* Number of visible sprites on current line */
    
    /* Frame skipping for performance */
    uint32_t frame_skip_counter;
    bool frame_skip_enabled;
    
    /* Tile rendering cache */
    uint32_t tile_pixel_cache[384][64];  /* 8x8 pixels per tile, pre-rendered */
    bool tile_cache_valid[384];
    
    /* Background rendering optimization */
    uint32_t bg_line_buffer[256];  /* Extended background line for scrolling */
    
    /* Statistics */
    uint64_t scanlines_rendered;
    uint64_t sprites_rendered;
    uint64_t cache_hits;
    uint64_t cache_misses;
} PPUOptimization;

/* Function prototypes */
void ppu_optimization_init(PPUOptimization* opt);
void ppu_optimization_cleanup(PPUOptimization* opt);
void ppu_optimization_reset(PPUOptimization* opt);

/* Optimized rendering functions */
void ppu_render_scanline_optimized(PPU* ppu, PPUOptimization* opt, uint8_t ly);
void ppu_render_background_optimized(PPU* ppu, PPUOptimization* opt, uint8_t ly);
void ppu_render_sprites_optimized(PPU* ppu, PPUOptimization* opt, uint8_t ly);

/* SIMD-optimized functions */
#if USE_SIMD_RENDERING
void ppu_blend_scanline_simd(uint32_t* dest, const uint32_t* bg, const uint32_t* sprites, int width);
void ppu_convert_palette_simd(const uint8_t* src, uint32_t* dest, const uint32_t* palette, int count);
#endif

/* Cache management */
void ppu_invalidate_tile_cache(PPUOptimization* opt, uint16_t tile_addr);
void ppu_update_tile_cache(PPU* ppu, PPUOptimization* opt, uint16_t tile_index);
bool ppu_is_tile_cached(PPUOptimization* opt, uint16_t tile_index);

/* Performance monitoring */
void ppu_optimization_print_stats(PPUOptimization* opt);
void ppu_optimization_reset_stats(PPUOptimization* opt);

/* Frame skipping controls */
void ppu_set_frame_skip(PPUOptimization* opt, bool enabled);
bool ppu_should_skip_frame(PPUOptimization* opt);

/* Inline helper functions for speed-critical operations */
static inline uint32_t ppu_get_bg_pixel_fast(PPU* ppu, int x, int y) {
    /* Fast background pixel lookup with minimal bounds checking */
    uint8_t scroll_x = ppu->scx;
    uint8_t scroll_y = ppu->scy;
    
    int bg_x = (x + scroll_x) & 0xFF;
    int bg_y = (y + scroll_y) & 0xFF;
    
    /* Calculate tile map address */
    uint16_t tile_map = (ppu->lcdc & 0x08) ? 0x9C00 : 0x9800;
    uint16_t tile_addr = tile_map + ((bg_y >> 3) << 5) + (bg_x >> 3);
    
    /* Get tile index and pixel within tile */
    uint8_t tile_index = ppu->vram[tile_addr - 0x8000];
    int tile_x = bg_x & 7;
    int tile_y = bg_y & 7;
    
    /* Calculate tile data address */
    uint16_t tile_data_base = (ppu->lcdc & 0x10) ? 0x8000 : 0x9000;
    uint16_t tile_data_addr;
    
    if (ppu->lcdc & 0x10) {
        tile_data_addr = tile_data_base + (tile_index << 4);
    } else {
        tile_data_addr = tile_data_base + (((int8_t)tile_index) << 4);
    }
    
    /* Get pixel data */
    uint16_t row_addr = tile_data_addr + (tile_y << 1);
    uint8_t byte1 = ppu->vram[row_addr - 0x8000];
    uint8_t byte2 = ppu->vram[row_addr + 1 - 0x8000];
    
    int bit = 7 - tile_x;
    uint8_t color = ((byte1 >> bit) & 1) | (((byte2 >> bit) & 1) << 1);
    
    return color;
}

static inline bool ppu_is_sprite_visible(const OptimizedSprite* sprite, uint8_t ly) {
    /* Quick sprite visibility check */
    int sprite_height = 8;  /* Assume 8x8 sprites for now */
    return (ly >= sprite->y && ly < sprite->y + sprite_height);
}

#endif /* PPU_OPTIMIZED_H */
