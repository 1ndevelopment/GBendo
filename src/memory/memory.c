#include "memory.h"
#include "../ppu/ppu.h"
#include "../apu/apu.h"
#include "../gbendo.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
/* input.h not needed here; JOYP logic is implemented in this module */

/* Update JOYP register from internal input state (used by memory and input) */
void memory_update_joyp(Memory* mem) {
    uint8_t reg = mem->io_registers[0x00] & 0xF0; /* keep upper bits */
    bool select_buttons = !(reg & (1 << 5)); /* P15 low means buttons selected */
    bool select_dirs = !(reg & (1 << 4)); /* P14 low means directions selected */

    uint8_t value = 0x0F; /* default all released */
    bool had_visible_input = false;

    if (select_buttons) {
        uint8_t btn = 0;
        if (mem->joypad_state_buttons & 0x01) btn |= 1<<0; /* A */
        if (mem->joypad_state_buttons & 0x02) btn |= 1<<1; /* B */
        if (mem->joypad_state_buttons & 0x04) btn |= 1<<2; /* Select */
        if (mem->joypad_state_buttons & 0x08) btn |= 1<<3; /* Start */
        value &= (~btn) & 0x0F;
        had_visible_input = true;
    }

    if (select_dirs) {
        uint8_t d = 0;
        if (mem->joypad_state_dirs & 0x01) d |= 1<<0; /* Right */
        if (mem->joypad_state_dirs & 0x02) d |= 1<<1; /* Left */
        if (mem->joypad_state_dirs & 0x04) d |= 1<<2; /* Up */
        if (mem->joypad_state_dirs & 0x08) d |= 1<<3; /* Down */
        uint8_t val_dirs = (~d) & 0x0F;
        if (had_visible_input) {
            value &= val_dirs;
        } else {
            value = val_dirs;
        }
    }

    mem->io_registers[0x00] = (reg & 0x30) | value | 0xC0;
}

/* ROM Bank sizes */
#define ROM_BANK_SIZE 0x4000    /* 16KB */
#define RAM_BANK_SIZE 0x2000    /* 8KB */
#define WRAM_SIZE     0x2000    /* 8KB */
#define VRAM_SIZE     0x2000    /* 8KB */
#define OAM_SIZE      0xA0      /* 160 bytes */
#define HRAM_SIZE     0x7F      /* 127 bytes */

void memory_init(Memory* mem) {
    /* Zero the Memory struct housekeeping fields to avoid uninitialized state */
    memset(mem, 0, sizeof(Memory));

    /* Allocate memory regions */
    mem->rom_bank0 = NULL;      /* Will be set when loading ROM */
    mem->rom_bankn = NULL;      /* Will be set when loading ROM */
    mem->vram = malloc(VRAM_SIZE);
    mem->ext_ram = NULL;        /* Will be allocated if cart has RAM */
    mem->wram = malloc(WRAM_SIZE);
    mem->oam = malloc(OAM_SIZE);
    mem->hram = malloc(HRAM_SIZE);

    /* Clear all memory */
    memset(mem->vram, 0, VRAM_SIZE);
    memset(mem->wram, 0, WRAM_SIZE);
    memset(mem->oam, 0, OAM_SIZE);
    memset(mem->hram, 0, HRAM_SIZE);
    memset(mem->io_registers, 0, sizeof(mem->io_registers));

    /* Initialize joypad state to all released */
    mem->joypad_state_buttons = 0;
    mem->joypad_state_dirs = 0;
    /* Default JOYP register: all bits high (no buttons pressed, selects high) */
    mem->io_registers[0x00] = 0xFF;
    mem->ie_register = 0;

    /* Initialize MBC state */
    mem->mbc_data = NULL;
    mem->mbc_type = ROM_ONLY;

    /* Initialize timer state (DIV/TIMA) */
    memory_timer_init(mem);
}
void memory_cleanup(Memory* mem) {
    if (mem->mbc_data) {
        MBC_State* mbc = (MBC_State*)mem->mbc_data;
        
        /* Additional safety checks to prevent corruption */
        if (mbc != NULL) {
            if (mbc->rom_data) {
                free(mbc->rom_data);
                mbc->rom_data = NULL;
            }
            if (mbc->ram_data) {
                free(mbc->ram_data);
                mbc->ram_data = NULL;
            }
            if (mbc->extra_data) {
                free(mbc->extra_data);
                mbc->extra_data = NULL;
            }
            free(mbc);
        }
        mem->mbc_data = NULL;
    }
    
    if (mem->vram) {
        free(mem->vram);
        mem->vram = NULL;
    }
    if (mem->wram) {
        free(mem->wram);
        mem->wram = NULL;
    }
    if (mem->oam) {
        free(mem->oam);
        mem->oam = NULL;
    }
    if (mem->hram) {
        free(mem->hram);
        mem->hram = NULL;
    }
}

