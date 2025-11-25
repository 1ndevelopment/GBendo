#include "apu.h"
#include "../gbendo.h"
#include "../ui/ui.h"
#include <stdio.h>
#include <string.h>

/* Channel 1/2 duty cycle waveforms */
static const bool DUTY_WAVEFORMS[4][8] = {
    {0, 0, 0, 0, 0, 0, 0, 1},  /* 12.5% */
    {0, 0, 0, 0, 0, 0, 1, 1},  /* 25% */
    {0, 0, 0, 0, 1, 1, 1, 1},  /* 50% */
    {1, 1, 1, 1, 1, 1, 0, 0}   /* 75% */
};

/* LFSR divisor values */
static const uint8_t NOISE_DIVISORS[] = {8, 16, 32, 48, 64, 80, 96, 112};

void apu_init(APU* apu) {
    memset(apu, 0, sizeof(APU));
    /* Initialize sample timer to cycles per sample */
    apu->sample_timer = CPU_CLOCK_SPEED / SAMPLE_RATE;
    apu->power = true;
    
    /* Set default master volume (max) and enable all channels on both outputs */
    apu->left_volume = 7;
    apu->right_volume = 7;
    apu->left_enables = 0xF;   /* Enable all 4 channels on left */
    apu->right_enables = 0xF;  /* Enable all 4 channels on right */
    
    /* Set default register values */
    apu->pulse1.initial_volume = 0;
    apu->pulse1.envelope_increase = false;
    apu->pulse1.envelope_period = 0;
    apu->pulse1.duty = 2;  /* 50% duty cycle */
    apu->pulse1.duty_position = 0;
    apu->pulse1.frequency_timer = 8192;
    
    apu->pulse2.initial_volume = 0;
    apu->pulse2.envelope_increase = false;
    apu->pulse2.envelope_period = 0;
    apu->pulse2.duty = 2;  /* 50% duty cycle */
    apu->pulse2.duty_position = 0;
    apu->pulse2.frequency_timer = 8192;
    
    apu->wave.volume = 0;
    apu->wave.frequency = 0;
    apu->wave.wave_table_enabled = false;
    apu->wave.wave_position = 0;
    apu->wave.frequency_timer = 4096;
    
    apu->noise.initial_volume = 0;
    apu->noise.envelope_increase = false;
    apu->noise.envelope_period = 0;
    apu->noise.divisor_code = 0;
    apu->noise.width_mode = 0;
    apu->noise.clock_shift = 0;
    apu->noise.lfsr = 0x7FFF;
    apu->noise.frequency_timer = 8;
    
    /* Initialize wave RAM with default pattern */
    for (int i = 0; i < 32; i++) {
        apu->wave.wave_pattern[i] = 0;
    }
    
}

void apu_reset(APU* apu) {
    apu_init(apu);
}

static void update_pulse_channel(PulseChannel* ch, uint32_t cycles) {
    if (!ch->enabled) return;
    
    /* Update frequency timer */
    while (cycles > 0) {
        if (ch->frequency_timer > cycles) {
            ch->frequency_timer -= cycles;
            cycles = 0;
        } else {
            cycles -= ch->frequency_timer;
            /* Timer expired, reset and advance duty position */
            ch->frequency_timer = (2048 - ch->frequency) * 4;
            ch->duty_position = (ch->duty_position + 1) & 7;
        }
    }
}

static void update_wave_channel(WaveChannel* ch, uint32_t cycles) {
    if (!ch->enabled || !ch->wave_table_enabled) return;
    
    /* Update frequency timer */
    while (cycles > 0) {
        if (ch->frequency_timer > cycles) {
            ch->frequency_timer -= cycles;
            cycles = 0;
        } else {
            cycles -= ch->frequency_timer;
            /* Timer expired, reset and advance wave position */
            ch->frequency_timer = (2048 - ch->frequency) * 2;
            ch->wave_position = (ch->wave_position + 1) & 31;
        }
    }
}

