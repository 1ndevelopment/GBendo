#include "memory.h"
#include <zlib.h>
#include <time.h>
#include <stdio.h>

/* CGB speed switching state */
static struct {
    Speed_Mode current_speed;
    bool speed_switch_pending;
    uint32_t switch_delay_cycles;
} cgb_speed;

void memory_init_cgb(Memory* mem) {
    (void)mem;  /* Parameter reserved for future use */
    cgb_speed.current_speed = SPEED_NORMAL;
    cgb_speed.speed_switch_pending = false;
    cgb_speed.switch_delay_cycles = 0;
}

void memory_handle_speed_switch(Memory* mem) {
    if (cgb_speed.speed_switch_pending) {
        if (cgb_speed.switch_delay_cycles > 0) {
            cgb_speed.switch_delay_cycles--;
        } else {
            cgb_speed.current_speed = (cgb_speed.current_speed == SPEED_NORMAL) ? 
                                     SPEED_DOUBLE : SPEED_NORMAL;
            cgb_speed.speed_switch_pending = false;
            
            /* Update KEY1 register */
            mem->io_registers[0x4D] = (cgb_speed.current_speed == SPEED_DOUBLE) ? 0x80 : 0x00;
        }
    }
}

void memory_request_speed_switch(Memory* mem) {
    (void)mem;  /* Parameter reserved for future use */
    if (!cgb_speed.speed_switch_pending) {
        cgb_speed.speed_switch_pending = true;
        cgb_speed.switch_delay_cycles = CGB_SPEED_SWITCH_DELAY;
    }
}

Speed_Mode memory_get_current_speed(Memory* mem) {
    (void)mem;  /* Parameter reserved for future use */
    return cgb_speed.current_speed;
}

/* Compressed save state support */
#define COMPRESSION_LEVEL 6
#define CHUNK 16384

bool memory_save_state_compressed(Memory* mem, const char* filename) {
    FILE* file = fopen(filename, "wb");
    if (!file) return false;
    
    z_stream strm;
    strm.zalloc = Z_NULL;
    strm.zfree = Z_NULL;
    strm.opaque = Z_NULL;
    
    if (deflateInit(&strm, COMPRESSION_LEVEL) != Z_OK) {
        fclose(file);
        return false;
    }
    
    /* Prepare state data */
    MBC_State* mbc = (MBC_State*)mem->mbc_data;
    size_t state_size = sizeof(SaveState) + (mbc ? mbc->ram_size : 0);
    SaveState* state = malloc(state_size);
    
    /* Fill state data (same as uncompressed version) */
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
    
    /* Write compressed data */
    unsigned char out[CHUNK];
    strm.avail_in = state_size;
    strm.next_in = (unsigned char*)state;
    
    do {
        strm.avail_out = CHUNK;
        strm.next_out = out;
        
        int ret = deflate(&strm, Z_FINISH);
        if (ret == Z_STREAM_ERROR) {
            deflateEnd(&strm);
            free(state);
            fclose(file);
            return false;
        }
        
        unsigned have = CHUNK - strm.avail_out;
        if (fwrite(out, 1, have, file) != have) {
            deflateEnd(&strm);
            free(state);
            fclose(file);
            return false;
        }
    } while (strm.avail_out == 0);
    
    deflateEnd(&strm);
    free(state);
    fclose(file);
    return true;
}

