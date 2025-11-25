#include "sm83_ops.h"
#include "../memory/memory.h"

/* Rotate and Shift Instructions */
void rlca(SM83_CPU* cpu) {
    uint8_t carry = cpu->a >> 7;
    cpu->a = (cpu->a << 1) | carry;
    
    cpu->f = 0;
    if (carry) cpu->f |= FLAG_C;
}

void rla(SM83_CPU* cpu) {
    uint8_t old_carry = (cpu->f & FLAG_C) ? 1 : 0;
    uint8_t new_carry = cpu->a >> 7;
    
    cpu->a = (cpu->a << 1) | old_carry;
    cpu->f = 0;
    if (new_carry) cpu->f |= FLAG_C;
}

void rrca(SM83_CPU* cpu) {
    uint8_t carry = cpu->a & 1;
    cpu->a = (cpu->a >> 1) | (carry << 7);
    
    cpu->f = 0;
    if (carry) cpu->f |= FLAG_C;
}

void rra(SM83_CPU* cpu) {
    uint8_t old_carry = (cpu->f & FLAG_C) ? 0x80 : 0;
    uint8_t new_carry = cpu->a & 1;
    
    cpu->a = (cpu->a >> 1) | old_carry;
    cpu->f = 0;
    if (new_carry) cpu->f |= FLAG_C;
}

/* CB-prefixed Instructions */
void rlc_r(SM83_CPU* cpu, uint8_t* reg) {
    uint8_t carry = *reg >> 7;
    *reg = (*reg << 1) | carry;
    
    cpu->f = 0;
    if (*reg == 0) cpu->f |= FLAG_Z;
    if (carry) cpu->f |= FLAG_C;
}

void rrc_r(SM83_CPU* cpu, uint8_t* reg) {
    uint8_t carry = *reg & 1;
    *reg = (*reg >> 1) | (carry << 7);
    
    cpu->f = 0;
    if (*reg == 0) cpu->f |= FLAG_Z;
    if (carry) cpu->f |= FLAG_C;
}

void rl_r(SM83_CPU* cpu, uint8_t* reg) {
    uint8_t old_carry = (cpu->f & FLAG_C) ? 1 : 0;
    uint8_t new_carry = *reg >> 7;
    
    *reg = (*reg << 1) | old_carry;
    cpu->f = 0;
    if (*reg == 0) cpu->f |= FLAG_Z;
    if (new_carry) cpu->f |= FLAG_C;
}

void rr_r(SM83_CPU* cpu, uint8_t* reg) {
    uint8_t old_carry = (cpu->f & FLAG_C) ? 0x80 : 0;
    uint8_t new_carry = *reg & 1;
    
    *reg = (*reg >> 1) | old_carry;
    cpu->f = 0;
    if (*reg == 0) cpu->f |= FLAG_Z;
    if (new_carry) cpu->f |= FLAG_C;
}

void sla_r(SM83_CPU* cpu, uint8_t* reg) {
    uint8_t carry = *reg >> 7;
    *reg <<= 1;
    
    cpu->f = 0;
    if (*reg == 0) cpu->f |= FLAG_Z;
    if (carry) cpu->f |= FLAG_C;
}

void sra_r(SM83_CPU* cpu, uint8_t* reg) {
    uint8_t carry = *reg & 1;
    uint8_t msb = *reg & 0x80;
    *reg = (*reg >> 1) | msb;
    
    cpu->f = 0;
    if (*reg == 0) cpu->f |= FLAG_Z;
    if (carry) cpu->f |= FLAG_C;
}

void swap_r(SM83_CPU* cpu, uint8_t* reg) {
    *reg = (*reg >> 4) | (*reg << 4);
    
    cpu->f = 0;
    if (*reg == 0) cpu->f |= FLAG_Z;
}

void srl_r(SM83_CPU* cpu, uint8_t* reg) {
    uint8_t carry = *reg & 1;
    *reg >>= 1;
    
    cpu->f = 0;
    if (*reg == 0) cpu->f |= FLAG_Z;
    if (carry) cpu->f |= FLAG_C;
}

void bit_n_r(SM83_CPU* cpu, uint8_t bit, uint8_t val) {
    cpu->f &= FLAG_C;  /* Preserve carry flag */
    cpu->f |= FLAG_H;
    
    if (!(val & (1 << bit))) cpu->f |= FLAG_Z;
}

void set_n_r(SM83_CPU* cpu, uint8_t bit, uint8_t* reg) {
    (void)cpu;  /* Parameter not needed for this operation */
    *reg |= (1 << bit);
}

void res_n_r(SM83_CPU* cpu, uint8_t bit, uint8_t* reg) {
    (void)cpu;  /* Parameter not needed for this operation */
    *reg &= ~(1 << bit);
}