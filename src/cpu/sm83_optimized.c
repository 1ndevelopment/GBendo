#include "sm83_optimized.h"
#include "sm83_ops.h"
#include "../memory/memory.h"
#include <string.h>

/* Instruction cycles lookup table */
__attribute__((unused)) static const uint8_t instruction_cycles[256] = {
    /*  0   1   2   3   4   5   6   7   8   9   A   B   C   D   E   F */
    /*0*/ 4, 12,  8,  8,  4,  4,  8,  4, 20,  8,  8,  8,  4,  4,  8,  4,
    /*1*/ 4, 12,  8,  8,  4,  4,  8,  4, 12,  8,  8,  8,  4,  4,  8,  4,
    /*2*/ 8, 12,  8,  8,  4,  4,  8,  4,  8,  8,  8,  8,  4,  4,  8,  4,
    /*3*/ 8, 12,  8,  8, 12, 12, 12,  4,  8,  8,  8,  8,  4,  4,  8,  4,
    /*4*/ 4,  4,  4,  4,  4,  4,  8,  4,  4,  4,  4,  4,  4,  4,  8,  4,
    /*5*/ 4,  4,  4,  4,  4,  4,  8,  4,  4,  4,  4,  4,  4,  4,  8,  4,
    /*6*/ 4,  4,  4,  4,  4,  4,  8,  4,  4,  4,  4,  4,  4,  4,  8,  4,
    /*7*/ 8,  8,  8,  8,  8,  8,  4,  8,  4,  4,  4,  4,  4,  4,  8,  4,
    /*8*/ 4,  4,  4,  4,  4,  4,  8,  4,  4,  4,  4,  4,  4,  4,  8,  4,
    /*9*/ 4,  4,  4,  4,  4,  4,  8,  4,  4,  4,  4,  4,  4,  4,  8,  4,
    /*A*/ 4,  4,  4,  4,  4,  4,  8,  4,  4,  4,  4,  4,  4,  4,  8,  4,
    /*B*/ 4,  4,  4,  4,  4,  4,  8,  4,  4,  4,  4,  4,  4,  4,  8,  4,
    /*C*/ 8, 12, 12, 16, 12, 16,  8, 16,  8, 16, 12,  4, 12, 24,  8, 16,
    /*D*/ 8, 12, 12,  0, 12, 16,  8, 16,  8, 16, 12,  0, 12,  0,  8, 16,
    /*E*/ 12, 12,  8,  0,  0, 16,  8, 16, 16,  4, 16,  0,  0,  0,  8, 16,
    /*F*/ 12, 12,  8,  4,  0, 16,  8, 16, 12,  8, 16,  4,  0,  0,  8, 16
};

/* Jump tables for instruction dispatch */
InstructionHandler instruction_table[256];
InstructionHandler cb_instruction_table[256];

/* Fast instruction implementations - no function call overhead */

/* NOP - 0x00 */
static inline int exec_nop(SM83_CPU* cpu, void* mem) {
    (void)cpu; (void)mem;
    return 4;
}

/* LD BC,nn - 0x01 */
static inline int exec_ld_bc_nn(SM83_CPU* cpu, void* mem) {
    uint8_t low = memory_read((Memory*)mem, cpu->pc++);
    uint8_t high = memory_read((Memory*)mem, cpu->pc++);
    cpu->bc = (high << 8) | low;
    return 12;
}

/* LD (BC),A - 0x02 */
static inline int exec_ld_bc_a(SM83_CPU* cpu, void* mem) {
    memory_write((Memory*)mem, cpu->bc, cpu->a);
    return 8;
}

/* INC BC - 0x03 */
static inline int exec_inc_bc(SM83_CPU* cpu, void* mem) {
    (void)mem;
    cpu->bc++;
    return 8;
}

/* INC B - 0x04 */
static inline int exec_inc_b(SM83_CPU* cpu, void* mem) {
    (void)mem;
    uint8_t result = cpu->b + 1;
    cpu->f = (cpu->f & FLAG_C);  /* Preserve carry flag */
    if (result == 0) cpu->f |= FLAG_Z;
    if ((cpu->b & 0xF) == 0xF) cpu->f |= FLAG_H;
    cpu->b = result;
    return 4;
}

/* DEC B - 0x05 */
static inline int exec_dec_b(SM83_CPU* cpu, void* mem) {
    (void)mem;
    uint8_t result = cpu->b - 1;
    cpu->f = (cpu->f & FLAG_C) | FLAG_N;  /* Preserve carry, set subtract */
    if (result == 0) cpu->f |= FLAG_Z;
    if ((cpu->b & 0xF) == 0) cpu->f |= FLAG_H;
    cpu->b = result;
    return 4;
}