static void update_noise_channel(NoiseChannel* ch, uint32_t cycles) {
    if (!ch->enabled) return;
    
    /* Update LFSR timer */
    while (cycles > 0) {
        if (ch->frequency_timer > cycles) {
            ch->frequency_timer -= cycles;
            cycles = 0;
        } else {
            cycles -= ch->frequency_timer;
            /* Timer expired, clock LFSR and reset timer */
            uint32_t divisor = NOISE_DIVISORS[ch->divisor_code & 7];
            ch->frequency_timer = divisor << ch->clock_shift;
            
            /* Clock LFSR */
            uint16_t bit = (ch->lfsr & 1) ^ ((ch->lfsr >> 1) & 1);
            ch->lfsr >>= 1;
            ch->lfsr |= (bit << 14);
            if (ch->width_mode) {
                ch->lfsr &= ~(1 << 6);
                ch->lfsr |= (bit << 6);
            }
        }
    }
}

void apu_step(APU* apu, uint32_t cycles) {
    if (!apu->power) return;
    
    /* Update channels */
    update_pulse_channel(&apu->pulse1, cycles);
    update_pulse_channel(&apu->pulse2, cycles);
    update_wave_channel(&apu->wave, cycles);
    update_noise_channel(&apu->noise, cycles);
    
    /* Update frame sequencer (512 Hz for length/envelope/sweep) */
    apu->frame_sequencer += cycles;
    if (apu->frame_sequencer >= FRAME_SEQUENCER_RATE) {
        apu->frame_sequencer -= FRAME_SEQUENCER_RATE;
        
        /* Clock length counters (step 0, 2, 4, 6) */
        if ((apu->frame_sequencer & 1) == 0) {
            apu_update_length_counters(apu);
        }
        
        /* Clock volume envelopes (step 7) */
        if (apu->frame_sequencer == 7) {
            apu_update_envelopes(apu);
        }
        
        /* Clock sweep (step 2, 6) */
        if ((apu->frame_sequencer & 3) == 2) {
            apu_update_sweep(apu);
        }
    }
    
    /* Generate audio samples */
    apu->sample_timer -= cycles;
    while (apu->sample_timer <= 0) {
        apu->sample_timer += CPU_CLOCK_SPEED / SAMPLE_RATE;
        apu_generate_samples(apu);
    }
}

