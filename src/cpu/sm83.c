#include "sm83.h"
#include "sm83_ops.h"
#include "../memory/memory.h"
#include "../gbendo.h"
#include "../ui/ui.h"
#include <string.h>
#include <stdio.h>

/* CPU clock cycles per instruction */
static const uint8_t instruction_cycles[256] = {
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

/* CB prefix instruction cycles */
static const uint8_t cb_cycles[256] = {
    /*  0   1   2   3   4   5   6   7   8   9   A   B   C   D   E   F */
    /*0*/ 8,  8,  8,  8,  8,  8, 16,  8,  8,  8,  8,  8,  8,  8, 16,  8,
    /*1*/ 8,  8,  8,  8,  8,  8, 16,  8,  8,  8,  8,  8,  8,  8, 16,  8,
    /*2*/ 8,  8,  8,  8,  8,  8, 16,  8,  8,  8,  8,  8,  8,  8, 16,  8,
    /*3*/ 8,  8,  8,  8,  8,  8, 16,  8,  8,  8,  8,  8,  8,  8, 16,  8,
    /*4*/ 8,  8,  8,  8,  8,  8, 16,  8,  8,  8,  8,  8,  8,  8, 16,  8,
    /*5*/ 8,  8,  8,  8,  8,  8, 16,  8,  8,  8,  8,  8,  8,  8, 16,  8,
    /*6*/ 8,  8,  8,  8,  8,  8, 16,  8,  8,  8,  8,  8,  8,  8, 16,  8,
    /*7*/ 8,  8,  8,  8,  8,  8, 16,  8,  8,  8,  8,  8,  8,  8, 16,  8,
    /*8*/ 8,  8,  8,  8,  8,  8, 16,  8,  8,  8,  8,  8,  8,  8, 16,  8,
    /*9*/ 8,  8,  8,  8,  8,  8, 16,  8,  8,  8,  8,  8,  8,  8, 16,  8,
    /*A*/ 8,  8,  8,  8,  8,  8, 16,  8,  8,  8,  8,  8,  8,  8, 16,  8,
    /*B*/ 8,  8,  8,  8,  8,  8, 16,  8,  8,  8,  8,  8,  8,  8, 16,  8,
    /*C*/ 8,  8,  8,  8,  8,  8, 16,  8,  8,  8,  8,  8,  8,  8, 16,  8,
    /*D*/ 8,  8,  8,  8,  8,  8, 16,  8,  8,  8,  8,  8,  8,  8, 16,  8,
    /*E*/ 8,  8,  8,  8,  8,  8, 16,  8,  8,  8,  8,  8,  8,  8, 16,  8,
    /*F*/ 8,  8,  8,  8,  8,  8, 16,  8,  8,  8,  8,  8,  8,  8, 16,  8
};

/* CPU initialization */
void sm83_init(SM83_CPU* cpu) {
    memset(cpu, 0, sizeof(SM83_CPU));
}

void sm83_reset(SM83_CPU* cpu) {
    /* Initial register values after boot ROM */
    cpu->af = 0x01B0;
    cpu->bc = 0x0013;
    cpu->de = 0x00D8;
    cpu->hl = 0x014D;
    cpu->sp = 0xFFFE;
    cpu->pc = 0x0100;  /* Start of cartridge ROM after boot ROM */
    cpu->ime = false;  /* IME disabled at reset (matches DMG power-on) */
    cpu->cycles = 0;
}

/* Flag operations */
void sm83_set_flag(SM83_CPU* cpu, uint8_t flag, bool value) {
    if (value) {
        cpu->f |= flag;
    } else {
        cpu->f &= ~flag;
    }
}

bool sm83_get_flag(SM83_CPU* cpu, uint8_t flag) {
    return (cpu->f & flag) != 0;
}

/* ALU Helper Functions */
__attribute__((unused)) static uint8_t add_bytes(SM83_CPU* cpu, uint8_t a, uint8_t b, bool with_carry) {
    uint16_t carry = with_carry && sm83_get_flag(cpu, FLAG_C) ? 1 : 0;
    uint16_t result = a + b + carry;
    uint16_t h = ((a & 0xF) + (b & 0xF) + carry) & 0x10;

    sm83_set_flag(cpu, FLAG_Z, (result & 0xFF) == 0);
    sm83_set_flag(cpu, FLAG_N, false);
    sm83_set_flag(cpu, FLAG_H, h != 0);
    sm83_set_flag(cpu, FLAG_C, result > 0xFF);

    return (uint8_t)result;
}

__attribute__((unused)) static uint8_t sub_bytes(SM83_CPU* cpu, uint8_t a, uint8_t b, bool with_carry) {
    uint16_t carry = with_carry && sm83_get_flag(cpu, FLAG_C) ? 1 : 0;
    uint16_t result = a - b - carry;
    uint16_t h = ((a & 0xF) - (b & 0xF) - carry) & 0x10;

    sm83_set_flag(cpu, FLAG_Z, (result & 0xFF) == 0);
    sm83_set_flag(cpu, FLAG_N, true);
    sm83_set_flag(cpu, FLAG_H, h != 0);
    sm83_set_flag(cpu, FLAG_C, result > 0xFF);

    return (uint8_t)result;
}

__attribute__((unused)) static uint16_t add_words(SM83_CPU* cpu, uint16_t a, uint16_t b) {
    uint32_t result = a + b;
    uint16_t h = ((a & 0xFFF) + (b & 0xFFF)) & 0x1000;

    sm83_set_flag(cpu, FLAG_N, false);
    sm83_set_flag(cpu, FLAG_H, h != 0);
    sm83_set_flag(cpu, FLAG_C, result > 0xFFFF);

    return (uint16_t)result;
}

/* Bit Operations */
__attribute__((unused)) static void bit_op(SM83_CPU* cpu, uint8_t bit, uint8_t value) {
    sm83_set_flag(cpu, FLAG_Z, !(value & (1 << bit)));
    sm83_set_flag(cpu, FLAG_N, false);
    sm83_set_flag(cpu, FLAG_H, true);
}

__attribute__((unused)) static uint8_t rotate_left(SM83_CPU* cpu, uint8_t value, bool through_carry) {
    uint8_t carry = through_carry ? (sm83_get_flag(cpu, FLAG_C) ? 1 : 0) : (value >> 7);
    uint8_t result = (value << 1) | carry;

    sm83_set_flag(cpu, FLAG_Z, result == 0);
    sm83_set_flag(cpu, FLAG_N, false);
    sm83_set_flag(cpu, FLAG_H, false);
    sm83_set_flag(cpu, FLAG_C, value & 0x80);

    return result;
}

__attribute__((unused)) static uint8_t rotate_right(SM83_CPU* cpu, uint8_t value, bool through_carry) {
    uint8_t carry = through_carry ? (sm83_get_flag(cpu, FLAG_C) ? 0x80 : 0) : ((value & 1) << 7);
    uint8_t result = (value >> 1) | carry;

    sm83_set_flag(cpu, FLAG_Z, result == 0);
    sm83_set_flag(cpu, FLAG_N, false);
    sm83_set_flag(cpu, FLAG_H, false);
    sm83_set_flag(cpu, FLAG_C, value & 1);

    return result;
}

/* Interrupt Handling */
void sm83_request_interrupt(SM83_CPU* cpu, uint8_t interrupt) {
    /* Set the corresponding interrupt flag in memory's IF register */
    Memory* mem = (Memory*)cpu->mem;
    if (!mem) return;
    mem->io_registers[0x0F] |= interrupt;
}

void sm83_service_interrupts(SM83_CPU* cpu) {
    Memory* mem = (Memory*)cpu->mem;
    if (!mem) return;
    uint8_t if_reg = mem->io_registers[0x0F];  /* Interrupt Flag */
    uint8_t ie_reg = mem->ie_register;         /* Interrupt Enable */
    uint8_t pending = if_reg & ie_reg;
    
    if (!pending) return;

    /* Handle interrupts in priority order */
    static const struct {
        uint8_t bit;
        uint16_t vector;
    } interrupts[] = {
        { 1 << 0, 0x0040 },  /* V-Blank  - bit 0 */
        { 1 << 1, 0x0048 },  /* LCD STAT - bit 1 */
        { 1 << 2, 0x0050 },  /* Timer    - bit 2 */
        { 1 << 3, 0x0058 },  /* Serial   - bit 3 */
        { 1 << 4, 0x0060 }   /* Joypad   - bit 4 */
    };

    for (int i = 0; i < 5; i++) {
        if (pending & interrupts[i].bit) {
            /* Exit HALT/STOP when taking an interrupt */
            cpu->halted = false;
            cpu->stopped = false;
            
            /* Push PC onto stack */
            cpu->sp -= 2;
            uint16_t sp = cpu->sp;
            memory_write(mem, sp, cpu->pc & 0xFF);
            memory_write(mem, sp + 1, cpu->pc >> 8);

            /* Jump to interrupt vector */
            cpu->pc = interrupts[i].vector;
            
            /* Clear interrupt flag and disable IME */
            mem->io_registers[0x0F] &= ~interrupts[i].bit;
            cpu->ime = false;

            /* Add cycles for interrupt handling */
            cpu->cycles += 20;
            break;
        }
    }
}

/* CPU clock cycles management */
void sm83_add_cycles(SM83_CPU* cpu, uint32_t cycles) {
    cpu->cycles += cycles;
    /* Update timers in memory (DIV/TIMA) if memory is attached */
    Memory* mem = (Memory*)cpu->mem;
    if (mem) {
        memory_timer_step(mem, cycles);
    }
}

uint32_t sm83_get_cycles(SM83_CPU* cpu) {
    return cpu->cycles;
}

/* Helper functions for push/pop (not in header) */
static void push_rr(SM83_CPU* cpu, uint16_t val, Memory* mem) {
    cpu->sp -= 2;
    memory_write(mem, cpu->sp + 1, val >> 8);
    memory_write(mem, cpu->sp, val & 0xFF);
}

static void pop_rr(SM83_CPU* cpu, uint16_t* rp, Memory* mem) {
    uint8_t low = memory_read(mem, cpu->sp++);
    uint8_t high = memory_read(mem, cpu->sp++);
    *rp = (high << 8) | low;
}

/* Main instruction execution */
int sm83_step(SM83_CPU* cpu) {
    Memory* mem = (Memory*)cpu->mem;
    if (!mem) return -1;  /* Can't execute without memory */
    
    /* Handle interrupts before instruction */
    if (cpu->ime) {
        sm83_service_interrupts(cpu);
    }
    
    /* HALT bug wake: if HALTed and any pending IE&IF, wake even if IME=0 */
    {
        uint8_t pending = mem->io_registers[0x0F] & mem->ie_register;
        if (cpu->halted && pending) {
            cpu->halted = false;
        }
    }
    
    /* Handle halt/stop */
    if (cpu->halted || cpu->stopped) {
        sm83_add_cycles(cpu, 4);
        return 4;
    }
    
    /* EI delay is applied after the next instruction completes (handled at end of step) */
    
    uint16_t pc_before = cpu->pc;
    uint8_t opcode = memory_read(mem, cpu->pc++);
    uint32_t cycles = instruction_cycles[opcode];
    bool executed_ei = false;
    
    /* Save register state before execution for debug */
    uint16_t af_before = cpu->af;
    uint16_t bc_before = cpu->bc;
    uint16_t de_before = cpu->de;
    uint16_t hl_before = cpu->hl;
    uint16_t sp_before = cpu->sp;

    /* Execute instruction based on opcode */
    switch (opcode) {
        /* 0x00 - NOP */
        case 0x00: nop(cpu); break;
        
        /* 0x01-0x0F - Various instructions */
        case 0x01: ld_rr_nn(cpu, &cpu->bc, mem); break;
        case 0x02: ld_bc_a(cpu, mem); break;
        case 0x03: inc_rr(&cpu->bc); break;
        case 0x04: inc_r(cpu, &cpu->b); break;
        case 0x05: dec_r(cpu, &cpu->b); break;
        case 0x06: ld_r_n(cpu, &cpu->b, mem); break;
        case 0x07: rlca(cpu); break;
        case 0x08: ld_nn_sp(cpu, mem); break;
        case 0x09: add_hl_rr(cpu, cpu->bc); break;
        case 0x0A: ld_a_bc(cpu, mem); break;
        case 0x0B: dec_rr(&cpu->bc); break;
        case 0x0C: inc_r(cpu, &cpu->c); break;
        case 0x0D: dec_r(cpu, &cpu->c); break;
        case 0x0E: ld_r_n(cpu, &cpu->c, mem); break;
        case 0x0F: rrca(cpu); break;
        
        /* 0x10 - STOP */
        case 0x10: stop(cpu); break;
        
        /* 0x11-0x1F */
        case 0x11: ld_rr_nn(cpu, &cpu->de, mem); break;
        case 0x12: ld_de_a(cpu, mem); break;
        case 0x13: inc_rr(&cpu->de); break;
        case 0x14: inc_r(cpu, &cpu->d); break;
        case 0x15: dec_r(cpu, &cpu->d); break;
        case 0x16: ld_r_n(cpu, &cpu->d, mem); break;
        case 0x17: rla(cpu); break;
        case 0x18: jr_d(cpu, mem); break;
        case 0x19: add_hl_rr(cpu, cpu->de); break;
        case 0x1A: ld_a_de(cpu, mem); break;
        case 0x1B: dec_rr(&cpu->de); break;
        case 0x1C: inc_r(cpu, &cpu->e); break;
        case 0x1D: dec_r(cpu, &cpu->e); break;
        case 0x1E: ld_r_n(cpu, &cpu->e, mem); break;
        case 0x1F: rra(cpu); break;
        
        /* 0x20-0x2F */
        case 0x20: jr_cc_d(cpu, !sm83_get_flag(cpu, FLAG_Z), mem); break;
        case 0x21: ld_rr_nn(cpu, &cpu->hl, mem); break;
        case 0x22: ldi_hl_a(cpu, mem); break;
        case 0x23: inc_rr(&cpu->hl); break;
        case 0x24: inc_r(cpu, &cpu->h); break;
        case 0x25: dec_r(cpu, &cpu->h); break;
        case 0x26: ld_r_n(cpu, &cpu->h, mem); break;
        case 0x27: {
            /* DAA - Decimal Adjust Accumulator */
            uint8_t a = cpu->a;
            uint8_t adjust = 0;
            bool carry = sm83_get_flag(cpu, FLAG_C);
            
            if (sm83_get_flag(cpu, FLAG_N)) {
                /* After subtraction */
                if (sm83_get_flag(cpu, FLAG_H)) {
                    adjust |= 0x06;
                }
                if (carry) {
                    adjust |= 0x60;
                }
                a -= adjust;
            } else {
                /* After addition */
                if (sm83_get_flag(cpu, FLAG_H) || (a & 0x0F) > 9) {
                    adjust |= 0x06;
                }
                if (carry || (a > 0x99) || ((a + adjust) > 0x99)) {
                    adjust |= 0x60;
                    carry = true;
                }
                a += adjust;
            }
            
            cpu->f = 0;
            if (a == 0) cpu->f |= FLAG_Z;
            if (carry) cpu->f |= FLAG_C;
            /* H flag is always cleared by DAA */
            
            cpu->a = a;
            break;
        }
        case 0x28: jr_cc_d(cpu, sm83_get_flag(cpu, FLAG_Z), mem); break;
        case 0x29: add_hl_rr(cpu, cpu->hl); break;
        case 0x2A: ldi_a_hl(cpu, mem); break;
        case 0x2B: dec_rr(&cpu->hl); break;
        case 0x2C: inc_r(cpu, &cpu->l); break;
        case 0x2D: dec_r(cpu, &cpu->l); break;
        case 0x2E: ld_r_n(cpu, &cpu->l, mem); break;
        case 0x2F: {
            /* CPL - Complement A register */
            cpu->a = ~cpu->a;
            cpu->f |= FLAG_N | FLAG_H;  /* Set N and H flags */
            break;
        }
        
        /* 0x30-0x3F */
        case 0x30: jr_cc_d(cpu, !sm83_get_flag(cpu, FLAG_C), mem); break;
        case 0x31: ld_rr_nn(cpu, &cpu->sp, mem); break;
        case 0x32: ldd_hl_a(cpu, mem); break;
        case 0x33: inc_rr(&cpu->sp); break;
        case 0x34: {
            uint8_t val = memory_read(mem, cpu->hl);
            inc_r(cpu, &val);
            memory_write(mem, cpu->hl, val);
            break;
        }
        case 0x35: {
            uint8_t val = memory_read(mem, cpu->hl);
            dec_r(cpu, &val);
            memory_write(mem, cpu->hl, val);
            break;
        }
        case 0x36: ld_hl_n(cpu, mem); break;
        case 0x37: scf(cpu); break;
        case 0x38: jr_cc_d(cpu, sm83_get_flag(cpu, FLAG_C), mem); break;
        case 0x39: add_hl_rr(cpu, cpu->sp); break;
        case 0x3A: ldd_a_hl(cpu, mem); break;
        case 0x3B: dec_rr(&cpu->sp); break;
        case 0x3C: inc_r(cpu, &cpu->a); break;
        case 0x3D: dec_r(cpu, &cpu->a); break;
        case 0x3E: ld_r_n(cpu, &cpu->a, mem); break;
        case 0x3F: ccf(cpu); break;
        
        /* 0x40-0x7F - 8-bit load instructions */
        case 0x40: ld_r_r(cpu, &cpu->b, cpu->b); break;
        case 0x41: ld_r_r(cpu, &cpu->b, cpu->c); break;
        case 0x42: ld_r_r(cpu, &cpu->b, cpu->d); break;
        case 0x43: ld_r_r(cpu, &cpu->b, cpu->e); break;
        case 0x44: ld_r_r(cpu, &cpu->b, cpu->h); break;
        case 0x45: ld_r_r(cpu, &cpu->b, cpu->l); break;
        case 0x46: ld_r_hl(cpu, &cpu->b, mem); break;
        case 0x47: ld_r_r(cpu, &cpu->b, cpu->a); break;
        case 0x48: ld_r_r(cpu, &cpu->c, cpu->b); break;
        case 0x49: ld_r_r(cpu, &cpu->c, cpu->c); break;
        case 0x4A: ld_r_r(cpu, &cpu->c, cpu->d); break;
        case 0x4B: ld_r_r(cpu, &cpu->c, cpu->e); break;
        case 0x4C: ld_r_r(cpu, &cpu->c, cpu->h); break;
        case 0x4D: ld_r_r(cpu, &cpu->c, cpu->l); break;
        case 0x4E: ld_r_hl(cpu, &cpu->c, mem); break;
        case 0x4F: ld_r_r(cpu, &cpu->c, cpu->a); break;
        case 0x50: ld_r_r(cpu, &cpu->d, cpu->b); break;
        case 0x51: ld_r_r(cpu, &cpu->d, cpu->c); break;
        case 0x52: ld_r_r(cpu, &cpu->d, cpu->d); break;
        case 0x53: ld_r_r(cpu, &cpu->d, cpu->e); break;
        case 0x54: ld_r_r(cpu, &cpu->d, cpu->h); break;
        case 0x55: ld_r_r(cpu, &cpu->d, cpu->l); break;
        case 0x56: ld_r_hl(cpu, &cpu->d, mem); break;
        case 0x57: ld_r_r(cpu, &cpu->d, cpu->a); break;
        case 0x58: ld_r_r(cpu, &cpu->e, cpu->b); break;
        case 0x59: ld_r_r(cpu, &cpu->e, cpu->c); break;
        case 0x5A: ld_r_r(cpu, &cpu->e, cpu->d); break;
        case 0x5B: ld_r_r(cpu, &cpu->e, cpu->e); break;
        case 0x5C: ld_r_r(cpu, &cpu->e, cpu->h); break;
        case 0x5D: ld_r_r(cpu, &cpu->e, cpu->l); break;
        case 0x5E: ld_r_hl(cpu, &cpu->e, mem); break;
        case 0x5F: ld_r_r(cpu, &cpu->e, cpu->a); break;
        case 0x60: ld_r_r(cpu, &cpu->h, cpu->b); break;
        case 0x61: ld_r_r(cpu, &cpu->h, cpu->c); break;
        case 0x62: ld_r_r(cpu, &cpu->h, cpu->d); break;
        case 0x63: ld_r_r(cpu, &cpu->h, cpu->e); break;
        case 0x64: ld_r_r(cpu, &cpu->h, cpu->h); break;
        case 0x65: ld_r_r(cpu, &cpu->h, cpu->l); break;
        case 0x66: ld_r_hl(cpu, &cpu->h, mem); break;
        case 0x67: ld_r_r(cpu, &cpu->h, cpu->a); break;
        case 0x68: ld_r_r(cpu, &cpu->l, cpu->b); break;
        case 0x69: ld_r_r(cpu, &cpu->l, cpu->c); break;
        case 0x6A: ld_r_r(cpu, &cpu->l, cpu->d); break;
        case 0x6B: ld_r_r(cpu, &cpu->l, cpu->e); break;
        case 0x6C: ld_r_r(cpu, &cpu->l, cpu->h); break;
        case 0x6D: ld_r_r(cpu, &cpu->l, cpu->l); break;
        case 0x6E: ld_r_hl(cpu, &cpu->l, mem); break;
        case 0x6F: ld_r_r(cpu, &cpu->l, cpu->a); break;
        case 0x70: ld_hl_r(cpu, cpu->b, mem); break;
        case 0x71: ld_hl_r(cpu, cpu->c, mem); break;
        case 0x72: ld_hl_r(cpu, cpu->d, mem); break;
        case 0x73: ld_hl_r(cpu, cpu->e, mem); break;
        case 0x74: ld_hl_r(cpu, cpu->h, mem); break;
        case 0x75: ld_hl_r(cpu, cpu->l, mem); break;
        case 0x76: halt(cpu); break;
        case 0x77: ld_hl_r(cpu, cpu->a, mem); break;
        case 0x78: ld_r_r(cpu, &cpu->a, cpu->b); break;
        case 0x79: ld_r_r(cpu, &cpu->a, cpu->c); break;
        case 0x7A: ld_r_r(cpu, &cpu->a, cpu->d); break;
        case 0x7B: ld_r_r(cpu, &cpu->a, cpu->e); break;
        case 0x7C: ld_r_r(cpu, &cpu->a, cpu->h); break;
        case 0x7D: ld_r_r(cpu, &cpu->a, cpu->l); break;
        case 0x7E: ld_r_hl(cpu, &cpu->a, mem); break;
        case 0x7F: ld_r_r(cpu, &cpu->a, cpu->a); break;
        
        /* 0x80-0xBF - Arithmetic/Logic instructions */
        case 0x80: add_a_r(cpu, cpu->b); break;
        case 0x81: add_a_r(cpu, cpu->c); break;
        case 0x82: add_a_r(cpu, cpu->d); break;
        case 0x83: add_a_r(cpu, cpu->e); break;
        case 0x84: add_a_r(cpu, cpu->h); break;
        case 0x85: add_a_r(cpu, cpu->l); break;
        case 0x86: add_a_r(cpu, memory_read(mem, cpu->hl)); break;
        case 0x87: add_a_r(cpu, cpu->a); break;
        case 0x88: adc_a_r(cpu, cpu->b); break;
        case 0x89: adc_a_r(cpu, cpu->c); break;
        case 0x8A: adc_a_r(cpu, cpu->d); break;
        case 0x8B: adc_a_r(cpu, cpu->e); break;
        case 0x8C: adc_a_r(cpu, cpu->h); break;
        case 0x8D: adc_a_r(cpu, cpu->l); break;
        case 0x8E: adc_a_r(cpu, memory_read(mem, cpu->hl)); break;
        case 0x8F: adc_a_r(cpu, cpu->a); break;
        case 0x90: sub_a_r(cpu, cpu->b); break;
        case 0x91: sub_a_r(cpu, cpu->c); break;
        case 0x92: sub_a_r(cpu, cpu->d); break;
        case 0x93: sub_a_r(cpu, cpu->e); break;
        case 0x94: sub_a_r(cpu, cpu->h); break;
        case 0x95: sub_a_r(cpu, cpu->l); break;
        case 0x96: sub_a_r(cpu, memory_read(mem, cpu->hl)); break;
        case 0x97: sub_a_r(cpu, cpu->a); break;
        case 0x98: sbc_a_r(cpu, cpu->b); break;
        case 0x99: sbc_a_r(cpu, cpu->c); break;
        case 0x9A: sbc_a_r(cpu, cpu->d); break;
        case 0x9B: sbc_a_r(cpu, cpu->e); break;
        case 0x9C: sbc_a_r(cpu, cpu->h); break;
        case 0x9D: sbc_a_r(cpu, cpu->l); break;
        case 0x9E: sbc_a_r(cpu, memory_read(mem, cpu->hl)); break;
        case 0x9F: sbc_a_r(cpu, cpu->a); break;
        case 0xA0: and_a_r(cpu, cpu->b); break;
        case 0xA1: and_a_r(cpu, cpu->c); break;
        case 0xA2: and_a_r(cpu, cpu->d); break;
        case 0xA3: and_a_r(cpu, cpu->e); break;
        case 0xA4: and_a_r(cpu, cpu->h); break;
        case 0xA5: and_a_r(cpu, cpu->l); break;
        case 0xA6: and_a_r(cpu, memory_read(mem, cpu->hl)); break;
        case 0xA7: and_a_r(cpu, cpu->a); break;
        case 0xA8: xor_a_r(cpu, cpu->b); break;
        case 0xA9: xor_a_r(cpu, cpu->c); break;
        case 0xAA: xor_a_r(cpu, cpu->d); break;
        case 0xAB: xor_a_r(cpu, cpu->e); break;
        case 0xAC: xor_a_r(cpu, cpu->h); break;
        case 0xAD: xor_a_r(cpu, cpu->l); break;
        case 0xAE: xor_a_r(cpu, memory_read(mem, cpu->hl)); break;
        case 0xAF: xor_a_r(cpu, cpu->a); break;
        case 0xB0: or_a_r(cpu, cpu->b); break;
        case 0xB1: or_a_r(cpu, cpu->c); break;
        case 0xB2: or_a_r(cpu, cpu->d); break;
        case 0xB3: or_a_r(cpu, cpu->e); break;
        case 0xB4: or_a_r(cpu, cpu->h); break;
        case 0xB5: or_a_r(cpu, cpu->l); break;
        case 0xB6: or_a_r(cpu, memory_read(mem, cpu->hl)); break;
        case 0xB7: or_a_r(cpu, cpu->a); break;
        case 0xB8: cp_a_r(cpu, cpu->b); break;
        case 0xB9: cp_a_r(cpu, cpu->c); break;
        case 0xBA: cp_a_r(cpu, cpu->d); break;
        case 0xBB: cp_a_r(cpu, cpu->e); break;
        case 0xBC: cp_a_r(cpu, cpu->h); break;
        case 0xBD: cp_a_r(cpu, cpu->l); break;
        case 0xBE: cp_a_r(cpu, memory_read(mem, cpu->hl)); break;
        case 0xBF: cp_a_r(cpu, cpu->a); break;
        
        /* 0xC0-0xFF - Control and I/O instructions */
        case 0xC0: ret_cc(cpu, !sm83_get_flag(cpu, FLAG_Z), mem); break;
        case 0xC1: pop_rr(cpu, &cpu->bc, mem); break;
        case 0xC2: jp_cc_nn(cpu, !sm83_get_flag(cpu, FLAG_Z), mem); break;
        case 0xC3: jp_nn(cpu, mem); break;
        case 0xC4: call_cc_nn(cpu, !sm83_get_flag(cpu, FLAG_Z), mem); break;
        case 0xC5: push_rr(cpu, cpu->bc, mem); break;
        case 0xC6: add_a_r(cpu, memory_read(mem, cpu->pc++)); break;
        case 0xC7: rst_n(cpu, 0x00, mem); break;
        case 0xC8: ret_cc(cpu, sm83_get_flag(cpu, FLAG_Z), mem); break;
        case 0xC9: ret(cpu, mem); break;
        case 0xCA: jp_cc_nn(cpu, sm83_get_flag(cpu, FLAG_Z), mem); break;
        case 0xCB: {
            /* CB-prefixed instructions */
            uint8_t cb_opcode = memory_read(mem, cpu->pc++);
            cycles = cb_cycles[cb_opcode];
            uint8_t bit = (cb_opcode >> 3) & 0x07;
            uint8_t reg = cb_opcode & 0x07;
            uint8_t op = (cb_opcode >> 3) & 0x07;
            uint8_t* reg_ptr = NULL;
            
            /* Get register pointer */
            switch (reg) {
                case 0: reg_ptr = &cpu->b; break;
                case 1: reg_ptr = &cpu->c; break;
                case 2: reg_ptr = &cpu->d; break;
                case 3: reg_ptr = &cpu->e; break;
                case 4: reg_ptr = &cpu->h; break;
                case 5: reg_ptr = &cpu->l; break;
                case 6: reg_ptr = NULL; break; /* (HL) handled separately */
                case 7: reg_ptr = &cpu->a; break;
            }
            
            /* Handle (HL) memory operations */
            if (reg == 6) {
                uint8_t val = memory_read(mem, cpu->hl);
                
                if ((cb_opcode & 0xC0) == 0x40) { /* BIT */
                    bit_n_r(cpu, bit, val);
                } else if ((cb_opcode & 0xC0) == 0xC0) { /* SET */
                    set_n_r(cpu, bit, &val);
                    memory_write(mem, cpu->hl, val);
                } else if ((cb_opcode & 0xC0) == 0x80) { /* RES */
                    res_n_r(cpu, bit, &val);
                    memory_write(mem, cpu->hl, val);
                } else { /* Rotate/Shift operations */
                    switch (op) {
                        case 0: rlc_r(cpu, &val); break;
                        case 1: rrc_r(cpu, &val); break;
                        case 2: rl_r(cpu, &val); break;
                        case 3: rr_r(cpu, &val); break;
                        case 4: sla_r(cpu, &val); break;
                        case 5: sra_r(cpu, &val); break;
                        case 6: swap_r(cpu, &val); break;
                        case 7: srl_r(cpu, &val); break;
                    }
                    memory_write(mem, cpu->hl, val);
                }
            } else if (reg_ptr) {
                /* Handle register operations */
                if ((cb_opcode & 0xC0) == 0x40) { /* BIT */
                    bit_n_r(cpu, bit, *reg_ptr);
                } else if ((cb_opcode & 0xC0) == 0xC0) { /* SET */
                    set_n_r(cpu, bit, reg_ptr);
                } else if ((cb_opcode & 0xC0) == 0x80) { /* RES */
                    res_n_r(cpu, bit, reg_ptr);
                } else { /* Rotate/Shift operations */
                    switch (op) {
                        case 0: rlc_r(cpu, reg_ptr); break;
                        case 1: rrc_r(cpu, reg_ptr); break;
                        case 2: rl_r(cpu, reg_ptr); break;
                        case 3: rr_r(cpu, reg_ptr); break;
                        case 4: sla_r(cpu, reg_ptr); break;
                        case 5: sra_r(cpu, reg_ptr); break;
                        case 6: swap_r(cpu, reg_ptr); break;
                        case 7: srl_r(cpu, reg_ptr); break;
                    }
                }
            }
            break;
        }
        case 0xCC: call_cc_nn(cpu, sm83_get_flag(cpu, FLAG_Z), mem); break;
        case 0xCD: call_nn(cpu, mem); break;
        case 0xCE: adc_a_r(cpu, memory_read(mem, cpu->pc++)); break;
        case 0xCF: rst_n(cpu, 0x08, mem); break;
        case 0xD0: ret_cc(cpu, !sm83_get_flag(cpu, FLAG_C), mem); break;
        case 0xD1: pop_rr(cpu, &cpu->de, mem); break;
        case 0xD2: jp_cc_nn(cpu, !sm83_get_flag(cpu, FLAG_C), mem); break;
        case 0xD4: call_cc_nn(cpu, !sm83_get_flag(cpu, FLAG_C), mem); break;
        case 0xD5: push_rr(cpu, cpu->de, mem); break;
        case 0xD6: sub_a_r(cpu, memory_read(mem, cpu->pc++)); break;
        case 0xD7: rst_n(cpu, 0x10, mem); break;
        case 0xD8: ret_cc(cpu, sm83_get_flag(cpu, FLAG_C), mem); break;
        case 0xD9: reti(cpu, mem); break;
        case 0xDA: jp_cc_nn(cpu, sm83_get_flag(cpu, FLAG_C), mem); break;
        case 0xDC: call_cc_nn(cpu, sm83_get_flag(cpu, FLAG_C), mem); break;
        case 0xDE: sbc_a_r(cpu, memory_read(mem, cpu->pc++)); break;
        case 0xDF: rst_n(cpu, 0x18, mem); break;
        case 0xE0: ldh_n_a(cpu, mem); break;
        case 0xE1: pop_rr(cpu, &cpu->hl, mem); break;
        case 0xE2: ldh_c_a(cpu, mem); break;
        case 0xE5: push_rr(cpu, cpu->hl, mem); break;
        case 0xE6: and_a_r(cpu, memory_read(mem, cpu->pc++)); break;
        case 0xE7: rst_n(cpu, 0x20, mem); break;
        case 0xE8: add_sp_d(cpu, mem); break;
        case 0xE9: jp_hl(cpu); break;
        case 0xEA: ld_nn_a(cpu, mem); break;
        case 0xEE: xor_a_r(cpu, memory_read(mem, cpu->pc++)); break;
        case 0xEF: rst_n(cpu, 0x28, mem); break;
        case 0xF0: ldh_a_n(cpu, mem); break;
        case 0xF1: {
            pop_rr(cpu, &cpu->af, mem);
            cpu->f &= 0xF0; /* Lower 4 bits always zero */
            break;
        }
        case 0xF2: ldh_a_c(cpu, mem); break;
        case 0xF3: di(cpu); break;
        case 0xF5: push_rr(cpu, cpu->af, mem); break;
        case 0xF6: or_a_r(cpu, memory_read(mem, cpu->pc++)); break;
        case 0xF7: rst_n(cpu, 0x30, mem); break;
        case 0xF8: ld_hl_sp_d(cpu, mem); break;
        case 0xF9: ld_sp_hl(cpu); break;
        case 0xFA: ld_a_nn(cpu, mem); break;
        case 0xFB: ei(cpu); executed_ei = true; break;
        case 0xFE: cp_a_r(cpu, memory_read(mem, cpu->pc++)); break;
        case 0xFF: rst_n(cpu, 0x38, mem); break;
        
        default:
            /* Handle invalid opcode */
            ui_debug_log(UI_DEBUG_CPU, "[CPU] ERROR: Invalid opcode 0x%02X at PC=0x%04X", opcode, pc_before);
            return -1;
    }

    sm83_add_cycles(cpu, cycles);
    
    /* Apply EI delay now: enable IME after completing the instruction
       following EI (not the EI instruction itself). */
    if (cpu->ei_delay && !executed_ei) {
        cpu->ei_delay = false;
        cpu->ime = true;
    }
    
    /* Debug output - show state after execution */
    GBEmulator* gb = gb_get_debug_gb();
    if (gb) {
        /* Show every instruction for first 1000 cycles, then every 1000th cycle */
        bool should_log = (gb->cycles < 1000) || (gb->cycles % 1000 == 0);
        
        if (should_log) {
            char debug_msg[512];
            int pos = snprintf(debug_msg, sizeof(debug_msg),
                "[CPU] PC:0x%04X->0x%04X Op:0x%02X Cyc:%u AF:0x%04X BC:0x%04X DE:0x%04X HL:0x%04X SP:0x%04X F:Z%cN%cH%cC%c",
                pc_before, cpu->pc, opcode, cycles,
                cpu->af, cpu->bc, cpu->de, cpu->hl, cpu->sp,
                (cpu->f & FLAG_Z) ? '1' : '0',
                (cpu->f & FLAG_N) ? '1' : '0',
                (cpu->f & FLAG_H) ? '1' : '0',
                (cpu->f & FLAG_C) ? '1' : '0');
            
            /* Show register changes */
            if (af_before != cpu->af || bc_before != cpu->bc || 
                de_before != cpu->de || hl_before != cpu->hl || 
                sp_before != cpu->sp) {
                pos += snprintf(debug_msg + pos, sizeof(debug_msg) - pos, " [");
                if (af_before != cpu->af) pos += snprintf(debug_msg + pos, sizeof(debug_msg) - pos, "AF:0x%04X->0x%04X ", af_before, cpu->af);
                if (bc_before != cpu->bc) pos += snprintf(debug_msg + pos, sizeof(debug_msg) - pos, "BC:0x%04X->0x%04X ", bc_before, cpu->bc);
                if (de_before != cpu->de) pos += snprintf(debug_msg + pos, sizeof(debug_msg) - pos, "DE:0x%04X->0x%04X ", de_before, cpu->de);
                if (hl_before != cpu->hl) pos += snprintf(debug_msg + pos, sizeof(debug_msg) - pos, "HL:0x%04X->0x%04X ", hl_before, cpu->hl);
                if (sp_before != cpu->sp) pos += snprintf(debug_msg + pos, sizeof(debug_msg) - pos, "SP:0x%04X->0x%04X ", sp_before, cpu->sp);
                pos += snprintf(debug_msg + pos, sizeof(debug_msg) - pos, "]");
            }
            
            /* Show instruction-specific details */
            if (opcode == 0xF0) {
                uint8_t offset = memory_read(mem, pc_before + 1);
                uint16_t io_addr = 0xFF00 | offset;
                uint8_t io_value = memory_read(mem, io_addr);
                snprintf(debug_msg + pos, sizeof(debug_msg) - pos, " [LDH A,(0x%04X)=0x%02X]", io_addr, io_value);
            } else if (opcode == 0xE0) {
                uint8_t offset = memory_read(mem, pc_before + 1);
                uint16_t io_addr = 0xFF00 | offset;
                snprintf(debug_msg + pos, sizeof(debug_msg) - pos, " [LDH (0x%04X),A=0x%02X]", io_addr, cpu->a);
            } else if (opcode == 0xFE) {
                uint8_t compare_val = memory_read(mem, pc_before + 1);
                snprintf(debug_msg + pos, sizeof(debug_msg) - pos, " [CP A,0x%02X A=0x%02X]", compare_val, cpu->a);
            } else if (opcode == 0x20 || opcode == 0x28 || opcode == 0x30 || opcode == 0x38) {
                int8_t offset = (int8_t)memory_read(mem, pc_before + 1);
                bool condition_met = false;
                if (opcode == 0x20) condition_met = !(cpu->f & FLAG_Z);
                else if (opcode == 0x28) condition_met = (cpu->f & FLAG_Z);
                else if (opcode == 0x30) condition_met = !(cpu->f & FLAG_C);
                else if (opcode == 0x38) condition_met = (cpu->f & FLAG_C);
                snprintf(debug_msg + pos, sizeof(debug_msg) - pos, " [JR%s %+d %s]",
                    (opcode == 0x20) ? "NZ" : (opcode == 0x28) ? "Z" : (opcode == 0x30) ? "NC" : "C",
                    offset, condition_met ? "TAKEN" : "NOT TAKEN");
            }
            
            ui_debug_log(UI_DEBUG_CPU, "%s", debug_msg);
        }
    }
    
    return cycles;
}