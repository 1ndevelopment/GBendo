#include "gbendo.h"
#include <stdio.h>

/* Global debug flag - set by gb_enable_debug/gb_disable_debug */
static GBEmulator* g_debug_gb = NULL;

/* Helper function to check if debug is enabled */
bool gb_is_debug_enabled(void) {
    return g_debug_gb != NULL && g_debug_gb->debug_mode;
}

/* Helper function to get debug GB pointer (for subsystems) */
GBEmulator* gb_get_debug_gb(void) {
    return g_debug_gb;
}

void gb_init(GBEmulator* gb) {
    /* Initialize subsystems */
    memory_init(&gb->memory);
    sm83_init(&gb->cpu);
    ppu_init(&gb->ppu, &gb->memory);  /* Pass memory to PPU for interrupt requests */
    apu_init(&gb->apu);

    /* Wire CPU <-> Memory */
    gb->cpu.mem = &gb->memory;
    
    /* Wire PPU <-> Memory (bidirectional) */
    gb->ppu.memory = &gb->memory;
    gb->memory.ppu = &gb->ppu;
    
    /* Wire APU <-> Memory */
    gb->memory.apu = &gb->apu;

    /* Mark initial state */
    gb->cycles = 0;
    gb->frame_complete = false;
    gb->debug_mode = false;
}

void gb_reset(GBEmulator* gb) {
    sm83_reset(&gb->cpu);
    memory_reset(&gb->memory);
    ppu_reset(&gb->ppu);
    apu_reset(&gb->apu);
    gb->cycles = 0;
    gb->frame_complete = false;
}

void gb_cleanup(GBEmulator* gb) {
    memory_cleanup(&gb->memory);
}

bool gb_load_rom(GBEmulator* gb, const char* filename) {
    return memory_load_rom(&gb->memory, filename);
}

void gb_unload_rom(GBEmulator* gb) {
    /* Reset emulator state to stop any running processes */
    gb->cycles = 0;
    gb->frame_complete = false;
    
    /* Reset subsystems first to ensure they stop accessing memory */
    sm83_reset(&gb->cpu);
    ppu_reset(&gb->ppu);
    apu_reset(&gb->apu);
    
    /* Just reset pointers - DON'T free memory here!
     * The memory will be freed either by:
     * 1. memory_load_rom() when loading a new ROM, or
     * 2. memory_cleanup() when the emulator shuts down
     * 
     * Freeing here causes double-free bugs because memory_cleanup()
     * will also try to free this memory.
     */
    gb->memory.rom_bank0 = NULL;
    gb->memory.rom_bankn = NULL;
    gb->memory.ext_ram = NULL;
    gb->memory.mbc_type = ROM_ONLY;
    
    /* Note: mbc_data is intentionally NOT freed or set to NULL here.
     * It will be cleaned up properly by memory_cleanup() or memory_load_rom(). */
}

void gb_run_frame(GBEmulator* gb) {
    /* Simple runner: step CPU until a frame is complete (ppu.frame_ready)
     * For now, run a fixed number of cycles approximating one frame.
     */
    const uint32_t cycles_per_frame = 70224; /* DMG cycles per frame */
    uint32_t target = gb->cycles + cycles_per_frame;
    while (gb->cycles < target) {
        int cyc = sm83_step(&gb->cpu);
        if (cyc <= 0) break;
        gb->cycles += cyc;
        
        /* Step PPU with the same cycles */
        ppu_step(&gb->ppu, cyc);
        
        /* Step APU with the same cycles */
        apu_step(&gb->apu, cyc);
    }
    gb->frame_complete = true;
}

void gb_run_frame_optimized(GBEmulator* gb) {
    /* Optimized frame execution with batched operations */
    const uint32_t cycles_per_frame = 70224; /* DMG cycles per frame */
    uint32_t target = gb->cycles + cycles_per_frame;
    uint32_t batch_cycles = 0;
    
    /* Process instructions in batches to reduce function call overhead */
    while (gb->cycles < target) {
        /* Process up to 16 instructions before updating subsystems */
        const int batch_size = 16;
        batch_cycles = 0;
        
        for (int i = 0; i < batch_size && gb->cycles < target; i++) {
            int cyc = sm83_step(&gb->cpu);
            if (cyc <= 0) break;
            
            gb->cycles += cyc;
            batch_cycles += cyc;
        }
        
        /* Update subsystems once per batch */
        if (batch_cycles > 0) {
            ppu_step(&gb->ppu, batch_cycles);
            apu_step(&gb->apu, batch_cycles);
        }
    }
    gb->frame_complete = true;
}

