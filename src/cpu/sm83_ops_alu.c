#include "sm83_ops.h"
#include "../memory/memory.h"

/* 16-bit Load Instructions */
void ld_rr_nn(SM83_CPU* cpu, uint16_t* rp, Memory* mem) {
    uint8_t low = memory_read(mem, cpu->pc++);
    uint8_t high = memory_read(mem, cpu->pc++);
    *rp = (high << 8) | low;
}

void ld_nn_sp(SM83_CPU* cpu, Memory* mem) {
    uint16_t addr = memory_read(mem, cpu->pc++);
    addr |= memory_read(mem, cpu->pc++) << 8;
    memory_write(mem, addr, cpu->sp & 0xFF);
    memory_write(mem, addr + 1, cpu->sp >> 8);
}

void ld_sp_hl(SM83_CPU* cpu) {
    cpu->sp = cpu->hl;
}

void push_rr(SM83_CPU* cpu, uint16_t val, Memory* mem) {
    cpu->sp -= 2;
    memory_write(mem, cpu->sp + 1, val >> 8);
    memory_write(mem, cpu->sp, val & 0xFF);
}

void pop_rr(SM83_CPU* cpu, uint16_t* rp, Memory* mem) {
    uint8_t low = memory_read(mem, cpu->sp++);
    uint8_t high = memory_read(mem, cpu->sp++);
    *rp = (high << 8) | low;
}

/* 8-bit Arithmetic/Logic Instructions */
void add_a_r(SM83_CPU* cpu, uint8_t val) {
    uint16_t result = cpu->a + val;
    uint8_t half_carry = ((cpu->a & 0xF) + (val & 0xF)) & 0x10;
    
    cpu->f = 0;
    if ((result & 0xFF) == 0) cpu->f |= FLAG_Z;
    if (half_carry) cpu->f |= FLAG_H;
    if (result > 0xFF) cpu->f |= FLAG_C;
    
    cpu->a = result & 0xFF;
}

void adc_a_r(SM83_CPU* cpu, uint8_t val) {
    uint8_t carry = (cpu->f & FLAG_C) ? 1 : 0;
    uint16_t result = cpu->a + val + carry;
    uint8_t half_carry = ((cpu->a & 0xF) + (val & 0xF) + carry) & 0x10;
    
    cpu->f = 0;
    if ((result & 0xFF) == 0) cpu->f |= FLAG_Z;
    if (half_carry) cpu->f |= FLAG_H;
    if (result > 0xFF) cpu->f |= FLAG_C;
    
    cpu->a = result & 0xFF;
}

void sub_a_r(SM83_CPU* cpu, uint8_t val) {
    uint16_t result = cpu->a - val;
    uint8_t half_carry = ((cpu->a & 0xF) - (val & 0xF)) & 0x10;
    
    cpu->f = FLAG_N;
    if ((result & 0xFF) == 0) cpu->f |= FLAG_Z;
    if (half_carry) cpu->f |= FLAG_H;
    if (result & 0x100) cpu->f |= FLAG_C;
    
    cpu->a = result & 0xFF;
}

void sbc_a_r(SM83_CPU* cpu, uint8_t val) {
    uint8_t carry = (cpu->f & FLAG_C) ? 1 : 0;
    uint16_t result = cpu->a - val - carry;
    uint8_t half_carry = ((cpu->a & 0xF) - (val & 0xF) - carry) & 0x10;
    
    cpu->f = FLAG_N;
    if ((result & 0xFF) == 0) cpu->f |= FLAG_Z;
    if (half_carry) cpu->f |= FLAG_H;
    if (result & 0x100) cpu->f |= FLAG_C;
    
    cpu->a = result & 0xFF;
}

void and_a_r(SM83_CPU* cpu, uint8_t val) {
    cpu->a &= val;
    cpu->f = FLAG_H;
    if (cpu->a == 0) cpu->f |= FLAG_Z;
}

void xor_a_r(SM83_CPU* cpu, uint8_t val) {
    cpu->a ^= val;
    cpu->f = 0;
    if (cpu->a == 0) cpu->f |= FLAG_Z;
}

void or_a_r(SM83_CPU* cpu, uint8_t val) {
    cpu->a |= val;
    cpu->f = 0;
    if (cpu->a == 0) cpu->f |= FLAG_Z;
}

void cp_a_r(SM83_CPU* cpu, uint8_t val) {
    uint16_t result = cpu->a - val;
    uint8_t half_carry = ((cpu->a & 0xF) - (val & 0xF)) & 0x10;
    
    cpu->f = FLAG_N;
    if ((result & 0xFF) == 0) cpu->f |= FLAG_Z;
    if (half_carry) cpu->f |= FLAG_H;
    if (result & 0x100) cpu->f |= FLAG_C;
}

void inc_r(SM83_CPU* cpu, uint8_t* reg) {
    uint8_t result = *reg + 1;
    uint8_t half_carry = (*reg & 0xF) == 0xF;
    
    cpu->f &= FLAG_C;  /* Preserve carry flag */
    if (result == 0) cpu->f |= FLAG_Z;
    if (half_carry) cpu->f |= FLAG_H;
    
    *reg = result;
}

void dec_r(SM83_CPU* cpu, uint8_t* reg) {
    uint8_t result = *reg - 1;
    uint8_t half_carry = (*reg & 0xF) == 0;
    
    cpu->f &= FLAG_C;  /* Preserve carry flag */
    cpu->f |= FLAG_N;
    if (result == 0) cpu->f |= FLAG_Z;
    if (half_carry) cpu->f |= FLAG_H;
    
    *reg = result;
}

/* 16-bit Arithmetic Instructions */
void add_hl_rr(SM83_CPU* cpu, uint16_t val) {
    uint32_t result = cpu->hl + val;
    uint16_t half_carry = ((cpu->hl & 0xFFF) + (val & 0xFFF)) & 0x1000;
    
    cpu->f &= FLAG_Z;  /* Preserve zero flag */
    if (half_carry) cpu->f |= FLAG_H;
    if (result & 0x10000) cpu->f |= FLAG_C;
    
    cpu->hl = result & 0xFFFF;
}

void inc_rr(uint16_t* rr) {
    (*rr)++;
}

void dec_rr(uint16_t* rr) {
    (*rr)--;
}

void add_sp_d(SM83_CPU* cpu, Memory* mem) {
    int8_t offset = (int8_t)memory_read(mem, cpu->pc++);
    uint32_t result = cpu->sp + offset;
    
    cpu->f = 0;
    if ((cpu->sp & 0xF) + (offset & 0xF) > 0xF) cpu->f |= FLAG_H;
    if ((cpu->sp & 0xFF) + (offset & 0xFF) > 0xFF) cpu->f |= FLAG_C;
    
    cpu->sp = result & 0xFFFF;
}

void ld_hl_sp_d(SM83_CPU* cpu, Memory* mem) {
    int8_t offset = (int8_t)memory_read(mem, cpu->pc++);
    uint32_t result = cpu->sp + offset;
    
    cpu->f = 0;
    if ((cpu->sp & 0xF) + (offset & 0xF) > 0xF) cpu->f |= FLAG_H;
    if ((cpu->sp & 0xFF) + (offset & 0xFF) > 0xFF) cpu->f |= FLAG_C;
    
    cpu->hl = result & 0xFFFF;
}