void memory_reset(Memory* mem) {
    /* Clear RAM regions */
    memset(mem->vram, 0, VRAM_SIZE);
    memset(mem->wram, 0, WRAM_SIZE);
    memset(mem->oam, 0, OAM_SIZE);
    memset(mem->hram, 0, HRAM_SIZE);
    memset(mem->io_registers, 0, sizeof(mem->io_registers));
    /* Reset joypad state and JOYP default */
    mem->joypad_state_buttons = 0;
    mem->joypad_state_dirs = 0;
    mem->io_registers[0x00] = 0xFF;
    mem->ie_register = 0;
    /* Reset timer state */
    memory_timer_init(mem);
    /* Reset MBC state if present */
    if (mem->mbc_data) {
        MBC_State* mbc = (MBC_State*)mem->mbc_data;
        mbc->current_rom_bank = 1;
        mbc->current_ram_bank = 0;
        mbc->ram_enabled = false;
        mbc->rom_banking_enabled = true;
        mbc->banking_mode = 0;
    }
}

/* MBC implementations */
uint8_t mbc1_read(Memory* mem, uint16_t addr) {
    MBC_State* mbc = (MBC_State*)mem->mbc_data;
    
    if (addr < 0x4000) {
        /* ROM Bank 0 */
        return mbc->rom_data[addr];
    }
    else if (addr < 0x8000) {
        /* ROM Bank 1-127 */
        uint32_t rom_addr = (addr - 0x4000) + (mbc->current_rom_bank * 0x4000);
        return mbc->rom_data[rom_addr];
    }
    else if (addr >= 0xA000 && addr < 0xC000) {
        /* External RAM */
        if (mbc->ram_enabled && mbc->ram_data) {
            uint32_t ram_addr = (addr - 0xA000) + (mbc->current_ram_bank * RAM_BANK_SIZE);
            return mbc->ram_data[ram_addr];
        }
        return 0xFF;
    }
    return 0xFF;
}

void mbc1_write(Memory* mem, uint16_t addr, uint8_t value) {
    MBC_State* mbc = (MBC_State*)mem->mbc_data;
    
    if (addr < 0x2000) {
        /* RAM Enable */
        mbc->ram_enabled = ((value & 0x0F) == 0x0A);
    }
    else if (addr < 0x4000) {
        /* ROM Bank Number */
        uint8_t bank = value & 0x1F;
        if (bank == 0) bank = 1;
        mbc->current_rom_bank = (mbc->current_rom_bank & 0x60) | bank;
    }
    else if (addr < 0x6000) {
        /* RAM Bank Number or Upper ROM Bank Number */
        if (mbc->banking_mode == 0) {
            /* ROM Banking Mode */
            mbc->current_rom_bank = (mbc->current_rom_bank & 0x1F) | ((value & 0x03) << 5);
        } else {
            /* RAM Banking Mode */
            mbc->current_ram_bank = value & 0x03;
        }
    }
    else if (addr < 0x8000) {
        /* Banking Mode Select */
        mbc->banking_mode = value & 0x01;
    }
    else if (addr >= 0xA000 && addr < 0xC000) {
        /* External RAM */
        if (mbc->ram_enabled && mbc->ram_data) {
            uint32_t ram_addr = (addr - 0xA000) + (mbc->current_ram_bank * RAM_BANK_SIZE);
            mbc->ram_data[ram_addr] = value;
        }
    }
}

