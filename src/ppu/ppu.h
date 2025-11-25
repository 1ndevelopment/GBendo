#ifndef GB_PPU_H
#define GB_PPU_H

#include <stdint.h>
#include <stdbool.h>
#include "../memory/memory.h"  /* For Memory type */

/* PPU Constants */
#define SCREEN_WIDTH    160
#define SCREEN_HEIGHT   144
#define TOTAL_LINES    154

/* LCD Control Register bits */
#define LCDC_BG_ENABLE         0x01
#define LCDC_OBJ_ENABLE        0x02
#define LCDC_OBJ_SIZE          0x04
#define LCDC_BG_MAP            0x08
#define LCDC_TILE_SELECT       0x10
#define LCDC_WINDOW_ENABLE     0x20
#define LCDC_WINDOW_MAP        0x40
#define LCDC_DISPLAY_ENABLE    0x80

/* LCD Status Register bits */
#define STAT_MODE              0x03
#define STAT_LYC_MATCH        0x04
#define STAT_MODE0_INT        0x08
#define STAT_MODE1_INT        0x10
#define STAT_MODE2_INT        0x20
#define STAT_LYC_INT          0x40

/* PPU Modes */
typedef enum {
    MODE_HBLANK = 0,
    MODE_VBLANK = 1,
    MODE_OAM_SCAN = 2,
    MODE_PIXEL_TRANSFER = 3
} PPU_Mode;

typedef struct {
    /* LCD registers */
    uint8_t lcdc;    /* LCD Control */
    uint8_t stat;    /* LCD Status */
    uint8_t scy;     /* Scroll Y */
    uint8_t scx;     /* Scroll X */
    uint8_t ly;      /* Current scanline */
    uint8_t lyc;     /* Scanline compare */
    uint8_t bgp;     /* BG Palette data */
    uint8_t obp0;    /* Object Palette 0 data */
    uint8_t obp1;    /* Object Palette 1 data */
    uint8_t wy;      /* Window Y position */
    uint8_t wx;      /* Window X position - 7 */
    
    /* CGB-specific color palette data */
    uint8_t bgpi;    /* Background Palette Index */
    uint8_t obpi;    /* Sprite Palette Index */
    uint8_t bgpd[64];/* Background Palette Data */
    uint8_t obpd[64];/* Sprite Palette Data */
    
    /* PPU state */
    PPU_Mode mode;
    uint32_t clock;  /* Dot clock counter */
    uint32_t line_cycles;  /* Cycles on current line */
    
    /* Frame buffer */
    uint32_t framebuffer[SCREEN_WIDTH * SCREEN_HEIGHT];
    bool frame_ready;

    /* VRAM and CGB mode */
    uint8_t vram[2][0x2000]; /* support 2 VRAM banks for CGB */
    bool cgb_mode;
    uint8_t vram_bank;

    /* Sprite data */
    struct {
        uint8_t y;
        uint8_t x;
        uint8_t tile;
        uint8_t flags;
    } oam[40];  /* Object Attribute Memory */

    /* Back-reference to memory for interrupt requests */
    Memory* memory;

    /* HDMA state (CGB) */
    bool hdma_active;
    bool hdma_hblank; /* true = H-Blank HDMA, false = General Purpose DMA */
    uint16_t hdma_source;
    uint16_t hdma_dest;
    uint16_t hdma_remaining; /* bytes remaining */
} PPU;

/* PPU initialization and control */
void ppu_init(PPU* ppu, Memory* mem);
void ppu_reset(PPU* ppu);
void ppu_step(PPU* ppu, uint32_t cycles);

/* LCD register access */
uint8_t ppu_read_register(PPU* ppu, uint16_t address);
void ppu_write_register(PPU* ppu, uint16_t address, uint8_t value);

/* Rendering functions */
void ppu_render_scanline(PPU* ppu);
void ppu_render_background(PPU* ppu, uint8_t* scanline);
void ppu_render_window(PPU* ppu, uint8_t* scanline);
void ppu_render_sprites(PPU* ppu, uint8_t* scanline, uint8_t* sprite_scanline, uint8_t* sprite_palette);

/* OAM access */
void ppu_oam_write(PPU* ppu, uint8_t index, uint8_t value);
uint8_t ppu_oam_read(PPU* ppu, uint8_t index);

/* VRAM/OAM access helpers (respect mode access restrictions) */
uint8_t ppu_read_vram(PPU* ppu, Memory* mem, uint16_t address);
void ppu_write_vram(PPU* ppu, Memory* mem, uint16_t address, uint8_t value);
uint8_t ppu_read_oam(PPU* ppu, Memory* mem, uint16_t address);
void ppu_write_oam(PPU* ppu, Memory* mem, uint16_t address, uint8_t value);

/* DMA/HDMA helpers */
void ppu_dma_transfer(PPU* ppu, Memory* mem, uint8_t start);
void ppu_hdma_start(PPU* ppu, Memory* mem, uint16_t source, uint16_t dest, uint16_t length, bool hblank);
bool ppu_hdma_step(PPU* ppu, Memory* mem);
void ppu_hdma_cancel(PPU* ppu);

/* VRAM tile mapping functions */
#include "ppu_vram.h"

/* CGB-specific rendering functions */
void ppu_render_background_cgb(PPU* ppu, uint8_t* scanline, uint8_t* attributes, uint8_t* priorities);
void ppu_render_window_cgb(PPU* ppu, uint8_t* scanline, uint8_t* attributes, uint8_t* priorities);
void ppu_render_sprites_cgb(PPU* ppu, uint8_t* scanline, uint8_t* attributes, uint8_t* priorities);
/* CGB register helpers */
void ppu_init_cgb(PPU* ppu);
void ppu_write_cgb_registers(PPU* ppu, uint16_t address, uint8_t value);
void ppu_render_scanline_cgb(PPU* ppu);

/* Palette management functions */
void ppu_set_palette(int palette_index);
int ppu_get_palette_index(void);
const char* ppu_get_palette_name(int index);
int ppu_get_palette_count(void);
const uint32_t* ppu_get_palette_colors(int index);

#endif /* GB_PPU_H */