void gb_step(GBEmulator* gb) {
    int cyc = sm83_step(&gb->cpu);
    if (cyc > 0) {
        gb->cycles += cyc;
        
        /* Step PPU with the same cycles */
        ppu_step(&gb->ppu, cyc);
        
        /* Step APU with the same cycles */
        apu_step(&gb->apu, cyc);
    }
}

void gb_pause(GBEmulator* gb) { (void)gb; }
void gb_resume(GBEmulator* gb) { (void)gb; }

void gb_enable_debug(GBEmulator* gb) {
    gb->debug_mode = true;
    g_debug_gb = gb;
}

void gb_disable_debug(GBEmulator* gb) {
    gb->debug_mode = false;
    if (g_debug_gb == gb) {
        g_debug_gb = NULL;
    }
}

void gb_set_breakpoint(GBEmulator* gb, uint16_t address) {
    gb->breakpoint = address;
}

void gb_clear_breakpoint(GBEmulator* gb) {
    gb->breakpoint = 0;
}

/* Save state version for compatibility checking */
#define GB_SAVE_STATE_VERSION 1

/* Complete emulator save state structure */
typedef struct {
    uint32_t version;
    
    /* CPU state */
    struct {
        uint16_t af;
        uint16_t bc;
        uint16_t de;
        uint16_t hl;
        uint16_t sp;
        uint16_t pc;
        bool ime;
        bool ei_delay;
        bool halted;
        bool stopped;
        uint32_t cycles;
    } cpu;
    
    /* PPU state */
    struct {
        uint8_t lcdc;
        uint8_t stat;
        uint8_t scy;
        uint8_t scx;
        uint8_t ly;
        uint8_t lyc;
        uint8_t bgp;
        uint8_t obp0;
        uint8_t obp1;
        uint8_t wy;
        uint8_t wx;
        uint8_t bgpi;
        uint8_t obpi;
        uint8_t bgpd[64];
        uint8_t obpd[64];
        uint8_t mode;
        uint32_t clock;
        uint32_t line_cycles;
        bool frame_ready;
        bool cgb_mode;
        uint8_t vram_bank;
        uint8_t vram[2][0x2000];
        uint8_t oam[160];
        bool hdma_active;
        bool hdma_hblank;
        uint16_t hdma_source;
        uint16_t hdma_dest;
        uint16_t hdma_remaining;
    } ppu;
    
    /* APU state */
    struct {
        /* Pulse channels */
        struct {
            bool enabled;
            uint8_t volume;
            uint16_t frequency;
            bool counter_selection;
            uint16_t length_timer;
            uint8_t duty;
            uint8_t duty_position;
            uint16_t frequency_timer;
            uint8_t initial_volume;
            bool envelope_increase;
            uint8_t envelope_period;
            uint8_t envelope_timer;
            uint8_t sweep_period;
            bool sweep_decrease;
            uint8_t sweep_shift;
            uint8_t sweep_timer;
        } pulse1, pulse2;
        
        /* Wave channel */
        struct {
            bool enabled;
            uint8_t volume;
            uint16_t frequency;
            uint16_t length_timer;
            uint16_t frequency_timer;
            uint8_t wave_position;
            uint8_t wave_pattern[32];
            bool wave_table_enabled;
        } wave;
        
        /* Noise channel */
        struct {
            bool enabled;
            uint8_t volume;
            uint8_t divisor_code;
            uint8_t width_mode;
            uint8_t clock_shift;
            uint16_t length_timer;
            uint16_t frequency_timer;
            uint16_t lfsr;
            uint8_t initial_volume;
            bool envelope_increase;
            uint8_t envelope_period;
            uint8_t envelope_timer;
        } noise;
        
        bool power;
        uint8_t left_volume;
        uint8_t right_volume;
        uint8_t left_enables;
        uint8_t right_enables;
        int32_t sample_timer;
        uint32_t frame_sequencer;
    } apu;
    
    /* Emulator timing state */
    uint32_t cycles;
    bool frame_complete;
} GB_SaveState;

/* Helper macros for save state serialization */
#define SAVE_FIELD(dst, src, field) ((dst).field = (src).field)
#define SAVE_ARRAY(dst, src, field) memcpy((dst).field, (src).field, sizeof((dst).field))