/* Memory access functions */
uint8_t memory_read(Memory* mem, uint16_t addr) {
    /* ROM Access (potentially MBC controlled) */
    if (addr < 0x8000) {
        switch (mem->mbc_type) {
            case MBC1:
                return mbc1_read(mem, addr);
            case ROM_ONLY:
                if (addr < 0x4000) {
                    return mem->rom_bank0[addr];
                } else {
                    return mem->rom_bankn[addr - 0x4000];
                }
            default:
                return 0xFF;
        }
    }
    
    /* VRAM */
    if (addr < 0xA000) {
        /* Route through PPU to respect access restrictions */
        if (mem->ppu) {
            return ppu_read_vram((PPU*)mem->ppu, mem, addr);
        } else {
            /* Fallback if PPU not set */
            return mem->vram[addr - 0x8000];
        }
    }
    
    /* External RAM */
    if (addr < 0xC000) {
        if (mem->mbc_type == ROM_ONLY) {
            if (mem->ext_ram) {
                return mem->ext_ram[addr - 0xA000];
            }
            return 0xFF;
        }
        return mbc1_read(mem, addr);
    }
    
    /* Work RAM */
    if (addr < 0xE000) {
        return mem->wram[addr - 0xC000];
    }
    
    /* Echo RAM */
    if (addr < 0xFE00) {
        return mem->wram[addr - 0xE000];
    }
    
    /* OAM */
    if (addr < 0xFEA0) {
        /* Route through PPU to respect access restrictions */
        if (mem->ppu) {
            return ppu_read_oam((PPU*)mem->ppu, mem, addr);
        } else {
            /* Fallback if PPU not set */
            return mem->oam[addr - 0xFE00];
        }
    }
    
    /* Unusable */
    if (addr < 0xFF00) {
        return 0xFF;
    }
    
    /* I/O Registers */
    if (addr < 0xFF80) {
        uint8_t reg_addr = addr - 0xFF00;
        switch (reg_addr) {
            case 0x00: /* JOYP */
                /* Update joypad state before read */
                memory_update_joyp(mem);
                /* Return current JOYP value */
                return mem->io_registers[0x00];

            case 0x0F: /* IF - unused bits read as 1 */
                return mem->io_registers[0x0F] | 0xE0;
            
            case 0x40: /* LCDC - route through PPU */
            case 0x41: /* STAT - route through PPU */
            case 0x42: /* SCY - route through PPU */
            case 0x43: /* SCX - route through PPU */
            case 0x44: /* LY - route through PPU (must be current value) */
            case 0x45: /* LYC - route through PPU */
            case 0x47: /* BGP - route through PPU */
            case 0x48: /* OBP0 - route through PPU */
            case 0x49: /* OBP1 - route through PPU */
            case 0x4A: /* WY - route through PPU */
            case 0x4B: /* WX - route through PPU */
            case 0x4F: /* VBK (CGB) */
            case 0x68: /* BGPI (CGB) */
            case 0x69: /* BGPD (CGB) */
            case 0x6A: /* OBPI (CGB) */
            case 0x6B: /* OBPD (CGB) */
                if (mem->ppu) {
                    return ppu_read_register((PPU*)mem->ppu, addr);
                }
                return mem->io_registers[reg_addr];
            
            /* APU registers */
            case 0x10: case 0x11: case 0x12: case 0x13: case 0x14: /* NR10-NR14 */
            case 0x16: case 0x17: case 0x18: case 0x19:             /* NR21-NR24 */
            case 0x1A: case 0x1B: case 0x1C: case 0x1D: case 0x1E: /* NR30-NR34 */
            case 0x20: case 0x21: case 0x22: case 0x23:             /* NR41-NR44 */
            case 0x24: case 0x25: case 0x26:                        /* NR50-NR52 */
            case 0x30: case 0x31: case 0x32: case 0x33:             /* Wave RAM */
            case 0x34: case 0x35: case 0x36: case 0x37:
            case 0x38: case 0x39: case 0x3A: case 0x3B:
            case 0x3C: case 0x3D: case 0x3E: case 0x3F:
                if (mem->apu) {
                    return apu_read_register((APU*)mem->apu, addr);
                }
                return mem->io_registers[reg_addr];

            default:
                return mem->io_registers[reg_addr];
        }
    }
    
    /* HRAM */
    if (addr < 0xFFFF) {
        return mem->hram[addr - 0xFF80];
    }
    
    /* Interrupt Enable Register */
    return mem->ie_register;
}

