#include "memory.h"
#include <time.h>

/* MBC6 Implementation - Flash Memory and 32Mb ROM support */
__attribute__((unused)) static uint8_t mbc6_read(Memory* mem, uint16_t addr) {
    MBC_State* mbc = (MBC_State*)mem->mbc_data;
    
    if (addr < 0x4000) {
        /* ROM Bank 0 */
        return mbc->rom_data[addr];
    }
    else if (addr < 0x8000) {
        /* ROM Bank 1-512 (2 separate regions) */
        uint8_t region = (addr & 0x2000) ? 1 : 0;
        uint32_t bank = region ? mbc->current_rom_bank2 : mbc->current_rom_bank;
        uint32_t rom_addr = (addr & 0x1FFF) + (bank * 0x2000);
        return mbc->rom_data[rom_addr];
    }
    else if (addr >= 0xA000 && addr < 0xC000) {
        /* Flash RAM (2 separate regions) */
        if (mbc->ram_enabled && mbc->ram_data) {
            uint8_t region = (addr & 0x1000) ? 1 : 0;
            uint8_t bank = region ? mbc->current_ram_bank2 : mbc->current_ram_bank;
            uint32_t ram_addr = (addr & 0xFFF) + (bank * 0x1000);
            return mbc->ram_data[ram_addr];
        }
    }
    return 0xFF;
}

__attribute__((unused)) static void mbc6_write(Memory* mem, uint16_t addr, uint8_t value) {
    MBC_State* mbc = (MBC_State*)mem->mbc_data;
    
    if (addr < 0x2000) {
        /* RAM Enable */
        mbc->ram_enabled = ((value & 0x0F) == 0x0A);
    }
    else if (addr < 0x3000) {
        /* ROM Bank Number (Region 1) */
        mbc->current_rom_bank = value;
    }
    else if (addr < 0x4000) {
        /* ROM Bank Number (Region 2) */
        mbc->current_rom_bank2 = value;
    }
    else if (addr < 0x5000) {
        /* RAM Bank Number (Region 1) */
        mbc->current_ram_bank = value & 0x7;
    }
    else if (addr < 0x6000) {
        /* RAM Bank Number (Region 2) */
        mbc->current_ram_bank2 = value & 0x7;
    }
    else if (addr >= 0xA000 && addr < 0xC000) {
        /* Flash RAM write with bank selection */
        if (mbc->ram_enabled && mbc->ram_data) {
            uint8_t region = (addr & 0x1000) ? 1 : 0;
            uint8_t bank = region ? mbc->current_ram_bank2 : mbc->current_ram_bank;
            uint32_t ram_addr = (addr & 0xFFF) + (bank * 0x1000);
            
            /* Flash memory specific commands could be implemented here */
            mbc->ram_data[ram_addr] = value;
        }
    }
}

/* MBC7 Implementation - Accelerometer and EEPROM */
typedef struct {
    uint16_t accel_x;
    uint16_t accel_y;
    uint8_t eeprom_command;
    uint16_t eeprom_address;
    uint8_t eeprom_data[256];
    uint8_t eeprom_state;
    uint8_t eeprom_buffer;
    uint8_t eeprom_bit_count;
    bool eeprom_write_enable;
} MBC7_Data;

__attribute__((unused)) static uint8_t mbc7_read(Memory* mem, uint16_t addr) {
    MBC_State* mbc = (MBC_State*)mem->mbc_data;
    MBC7_Data* mbc7 = (MBC7_Data*)mbc->extra_data;
    
    if (addr < 0x4000) {
        return mbc->rom_data[addr];
    }
    else if (addr < 0x8000) {
        uint32_t rom_addr = (addr - 0x4000) + (mbc->current_rom_bank * 0x4000);
        return mbc->rom_data[rom_addr];
    }
    else if (addr >= 0xA000 && addr < 0xB000) {
        if (mbc->ram_enabled) {
            /* Accelerometer/EEPROM registers */
            switch (addr & 0xF0) {
                case 0x20: return (mbc7->accel_x >> 8) & 0xFF;
                case 0x30: return mbc7->accel_x & 0xFF;
                case 0x40: return (mbc7->accel_y >> 8) & 0xFF;
                case 0x50: return mbc7->accel_y & 0xFF;
                case 0x60: return mbc7->eeprom_state;
                default: return 0xFF;
            }
        }
    }
    return 0xFF;
}

