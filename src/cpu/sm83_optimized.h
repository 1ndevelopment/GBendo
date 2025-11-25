#ifndef GB_SM83_OPTIMIZED_H
#define GB_SM83_OPTIMIZED_H

#include "sm83.h"
#include "../memory/memory.h"

/* Fast instruction dispatch using function pointers */
typedef int (*InstructionHandler)(SM83_CPU* cpu, void* mem);

/* Jump tables for faster instruction dispatch */
extern InstructionHandler instruction_table[256];
extern InstructionHandler cb_instruction_table[256];

/* Optimized CPU step function using jump table */
static inline int sm83_step_optimized(SM83_CPU* cpu) {
    void* mem = cpu->mem;
    if (!mem) return -1;
    
    /* Handle halt/stop states */
    if (cpu->halted || cpu->stopped) {
        sm83_add_cycles(cpu, 4);
        return 4;
    }
    
    /* Fetch and execute instruction */
    uint8_t opcode = memory_read((Memory*)mem, cpu->pc++);
    return instruction_table[opcode](cpu, mem);
}

/* Initialize jump tables */
void sm83_init_jump_tables(void);

/* Optimized memory access macros */
#define FAST_READ8(mem, addr) (((Memory*)(mem))->memory_read_fast(mem, addr))
#define FAST_WRITE8(mem, addr, val) (((Memory*)(mem))->memory_write_fast(mem, addr, val))

/* CPU register access optimizations */
#define GET_REG8(cpu, reg) ((cpu)->reg)
#define SET_REG8(cpu, reg, val) ((cpu)->reg = (val))
#define GET_REG16(cpu, reg) ((cpu)->reg)
#define SET_REG16(cpu, reg, val) ((cpu)->reg = (val))

/* Flag operations - optimized inline versions */
static inline void set_flag_fast(SM83_CPU* cpu, uint8_t flag, bool value) {
    cpu->f = value ? (cpu->f | flag) : (cpu->f & ~flag);
}

static inline bool get_flag_fast(SM83_CPU* cpu, uint8_t flag) {
    return (cpu->f & flag) != 0;
}

/* Optimized ALU operations */
static inline uint8_t add_bytes_fast(SM83_CPU* cpu, uint8_t a, uint8_t b, bool carry) {
    uint16_t c = carry && get_flag_fast(cpu, FLAG_C) ? 1 : 0;
    uint16_t result = a + b + c;
    uint16_t h = ((a & 0xF) + (b & 0xF) + c) & 0x10;

    cpu->f = 0;  /* Clear all flags first */
    if ((result & 0xFF) == 0) cpu->f |= FLAG_Z;
    if (h) cpu->f |= FLAG_H;
    if (result > 0xFF) cpu->f |= FLAG_C;

    return (uint8_t)result;
}

static inline uint8_t sub_bytes_fast(SM83_CPU* cpu, uint8_t a, uint8_t b, bool carry) {
    uint16_t c = carry && get_flag_fast(cpu, FLAG_C) ? 1 : 0;
    uint16_t result = a - b - c;
    uint16_t h = ((a & 0xF) - (b & 0xF) - c) & 0x10;

    cpu->f = FLAG_N;  /* Set subtract flag */
    if ((result & 0xFF) == 0) cpu->f |= FLAG_Z;
    if (h) cpu->f |= FLAG_H;
    if (result > 0xFF) cpu->f |= FLAG_C;

    return (uint8_t)result;
}

#endif /* GB_SM83_OPTIMIZED_H */
