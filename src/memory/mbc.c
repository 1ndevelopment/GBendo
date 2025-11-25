#include "memory.h"
#include <time.h>

/* MBC2 Implementation */
static uint8_t mbc2_read(Memory* mem, uint16_t addr) {
    MBC_State* mbc = (MBC_State*)mem->mbc_data;
    
    if (addr < 0x4000) {
        return mbc->rom_data[addr];
    }
    else if (addr < 0x8000) {
        uint32_t rom_addr = (addr - 0x4000) + (mbc->current_rom_bank * 0x4000);
        return mbc->rom_data[rom_addr];
    }
    else if (addr >= 0xA000 && addr < 0xA200) {
        /* MBC2 has built-in 512x4 bits RAM */
        if (mbc->ram_enabled) {
            return mbc->ram_data[addr - 0xA000] & 0x0F;
        }
    }
    return 0xFF;
}

static void mbc2_write(Memory* mem, uint16_t addr, uint8_t value) {
    MBC_State* mbc = (MBC_State*)mem->mbc_data;
    
    if (addr < 0x2000) {
        /* RAM Enable (lower 4 bits must be 0xA) */
        if (!(addr & 0x0100)) {
            mbc->ram_enabled = ((value & 0x0F) == 0x0A);
        }
    }
    else if (addr < 0x4000) {
        /* ROM Bank Number */
        if (addr & 0x0100) {
            uint8_t bank = value & 0x0F;
            mbc->current_rom_bank = bank ? bank : 1;
        }
    }
    else if (addr >= 0xA000 && addr < 0xA200) {
        /* RAM Write */
        if (mbc->ram_enabled) {
            mbc->ram_data[addr - 0xA000] = value & 0x0F;
        }
    }
}

/* MBC3 Implementation with RTC */
static void rtc_update(RTC_Data* rtc) {
    if (rtc->halt) return;
    
    time_t current_time = time(NULL);
    time_t diff = current_time - rtc->last_time;
    rtc->last_time = current_time;
    
    /* Update RTC registers */
    uint32_t total_seconds = rtc->seconds + diff;
    rtc->seconds = total_seconds % 60;
    
    uint32_t total_minutes = (total_seconds / 60) + rtc->minutes;
    rtc->minutes = total_minutes % 60;
    
    uint32_t total_hours = (total_minutes / 60) + rtc->hours;
    rtc->hours = total_hours % 24;
    
    uint32_t total_days = (total_hours / 24) + rtc->days;
    rtc->days = total_days % 512;
}

static uint8_t mbc3_read(Memory* mem, uint16_t addr) {
    MBC_State* mbc = (MBC_State*)mem->mbc_data;
    
    if (addr < 0x4000) {
        return mbc->rom_data[addr];
    }
    else if (addr < 0x8000) {
        uint32_t rom_addr = (addr - 0x4000) + (mbc->current_rom_bank * 0x4000);
        return mbc->rom_data[rom_addr];
    }
    else if (addr >= 0xA000 && addr < 0xC000) {
        if (mbc->ram_enabled) {
            if (mbc->current_ram_bank <= 0x03) {
                /* RAM access */
                uint32_t ram_addr = (addr - 0xA000) + (mbc->current_ram_bank * 0x2000);
                return mbc->ram_data[ram_addr];
            }
            else {
                /* RTC register access */
                RTC_Data* rtc = (RTC_Data*)mbc->rtc_data;
                rtc_update(rtc);
                switch (mbc->current_ram_bank) {
                    case 0x08: return rtc->seconds;
                    case 0x09: return rtc->minutes;
                    case 0x0A: return rtc->hours;
                    case 0x0B: return rtc->days & 0xFF;
                    case 0x0C: return ((rtc->days >> 8) & 0x01) | (rtc->halt ? 0x40 : 0x00);
                }
            }
        }
    }
    return 0xFF;
}

