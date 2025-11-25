#include "memory.h"

/* Precise timer implementation using an internal 16-bit divider counter.
 * - `div_internal` increments once per CPU cycle.
 * - Visible DIV (0xFF04) is upper 8 bits of div_internal.
 * - TIMA increments on the falling edge of the selected divider bit as
 *   determined by TAC (0xFF07).
 * - On TIMA overflow, TIMA is set to 0, a reload is scheduled, and after
 *   4 cycles TIMA is reloaded from TMA and the timer interrupt is requested.
 */

void memory_timer_init(Memory* mem) {
    mem->div_internal = 0;
    mem->div = 0;
    mem->tima = 0;
    mem->tma = 0;
    mem->tac = 0;
    mem->timer_enabled = false;
    mem->tima_reload_delay = 0;
    mem->tima_reload_pending = false;
    mem->last_timer_bit = 0;

    /* Mirror visible registers into io_registers for consistency */
    mem->io_registers[0x04] = mem->div;
    mem->io_registers[0x05] = mem->tima;
    mem->io_registers[0x06] = mem->tma;
    mem->io_registers[0x07] = mem->tac;
}

/* Helper: returns which divider bit is used by TAC selection */
static inline uint8_t tac_to_bit(uint8_t tac) {
    switch (tac & 0x03) {
        case 0: return 9;  /* 4096 Hz -> bit 9 (1024 cycles) */
        case 1: return 3;  /* 262144 Hz -> bit 3 (16 cycles) */
        case 2: return 5;  /* 65536 Hz -> bit 5 (64 cycles) */
        case 3: return 7;  /* 16384 Hz -> bit 7 (256 cycles) */
        default: return 9;
    }
}

void memory_timer_step(Memory* mem, uint32_t cycles) {
    if (!mem || cycles == 0) return;

    /* If a reload is pending, decrement its delay first - it's relative to CPU cycles */
    if (mem->tima_reload_pending) {
        if (cycles >= mem->tima_reload_delay) {
            cycles -= mem->tima_reload_delay;
            mem->tima_reload_pending = false;
            mem->tima_reload_delay = 0;
            /* Perform reload now */
            mem->tima = mem->tma;
            mem->io_registers[0x05] = mem->tima;
            mem->io_registers[0x0F] |= 0x04; /* Request Timer interrupt */
            /* Note: subsequent cycles in this step still advance divider/timer */
        } else {
            mem->tima_reload_delay -= cycles;
            /* Still in reload delay; advance divider but TIMA not affected */
            for (uint32_t i = 0; i < cycles; ++i) {
                mem->div_internal++;
            }
            mem->div = (uint8_t)(mem->div_internal >> 8);
            mem->io_registers[0x04] = mem->div;
            return;
        }
    }

    uint8_t selected_bit = tac_to_bit(mem->tac);
    bool enabled = (mem->tac & 0x04) != 0;

    /* For stricter edge behavior, process per CPU cycle and detect falling edges
     * of the selected divider bit. cycles are typically small (instruction cycles)
     * so per-cycle loop is acceptable and accurate.
     */
    for (uint32_t c = 0; c < cycles; ++c) {
        uint16_t prev = mem->div_internal;
        uint8_t prev_bit = (prev >> selected_bit) & 1;

        mem->div_internal++;

        uint16_t now = mem->div_internal;
        uint8_t now_bit = (now >> selected_bit) & 1;

        /* Update visible DIV every time upper 8 bits change (we can simply write each loop) */
        mem->div = (uint8_t)(mem->div_internal >> 8);
        mem->io_registers[0x04] = mem->div;

        /* If timer is enabled, check for falling edge from 1 -> 0 */
        if (enabled && prev_bit == 1 && now_bit == 0) {
            /* Increment TIMA and handle overflow */
            mem->tima++;
            if (mem->tima == 0x00) {
                /* Overflow: TIMA will be reloaded after 4 cycles */
                mem->tima_reload_pending = true;
                mem->tima_reload_delay = 4;
                /* Per hardware behavior, TIMA becomes 0 on overflow immediately */
                mem->io_registers[0x05] = 0x00;
            } else {
                mem->io_registers[0x05] = mem->tima;
            }
        }
    }
}

