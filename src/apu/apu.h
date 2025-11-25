#ifndef GB_APU_H
#define GB_APU_H

#include <stdint.h>
#include <stdbool.h>

/* APU Constants */
#define SAMPLE_RATE 44100
#define FRAME_SEQUENCER_RATE 512  /* 512 Hz */

/* Channel Control */
typedef struct {
    bool enabled;
    uint8_t volume;
    uint16_t frequency;
    bool counter_selection;
    uint16_t length_timer;
    uint8_t duty;
    uint8_t duty_position;  /* Phase position in duty cycle (0-7) */
    uint16_t frequency_timer;
    /* Envelope */
    uint8_t initial_volume;
    bool envelope_increase;
    uint8_t envelope_period;
    uint8_t envelope_timer;
    /* Sweep (Channel 1 only) */
    uint8_t sweep_period;
    bool sweep_decrease;
    uint8_t sweep_shift;
    uint8_t sweep_timer;
} PulseChannel;

typedef struct {
    bool enabled;
    uint8_t volume;
    uint16_t frequency;
    uint16_t length_timer;
    uint16_t frequency_timer;
    uint8_t wave_position;  /* Position in wave table (0-31) */
    /* Wave table */
    uint8_t wave_pattern[32];
    bool wave_table_enabled;
} WaveChannel;

typedef struct {
    bool enabled;
    uint8_t volume;
    uint8_t divisor_code;
    uint8_t width_mode;
    uint8_t clock_shift;
    uint16_t length_timer;
    uint16_t frequency_timer;
    uint16_t lfsr;  /* Linear Feedback Shift Register */
    /* Envelope */
    uint8_t initial_volume;
    bool envelope_increase;
    uint8_t envelope_period;
    uint8_t envelope_timer;
} NoiseChannel;

typedef struct {
    /* Sound channels */
    PulseChannel pulse1;
    PulseChannel pulse2;
    WaveChannel wave;
    NoiseChannel noise;

    /* Master control */
    bool power;
    uint8_t left_volume;
    uint8_t right_volume;
    uint8_t left_enables;   /* Which channels output to left */
    uint8_t right_enables;  /* Which channels output to right */

    /* Timing */
    int32_t sample_timer;  /* Must be signed to detect negative for sample generation */
    uint32_t frame_sequencer;

    /* Audio buffer */
    float buffer[SAMPLE_RATE / 60];  /* 1 frame worth of samples */
    uint32_t buffer_position;
} APU;

/* APU initialization and control */
void apu_init(APU* apu);
void apu_reset(APU* apu);
void apu_step(APU* apu, uint32_t cycles);

/* Register access */
uint8_t apu_read_register(APU* apu, uint16_t address);
void apu_write_register(APU* apu, uint16_t address, uint8_t value);

/* Channel control */
void apu_trigger_channel(APU* apu, int channel);
void apu_update_length_counters(APU* apu);
void apu_update_envelopes(APU* apu);
void apu_update_sweep(APU* apu);

/* Audio generation */
void apu_generate_samples(APU* apu);
float apu_get_channel_output(APU* apu, int channel);

/* Get samples from buffer and reset buffer position */
uint32_t apu_get_samples(APU* apu, float* samples, uint32_t max_count);

#endif /* GB_APU_H */