#ifndef MEMORY_H
#define MEMORY_H

#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>

/* Forward declaration - PPU is defined in ppu.h */
/* We can't include ppu.h here due to circular dependency, so use void* and cast */

/* CGB Speed modes */
typedef enum {
    SPEED_NORMAL,
    SPEED_DOUBLE
} Speed_Mode;

#define CGB_SPEED_SWITCH_DELAY 2048  /* Number of cycles for speed switch */

/* Memory map constants */
#define ROM_BANK_0_START     0x0000
#define ROM_BANK_0_END       0x3FFF
#define ROM_BANK_N_START     0x4000
#define ROM_BANK_N_END       0x7FFF
#define VRAM_START          0x8000
#define VRAM_END            0x9FFF
#define EXT_RAM_START       0xA000
#define EXT_RAM_END         0xBFFF
#define WRAM_START          0xC000
#define WRAM_END            0xDFFF
#define ECHO_START          0xE000
#define ECHO_END            0xFDFF
#define OAM_START          0xFE00
#define OAM_END            0xFE9F
#define UNUSED_START       0xFEA0
#define UNUSED_END         0xFEFF
#define IO_START           0xFF00
#define IO_END             0xFF7F
#define HRAM_START         0xFF80
#define HRAM_END           0xFFFE
#define IE_REGISTER        0xFFFF

/* Memory access timing in T-cycles (1 T-cycle = 1/4 CPU clock) */
#define VRAM_ACCESS_TIME    2
#define WRAM_ACCESS_TIME    2
#define OAM_ACCESS_TIME     2
#define ROM_ACCESS_TIME     4
#define EXT_RAM_ACCESS_TIME 4
#define HRAM_ACCESS_TIME    1

/* Memory Controller Types */
typedef enum {
    ROM_ONLY,
    MBC1,
    MBC2,
    MBC3,
    MBC3_TIMER,
    MBC5,
    MBC5_RUMBLE,
    MBC6,
    MBC7,
    MMM01,
    POCKET_CAMERA
} MBC_Type;

/* CGB Speed Mode */
/* Memory timing defines */
#define CGB_SPEED_SWITCH_DELAY 2048 /* Number of cycles for speed switch */

/* RTC registers for MBC3 */
typedef struct {
    uint8_t seconds;    /* 0-59 */
    uint8_t minutes;    /* 0-59 */
    uint8_t hours;      /* 0-23 */
    uint16_t days;      /* 0-511 */
    bool halt;          /* RTC halt flag */
    time_t last_time;   /* Last update time */
} RTC_Data;

typedef struct Memory {
    /* Memory regions */
    uint8_t* rom_bank0;     /* 16KB ROM bank #0 */
    uint8_t* rom_bankn;     /* 16KB ROM bank #n */
    uint8_t* vram;          /* 8KB Video RAM */
    uint8_t* ext_ram;       /* 8KB External RAM */
    uint8_t* wram;          /* 8KB Work RAM */
    uint8_t* oam;           /* Object Attribute Memory */
    uint8_t* hram;          /* High RAM */
    uint8_t io_registers[0x80];
    uint8_t ie_register;    /* Interrupt Enable register */

    /* Pointer for MBC-specific state (malloc'd by memory_load_rom) */
    void* mbc_data;

    /* Joypad internal state */
    uint8_t joypad_state_buttons; /* bits: A, B, Select, Start (0..3) -> 1 = pressed */
    uint8_t joypad_state_dirs;    /* bits: Right, Left, Up, Down (0..3) -> 1 = pressed */

    /* Timer state (DIV/TIMA/TMA/TAC) */
    uint16_t div_internal;  /* 16-bit internal divider counter (increments each CPU cycle) */
    uint8_t div;            /* Visible DIV register (upper 8 bits of div_internal) */
    uint8_t tima;           /* TIMA (0xFF05) - mirrored here for convenience */
    uint8_t tma;            /* TMA (0xFF06) */
    uint8_t tac;            /* TAC (0xFF07) */
    bool timer_enabled;     /* Derived from TAC bit 2 */
    uint8_t tima_reload_delay; /* Delay counter for TIMA reload after overflow (cycles) */
    bool tima_reload_pending;  /* TIMA is pending reload from TMA */
    uint8_t last_timer_bit; /* Last sampled value of the selected divider bit (0/1) */

    /* Memory banking */
    MBC_Type mbc_type;
    uint8_t current_rom_bank;
    uint8_t current_ram_bank;
    bool ram_enabled;
    bool rom_banking_enabled;
    
    /* Back-reference to PPU for VRAM/OAM access restrictions (void* to avoid circular dependency) */
    void* ppu;
    /* Back-reference to APU for audio register access (void* to avoid circular dependency) */
    void* apu;
} Memory;