void memory_write(Memory* mem, uint16_t addr, uint8_t value) {
    /* ROM Access (potentially MBC controlled) */
    if (addr < 0x8000) {
        if (mem->mbc_type == MBC1) {
            mbc1_write(mem, addr, value);
        }
        return;
    }
    
    /* VRAM */
    if (addr < 0xA000) {
        /* Route through PPU to respect access restrictions */
        if (mem->ppu) {
            ppu_write_vram((PPU*)mem->ppu, mem, addr, value);
        } else {
            /* Fallback if PPU not set (shouldn't happen in normal operation) */
            mem->vram[addr - 0x8000] = value;
        }
        return;
    }
    
    /* External RAM */
    if (addr < 0xC000) {
        if (mem->mbc_type == ROM_ONLY) {
            if (mem->ext_ram) {
                mem->ext_ram[addr - 0xA000] = value;
            }
        } else {
            mbc1_write(mem, addr, value);
        }
        return;
    }
    
    /* Work RAM */
    if (addr < 0xE000) {
        mem->wram[addr - 0xC000] = value;
        return;
    }
    
    /* Echo RAM */
    if (addr < 0xFE00) {
        mem->wram[addr - 0xE000] = value;
        return;
    }
    
    /* OAM */
    if (addr < 0xFEA0) {
        /* Route through PPU to respect access restrictions */
        if (mem->ppu) {
            ppu_write_oam((PPU*)mem->ppu, mem, addr, value);
        } else {
            /* Fallback if PPU not set (shouldn't happen in normal operation) */
            mem->oam[addr - 0xFE00] = value;
        }
        return;
    }
    
    /* Unusable */
    if (addr < 0xFF00) {
        return;
    }
    
    /* I/O Registers */
    if (addr < 0xFF80) {
        /* Debug output for important I/O writes */
        if (addr >= 0xFF40 && addr <= 0xFF4B && gb_is_debug_enabled()) {
            printf("[MEM] I/O Write: 0x%04X = 0x%02X\n", addr, value);
        }
        
        /* Handle special registers with cycle-exact behavior for timers */
        switch (addr & 0xFF) {
            case 0x00: /* JOYP */
                /* Only bits 4-5 are writable (P14/P15 select lines) */
                mem->io_registers[0x00] = (value & 0x30) | (mem->io_registers[0x00] & 0xCF);
                /* Update input lines based on new select state */
                memory_update_joyp(mem);
                return;

            case 0x04: /* DIV - writing any value resets DIV to zero */
                mem->div_internal = 0;
                mem->div = 0;
                mem->io_registers[0x04] = 0;
                return;

            case 0x05: /* TIMA */
                mem->tima = value;
                mem->io_registers[0x05] = value;
                return;
            case 0x06: /* TMA */
                mem->tma = value;
                mem->io_registers[0x06] = value;
                return;
            case 0x07: /* TAC */
            {
                uint8_t old_tac = mem->tac;
                uint8_t old_enabled = (old_tac & 0x04) != 0;
                uint8_t old_bit_index;
                switch (old_tac & 0x03) {
                    case 0: old_bit_index = 9; break;
                    case 1: old_bit_index = 3; break;
                    case 2: old_bit_index = 5; break;
                    default: old_bit_index = 7; break;
                }

                mem->tac = value & 0x07;
                mem->io_registers[0x07] = mem->tac;

                uint8_t new_enabled = (mem->tac & 0x04) != 0;
                uint8_t new_bit_index;
                switch (mem->tac & 0x03) {
                    case 0: new_bit_index = 9; break;
                    case 1: new_bit_index = 3; break;
                    case 2: new_bit_index = 5; break;
                    default: new_bit_index = 7; break;
                }

                uint8_t old_bit = (mem->div_internal >> old_bit_index) & 1;
                uint8_t new_bit = (mem->div_internal >> new_bit_index) & 1;

                if (!old_enabled && new_enabled) {
                    /* If enabling timer and selected bit had a falling edge, increment TIMA */
                    if (old_bit == 1 && new_bit == 0) {
                        mem->tima++;
                        if (mem->tima == 0x00) {
                            mem->tima_reload_pending = true;
                            mem->tima_reload_delay = 4;
                            mem->io_registers[0x05] = 0x00;
                        } else {
                            mem->io_registers[0x05] = mem->tima;
                        }
                    }
                } else if (old_enabled && new_enabled && old_bit_index != new_bit_index) {
                    /* Clock select changed while enabled; check for falling edge */
                    if (old_bit == 1 && new_bit == 0) {
                        mem->tima++;
                        if (mem->tima == 0x00) {
                            mem->tima_reload_pending = true;
                            {
                                mem->tima_reload_delay = 4;
                                mem->io_registers[0x05] = 0x00;
                            }
                            mem->io_registers[0x05] = 0x00;
                        } else {
                            mem->io_registers[0x05] = mem->tima;
                        }
                    }
                }
                return;
            }
            case 0x46: /* DMA Transfer */
                memory_dma_transfer(mem, value);
                return;
            default: {
                    uint8_t reg_addr = addr - 0xFF00;
                    switch ((uint8_t)(addr & 0xFF)) {
                        case 0x0F: /* IF */
                            /* Only lower 5 bits are writable */
                            mem->io_registers[0x0F] = value & 0x1F;
                            return;
                        
                        case 0x40: /* LCDC - route through PPU */
                        case 0x41: /* STAT - route through PPU */
                        case 0x42: /* SCY - route through PPU */
                        case 0x43: /* SCX - route through PPU */
                        case 0x45: /* LYC - route through PPU */
                        case 0x47: /* BGP - route through PPU */
                        case 0x48: /* OBP0 - route through PPU */
                        case 0x49: /* OBP1 - route through PPU */
                        case 0x4A: /* WY - route through PPU */
                        case 0x4B: /* WX - route through PPU */
                        case 0x4F: /* VBK - CGB VRAM bank select */
                            if (mem->ppu) {
                                ppu_write_register((PPU*)mem->ppu, addr, value);
                            } else {
                                mem->io_registers[reg_addr] = value; /* Fallback */
                            }
                            return;
                        case 0x68: /* BGPI */
                        case 0x69: /* BGPD */
                        case 0x6A: /* OBPI */
                        case 0x6B: /* OBPD */
                            if (mem->ppu) {
                                ppu_write_cgb_registers((PPU*)mem->ppu, addr, value);
                            } else {
                                mem->io_registers[reg_addr] = value;
                            }
                            return;
                        
                        /* APU registers */
                        case 0x10: case 0x11: case 0x12: case 0x13: case 0x14: /* NR10-NR14 */
                        case 0x16: case 0x17: case 0x18: case 0x19:             /* NR21-NR24 */
                        case 0x1A: case 0x1B: case 0x1C: case 0x1D: case 0x1E: /* NR30-NR34 */
                        case 0x20: case 0x21: case 0x22: case 0x23:             /* NR41-NR44 */
                        case 0x24: case 0x25: case 0x26:                        /* NR50-NR52 */
                        case 0x30: case 0x31: case 0x32: case 0x33:             /* Wave RAM */
                        case 0x34: case 0x35: case 0x36: case 0x37:
                        case 0x38: case 0x39: case 0x3A: case 0x3B:
                        case 0x3C: case 0x3D: case 0x3E: case 0x3F:
                            if (mem->apu) {
                                apu_write_register((APU*)mem->apu, addr, value);
                            } else {
                                mem->io_registers[reg_addr] = value;
                            }
                            return;
                        
                        default:
                            mem->io_registers[reg_addr] = value;
                            return;
                    }
                } /* end default case block */
            } /* end switch (addr & 0xFF) */
        } /* end if (addr < 0xFF80) */
    
    /* HRAM */
    if (addr < 0xFFFF) {
        mem->hram[addr - 0xFF80] = value;
        return;
    }
    
    /* Interrupt Enable Register */
    mem->ie_register = value;
}