static void mbc3_write(Memory* mem, uint16_t addr, uint8_t value) {
    MBC_State* mbc = (MBC_State*)mem->mbc_data;
    
    if (addr < 0x2000) {
        /* RAM/Timer Enable */
        mbc->ram_enabled = ((value & 0x0F) == 0x0A);
    }
    else if (addr < 0x4000) {
        /* ROM Bank Number */
        uint8_t bank = value & 0x7F;
        mbc->current_rom_bank = bank ? bank : 1;
    }
    else if (addr < 0x6000) {
        /* RAM Bank Number or RTC Register Select */
        mbc->current_ram_bank = value;
    }
    else if (addr < 0x8000) {
        /* Latch Clock Data */
        if (mbc->rtc_data) {
            static uint8_t latch_state = 0;
            if (latch_state == 0 && value == 0) latch_state = 1;
            else if (latch_state == 1 && value == 1) {
                RTC_Data* rtc = (RTC_Data*)mbc->rtc_data;
                rtc_update(rtc);
                latch_state = 0;
            }
        }
    }
    else if (addr >= 0xA000 && addr < 0xC000) {
        if (mbc->ram_enabled) {
            if (mbc->current_ram_bank <= 0x03) {
                /* RAM write */
                uint32_t ram_addr = (addr - 0xA000) + (mbc->current_ram_bank * 0x2000);
                mbc->ram_data[ram_addr] = value;
            }
            else {
                /* RTC register write */
                RTC_Data* rtc = (RTC_Data*)mbc->rtc_data;
                switch (mbc->current_ram_bank) {
                    case 0x08: rtc->seconds = value % 60; break;
                    case 0x09: rtc->minutes = value % 60; break;
                    case 0x0A: rtc->hours = value % 24; break;
                    case 0x0B: rtc->days = (rtc->days & 0x100) | value; break;
                    case 0x0C:
                        rtc->days = (rtc->days & 0xFF) | ((value & 0x01) << 8);
                        rtc->halt = (value & 0x40) != 0;
                        break;
                }
            }
        }
    }
}

/* MBC5 Implementation */
static uint8_t mbc5_read(Memory* mem, uint16_t addr) {
    MBC_State* mbc = (MBC_State*)mem->mbc_data;
    
    if (addr < 0x4000) {
        return mbc->rom_data[addr];
    }
    else if (addr < 0x8000) {
        uint32_t rom_addr = (addr - 0x4000) + (mbc->current_rom_bank * 0x4000);
        return mbc->rom_data[rom_addr];
    }
    else if (addr >= 0xA000 && addr < 0xC000) {
        if (mbc->ram_enabled && mbc->ram_data) {
            uint32_t ram_addr = (addr - 0xA000) + (mbc->current_ram_bank * 0x2000);
            return mbc->ram_data[ram_addr];
        }
    }
    return 0xFF;
}

static void mbc5_write(Memory* mem, uint16_t addr, uint8_t value) {
    MBC_State* mbc = (MBC_State*)mem->mbc_data;
    
    if (addr < 0x2000) {
        /* RAM Enable */
        mbc->ram_enabled = ((value & 0x0F) == 0x0A);
    }
    else if (addr < 0x3000) {
        /* Lower 8 bits of ROM Bank Number */
        mbc->current_rom_bank = (mbc->current_rom_bank & 0x100) | value;
    }
    else if (addr < 0x4000) {
        /* 9th bit of ROM Bank Number */
        mbc->current_rom_bank = (mbc->current_rom_bank & 0xFF) | ((value & 0x01) << 8);
    }
    else if (addr < 0x6000) {
        /* RAM Bank Number */
        mbc->current_ram_bank = value & 0x0F;
    }
    else if (addr >= 0xA000 && addr < 0xC000) {
        if (mbc->ram_enabled && mbc->ram_data) {
            uint32_t ram_addr = (addr - 0xA000) + (mbc->current_ram_bank * 0x2000);
            mbc->ram_data[ram_addr] = value;
        }
    }
}

/* MBC Read/Write function pointers */
typedef uint8_t (*mbc_read_fn)(Memory*, uint16_t);
typedef void (*mbc_write_fn)(Memory*, uint16_t, uint8_t);

__attribute__((unused)) static const struct {
    mbc_read_fn read;
    mbc_write_fn write;
} mbc_handlers[] = {
    { NULL, NULL },           /* ROM_ONLY */
    { mbc1_read, mbc1_write },
    { mbc2_read, mbc2_write },
    { mbc3_read, mbc3_write },
    { mbc3_read, mbc3_write }, /* MBC3_TIMER */
    { mbc5_read, mbc5_write },
    { mbc5_read, mbc5_write }  /* MBC5_RUMBLE */
};