/* MBC state structure (used by MBC handlers)
   Defined here so MBC implementation files can access the concrete state */
typedef struct {
    uint8_t* rom_data;          /* Full ROM data */
    uint8_t* ram_data;          /* Full RAM data */
    size_t rom_size;            /* Total ROM size */
    size_t ram_size;            /* Total RAM size */
    uint16_t rom_bank_count;    /* Number of ROM banks */
    uint8_t ram_bank_count;     /* Number of RAM banks */
    uint8_t current_rom_bank;   /* Current ROM bank number */
    uint8_t current_ram_bank;   /* Current RAM bank number */
    /* Additional banks/fields used by extended MBC implementations */
    uint8_t current_rom_bank2;  /* Secondary ROM bank region (MBC6) */
    uint8_t current_ram_bank2;  /* Secondary RAM bank region (MBC6) */
    bool ram_enabled;           /* RAM access enabled flag */
    bool rom_banking_enabled;   /* ROM banking enabled flag */
    uint8_t banking_mode;       /* Current banking mode */
    /* Optional pointer to RTC data for MBC3 carts (allocated when needed) */
    RTC_Data* rtc_data;
    /* Optional pointer for MBC-specific extra data (e.g. MBC7, Pocket Camera) */
    void* extra_data;
    /* Fields used by some extended MBCs (MMM01/MBC variants) */
    uint8_t base_rom_bank;
    uint8_t rom_bank_mask;
} MBC_State;

/* Save state structure version for compatibility */
#define SAVE_STATE_VERSION 1

/* Persistent save state layout used by save/load functions.
   The flexible array member at the end holds RAM contents when present. */
typedef struct {
    uint32_t version;
    MBC_Type mbc_type;
    size_t rom_size;
    size_t ram_size;
    uint8_t current_rom_bank;
    uint8_t current_ram_bank;
    bool ram_enabled;
    bool rom_banking_enabled;
    uint8_t banking_mode;
    /* Memory regions */
    uint8_t vram[0x2000];
    uint8_t wram[0x2000];
    uint8_t oam[0xA0];
    uint8_t hram[0x7F];
    uint8_t io_registers[0x80];
    uint8_t ie_register;
    /* RTC data if present */
    RTC_Data rtc;
    /* RAM data follows */
    uint8_t ram_data[];
} SaveState;

/* MBC1 function prototypes (used by other MBC implementation files) */
uint8_t mbc1_read(Memory* mem, uint16_t addr);
void mbc1_write(Memory* mem, uint16_t addr, uint8_t value);

/* Memory initialization and cleanup */
void memory_init(Memory* mem);
void memory_cleanup(Memory* mem);
void memory_reset(Memory* mem);

/* Memory access */
uint8_t memory_read(Memory* mem, uint16_t address);
void memory_write(Memory* mem, uint16_t address, uint8_t value);

/* DMA transfer */
void memory_dma_transfer(Memory* mem, uint8_t start);

/* Memory bank switching */
void memory_switch_rom_bank(Memory* mem, uint8_t bank);
void memory_switch_ram_bank(Memory* mem, uint8_t bank);

/* ROM/RAM loading */
bool memory_load_rom(Memory* mem, const char* filename);
void memory_setup_banking(Memory* mem, MBC_Type type);

/* Save/Load state */
bool memory_save_state(Memory* mem, const char* filename);
bool memory_load_state(Memory* mem, const char* filename);
bool memory_save_ram(Memory* mem, const char* filename);
bool memory_load_ram(Memory* mem, const char* filename);

/* Memory timing */
uint8_t memory_read_timed(Memory* mem, uint16_t addr, uint32_t* cycles);
void memory_write_timed(Memory* mem, uint16_t addr, uint8_t value, uint32_t* cycles);

/* Timer (DIV/TIMA/TMA/TAC) */
void memory_timer_init(Memory* mem);
void memory_timer_step(Memory* mem, uint32_t cycles);
/* Update JOYP register from internal input state */
void memory_update_joyp(Memory* mem);
/* CGB specific functions */
void memory_init_cgb(Memory* mem);
void memory_handle_speed_switch(Memory* mem);
void memory_request_speed_switch(Memory* mem);
Speed_Mode memory_get_current_speed(Memory* mem);

/* Compressed state and RAM save functions */
bool memory_save_state_compressed(Memory* mem, const char* filename);
bool memory_load_state_compressed(Memory* mem, const char* filename);
bool memory_save_ram_compressed(Memory* mem, const char* filename);
bool memory_load_ram_compressed(Memory* mem, const char* filename);

#endif /* GB_MEMORY_H */