/* LD B,n - 0x06 */
static inline int exec_ld_b_n(SM83_CPU* cpu, void* mem) {
    cpu->b = memory_read((Memory*)mem, cpu->pc++);
    return 8;
}

/* RLCA - 0x07 */
static inline int exec_rlca(SM83_CPU* cpu, void* mem) {
    (void)mem;
    uint8_t carry = (cpu->a >> 7) & 1;
    cpu->a = (cpu->a << 1) | carry;
    cpu->f = carry ? FLAG_C : 0;
    return 4;
}

/* LD (nn),SP - 0x08 */
static inline int exec_ld_nn_sp(SM83_CPU* cpu, void* mem) {
    uint8_t low = memory_read((Memory*)mem, cpu->pc++);
    uint8_t high = memory_read((Memory*)mem, cpu->pc++);
    uint16_t addr = (high << 8) | low;
    memory_write((Memory*)mem, addr, cpu->sp & 0xFF);
    memory_write((Memory*)mem, addr + 1, cpu->sp >> 8);
    return 20;
}

/* ADD HL,BC - 0x09 */
static inline int exec_add_hl_bc(SM83_CPU* cpu, void* mem) {
    (void)mem;
    uint32_t result = cpu->hl + cpu->bc;
    cpu->f &= FLAG_Z;  /* Preserve zero flag */
    if (((cpu->hl & 0xFFF) + (cpu->bc & 0xFFF)) > 0xFFF) cpu->f |= FLAG_H;
    if (result > 0xFFFF) cpu->f |= FLAG_C;
    cpu->hl = (uint16_t)result;
    return 8;
}

/* Continue with other critical instructions... */
/* For brevity, implementing the most common ones first */

/* Generic instruction handlers - simple implementations */
static int generic_nop(SM83_CPU* cpu, void* mem) { 
    (void)cpu; (void)mem; 
    return 4; 
}

static int generic_ld_bc_nn(SM83_CPU* cpu, void* mem) { 
    Memory* memory = (Memory*)mem;
    uint8_t low = memory_read(memory, cpu->pc++);
    uint8_t high = memory_read(memory, cpu->pc++);
    cpu->bc = (high << 8) | low;
    return 12; 
}

static int generic_ld_bc_a(SM83_CPU* cpu, void* mem) { 
    memory_write((Memory*)mem, cpu->bc, cpu->a);
    return 8; 
}

static int generic_inc_bc(SM83_CPU* cpu, void* mem) { 
    (void)mem; 
    cpu->bc++;
    return 8; 
}

/* Initialize jump tables with optimized handlers */
void sm83_init_jump_tables(void) {
    /* Initialize all entries to NULL first */
    memset(instruction_table, 0, sizeof(instruction_table));
    memset(cb_instruction_table, 0, sizeof(cb_instruction_table));
    
    /* Install optimized handlers for most common instructions */
    instruction_table[0x00] = generic_nop;         /* NOP */
    instruction_table[0x01] = generic_ld_bc_nn;    /* LD BC,nn */
    instruction_table[0x02] = generic_ld_bc_a;     /* LD (BC),A */
    instruction_table[0x03] = generic_inc_bc;      /* INC BC */
    
    /* Install generic handlers for all other instructions */
    for (int i = 4; i < 256; i++) {
        if (!instruction_table[i]) {
            /* Install a generic handler that calls the original switch-based implementation */
            instruction_table[i] = NULL; /* Will be handled by fallback */
        }
    }
}


/* Enhanced instruction dispatch with fallback */
int sm83_step_enhanced(SM83_CPU* cpu) {
    Memory* mem = (Memory*)cpu->mem;
    if (!mem) return -1;
    
    /* Handle interrupts */
    if (cpu->ime) {
        sm83_service_interrupts(cpu);
    }
    
    /* Handle halt/stop states */
    if (cpu->halted || cpu->stopped) {
        sm83_add_cycles(cpu, 4);
        return 4;
    }
    
    /* Fetch instruction */
    uint8_t opcode = memory_read(mem, cpu->pc++);
    
    /* Use jump table if handler exists, otherwise use fallback */
    InstructionHandler handler = instruction_table[opcode];
    if (handler) {
        int cycles = handler(cpu, mem);
        sm83_add_cycles(cpu, cycles);
        return cycles;
    } else {
        /* Fallback to original implementation */
        cpu->pc--; /* Restore PC for original handler */
        return sm83_step(cpu); /* Call original function */
    }
}