__attribute__((unused)) static void mbc7_write(Memory* mem, uint16_t addr, uint8_t value) {
    MBC_State* mbc = (MBC_State*)mem->mbc_data;
    MBC7_Data* mbc7 = (MBC7_Data*)mbc->extra_data;
    
    if (addr < 0x2000) {
        /* RAM Enable */
        mbc->ram_enabled = ((value & 0x0F) == 0x0A);
    }
    else if (addr < 0x4000) {
        /* ROM Bank Number */
        mbc->current_rom_bank = value & 0x7F;
    }
    else if (addr >= 0xA000 && addr < 0xB000) {
        if (mbc->ram_enabled) {
            switch (addr & 0xF0) {
                case 0x00: /* Accelerometer calibration */
                    break;
                case 0x10: /* Accelerometer calibration */
                    break;
                case 0x60: /* EEPROM commands */
                    /* Handle EEPROM protocol */
                    {
                        bool cs = (value & 0x80) != 0;
                        bool clock = (value & 0x40) != 0;
                        bool data = (value & 0x02) != 0;
                        
                        if (cs) {
                            if (!clock && mbc7->eeprom_state & 0x40) {
                                /* Falling clock edge */
                                if (mbc7->eeprom_bit_count == 0) {
                                    /* Start new command */
                                    mbc7->eeprom_command = 0;
                                    mbc7->eeprom_buffer = 0;
                                }
                                
                                mbc7->eeprom_buffer = (mbc7->eeprom_buffer << 1) | (data ? 1 : 0);
                                mbc7->eeprom_bit_count++;
                                
                                if (mbc7->eeprom_bit_count == 8) {
                                    /* Command complete */
                                    mbc7->eeprom_command = mbc7->eeprom_buffer;
                                    mbc7->eeprom_bit_count = 0;
                                    
                                    switch (mbc7->eeprom_command) {
                                        case 0x06: /* WREN - Write Enable */
                                            mbc7->eeprom_write_enable = true;
                                            break;
                                        case 0x04: /* WRDI - Write Disable */
                                            mbc7->eeprom_write_enable = false;
                                            break;
                                        /* Add other EEPROM commands as needed */
                                    }
                                }
                            }
                            mbc7->eeprom_state = (clock << 6) | 0x02;
                        } else {
                            mbc7->eeprom_state = 0x00;
                            mbc7->eeprom_bit_count = 0;
                        }
                    }
                    break;
            }
        }
    }
}

/* MMM01 Implementation */
__attribute__((unused)) static uint8_t mmm01_read(Memory* mem, uint16_t addr) {
    MBC_State* mbc = (MBC_State*)mem->mbc_data;
    
    if (!mbc->rom_banking_enabled) {
        /* Pre-initialization mode */
        if (addr < 0x4000) {
            return mbc->rom_data[addr];
        }
        else if (addr < 0x8000) {
            return mbc->rom_data[addr];
        }
    } else {
        /* Normal MBC mode */
        if (addr < 0x4000) {
            uint32_t base = mbc->base_rom_bank * 0x4000;
            return mbc->rom_data[base + addr];
        }
        else if (addr < 0x8000) {
            uint32_t rom_addr = (addr - 0x4000) + (mbc->current_rom_bank * 0x4000);
            return mbc->rom_data[rom_addr];
        }
    }
    
    if (addr >= 0xA000 && addr < 0xC000) {
        if (mbc->ram_enabled && mbc->ram_data) {
            uint32_t ram_addr = (addr - 0xA000) + (mbc->current_ram_bank * 0x2000);
            return mbc->ram_data[ram_addr];
        }
    }
    
    return 0xFF;
}