bool memory_load_state_compressed(Memory* mem, const char* filename) {
    FILE* file = fopen(filename, "rb");
    if (!file) return false;
    
    z_stream strm;
    strm.zalloc = Z_NULL;
    strm.zfree = Z_NULL;
    strm.opaque = Z_NULL;
    strm.avail_in = 0;
    strm.next_in = Z_NULL;
    
    if (inflateInit(&strm) != Z_OK) {
        fclose(file);
        return false;
    }
    
    /* Read and decompress data */
    unsigned char in[CHUNK];
    unsigned char* out_buf = NULL;
    size_t out_size = 0;
    size_t out_capacity = 0;
    
    do {
        strm.avail_in = fread(in, 1, CHUNK, file);
        if (ferror(file)) {
            inflateEnd(&strm);
            free(out_buf);
            fclose(file);
            return false;
        }
        
        if (strm.avail_in == 0) break;
        strm.next_in = in;
        
        do {
            if (out_size + CHUNK > out_capacity) {
                out_capacity = out_capacity ? out_capacity * 2 : CHUNK;
                out_buf = realloc(out_buf, out_capacity);
            }
            
            strm.avail_out = CHUNK;
            strm.next_out = out_buf + out_size;
            
            int ret = inflate(&strm, Z_NO_FLUSH);
            if (ret == Z_STREAM_ERROR || ret == Z_NEED_DICT || 
                ret == Z_DATA_ERROR || ret == Z_MEM_ERROR) {
                inflateEnd(&strm);
                free(out_buf);
                fclose(file);
                return false;
            }
            
            out_size += CHUNK - strm.avail_out;
        } while (strm.avail_out == 0);
    } while (strm.avail_in != 0);
    
    inflateEnd(&strm);
    fclose(file);
    
    /* Process decompressed data */
    if (out_size < sizeof(SaveState)) {
        free(out_buf);
        return false;
    }
    
    SaveState* state = (SaveState*)out_buf;
    
    /* Verify version */
    if (state->version != SAVE_STATE_VERSION) {
        free(out_buf);
        return false;
    }
    
    /* Restore state (same as uncompressed version) */
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
        
        if (mbc->ram_data && state->ram_size == mbc->ram_size) {
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
    
    free(out_buf);
    return true;
}

/* Compressed RAM save support */
bool memory_save_ram_compressed(Memory* mem, const char* filename) {
    if (!mem->mbc_data) return false;
    
    MBC_State* mbc = (MBC_State*)mem->mbc_data;
    if (!mbc->ram_data || !mbc->ram_size) return false;
    
    FILE* file = fopen(filename, "wb");
    if (!file) return false;
    
    z_stream strm;
    strm.zalloc = Z_NULL;
    strm.zfree = Z_NULL;
    strm.opaque = Z_NULL;
    
    if (deflateInit(&strm, COMPRESSION_LEVEL) != Z_OK) {
        fclose(file);
        return false;
    }
    
    unsigned char out[CHUNK];
    strm.avail_in = mbc->ram_size;
    strm.next_in = mbc->ram_data;
    
    do {
        strm.avail_out = CHUNK;
        strm.next_out = out;
        
        int ret = deflate(&strm, Z_FINISH);
        if (ret == Z_STREAM_ERROR) {
            deflateEnd(&strm);
            fclose(file);
            return false;
        }
        
        unsigned have = CHUNK - strm.avail_out;
        if (fwrite(out, 1, have, file) != have) {
            deflateEnd(&strm);
            fclose(file);
            return false;
        }
    } while (strm.avail_out == 0);
    
    deflateEnd(&strm);
    fclose(file);
    return true;
}

bool memory_load_ram_compressed(Memory* mem, const char* filename) {
    if (!mem->mbc_data) return false;
    
    MBC_State* mbc = (MBC_State*)mem->mbc_data;
    if (!mbc->ram_data || !mbc->ram_size) return false;
    
    FILE* file = fopen(filename, "rb");
    if (!file) return false;
    
    z_stream strm;
    strm.zalloc = Z_NULL;
    strm.zfree = Z_NULL;
    strm.opaque = Z_NULL;
    strm.avail_in = 0;
    strm.next_in = Z_NULL;
    
    if (inflateInit(&strm) != Z_OK) {
        fclose(file);
        return false;
    }
    
    unsigned char in[CHUNK];
    
    do {
        strm.avail_in = fread(in, 1, CHUNK, file);
        if (ferror(file)) {
            inflateEnd(&strm);
            fclose(file);
            return false;
        }
        
        if (strm.avail_in == 0) break;
        strm.next_in = in;
        
        do {
            strm.avail_out = mbc->ram_size;
            strm.next_out = mbc->ram_data;
            
            int ret = inflate(&strm, Z_NO_FLUSH);
            if (ret == Z_STREAM_ERROR || ret == Z_NEED_DICT || 
                ret == Z_DATA_ERROR || ret == Z_MEM_ERROR) {
                inflateEnd(&strm);
                fclose(file);
                return false;
            }
        } while (strm.avail_out == 0);
    } while (strm.avail_in != 0);
    
    inflateEnd(&strm);
    fclose(file);
    return true;
}