void apu_trigger_channel(APU* apu, int channel) {
    switch (channel) {
        case 0: /* Pulse 1 */
            apu->pulse1.enabled = true;
            apu->pulse1.volume = apu->pulse1.initial_volume;
            apu->pulse1.envelope_timer = apu->pulse1.envelope_period;
            apu->pulse1.frequency_timer = (2048 - apu->pulse1.frequency) * 4;
            apu->pulse1.duty_position = 0;
            if (apu->pulse1.length_timer == 0) {
                apu->pulse1.length_timer = 64;
            }
            ui_debug_log(UI_DEBUG_APU, "[APU] Channel 1 triggered: freq=%u, vol=%u, duty=%u%%",
                       apu->pulse1.frequency, apu->pulse1.initial_volume,
                       (apu->pulse1.duty == 0) ? 12 : (apu->pulse1.duty == 1) ? 25 : 
                       (apu->pulse1.duty == 2) ? 50 : 75);
            break;
            
        case 1: /* Pulse 2 */
            apu->pulse2.enabled = true;
            apu->pulse2.volume = apu->pulse2.initial_volume;
            apu->pulse2.envelope_timer = apu->pulse2.envelope_period;
            apu->pulse2.frequency_timer = (2048 - apu->pulse2.frequency) * 4;
            apu->pulse2.duty_position = 0;
            if (apu->pulse2.length_timer == 0) {
                apu->pulse2.length_timer = 64;
            }
            ui_debug_log(UI_DEBUG_APU, "[APU] Channel 2 triggered: freq=%u, vol=%u, duty=%u%%",
                       apu->pulse2.frequency, apu->pulse2.initial_volume,
                       (apu->pulse2.duty == 0) ? 12 : (apu->pulse2.duty == 1) ? 25 : 
                       (apu->pulse2.duty == 2) ? 50 : 75);
            break;
            
        case 2: /* Wave */
            apu->wave.enabled = true;
            apu->wave.frequency_timer = (2048 - apu->wave.frequency) * 2;
            apu->wave.wave_position = 0;
            if (apu->wave.length_timer == 0) {
                apu->wave.length_timer = 256;
            }
            ui_debug_log(UI_DEBUG_APU, "[APU] Channel 3 (Wave) triggered: freq=%u, vol=%u, enabled=%d",
                       apu->wave.frequency, apu->wave.volume, apu->wave.wave_table_enabled);
            break;
            
        case 3: /* Noise */
            apu->noise.enabled = true;
            apu->noise.volume = apu->noise.initial_volume;
            apu->noise.envelope_timer = apu->noise.envelope_period;
            apu->noise.lfsr = 0x7FFF;
            uint32_t divisor = NOISE_DIVISORS[apu->noise.divisor_code & 7];
            apu->noise.frequency_timer = divisor << apu->noise.clock_shift;
            if (apu->noise.length_timer == 0) {
                apu->noise.length_timer = 64;
            }
            ui_debug_log(UI_DEBUG_APU, "[APU] Channel 4 (Noise) triggered: vol=%u, divisor=%u, shift=%u, width=%s",
                       apu->noise.initial_volume, divisor, apu->noise.clock_shift,
                       apu->noise.width_mode ? "7-bit" : "15-bit");
            break;
    }
}

void apu_update_length_counters(APU* apu) {
    /* Update length counters for all channels */
    if (apu->pulse1.counter_selection && apu->pulse1.length_timer > 0) {
        apu->pulse1.length_timer--;
        if (apu->pulse1.length_timer == 0) {
            apu->pulse1.enabled = false;
        }
    }
    
    if (apu->pulse2.counter_selection && apu->pulse2.length_timer > 0) {
        apu->pulse2.length_timer--;
        if (apu->pulse2.length_timer == 0) {
            apu->pulse2.enabled = false;
        }
    }
    
    if (apu->wave.length_timer > 0) {
        apu->wave.length_timer--;
        if (apu->wave.length_timer == 0) {
            apu->wave.enabled = false;
        }
    }
    
    if (apu->noise.length_timer > 0) {
        apu->noise.length_timer--;
        if (apu->noise.length_timer == 0) {
            apu->noise.enabled = false;
        }
    }
}

void apu_update_envelopes(APU* apu) {
    /* Update volume envelopes for pulse and noise channels */
    if (apu->pulse1.envelope_period > 0) {
        apu->pulse1.envelope_timer--;
        if (apu->pulse1.envelope_timer == 0) {
            apu->pulse1.envelope_timer = apu->pulse1.envelope_period;
            if (apu->pulse1.envelope_increase && apu->pulse1.volume < 15) {
                apu->pulse1.volume++;
            } else if (!apu->pulse1.envelope_increase && apu->pulse1.volume > 0) {
                apu->pulse1.volume--;
            }
        }
    }
    
    if (apu->pulse2.envelope_period > 0) {
        apu->pulse2.envelope_timer--;
        if (apu->pulse2.envelope_timer == 0) {
            apu->pulse2.envelope_timer = apu->pulse2.envelope_period;
            if (apu->pulse2.envelope_increase && apu->pulse2.volume < 15) {
                apu->pulse2.volume++;
            } else if (!apu->pulse2.envelope_increase && apu->pulse2.volume > 0) {
                apu->pulse2.volume--;
            }
        }
    }
    
    if (apu->noise.envelope_period > 0) {
        apu->noise.envelope_timer--;
        if (apu->noise.envelope_timer == 0) {
            apu->noise.envelope_timer = apu->noise.envelope_period;
            if (apu->noise.envelope_increase && apu->noise.volume < 15) {
                apu->noise.volume++;
            } else if (!apu->noise.envelope_increase && apu->noise.volume > 0) {
                apu->noise.volume--;
            }
        }
    }
}

