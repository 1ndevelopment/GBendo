#ifndef MEMORY_OPTIMIZED_H
#define MEMORY_OPTIMIZED_H

#include "memory.h"
#include <string.h>

/* Optimized memory access with reduced indirection and better cache locality */

/* Fast memory access macros - inline for maximum performance */
static inline uint8_t memory_read_fast(Memory* mem, uint16_t addr) {
    /* Use jump table for memory regions to reduce branching */
    if (addr < 0x8000) {
        /* ROM access - most common, handle first */
        if (addr < 0x4000) {
            return mem->rom_bank0[addr];
        } else {
            return mem->rom_bankn[addr - 0x4000];
        }
    } 
    else if (addr < 0xA000) {
        /* VRAM */
        return mem->vram[addr - 0x8000];
    }
    else if (addr < 0xC000) {
        /* External RAM */
        return mem->ext_ram ? mem->ext_ram[addr - 0xA000] : 0xFF;
    }
    else if (addr < 0xE000) {
        /* WRAM */
        return mem->wram[addr - 0xC000];
    }
    else if (addr < 0xFE00) {
        /* Echo RAM - mirrors WRAM */
        return mem->wram[(addr - 0xE000) % 0x2000];
    }
    else if (addr < 0xFEA0) {
        /* OAM */
        return mem->oam[addr - 0xFE00];
    }
    else if (addr < 0xFF00) {
        /* Unused area */
        return 0xFF;
    }
    else if (addr < 0xFF80) {
        /* I/O registers */
        return mem->io_registers[addr - 0xFF00];
    }
    else if (addr < 0xFFFF) {
        /* High RAM */
        return mem->hram[addr - 0xFF80];
    }
    else {
        /* Interrupt Enable register */
        return mem->ie_register;
    }
}

static inline void memory_write_fast(Memory* mem, uint16_t addr, uint8_t value) {
    if (addr < 0x8000) {
        /* ROM area - handle MBC writes */
        if (mem->mbc_data) {
            /* Call MBC-specific write handler */
            switch (mem->mbc_type) {
                case MBC1:
                    mbc1_write(mem, addr, value);
                    break;
                default:
                    /* ROM-only or other MBC types */
                    break;
            }
        }
        return;
    }
    else if (addr < 0xA000) {
        /* VRAM */
        mem->vram[addr - 0x8000] = value;
    }
    else if (addr < 0xC000) {
        /* External RAM */
        if (mem->ext_ram && mem->ram_enabled) {
            mem->ext_ram[addr - 0xA000] = value;
        }
    }
    else if (addr < 0xE000) {
        /* WRAM */
        mem->wram[addr - 0xC000] = value;
    }
    else if (addr < 0xFE00) {
        /* Echo RAM - mirrors WRAM */
        mem->wram[(addr - 0xE000) % 0x2000] = value;
    }
    else if (addr < 0xFEA0) {
        /* OAM */
        mem->oam[addr - 0xFE00] = value;
    }
    else if (addr < 0xFF00) {
        /* Unused area - ignore writes */
        return;
    }
    else if (addr < 0xFF80) {
        /* I/O registers */
        mem->io_registers[addr - 0xFF00] = value;
        
        /* Handle special I/O register side effects */
        switch (addr) {
            case 0xFF00: /* JOYP */
                memory_update_joyp(mem);
                break;
            case 0xFF04: /* DIV */
                mem->div_internal = 0;
                break;
            case 0xFF46: /* DMA */
                memory_dma_transfer(mem, value);
                break;
        }
    }
    else if (addr < 0xFFFF) {
        /* High RAM */
        mem->hram[addr - 0xFF80] = value;
    }
    else {
        /* Interrupt Enable register */
        mem->ie_register = value;
    }
}

/* Cache-optimized memory block operations */
static inline void memory_copy_block(uint8_t* dest, const uint8_t* src, size_t size) {
    /* Use optimized memory copy for better performance */
    memcpy(dest, src, size);
}

/* Prefetch hints for better cache performance */
#ifdef __GNUC__
#define PREFETCH_READ(addr) __builtin_prefetch((addr), 0, 3)
#define PREFETCH_WRITE(addr) __builtin_prefetch((addr), 1, 3)
#else
#define PREFETCH_READ(addr)
#define PREFETCH_WRITE(addr)
#endif

/* Memory access with prefetching for sequential reads/writes */
static inline uint8_t memory_read_sequential(Memory* mem, uint16_t addr) {
    PREFETCH_READ(&mem->rom_bank0[addr + 8]); /* Prefetch ahead for sequential access */
    return memory_read_fast(mem, addr);
}

/* Batch memory operations for better performance */
void memory_read_block(Memory* mem, uint16_t addr, uint8_t* buffer, size_t size);
void memory_write_block(Memory* mem, uint16_t addr, const uint8_t* buffer, size_t size);

/* Memory statistics for profiling */
typedef struct {
    uint64_t read_count;
    uint64_t write_count;
    uint64_t rom_accesses;
    uint64_t ram_accesses;
    uint64_t vram_accesses;
} MemoryStats;

extern MemoryStats g_memory_stats;

/* Enable/disable memory profiling */
void memory_profiling_enable(bool enable);
void memory_profiling_reset(void);
void memory_profiling_report(void);

#endif /* MEMORY_OPTIMIZED_H */