/* Helper macros for save state deserialization */
#define LOAD_FIELD(dst, src, field) ((dst).field = (src).field)
#define LOAD_ARRAY(dst, src, field) memcpy((dst).field, (src).field, sizeof((dst).field))


/* Helper function to save APU pulse channel state */
static inline void save_apu_pulse_channel(void* dst_channel, const void* src_channel, bool has_sweep) {
    typedef struct {
        bool enabled;
        uint8_t volume;
        uint16_t frequency;
        bool counter_selection;
        uint16_t length_timer;
        uint8_t duty;
        uint8_t duty_position;
        uint16_t frequency_timer;
        uint8_t initial_volume;
        bool envelope_increase;
        uint8_t envelope_period;
        uint8_t envelope_timer;
        uint8_t sweep_period;
        bool sweep_decrease;
        uint8_t sweep_shift;
        uint8_t sweep_timer;
    } PulseChannel;
    
    const PulseChannel* src = (const PulseChannel*)src_channel;
    PulseChannel* dst = (PulseChannel*)dst_channel;
    
    dst->enabled = src->enabled;
    dst->volume = src->volume;
    dst->frequency = src->frequency;
    dst->counter_selection = src->counter_selection;
    dst->length_timer = src->length_timer;
    dst->duty = src->duty;
    dst->duty_position = src->duty_position;
    dst->frequency_timer = src->frequency_timer;
    dst->initial_volume = src->initial_volume;
    dst->envelope_increase = src->envelope_increase;
    dst->envelope_period = src->envelope_period;
    dst->envelope_timer = src->envelope_timer;
    
    if (has_sweep) {
        dst->sweep_period = src->sweep_period;
        dst->sweep_decrease = src->sweep_decrease;
        dst->sweep_shift = src->sweep_shift;
        dst->sweep_timer = src->sweep_timer;
    }
}

/* Helper function to load APU pulse channel state */
static inline void load_apu_pulse_channel(void* dst_channel, const void* src_channel, bool has_sweep) {
    typedef struct {
        bool enabled;
        uint8_t volume;
        uint16_t frequency;
        bool counter_selection;
        uint16_t length_timer;
        uint8_t duty;
        uint8_t duty_position;
        uint16_t frequency_timer;
        uint8_t initial_volume;
        bool envelope_increase;
        uint8_t envelope_period;
        uint8_t envelope_timer;
        uint8_t sweep_period;
        bool sweep_decrease;
        uint8_t sweep_shift;
        uint8_t sweep_timer;
    } PulseChannel;
    
    PulseChannel* dst = (PulseChannel*)dst_channel;
    const PulseChannel* src = (const PulseChannel*)src_channel;
    
    dst->enabled = src->enabled;
    dst->volume = src->volume;
    dst->frequency = src->frequency;
    dst->counter_selection = src->counter_selection;
    dst->length_timer = src->length_timer;
    dst->duty = src->duty;
    dst->duty_position = src->duty_position;
    dst->frequency_timer = src->frequency_timer;
    dst->initial_volume = src->initial_volume;
    dst->envelope_increase = src->envelope_increase;
    dst->envelope_period = src->envelope_period;
    dst->envelope_timer = src->envelope_timer;
    
    if (has_sweep) {
        dst->sweep_period = src->sweep_period;
        dst->sweep_decrease = src->sweep_decrease;
        dst->sweep_shift = src->sweep_shift;
        dst->sweep_timer = src->sweep_timer;
    }
}