void apu_update_sweep(APU* apu) {
    /* Update sweep for pulse channel 1 */
    if (apu->pulse1.sweep_period > 0) {
        apu->pulse1.sweep_timer--;
        if (apu->pulse1.sweep_timer == 0) {
            apu->pulse1.sweep_timer = apu->pulse1.sweep_period;
            
            uint16_t freq = (apu->pulse1.frequency & 0x7FF);
            uint16_t sweep = freq >> apu->pulse1.sweep_shift;
            
            if (apu->pulse1.sweep_decrease) {
                freq -= sweep;
            } else {
                freq += sweep;
            }
            
            /* Check if frequency is valid */
            if (freq <= 2047) {
                apu->pulse1.frequency = freq;
            } else {
                apu->pulse1.enabled = false;
            }
        }
    }
}

float apu_get_channel_output(APU* apu, int channel) {
    float output = 0.0f;
    
    switch (channel) {
        case 0: /* Pulse 1 */
            if (apu->pulse1.enabled && apu->pulse1.volume > 0) {
                bool sample = DUTY_WAVEFORMS[apu->pulse1.duty & 3][apu->pulse1.duty_position];
                output = sample ? (float)apu->pulse1.volume / 15.0f : 0.0f;
            }
            break;
            
        case 1: /* Pulse 2 */
            if (apu->pulse2.enabled && apu->pulse2.volume > 0) {
                bool sample = DUTY_WAVEFORMS[apu->pulse2.duty & 3][apu->pulse2.duty_position];
                output = sample ? (float)apu->pulse2.volume / 15.0f : 0.0f;
            }
            break;
            
        case 2: /* Wave */
            if (apu->wave.enabled && apu->wave.wave_table_enabled) {
                uint8_t sample = apu->wave.wave_pattern[apu->wave.wave_position];
                /* Apply volume shift */
                switch (apu->wave.volume & 3) {
                    case 0: sample = 0; break;      /* Mute */
                    case 1: break;                   /* 100% */
                    case 2: sample >>= 1; break;    /* 50% */
                    case 3: sample >>= 2; break;    /* 25% */
                }
                output = (float)sample / 15.0f;
            }
            break;
            
        case 3: /* Noise */
            if (apu->noise.enabled && apu->noise.volume > 0) {
                /* LFSR output is inverted (0 = high) */
                bool sample = (apu->noise.lfsr & 1) == 0;
                output = sample ? (float)apu->noise.volume / 15.0f : 0.0f;
            }
            break;
    }
    
    return output;
}

void apu_generate_samples(APU* apu) {
    if (!apu->power) return;
    
    /* Generate one sample */
    float left = 0.0f;
    float right = 0.0f;
    
    /* Mix channels */
    for (int ch = 0; ch < 4; ch++) {
        float sample = apu_get_channel_output(apu, ch);
        
        if (apu->left_enables & (1 << ch)) {
            left += sample * (apu->left_volume / 7.0f);
        }
        if (apu->right_enables & (1 << ch)) {
            right += sample * (apu->right_volume / 7.0f);
        }
    }
    
    /* Store in buffer */
    uint32_t buffer_size = sizeof(apu->buffer) / sizeof(float);
    if (apu->buffer_position < buffer_size) {
        apu->buffer[apu->buffer_position++] = (left + right) * 0.5f;
    }
}

