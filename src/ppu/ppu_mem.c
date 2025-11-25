#include "ppu.h"
#include "../memory/memory.h"

/* VRAM access timing */
#define VRAM_ACCESS_TIME 2

void ppu_write_vram(PPU* ppu, Memory* mem, uint16_t address, uint8_t value) {
    /* Check if VRAM is accessible */
    /* VRAM is always accessible when LCD is disabled */
    if ((ppu->lcdc & 0x80) && ppu->mode == MODE_PIXEL_TRANSFER) {
        return;  /* VRAM is inaccessible during pixel transfer when LCD is enabled */
    }
    
    /* Write to Memory struct's VRAM (which is what the CPU writes to) */
    if (address >= 0x8000 && address <= 0x9FFF && mem) {
        uint16_t offset = address - 0x8000;
        if (ppu->cgb_mode) {
            /* In CGB mode, also update PPU's VRAM banks */
            ppu->vram[ppu->vram_bank][offset] = value;
        }
        /* Always update Memory struct's VRAM */
        mem->vram[offset] = value;
    }
}

uint8_t ppu_read_vram(PPU* ppu, Memory* mem, uint16_t address) {
    /* Check if VRAM is accessible */
    /* VRAM is always accessible when LCD is disabled */
    if ((ppu->lcdc & 0x80) && ppu->mode == MODE_PIXEL_TRANSFER) {
        return 0xFF;  /* VRAM is inaccessible during pixel transfer when LCD is enabled */
    }
    
    /* Read from Memory struct's VRAM (which is what the CPU writes to) */
    if (address >= 0x8000 && address <= 0x9FFF && mem) {
        uint16_t offset = address - 0x8000;
        if (ppu->cgb_mode) {
            /* In CGB mode, use PPU's VRAM banks */
            return ppu->vram[ppu->vram_bank][offset];
        } else {
            /* In DMG mode, read from Memory struct's VRAM */
            return mem->vram[offset];
        }
    }
    
    return 0xFF;
}

void ppu_write_oam(PPU* ppu, Memory* mem, uint16_t address, uint8_t value) {
    (void)mem;  /* Parameter not needed for OAM access */
    /* Check if OAM is accessible */
    /* OAM is always accessible when LCD is disabled */
    if ((ppu->lcdc & 0x80) && (ppu->mode == MODE_OAM_SCAN || ppu->mode == MODE_PIXEL_TRANSFER)) {
        return;  /* OAM is inaccessible during OAM scan and pixel transfer when LCD is enabled */
    }
    
    if (address >= 0xFE00 && address <= 0xFE9F) {
        uint8_t offset = address - 0xFE00;
        ((uint8_t*)ppu->oam)[offset] = value;
    }
}

uint8_t ppu_read_oam(PPU* ppu, Memory* mem, uint16_t address) {
    (void)mem;  /* Parameter not needed for OAM access */
    /* Check if OAM is accessible */
    /* OAM is always accessible when LCD is disabled */
    if ((ppu->lcdc & 0x80) && (ppu->mode == MODE_OAM_SCAN || ppu->mode == MODE_PIXEL_TRANSFER)) {
        return 0xFF;  /* OAM is inaccessible during OAM scan and pixel transfer when LCD is enabled */
    }
    
    if (address >= 0xFE00 && address <= 0xFE9F) {
        uint8_t offset = address - 0xFE00;
        return ((uint8_t*)ppu->oam)[offset];
    }
    
    return 0xFF;
}

void ppu_dma_transfer(PPU* ppu, Memory* mem, uint8_t start) {
    uint16_t source = start << 8;  /* Source address is start * 0x100 */
    
    /* DMA takes 160 microseconds (640 cycles) */
    /* Emulate DMA blocking time */
    /* Note: Memory struct doesn't own a global cycle counter in some builds; if present, increment it. */
        (void)mem; /* caller may account timing; silence unused param */
    for (int i = 0; i < 160; i++) {
        ((uint8_t*)ppu->oam)[i] = memory_read(mem, source + i);
    }
}

/* CGB HDMA functions */
void ppu_hdma_start(PPU* ppu, Memory* mem, uint16_t source, uint16_t dest, uint16_t length, bool hblank) {
    /* Implement HDMA transfer */
    if (!ppu) return;
    /* length is number of bytes to transfer */
    if (hblank) {
        ppu->hdma_active = true;
        ppu->hdma_hblank = true;
        ppu->hdma_source = source;
        ppu->hdma_dest = dest;
        ppu->hdma_remaining = length;
    } else {
        /* General Purpose DMA: Transfer all at once */
        for (uint16_t i = 0; i < length; i++) {
            uint8_t value = memory_read(mem, source + i);
            ppu_write_vram(ppu, mem, dest + i, value);
        }
        ppu->hdma_active = false;
        ppu->hdma_remaining = 0;
    }
}

bool ppu_hdma_step(PPU* ppu, Memory* mem) {
    if (!ppu || !ppu->hdma_active) return true;

    /* Only perform HDMA blocks during H-Blank; caller should ensure it's H-Blank timing */
    const uint16_t block_size = 16;
    uint16_t to_copy = (ppu->hdma_remaining >= block_size) ? block_size : ppu->hdma_remaining;
    if (to_copy == 0) {
        ppu->hdma_active = false;
        return true;
    }

    /* Perform the copy */
    for (uint16_t i = 0; i < to_copy; i++) {
        uint8_t value = memory_read(mem, ppu->hdma_source + i);
        ppu_write_vram(ppu, mem, ppu->hdma_dest + i, value);
    }

    /* Advance pointers */
    ppu->hdma_source += to_copy;
    ppu->hdma_dest += to_copy;
    if (ppu->hdma_remaining > to_copy) ppu->hdma_remaining -= to_copy;
    else ppu->hdma_remaining = 0;

    if (ppu->hdma_remaining == 0) {
        ppu->hdma_active = false;
        return true;
    }
    return false;
}

void ppu_hdma_cancel(PPU* ppu) {
    if (!ppu || !ppu->hdma_active) return;
    
    /* Cancel ongoing HDMA transfer */
    ppu->hdma_active = false;
    ppu->hdma_hblank = false;
    ppu->hdma_remaining = 0;
    
    /* Note: The caller should also update the FF55h register to reflect cancellation 
     * (typically by setting bit 7 to 1 to indicate transfer is stopped) */
}