bool gb_save_state(GBEmulator* gb, const char* filename) {
    if (!gb || !filename) return false;
    
    FILE* file = fopen(filename, "wb");
    if (!file) {
        fprintf(stderr, "Failed to open save state file for writing: %s\n", filename);
        return false;
    }
    
    GB_SaveState state = {0};
    state.version = GB_SAVE_STATE_VERSION;
    
    /* Save CPU state */
    SAVE_FIELD(state.cpu, gb->cpu, af);
    SAVE_FIELD(state.cpu, gb->cpu, bc);
    SAVE_FIELD(state.cpu, gb->cpu, de);
    SAVE_FIELD(state.cpu, gb->cpu, hl);
    SAVE_FIELD(state.cpu, gb->cpu, sp);
    SAVE_FIELD(state.cpu, gb->cpu, pc);
    SAVE_FIELD(state.cpu, gb->cpu, ime);
    SAVE_FIELD(state.cpu, gb->cpu, ei_delay);
    SAVE_FIELD(state.cpu, gb->cpu, halted);
    SAVE_FIELD(state.cpu, gb->cpu, stopped);
    SAVE_FIELD(state.cpu, gb->cpu, cycles);
    
    /* Save PPU state */
    SAVE_FIELD(state.ppu, gb->ppu, lcdc);
    SAVE_FIELD(state.ppu, gb->ppu, stat);
    SAVE_FIELD(state.ppu, gb->ppu, scy);
    SAVE_FIELD(state.ppu, gb->ppu, scx);
    SAVE_FIELD(state.ppu, gb->ppu, ly);
    SAVE_FIELD(state.ppu, gb->ppu, lyc);
    SAVE_FIELD(state.ppu, gb->ppu, bgp);
    SAVE_FIELD(state.ppu, gb->ppu, obp0);
    SAVE_FIELD(state.ppu, gb->ppu, obp1);
    SAVE_FIELD(state.ppu, gb->ppu, wy);
    SAVE_FIELD(state.ppu, gb->ppu, wx);
    SAVE_FIELD(state.ppu, gb->ppu, bgpi);
    SAVE_FIELD(state.ppu, gb->ppu, obpi);
    SAVE_ARRAY(state.ppu, gb->ppu, bgpd);
    SAVE_ARRAY(state.ppu, gb->ppu, obpd);
    state.ppu.mode = (uint8_t)gb->ppu.mode;
    SAVE_FIELD(state.ppu, gb->ppu, clock);
    SAVE_FIELD(state.ppu, gb->ppu, line_cycles);
    SAVE_FIELD(state.ppu, gb->ppu, frame_ready);
    SAVE_FIELD(state.ppu, gb->ppu, cgb_mode);
    SAVE_FIELD(state.ppu, gb->ppu, vram_bank);
    SAVE_ARRAY(state.ppu, gb->ppu, vram);
    SAVE_ARRAY(state.ppu, gb->ppu, oam);
    SAVE_FIELD(state.ppu, gb->ppu, hdma_active);
    SAVE_FIELD(state.ppu, gb->ppu, hdma_hblank);
    SAVE_FIELD(state.ppu, gb->ppu, hdma_source);
    SAVE_FIELD(state.ppu, gb->ppu, hdma_dest);
    SAVE_FIELD(state.ppu, gb->ppu, hdma_remaining);
    
    /* Save APU state using helper functions */
    save_apu_pulse_channel(&state.apu.pulse1, &gb->apu.pulse1, true);  /* pulse1 has sweep */
    save_apu_pulse_channel(&state.apu.pulse2, &gb->apu.pulse2, false); /* pulse2 no sweep */
    
    /* Save wave channel */
    SAVE_FIELD(state.apu.wave, gb->apu.wave, enabled);
    SAVE_FIELD(state.apu.wave, gb->apu.wave, volume);
    SAVE_FIELD(state.apu.wave, gb->apu.wave, frequency);
    SAVE_FIELD(state.apu.wave, gb->apu.wave, length_timer);
    SAVE_FIELD(state.apu.wave, gb->apu.wave, frequency_timer);
    SAVE_FIELD(state.apu.wave, gb->apu.wave, wave_position);
    SAVE_ARRAY(state.apu.wave, gb->apu.wave, wave_pattern);
    SAVE_FIELD(state.apu.wave, gb->apu.wave, wave_table_enabled);
    
    /* Save noise channel */
    SAVE_FIELD(state.apu.noise, gb->apu.noise, enabled);
    SAVE_FIELD(state.apu.noise, gb->apu.noise, volume);
    SAVE_FIELD(state.apu.noise, gb->apu.noise, divisor_code);
    SAVE_FIELD(state.apu.noise, gb->apu.noise, width_mode);
    SAVE_FIELD(state.apu.noise, gb->apu.noise, clock_shift);
    SAVE_FIELD(state.apu.noise, gb->apu.noise, length_timer);
    SAVE_FIELD(state.apu.noise, gb->apu.noise, frequency_timer);
    SAVE_FIELD(state.apu.noise, gb->apu.noise, lfsr);
    SAVE_FIELD(state.apu.noise, gb->apu.noise, initial_volume);
    SAVE_FIELD(state.apu.noise, gb->apu.noise, envelope_increase);
    SAVE_FIELD(state.apu.noise, gb->apu.noise, envelope_period);
    SAVE_FIELD(state.apu.noise, gb->apu.noise, envelope_timer);
    
    /* Save APU global state */
    SAVE_FIELD(state.apu, gb->apu, power);
    SAVE_FIELD(state.apu, gb->apu, left_volume);
    SAVE_FIELD(state.apu, gb->apu, right_volume);
    SAVE_FIELD(state.apu, gb->apu, left_enables);
    SAVE_FIELD(state.apu, gb->apu, right_enables);
    SAVE_FIELD(state.apu, gb->apu, sample_timer);
    SAVE_FIELD(state.apu, gb->apu, frame_sequencer);
    
    /* Save emulator timing state */
    state.cycles = gb->cycles;
    state.frame_complete = gb->frame_complete;
    
    /* Write state structure */
    size_t written = fwrite(&state, 1, sizeof(GB_SaveState), file);
    if (written != sizeof(GB_SaveState)) {
        fclose(file);
        fprintf(stderr, "Failed to write save state data\n");
        return false;
    }
    
    /* Write memory state using existing function */
    fclose(file);
    
    /* Build memory state filename (append .mem to the save state file) */
    char mem_filename[2048];
    snprintf(mem_filename, sizeof(mem_filename), "%s.mem", filename);
    
    if (!memory_save_state(&gb->memory, mem_filename)) {
        fprintf(stderr, "Failed to save memory state\n");
        return false;
    }
    
    printf("Save state written to: %s\n", filename);
    return true;
}