uint32_t apu_get_samples(APU* apu, float* samples, uint32_t max_count) {
    uint32_t count = apu->buffer_position;
    if (count > max_count) count = max_count;
    
    if (count > 0 && samples) {
        memcpy(samples, apu->buffer, count * sizeof(float));
        apu->buffer_position = 0;  /* Reset buffer */
    }
    
    return count;
}

uint8_t apu_read_register(APU* apu, uint16_t address) {
    switch (address) {
        /* Channel 1 - Pulse with sweep */
        case 0xFF10: /* NR10 - Sweep */
            return (apu->pulse1.sweep_period << 4) |
                   (apu->pulse1.sweep_decrease << 3) |
                   apu->pulse1.sweep_shift;
            
        case 0xFF11: /* NR11 - Length timer & duty */
            return (apu->pulse1.duty << 6);
            
        case 0xFF12: /* NR12 - Volume & envelope */
            return (apu->pulse1.initial_volume << 4) |
                   (apu->pulse1.envelope_increase << 3) |
                   apu->pulse1.envelope_period;
            
        case 0xFF13: /* NR13 - Frequency LSB */
            return apu->pulse1.frequency & 0xFF;
            
        case 0xFF14: /* NR14 - Frequency MSB & control */
            return ((apu->pulse1.frequency >> 8) & 0x7) |
                   (apu->pulse1.counter_selection << 6);
            
        /* Channel 2 - Pulse */
        case 0xFF16: /* NR21 - Length timer & duty */
            return (apu->pulse2.duty << 6);
            
        case 0xFF17: /* NR22 - Volume & envelope */
            return (apu->pulse2.initial_volume << 4) |
                   (apu->pulse2.envelope_increase << 3) |
                   apu->pulse2.envelope_period;
            
        case 0xFF18: /* NR23 - Frequency LSB */
            return apu->pulse2.frequency & 0xFF;
            
        case 0xFF19: /* NR24 - Frequency MSB & control */
            return ((apu->pulse2.frequency >> 8) & 0x7) |
                   (apu->pulse2.counter_selection << 6);
            
        /* Channel 3 - Wave */
        case 0xFF1A: /* NR30 - DAC power */
            return apu->wave.wave_table_enabled << 7;
            
        case 0xFF1B: /* NR31 - Length timer */
            return 0xFF;  /* Write only */
            
        case 0xFF1C: /* NR32 - Output level */
            return apu->wave.volume << 5;
            
        case 0xFF1D: /* NR33 - Frequency LSB */
            return apu->wave.frequency & 0xFF;
            
        case 0xFF1E: /* NR34 - Frequency MSB & control */
            return ((apu->wave.frequency >> 8) & 0x7);
            
        /* Channel 4 - Noise */
        case 0xFF20: /* NR41 - Length timer */
            return 0xFF;  /* Write only */
            
        case 0xFF21: /* NR42 - Volume & envelope */
            return (apu->noise.initial_volume << 4) |
                   (apu->noise.envelope_increase << 3) |
                   apu->noise.envelope_period;
            
        case 0xFF22: /* NR43 - Frequency & randomness */
            return (apu->noise.clock_shift << 4) |
                   (apu->noise.width_mode << 3) |
                   apu->noise.divisor_code;
            
        case 0xFF23: /* NR44 - Control */
            return 0xFF;  /* Write only */
            
        /* Control */
        case 0xFF24: /* NR50 - Master volume & VIN */
            return (apu->left_volume << 4) | apu->right_volume;
            
        case 0xFF25: /* NR51 - Sound panning */
            return (apu->left_enables << 4) | apu->right_enables;
            
        case 0xFF26: /* NR52 - Sound on/off */
            return (apu->power << 7) |
                   (apu->noise.enabled << 3) |
                   (apu->wave.enabled << 2) |
                   (apu->pulse2.enabled << 1) |
                   apu->pulse1.enabled;
            
        /* Wave RAM */
        case 0xFF30 ... 0xFF3F:
            return apu->wave.wave_pattern[address - 0xFF30];
            
        default:
            return 0xFF;
    }
}

