#include "memory_optimized.h"
#include <string.h>
#include <stdio.h>

/* Global memory profiling statistics */
MemoryStats g_memory_stats = {0};
static bool g_profiling_enabled = false;

/* Batch memory operations */
void memory_read_block(Memory* mem, uint16_t addr, uint8_t* buffer, size_t size) {
    for (size_t i = 0; i < size; i++) {
        buffer[i] = memory_read_fast(mem, addr + i);
    }
    
    if (g_profiling_enabled) {
        g_memory_stats.read_count += size;
    }
}

void memory_write_block(Memory* mem, uint16_t addr, const uint8_t* buffer, size_t size) {
    for (size_t i = 0; i < size; i++) {
        memory_write_fast(mem, addr + i, buffer[i]);
    }
    
    if (g_profiling_enabled) {
        g_memory_stats.write_count += size;
    }
}

/* Memory profiling functions */
void memory_profiling_enable(bool enable) {
    g_profiling_enabled = enable;
}

void memory_profiling_reset(void) {
    memset(&g_memory_stats, 0, sizeof(g_memory_stats));
}

void memory_profiling_report(void) {
    if (!g_profiling_enabled) return;
    
    printf("Memory Access Statistics:\n");
    printf("  Total reads:  %llu\n", (unsigned long long)g_memory_stats.read_count);
    printf("  Total writes: %llu\n", (unsigned long long)g_memory_stats.write_count);
    printf("  ROM accesses: %llu (%.1f%%)\n", 
           (unsigned long long)g_memory_stats.rom_accesses,
           100.0 * g_memory_stats.rom_accesses / (g_memory_stats.read_count + 1));
    printf("  RAM accesses: %llu (%.1f%%)\n", 
           (unsigned long long)g_memory_stats.ram_accesses,
           100.0 * g_memory_stats.ram_accesses / (g_memory_stats.read_count + 1));
    printf("  VRAM accesses: %llu (%.1f%%)\n", 
           (unsigned long long)g_memory_stats.vram_accesses,
           100.0 * g_memory_stats.vram_accesses / (g_memory_stats.read_count + 1));
}

/* Optimized DMA transfer with reduced overhead */
void memory_dma_transfer_optimized(Memory* mem, uint8_t start) {
    uint16_t src_addr = (uint16_t)start << 8;
    
    /* Use fast block copy for OAM DMA */
    if (src_addr < 0x8000) {
        /* ROM source */
        const uint8_t* src = (src_addr < 0x4000) ? 
                            &mem->rom_bank0[src_addr] : 
                            &mem->rom_bankn[src_addr - 0x4000];
        memory_copy_block(mem->oam, src, 0xA0);
    }
    else if (src_addr < 0xA000) {
        /* VRAM source */
        memory_copy_block(mem->oam, &mem->vram[src_addr - 0x8000], 0xA0);
    }
    else if (src_addr < 0xC000) {
        /* External RAM source */
        if (mem->ext_ram) {
            memory_copy_block(mem->oam, &mem->ext_ram[src_addr - 0xA000], 0xA0);
        }
    }
    else if (src_addr < 0xE000) {
        /* WRAM source */
        memory_copy_block(mem->oam, &mem->wram[src_addr - 0xC000], 0xA0);
    }
    
    if (g_profiling_enabled) {
        g_memory_stats.write_count += 0xA0;
    }
}
