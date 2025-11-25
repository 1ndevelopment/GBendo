#ifndef GB_SM83_H
#define GB_SM83_H

#include <stdint.h>
#include <stdbool.h>

/* Sharp SM83 CPU (based on Intel 8080 architecture) */
typedef struct {
    /* Main registers */
    union {
        struct {
            uint8_t f;  /* Flag register */
            uint8_t a;  /* Accumulator */
        };
        uint16_t af;    /* Combined AF register */
    };
    
    union {
        struct {
            uint8_t c;
            uint8_t b;
        };
        uint16_t bc;
    };
    
    union {
        struct {
            uint8_t e;
            uint8_t d;
        };
        uint16_t de;
    };
    
    union {
        struct {
            uint8_t l;
            uint8_t h;
        };
        uint16_t hl;
    };

    /* Special registers */
    uint16_t sp;    /* Stack pointer */
    uint16_t pc;    /* Program counter */

    /* CPU state */
    bool ime;       /* Interrupt master enable flag */
    bool ei_delay;  /* Delay EI instruction by one cycle */
    bool halted;    /* CPU is halted flag */
    bool stopped;   /* CPU is stopped flag */
    void* mem;      /* Points to Memory struct */
    uint32_t cycles;/* Clock cycle counter */

    /* Flag register bit fields */
    struct {
        uint8_t unused : 4;  /* Lower 4 bits always zero */
        uint8_t c : 1;       /* Carry flag */
        uint8_t h : 1;       /* Half carry flag */
        uint8_t n : 1;       /* Subtract flag */
        uint8_t z : 1;       /* Zero flag */
    } flags;
} SM83_CPU;

/* CPU initialization and control */
void sm83_init(SM83_CPU* cpu);
void sm83_reset(SM83_CPU* cpu);
int sm83_step(SM83_CPU* cpu);  /* Execute one instruction */

/* Interrupt handling */
void sm83_request_interrupt(SM83_CPU* cpu, uint8_t interrupt);
void sm83_service_interrupts(SM83_CPU* cpu);

/* Flag operations */
void sm83_set_flag(SM83_CPU* cpu, uint8_t flag, bool value);
bool sm83_get_flag(SM83_CPU* cpu, uint8_t flag);

/* CPU clock cycles management */
void sm83_add_cycles(SM83_CPU* cpu, uint32_t cycles);
uint32_t sm83_get_cycles(SM83_CPU* cpu);

/* Instruction set constants */
#define FLAG_Z 0x80
#define FLAG_N 0x40
#define FLAG_H 0x20
#define FLAG_C 0x10

#endif /* GB_SM83_H */