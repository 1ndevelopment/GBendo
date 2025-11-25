#include "memory/memory.h"
#include "cpu/sm83.h"
#include "error_handling.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Global error context */
ErrorContext g_error_context = {0};

/* Error handling functions */
void error_init(void) {
    memset(&g_error_context, 0, sizeof(g_error_context));
}

void error_clear(void) {
    g_error_context.last_error = GB_SUCCESS;
    g_error_context.error_message[0] = '\0';
    g_error_context.error_file = NULL;
    g_error_context.error_line = 0;
    g_error_context.error_function = NULL;
}

GBError error_get_last(void) {
    return g_error_context.last_error;
}

const char* error_get_message(void) {
    return g_error_context.error_message;
}

const char* error_code_to_string(GBError code) {
    switch (code) {
        case GB_SUCCESS: return "Success";
        case GB_ERROR_NULL_POINTER: return "Null pointer";
        case GB_ERROR_INVALID_ARGUMENT: return "Invalid argument";
        case GB_ERROR_MEMORY_ALLOCATION: return "Memory allocation failed";
        case GB_ERROR_FILE_NOT_FOUND: return "File not found";
        case GB_ERROR_FILE_READ: return "File read error";
        case GB_ERROR_FILE_WRITE: return "File write error";
        case GB_ERROR_ROM_INVALID: return "Invalid ROM";
        case GB_ERROR_ROM_SIZE_INVALID: return "Invalid ROM size";
        case GB_ERROR_MBC_UNSUPPORTED: return "Unsupported MBC type";
        case GB_ERROR_SAVE_STATE_INVALID: return "Invalid save state";
        case GB_ERROR_AUDIO_INIT_FAILED: return "Audio initialization failed";
        case GB_ERROR_VIDEO_INIT_FAILED: return "Video initialization failed";
        case GB_ERROR_CPU_FAULT: return "CPU fault";
        case GB_ERROR_MEMORY_CORRUPT: return "Memory corruption detected";
        default: return "Unknown error";
    }
}

void error_print_backtrace(void) {
    if (g_error_context.last_error != GB_SUCCESS) {
        fprintf(stderr, "Error backtrace:\n");
        fprintf(stderr, "  Code: %s (%d)\n", 
                error_code_to_string(g_error_context.last_error), 
                g_error_context.last_error);
        fprintf(stderr, "  Message: %s\n", g_error_context.error_message);
        if (g_error_context.error_file) {
            fprintf(stderr, "  Location: %s:%d in %s()\n", 
                    g_error_context.error_file, 
                    g_error_context.error_line,
                    g_error_context.error_function);
        }
    }
}

/* ROM validation functions */
bool validate_rom_header(const uint8_t* rom_data, size_t rom_size) {
    VALIDATE_NOT_NULL(rom_data);
    
    if (rom_size < 0x8000) {
        SET_ERROR(GB_ERROR_ROM_SIZE_INVALID, "ROM too small (< 32KB)");
        return false;
    }
    
    /* Check Nintendo logo */
    static const uint8_t nintendo_logo[48] = {
        0xCE, 0xED, 0x66, 0x66, 0xCC, 0x0D, 0x00, 0x0B, 0x03, 0x73, 0x00, 0x83,
        0x00, 0x0C, 0x00, 0x0D, 0x00, 0x08, 0x11, 0x1F, 0x88, 0x89, 0x00, 0x0E,
        0xDC, 0xCC, 0x6E, 0xE6, 0xDD, 0xDD, 0xD9, 0x99, 0xBB, 0xBB, 0x67, 0x63,
        0x6E, 0x0E, 0xEC, 0xCC, 0xDD, 0xDC, 0x99, 0x9F, 0xBB, 0xB9, 0x33, 0x3E
    };
    
    if (memcmp(&rom_data[0x104], nintendo_logo, sizeof(nintendo_logo)) != 0) {
        SET_ERROR(GB_ERROR_ROM_INVALID, "Invalid Nintendo logo in ROM header");
        return false;
    }
    
    /* Check ROM size field */
    uint8_t rom_size_code = rom_data[0x148];
    if (rom_size_code > 0x08) {
        SET_ERROR(GB_ERROR_ROM_INVALID, "Invalid ROM size code in header");
        return false;
    }
    
    return true;
}