bool gb_load_state(GBEmulator* gb, const char* filename) {
    if (!gb || !filename) return false;
    
    FILE* file = fopen(filename, "rb");
    if (!file) {
        fprintf(stderr, "Failed to open save state file for reading: %s\n", filename);
        return false;
    }
    
    GB_SaveState state;
    size_t read = fread(&state, 1, sizeof(GB_SaveState), file);
    fclose(file);
    
    if (read != sizeof(GB_SaveState)) {
        fprintf(stderr, "Failed to read save state data\n");
        return false;
    }
    
    /* Verify version compatibility */
    if (state.version != GB_SAVE_STATE_VERSION) {
        fprintf(stderr, "Incompatible save state version: %u (expected %u)\n", 
                state.version, GB_SAVE_STATE_VERSION);
        return false;
    }
    
    /* Preserve critical pointer references that must not be overwritten */
    void* saved_cpu_mem = gb->cpu.mem;
    Memory* saved_ppu_memory = gb->ppu.memory;
    
    /* Restore CPU state */
    LOAD_FIELD(gb->cpu, state.cpu, af);
    LOAD_FIELD(gb->cpu, state.cpu, bc);
    LOAD_FIELD(gb->cpu, state.cpu, de);
    LOAD_FIELD(gb->cpu, state.cpu, hl);
    LOAD_FIELD(gb->cpu, state.cpu, sp);
    LOAD_FIELD(gb->cpu, state.cpu, pc);
    LOAD_FIELD(gb->cpu, state.cpu, ime);
    LOAD_FIELD(gb->cpu, state.cpu, ei_delay);
    LOAD_FIELD(gb->cpu, state.cpu, halted);
    LOAD_FIELD(gb->cpu, state.cpu, stopped);
    LOAD_FIELD(gb->cpu, state.cpu, cycles);
    
    /* Restore CPU pointer */
    gb->cpu.mem = saved_cpu_mem;
    
    /* Restore PPU state */
    LOAD_FIELD(gb->ppu, state.ppu, lcdc);
    LOAD_FIELD(gb->ppu, state.ppu, stat);
    LOAD_FIELD(gb->ppu, state.ppu, scy);
    LOAD_FIELD(gb->ppu, state.ppu, scx);
    LOAD_FIELD(gb->ppu, state.ppu, ly);
    LOAD_FIELD(gb->ppu, state.ppu, lyc);
    LOAD_FIELD(gb->ppu, state.ppu, bgp);
    LOAD_FIELD(gb->ppu, state.ppu, obp0);
    LOAD_FIELD(gb->ppu, state.ppu, obp1);
    LOAD_FIELD(gb->ppu, state.ppu, wy);
    LOAD_FIELD(gb->ppu, state.ppu, wx);
    LOAD_FIELD(gb->ppu, state.ppu, bgpi);
    LOAD_FIELD(gb->ppu, state.ppu, obpi);
    LOAD_ARRAY(gb->ppu, state.ppu, bgpd);
    LOAD_ARRAY(gb->ppu, state.ppu, obpd);
    gb->ppu.mode = (PPU_Mode)state.ppu.mode;
    LOAD_FIELD(gb->ppu, state.ppu, clock);
    LOAD_FIELD(gb->ppu, state.ppu, line_cycles);
    LOAD_FIELD(gb->ppu, state.ppu, frame_ready);
    LOAD_FIELD(gb->ppu, state.ppu, cgb_mode);
    LOAD_FIELD(gb->ppu, state.ppu, vram_bank);
    LOAD_ARRAY(gb->ppu, state.ppu, vram);
    LOAD_ARRAY(gb->ppu, state.ppu, oam);
    LOAD_FIELD(gb->ppu, state.ppu, hdma_active);
    LOAD_FIELD(gb->ppu, state.ppu, hdma_hblank);
    LOAD_FIELD(gb->ppu, state.ppu, hdma_source);
    LOAD_FIELD(gb->ppu, state.ppu, hdma_dest);
    LOAD_FIELD(gb->ppu, state.ppu, hdma_remaining);
    
    /* Restore PPU pointer */
    gb->ppu.memory = saved_ppu_memory;
    
    /* Restore APU state using helper functions */
    load_apu_pulse_channel(&gb->apu.pulse1, &state.apu.pulse1, true);  /* pulse1 has sweep */
    load_apu_pulse_channel(&gb->apu.pulse2, &state.apu.pulse2, false); /* pulse2 no sweep */
    
    /* Restore wave channel */
    LOAD_FIELD(gb->apu.wave, state.apu.wave, enabled);
    LOAD_FIELD(gb->apu.wave, state.apu.wave, volume);
    LOAD_FIELD(gb->apu.wave, state.apu.wave, frequency);
    LOAD_FIELD(gb->apu.wave, state.apu.wave, length_timer);
    LOAD_FIELD(gb->apu.wave, state.apu.wave, frequency_timer);
    LOAD_FIELD(gb->apu.wave, state.apu.wave, wave_position);
    LOAD_ARRAY(gb->apu.wave, state.apu.wave, wave_pattern);
    LOAD_FIELD(gb->apu.wave, state.apu.wave, wave_table_enabled);
    
    /* Restore noise channel */
    LOAD_FIELD(gb->apu.noise, state.apu.noise, enabled);
    LOAD_FIELD(gb->apu.noise, state.apu.noise, volume);
    LOAD_FIELD(gb->apu.noise, state.apu.noise, divisor_code);
    LOAD_FIELD(gb->apu.noise, state.apu.noise, width_mode);
    LOAD_FIELD(gb->apu.noise, state.apu.noise, clock_shift);
    LOAD_FIELD(gb->apu.noise, state.apu.noise, length_timer);
    LOAD_FIELD(gb->apu.noise, state.apu.noise, frequency_timer);
    LOAD_FIELD(gb->apu.noise, state.apu.noise, lfsr);
    LOAD_FIELD(gb->apu.noise, state.apu.noise, initial_volume);
    LOAD_FIELD(gb->apu.noise, state.apu.noise, envelope_increase);
    LOAD_FIELD(gb->apu.noise, state.apu.noise, envelope_period);
    LOAD_FIELD(gb->apu.noise, state.apu.noise, envelope_timer);
    
    /* Restore APU global state */
    LOAD_FIELD(gb->apu, state.apu, power);
    LOAD_FIELD(gb->apu, state.apu, left_volume);
    LOAD_FIELD(gb->apu, state.apu, right_volume);
    LOAD_FIELD(gb->apu, state.apu, left_enables);
    LOAD_FIELD(gb->apu, state.apu, right_enables);
    LOAD_FIELD(gb->apu, state.apu, sample_timer);
    LOAD_FIELD(gb->apu, state.apu, frame_sequencer);
    
    /* Restore emulator timing state */
    gb->cycles = state.cycles;
    gb->frame_complete = state.frame_complete;
    
    /* Preserve Memory struct pointers before loading memory state */
    void* saved_mem_ppu = gb->memory.ppu;
    void* saved_mem_apu = gb->memory.apu;
    
    /* Load memory state using existing function 
       Note: memory_load_state preserves the ppu and apu pointers in Memory struct */
    char mem_filename[2048];
    snprintf(mem_filename, sizeof(mem_filename), "%s.mem", filename);
    
    if (!memory_load_state(&gb->memory, mem_filename)) {
        fprintf(stderr, "Failed to load memory state\n");
        return false;
    }
    
    /* Restore Memory struct pointers */
    gb->memory.ppu = saved_mem_ppu;
    gb->memory.apu = saved_mem_apu;
    
    printf("Save state loaded from: %s\n", filename);
    return true;
}