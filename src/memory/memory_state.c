#include "memory.h"
#include <time.h>
#include <stdio.h>

/* SaveState is defined in memory.h so other modules can access it */

bool memory_save_state(Memory* mem, const char* filename) {
    FILE* file = fopen(filename, "wb");
    if (!file) return false;
    
    MBC_State* mbc = (MBC_State*)mem->mbc_data;
    size_t state_size = sizeof(SaveState) + (mbc ? mbc->ram_size : 0);
    SaveState* state = malloc(state_size);
    
    /* Fill state data */
    state->version = SAVE_STATE_VERSION;
    state->mbc_type = mem->mbc_type;
    if (mbc) {
        state->rom_size = mbc->rom_size;
        state->ram_size = mbc->ram_size;
        state->current_rom_bank = mbc->current_rom_bank;
        state->current_ram_bank = mbc->current_ram_bank;
        state->ram_enabled = mbc->ram_enabled;
        state->rom_banking_enabled = mbc->rom_banking_enabled;
        state->banking_mode = mbc->banking_mode;
        
        if (mbc->rtc_data) {
            memcpy(&state->rtc, mbc->rtc_data, sizeof(RTC_Data));
        }
        
        if (mbc->ram_data) {
            memcpy(state->ram_data, mbc->ram_data, mbc->ram_size);
        }
    }
    
    /* Copy memory regions */
    memcpy(state->vram, mem->vram, sizeof(state->vram));
    memcpy(state->wram, mem->wram, sizeof(state->wram));
    memcpy(state->oam, mem->oam, sizeof(state->oam));
    memcpy(state->hram, mem->hram, sizeof(state->hram));
    memcpy(state->io_registers, mem->io_registers, sizeof(state->io_registers));
    state->ie_register = mem->ie_register;
    
    /* Write state to file */
    size_t written = fwrite(state, 1, state_size, file);
    free(state);
    fclose(file);
    
    return written == state_size;
}

bool memory_load_state(Memory* mem, const char* filename) {
    FILE* file = fopen(filename, "rb");
    if (!file) return false;
    
    /* Read header first to get sizes */
    SaveState header;
    if (fread(&header, 1, sizeof(header), file) != sizeof(header)) {
        fclose(file);
        return false;
    }
    
    /* Verify version compatibility */
    if (header.version != SAVE_STATE_VERSION) {
        fclose(file);
        return false;
    }
    
    /* Rewind and read full state */
    fseek(file, 0, SEEK_SET);
    size_t state_size = sizeof(SaveState) + header.ram_size;
    SaveState* state = malloc(state_size);
    
    if (fread(state, 1, state_size, file) != state_size) {
        free(state);
        fclose(file);
        return false;
    }
    fclose(file);
    
    /* Restore MBC state */
    if (mem->mbc_data) {
        MBC_State* mbc = (MBC_State*)mem->mbc_data;
        mbc->current_rom_bank = state->current_rom_bank;
        mbc->current_ram_bank = state->current_ram_bank;
        mbc->ram_enabled = state->ram_enabled;
        mbc->rom_banking_enabled = state->rom_banking_enabled;
        mbc->banking_mode = state->banking_mode;
        
        if (mbc->rtc_data) {
            memcpy(mbc->rtc_data, &state->rtc, sizeof(RTC_Data));
        }
        
        if (mbc->ram_data && header.ram_size == mbc->ram_size) {
            memcpy(mbc->ram_data, state->ram_data, mbc->ram_size);
        }
    }
    
    /* Restore memory regions */
    memcpy(mem->vram, state->vram, sizeof(state->vram));
    memcpy(mem->wram, state->wram, sizeof(state->wram));
    memcpy(mem->oam, state->oam, sizeof(state->oam));
    memcpy(mem->hram, state->hram, sizeof(state->hram));
    memcpy(mem->io_registers, state->io_registers, sizeof(state->io_registers));
    mem->ie_register = state->ie_register;
    
    free(state);
    return true;
}