/* DMA Transfer */
void memory_dma_transfer(Memory* mem, uint8_t start) {
    /* Forward to PPU's DMA transfer which handles the actual copy */
    if (mem->ppu) {
        ppu_dma_transfer((PPU*)mem->ppu, mem, start);
    }
}

/* ROM loading and MBC setup */
bool memory_load_rom(Memory* mem, const char* filename) {
    FILE* file = fopen(filename, "rb");
    if (!file) return false;

    /* Clean up any existing ROM data first */
    if (mem->mbc_data) {
        MBC_State* old_mbc = (MBC_State*)mem->mbc_data;
        if (old_mbc->rom_data) free(old_mbc->rom_data);
        if (old_mbc->ram_data) free(old_mbc->ram_data);
        if (old_mbc->extra_data) free(old_mbc->extra_data);
        free(old_mbc);
        mem->mbc_data = NULL;
    }

    /* Read ROM header */
    uint8_t header[0x150];
    if (fread(header, 1, sizeof(header), file) != sizeof(header)) {
        fclose(file);
        return false;
    }

    /* Get ROM size */
    size_t rom_size = 32768 << header[0x148];  /* ROM size from header */
    
    /* Get RAM size */
    size_t ram_size = 0;
    switch (header[0x149]) {
        case 0x02: ram_size = 8192; break;    /* 8KB */
        case 0x03: ram_size = 32768; break;   /* 32KB */
        case 0x04: ram_size = 131072; break;  /* 128KB */
        case 0x05: ram_size = 65536; break;   /* 64KB */
    }

    /* Determine MBC type */
    switch (header[0x147]) {
        case 0x01:
        case 0x02:
        case 0x03:
            mem->mbc_type = MBC1;
            break;
        case 0x00:
            mem->mbc_type = ROM_ONLY;
            break;
        default:
            fclose(file);
            return false;  /* Unsupported MBC type */
    }

    /* Allocate ROM and RAM */
    MBC_State* mbc = malloc(sizeof(MBC_State));
    mbc->rom_data = malloc(rom_size);
    mbc->ram_data = ram_size > 0 ? malloc(ram_size) : NULL;
    mbc->rom_size = rom_size;
    mbc->ram_size = ram_size;
    mbc->rom_bank_count = rom_size / ROM_BANK_SIZE;
    mbc->ram_bank_count = ram_size / RAM_BANK_SIZE;
    mbc->current_rom_bank = 1;
    mbc->current_ram_bank = 0;
    mbc->ram_enabled = false;
    mbc->rom_banking_enabled = true;
    mbc->banking_mode = 0;

    /* Read ROM data */
    fseek(file, 0, SEEK_SET);
    if (fread(mbc->rom_data, 1, rom_size, file) != rom_size) {
        free(mbc->rom_data);
        if (mbc->ram_data) free(mbc->ram_data);
        free(mbc);
        fclose(file);
        return false;
    }

    fclose(file);

    /* Set up memory mapping */
    mem->mbc_data = mbc;
    mem->rom_bank0 = mbc->rom_data;
    mem->rom_bankn = mbc->rom_data + ROM_BANK_SIZE;
    mem->ext_ram = mbc->ram_data;

    return true;
}

void memory_setup_banking(Memory* mem, MBC_Type type) {
    mem->mbc_type = type;
}