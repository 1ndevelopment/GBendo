#ifndef GBENDO_H
#define GBENDO_H

#include "cpu/sm83.h"
#include "memory/memory.h"
#include "ppu/ppu.h"
#include "apu/apu.h"

/* Clock Frequencies */
#define CPU_CLOCK_SPEED 4194304  /* ~4.19 MHz */
#define FRAME_RATE 59.7275       /* ~59.73 Hz */

typedef struct {
    SM83_CPU cpu;
    Memory memory;
    PPU ppu;
    APU apu;
    
    /* Timing */
    uint32_t cycles;
    bool frame_complete;
    
    /* Debug */
    bool debug_mode;
    uint32_t breakpoint;
} GBEmulator;

/* Emulator lifecycle */
void gb_init(GBEmulator* gb);
void gb_reset(GBEmulator* gb);
void gb_cleanup(GBEmulator* gb);

/* Emulation control */
void gb_run_frame(GBEmulator* gb);
void gb_run_frame_optimized(GBEmulator* gb);
void gb_step(GBEmulator* gb);
void gb_pause(GBEmulator* gb);
void gb_resume(GBEmulator* gb);

/* ROM management */
bool gb_load_rom(GBEmulator* gb, const char* filename);
void gb_unload_rom(GBEmulator* gb);

/* State management */
bool gb_save_state(GBEmulator* gb, const char* filename);
bool gb_load_state(GBEmulator* gb, const char* filename);

/* Debug interface */
void gb_set_breakpoint(GBEmulator* gb, uint16_t address);
void gb_clear_breakpoint(GBEmulator* gb);
void gb_enable_debug(GBEmulator* gb);
void gb_disable_debug(GBEmulator* gb);
bool gb_is_debug_enabled(void);
GBEmulator* gb_get_debug_gb(void);

#endif /* GBENDO_H */