__attribute__((unused)) static void mmm01_write(Memory* mem, uint16_t addr, uint8_t value) {
    MBC_State* mbc = (MBC_State*)mem->mbc_data;
    
    if (!mbc->rom_banking_enabled) {
        if (addr < 0x2000) {
            /* Base ROM Bank high */
            mbc->base_rom_bank = (mbc->base_rom_bank & 0x3F) | ((value & 0x3) << 6);
        }
        else if (addr < 0x4000) {
            /* Base ROM Bank low */
            mbc->base_rom_bank = (mbc->base_rom_bank & 0xC0) | (value & 0x3F);
        }
        else if (addr < 0x6000) {
            /* ROM Bank Mask */
            mbc->rom_bank_mask = value;
        }
        else if (addr < 0x8000) {
            /* Finish initialization */
            mbc->rom_banking_enabled = true;
            mbc->current_rom_bank = 1;
        }
    } else {
        /* Normal MBC mode */
        if (addr < 0x2000) {
            mbc->ram_enabled = ((value & 0x0F) == 0x0A);
        }
        else if (addr < 0x4000) {
            mbc->current_rom_bank = value & mbc->rom_bank_mask;
            if (mbc->current_rom_bank == 0) mbc->current_rom_bank = 1;
        }
        else if (addr < 0x6000) {
            mbc->current_ram_bank = value & 0x03;
        }
    }
    
    if (addr >= 0xA000 && addr < 0xC000) {
        if (mbc->ram_enabled && mbc->ram_data) {
            uint32_t ram_addr = (addr - 0xA000) + (mbc->current_ram_bank * 0x2000);
            mbc->ram_data[ram_addr] = value;
        }
    }
}

/* Pocket Camera Implementation */
typedef struct {
    uint8_t camera_regs[0x36];
    uint8_t camera_ram[0x2000];
    bool camera_powered;
} Camera_Data;

__attribute__((unused)) static uint8_t pocket_camera_read(Memory* mem, uint16_t addr) {
    MBC_State* mbc = (MBC_State*)mem->mbc_data;
    Camera_Data* cam = (Camera_Data*)mbc->extra_data;
    
    if (addr < 0x4000) {
        return mbc->rom_data[addr];
    }
    else if (addr < 0x8000) {
        uint32_t rom_addr = (addr - 0x4000) + (mbc->current_rom_bank * 0x4000);
        return mbc->rom_data[rom_addr];
    }
    else if (addr >= 0xA000 && addr < 0xC000) {
        if (mbc->ram_enabled) {
            if (mbc->current_ram_bank == 0) {
                /* Camera registers */
                if (addr >= 0xA000 && addr < 0xA036) {
                    return cam->camera_regs[addr - 0xA000];
                }
            } else {
                /* Camera RAM */
                return cam->camera_ram[addr - 0xA000];
            }
        }
    }
    return 0xFF;
}

__attribute__((unused)) static void pocket_camera_write(Memory* mem, uint16_t addr, uint8_t value) {
    MBC_State* mbc = (MBC_State*)mem->mbc_data;
    Camera_Data* cam = (Camera_Data*)mbc->extra_data;
    
    if (addr < 0x2000) {
        /* RAM/Camera Enable */
        mbc->ram_enabled = ((value & 0x0F) == 0x0A);
    }
    else if (addr < 0x4000) {
        /* ROM Bank */
        mbc->current_rom_bank = value ?: 1;
    }
    else if (addr < 0x6000) {
        /* RAM Bank */
        mbc->current_ram_bank = value & 0x0F;
    }
    else if (addr >= 0xA000 && addr < 0xC000) {
        if (mbc->ram_enabled) {
            if (mbc->current_ram_bank == 0) {
                /* Camera registers */
                if (addr >= 0xA000 && addr < 0xA036) {
                    cam->camera_regs[addr - 0xA000] = value;
                    
                    /* Handle camera operations */
                    if (addr == 0xA000) {
                        cam->camera_powered = (value & 0x01) != 0;
                    }
                }
            } else {
                /* Camera RAM */
                cam->camera_ram[addr - 0xA000] = value;
            }
        }
    }
}