bool memory_save_ram(Memory* mem, const char* filename) {
    if (!mem->mbc_data) return false;
    
    MBC_State* mbc = (MBC_State*)mem->mbc_data;
    if (!mbc->ram_data || !mbc->ram_size) return false;
    
    FILE* file = fopen(filename, "wb");
    if (!file) return false;
    
    size_t written = fwrite(mbc->ram_data, 1, mbc->ram_size, file);
    fclose(file);
    
    return written == mbc->ram_size;
}

bool memory_load_ram(Memory* mem, const char* filename) {
    if (!mem->mbc_data) return false;
    
    MBC_State* mbc = (MBC_State*)mem->mbc_data;
    if (!mbc->ram_data || !mbc->ram_size) return false;
    
    FILE* file = fopen(filename, "rb");
    if (!file) return false;
    
    size_t read = fread(mbc->ram_data, 1, mbc->ram_size, file);
    fclose(file);
    
    return read == mbc->ram_size;
}

/* Memory timing implementation */
uint8_t memory_read_timed(Memory* mem, uint16_t addr, uint32_t* cycles) {
    uint8_t value;
    uint8_t delay = 0;
    
    if (addr < 0x8000) {
        /* ROM Access */
        delay = ROM_ACCESS_TIME;
        value = memory_read(mem, addr);
    }
    else if (addr < 0xA000) {
        /* VRAM */
        delay = VRAM_ACCESS_TIME;
        value = memory_read(mem, addr);
    }
    else if (addr < 0xC000) {
        /* External RAM */
        delay = EXT_RAM_ACCESS_TIME;
        value = memory_read(mem, addr);
    }
    else if (addr < 0xE000) {
        /* WRAM */
        delay = WRAM_ACCESS_TIME;
        value = memory_read(mem, addr);
    }
    else if (addr < 0xFE00) {
        /* Echo RAM */
        delay = WRAM_ACCESS_TIME;
        value = memory_read(mem, addr);
    }
    else if (addr < 0xFEA0) {
        /* OAM */
        delay = OAM_ACCESS_TIME;
        value = memory_read(mem, addr);
    }
    else if (addr < 0xFF80) {
        /* I/O Registers */
        delay = 1;
        value = memory_read(mem, addr);
    }
    else {
        /* HRAM and IE */
        delay = HRAM_ACCESS_TIME;
        value = memory_read(mem, addr);
    }
    
    if (cycles) *cycles += delay;
    return value;
}

void memory_write_timed(Memory* mem, uint16_t addr, uint8_t value, uint32_t* cycles) {
    uint8_t delay = 0;
    
    if (addr < 0x8000) {
        /* ROM Access */
        delay = ROM_ACCESS_TIME;
        memory_write(mem, addr, value);
    }
    else if (addr < 0xA000) {
        /* VRAM */
        delay = VRAM_ACCESS_TIME;
        memory_write(mem, addr, value);
    }
    else if (addr < 0xC000) {
        /* External RAM */
        delay = EXT_RAM_ACCESS_TIME;
        memory_write(mem, addr, value);
    }
    else if (addr < 0xE000) {
        /* WRAM */
        delay = WRAM_ACCESS_TIME;
        memory_write(mem, addr, value);
    }
    else if (addr < 0xFE00) {
        /* Echo RAM */
        delay = WRAM_ACCESS_TIME;
        memory_write(mem, addr, value);
    }
    else if (addr < 0xFEA0) {
        /* OAM */
        delay = OAM_ACCESS_TIME;
        memory_write(mem, addr, value);
    }
    else if (addr < 0xFF80) {
        /* I/O Registers */
        delay = 1;
        memory_write(mem, addr, value);
    }
    else {
        /* HRAM and IE */
        delay = HRAM_ACCESS_TIME;
        memory_write(mem, addr, value);
    }
    
    if (cycles) *cycles += delay;
}