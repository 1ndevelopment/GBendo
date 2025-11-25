#ifndef GB_PPU_VRAM_H
#define GB_PPU_VRAM_H

#include <stdint.h>

/* VRAM address space */
#define VRAM_START      0x8000
// VRAM_END is defined in memory.h
#define VRAM_SIZE       0x2000
#define VRAM_BANK_SIZE  0x2000

/* Tile data areas */
#define TILE_DATA_AREA1 0x8000  /* Using unsigned indexing */
#define TILE_DATA_AREA2 0x9000  /* Using signed indexing */

/* Tile map areas */
#define TILE_MAP_AREA1  0x9800
#define TILE_MAP_AREA2  0x9C00
#define TILE_MAP_SIZE   0x0400  /* 32x32 tiles = 1024 bytes */

/* Helper macros for VRAM address validation */
#define IS_VRAM_ADDR(addr) ((addr) >= VRAM_START && (addr) < VRAM_END)
#define IS_TILE_DATA_ADDR(addr) ((addr) >= VRAM_START && (addr) < TILE_MAP_AREA1)
#define IS_TILE_MAP_ADDR(addr) ((addr) >= TILE_MAP_AREA1 && (addr) < VRAM_END)

/* VRAM address calculation helpers */
static inline uint16_t vram_get_tile_addr(uint8_t tile_index, uint8_t use_area1) {
    if (use_area1) {
        return TILE_DATA_AREA1 + ((uint16_t)tile_index * 16);
    } else {
        return TILE_DATA_AREA2 + ((int8_t)tile_index * 16);
    }
}

static inline uint16_t vram_get_tile_map_addr(uint8_t use_area2, uint8_t row, uint8_t col) {
    uint16_t base = use_area2 ? TILE_MAP_AREA2 : TILE_MAP_AREA1;
    return base + (row * 32) + col;
}

static inline uint16_t vram_get_tile_row_addr(uint16_t tile_addr, uint8_t row) {
    return tile_addr + (row * 2);
}

static inline uint8_t vram_get_tile_pixel(uint8_t byte1, uint8_t byte2, uint8_t x) {
    uint8_t mask = 1 << (7 - x);
    uint8_t lo = (byte1 & mask) ? 1 : 0;
    uint8_t hi = (byte2 & mask) ? 2 : 0;
    return lo | hi;
}

/* Extract full 2-color pixel data from tile data at given coordinates */
static inline uint8_t get_tile_pixel(const uint8_t* tile_data, int x, int y) {
    if (!tile_data) return 0;
    uint8_t byte1 = tile_data[y * 2];
    uint8_t byte2 = tile_data[y * 2 + 1];
    return vram_get_tile_pixel(byte1, byte2, x);
}

#endif /* GB_PPU_VRAM_H */