bool validate_rom_checksum(const uint8_t* rom_data, size_t rom_size) {
    VALIDATE_NOT_NULL(rom_data);
    
    if (rom_size < 0x150) {
        SET_ERROR(GB_ERROR_ROM_SIZE_INVALID, "ROM too small for checksum validation");
        return false;
    }
    
    /* Calculate header checksum */
    uint8_t checksum = 0;
    for (int i = 0x134; i <= 0x14C; i++) {
        checksum = checksum - rom_data[i] - 1;
    }
    
    if (checksum != rom_data[0x14D]) {
        SET_ERROR(GB_ERROR_ROM_INVALID, "Invalid ROM header checksum");
        return false;
    }
    
    return true;
}

int detect_mbc_type(uint8_t cartridge_type) {
    switch (cartridge_type) {
        case 0x00: return ROM_ONLY;
        case 0x01: case 0x02: case 0x03: return MBC1;
        case 0x05: case 0x06: return MBC2;
        case 0x0F: case 0x10: case 0x11: case 0x12: case 0x13: return MBC3;
        case 0x19: case 0x1A: case 0x1B: case 0x1C: case 0x1D: case 0x1E: return MBC5;
        case 0x20: return MBC6;
        case 0x22: return MBC7;
        default:
            SET_ERROR(GB_ERROR_MBC_UNSUPPORTED, "Unsupported MBC type");
            return ROM_ONLY;
    }
}

/* Memory bounds checking */
bool is_valid_address(uint16_t addr, bool for_write) {
    /* All addresses are technically valid for read */
    if (!for_write) return true;
    
    /* Check write-protected regions */
    if (addr < 0x8000) {
        /* ROM area - only MBC register writes allowed */
        return true;
    }
    if (addr >= 0xFEA0 && addr < 0xFF00) {
        /* Unused area - writes ignored but not invalid */
        return true;
    }
    
    return true;
}

bool is_valid_rom_bank(uint8_t bank, size_t rom_size) {
    size_t max_banks = rom_size / 0x4000;  /* 16KB per bank */
    return bank < max_banks;
}

bool is_valid_ram_bank(uint8_t bank, size_t ram_size) {
    if (ram_size == 0) return bank == 0;
    size_t max_banks = ram_size / 0x2000;  /* 8KB per bank */
    return bank < max_banks;
}

/* CPU state validation */
bool validate_cpu_state(const void* cpu_ptr) {
    const SM83_CPU* cpu = (const SM83_CPU*)cpu_ptr;
    VALIDATE_NOT_NULL(cpu);
    
    /* PC and SP are uint16_t, so they're always <= 0xFFFF by design */
    /* Check for reasonable PC values (not in invalid ROM areas) */
    if (cpu->pc < 0x0100 && cpu->pc > 0x0000) {
        /* PC in boot ROM area after boot - might indicate issue */
    }
    
    /* Check for reasonable SP values (not in prohibited areas) */
    if (cpu->sp < 0x8000) {
        SET_ERROR(GB_ERROR_CPU_FAULT, "CPU SP in ROM/VRAM area - likely corrupted");
        return false;
    }
    
    /* Check flag register - lower 4 bits should be zero */
    if ((cpu->f & 0x0F) != 0) {
        SET_ERROR(GB_ERROR_CPU_FAULT, "CPU flag register lower bits corrupted");
        return false;
    }
    
    return true;
}

bool validate_memory_state(const void* mem_ptr) {
    const Memory* mem = (const Memory*)mem_ptr;
    VALIDATE_NOT_NULL(mem);
    VALIDATE_NOT_NULL(mem->vram);
    VALIDATE_NOT_NULL(mem->wram);
    VALIDATE_NOT_NULL(mem->oam);
    VALIDATE_NOT_NULL(mem->hram);
    
    /* Check MBC state if present */
    if (mem->mbc_data) {
        MBC_State* mbc = (MBC_State*)mem->mbc_data;
        VALIDATE_NOT_NULL(mbc->rom_data);
        
        if (mbc->current_rom_bank >= (mbc->rom_size / 0x4000)) {
            SET_ERROR(GB_ERROR_MEMORY_CORRUPT, "Invalid ROM bank selected");
            return false;
        }
        
        if (mbc->ram_size > 0 && mbc->current_ram_bank >= (mbc->ram_size / 0x2000)) {
            SET_ERROR(GB_ERROR_MEMORY_CORRUPT, "Invalid RAM bank selected");
            return false;
        }
    }
    
    return true;
}