void apu_write_register(APU* apu, uint16_t address, uint8_t value) {
    if (!apu->power && address != 0xFF26) return;
    
    /* Debug: Log APU register writes */
    static int write_count = 0;
    if (write_count < 50) {
        /* Decode register name */
        const char* reg_name = NULL;
        switch (address) {
            case 0xFF10: reg_name = "NR10 (CH1 Sweep)"; break;
            case 0xFF11: reg_name = "NR11 (CH1 Length/Duty)"; break;
            case 0xFF12: reg_name = "NR12 (CH1 Volume)"; break;
            case 0xFF13: reg_name = "NR13 (CH1 Freq Lo)"; break;
            case 0xFF14: reg_name = "NR14 (CH1 Freq Hi)"; break;
            case 0xFF16: reg_name = "NR21 (CH2 Length/Duty)"; break;
            case 0xFF17: reg_name = "NR22 (CH2 Volume)"; break;
            case 0xFF18: reg_name = "NR23 (CH2 Freq Lo)"; break;
            case 0xFF19: reg_name = "NR24 (CH2 Freq Hi)"; break;
            case 0xFF1A: reg_name = "NR30 (Wave On/Off)"; break;
            case 0xFF1B: reg_name = "NR31 (Wave Length)"; break;
            case 0xFF1C: reg_name = "NR32 (Wave Volume)"; break;
            case 0xFF1D: reg_name = "NR33 (Wave Freq Lo)"; break;
            case 0xFF1E: reg_name = "NR34 (Wave Freq Hi)"; break;
            case 0xFF20: reg_name = "NR41 (Noise Length)"; break;
            case 0xFF21: reg_name = "NR42 (Noise Volume)"; break;
            case 0xFF22: reg_name = "NR43 (Noise Freq)"; break;
            case 0xFF23: reg_name = "NR44 (Noise Control)"; break;
            case 0xFF24: reg_name = "NR50 (Master Volume)"; break;
            case 0xFF25: reg_name = "NR51 (Sound Panning)"; break;
            case 0xFF26: reg_name = "NR52 (Sound On/Off)"; break;
        }
        if (reg_name) {
            ui_debug_log(UI_DEBUG_APU, "[APU] Write 0x%04X = 0x%02X [%s]", address, value, reg_name);
        } else {
            ui_debug_log(UI_DEBUG_APU, "[APU] Write 0x%04X = 0x%02X", address, value);
        }
        write_count++;
    }
    
    switch (address) {
        /* Channel 1 - Pulse with sweep */
        case 0xFF10: /* NR10 - Sweep */
            apu->pulse1.sweep_period = (value >> 4) & 7;
            apu->pulse1.sweep_decrease = (value >> 3) & 1;
            apu->pulse1.sweep_shift = value & 7;
            break;
            
        case 0xFF11: /* NR11 - Length timer & duty */
            apu->pulse1.duty = (value >> 6) & 3;
            apu->pulse1.length_timer = 64 - (value & 0x3F);
            break;
            
        case 0xFF12: /* NR12 - Volume & envelope */
            apu->pulse1.initial_volume = (value >> 4) & 0xF;
            apu->pulse1.envelope_increase = (value >> 3) & 1;
            apu->pulse1.envelope_period = value & 7;
            if (value & 0xF8) {
                apu->pulse1.enabled = true;
            }
            break;
            
        case 0xFF13: /* NR13 - Frequency LSB */
            apu->pulse1.frequency = (apu->pulse1.frequency & 0x700) | value;
            break;
            
        case 0xFF14: /* NR14 - Frequency MSB & control */
            apu->pulse1.frequency = (apu->pulse1.frequency & 0xFF) | ((value & 7) << 8);
            apu->pulse1.counter_selection = (value >> 6) & 1;
            if (value & 0x80) {
                apu_trigger_channel(apu, 0);
            }
            break;
            
        /* Channel 2 - Pulse */
        case 0xFF16: /* NR21 - Length timer & duty */
            apu->pulse2.duty = (value >> 6) & 3;
            apu->pulse2.length_timer = 64 - (value & 0x3F);
            break;
            
        case 0xFF17: /* NR22 - Volume & envelope */
            apu->pulse2.initial_volume = (value >> 4) & 0xF;
            apu->pulse2.envelope_increase = (value >> 3) & 1;
            apu->pulse2.envelope_period = value & 7;
            if (value & 0xF8) {
                apu->pulse2.enabled = true;
            }
            break;
            
        case 0xFF18: /* NR23 - Frequency LSB */
            apu->pulse2.frequency = (apu->pulse2.frequency & 0x700) | value;
            break;
            
        case 0xFF19: /* NR24 - Frequency MSB & control */
            apu->pulse2.frequency = (apu->pulse2.frequency & 0xFF) | ((value & 7) << 8);
            apu->pulse2.counter_selection = (value >> 6) & 1;
            if (value & 0x80) {
                apu_trigger_channel(apu, 1);
            }
            break;
            
        /* Channel 3 - Wave */
        case 0xFF1A: /* NR30 - DAC power */
            apu->wave.wave_table_enabled = (value >> 7) & 1;
            break;
            
        case 0xFF1B: /* NR31 - Length timer */
            apu->wave.length_timer = 256 - value;
            break;
            
        case 0xFF1C: /* NR32 - Output level */
            apu->wave.volume = (value >> 5) & 3;
            break;
            
        case 0xFF1D: /* NR33 - Frequency LSB */
            apu->wave.frequency = (apu->wave.frequency & 0x700) | value;
            break;
            
        case 0xFF1E: /* NR34 - Frequency MSB & control */
            apu->wave.frequency = (apu->wave.frequency & 0xFF) | ((value & 7) << 8);
            if (value & 0x80) {
                apu_trigger_channel(apu, 2);
            }
            break;
            
        /* Channel 4 - Noise */
        case 0xFF20: /* NR41 - Length timer */
            apu->noise.length_timer = 64 - (value & 0x3F);
            break;
            
        case 0xFF21: /* NR42 - Volume & envelope */
            apu->noise.initial_volume = (value >> 4) & 0xF;
            apu->noise.envelope_increase = (value >> 3) & 1;
            apu->noise.envelope_period = value & 7;
            if (value & 0xF8) {
                apu->noise.enabled = true;
            }
            break;
            
        case 0xFF22: /* NR43 - Frequency & randomness */
            apu->noise.clock_shift = (value >> 4) & 0xF;
            apu->noise.width_mode = (value >> 3) & 1;
            apu->noise.divisor_code = value & 7;
            break;
            
        case 0xFF23: /* NR44 - Control */
            if (value & 0x80) {
                apu_trigger_channel(apu, 3);
            }
            break;
            
        /* Control */
        case 0xFF24: /* NR50 - Master volume & VIN */
            apu->left_volume = (value >> 4) & 7;
            apu->right_volume = value & 7;
            break;
            
        case 0xFF25: /* NR51 - Sound panning */
            apu->left_enables = (value >> 4) & 0xF;
            apu->right_enables = value & 0xF;
            break;
            
        case 0xFF26: /* NR52 - Sound on/off */
            apu->power = (value >> 7) & 1;
            if (!apu->power) {
                /* Power off - reset all registers except wave RAM */
                for (uint16_t addr = 0xFF10; addr <= 0xFF25; addr++) {
                    apu_write_register(apu, addr, 0);
                }
            }
            break;
            
        /* Wave RAM */
        case 0xFF30 ... 0xFF3F:
            if (apu->wave.enabled) {
                /* Can only write when channel is disabled */
                return;
            }
            apu->wave.wave_pattern[address - 0xFF30] = value;
            break;
    }
}