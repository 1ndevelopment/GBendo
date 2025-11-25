#include "sm83_ops.h"
#include "../memory/memory.h"

/* 8-bit Load Instructions */
void ld_r_r(SM83_CPU* cpu, uint8_t* dest, uint8_t src) {
    (void)cpu;  /* Parameter not needed for this operation */
    *dest = src;
}

void ld_r_n(SM83_CPU* cpu, uint8_t* reg, Memory* mem) {
    *reg = memory_read(mem, cpu->pc++);
}

void ld_r_hl(SM83_CPU* cpu, uint8_t* reg, Memory* mem) {
    *reg = memory_read(mem, cpu->hl);
}

void ld_hl_r(SM83_CPU* cpu, uint8_t val, Memory* mem) {
    memory_write(mem, cpu->hl, val);
}

void ld_hl_n(SM83_CPU* cpu, Memory* mem) {
    uint8_t n = memory_read(mem, cpu->pc++);
    memory_write(mem, cpu->hl, n);
}

void ld_a_bc(SM83_CPU* cpu, Memory* mem) {
    cpu->a = memory_read(mem, cpu->bc);
}

void ld_a_de(SM83_CPU* cpu, Memory* mem) {
    cpu->a = memory_read(mem, cpu->de);
}

void ld_bc_a(SM83_CPU* cpu, Memory* mem) {
    memory_write(mem, cpu->bc, cpu->a);
}

void ld_de_a(SM83_CPU* cpu, Memory* mem) {
    memory_write(mem, cpu->de, cpu->a);
}

void ld_a_nn(SM83_CPU* cpu, Memory* mem) {
    uint16_t addr = memory_read(mem, cpu->pc++);
    addr |= memory_read(mem, cpu->pc++) << 8;
    cpu->a = memory_read(mem, addr);
}

void ld_nn_a(SM83_CPU* cpu, Memory* mem) {
    uint16_t addr = memory_read(mem, cpu->pc++);
    addr |= memory_read(mem, cpu->pc++) << 8;
    memory_write(mem, addr, cpu->a);
}

/* Special A register load/store instructions */
void ldh_a_n(SM83_CPU* cpu, Memory* mem) {
    uint8_t offset = memory_read(mem, cpu->pc++);
    cpu->a = memory_read(mem, 0xFF00 | offset);
}

void ldh_n_a(SM83_CPU* cpu, Memory* mem) {
    uint8_t offset = memory_read(mem, cpu->pc++);
    memory_write(mem, 0xFF00 | offset, cpu->a);
}

void ldh_a_c(SM83_CPU* cpu, Memory* mem) {
    cpu->a = memory_read(mem, 0xFF00 | cpu->c);
}

void ldh_c_a(SM83_CPU* cpu, Memory* mem) {
    memory_write(mem, 0xFF00 | cpu->c, cpu->a);
}

void ldi_a_hl(SM83_CPU* cpu, Memory* mem) {
    cpu->a = memory_read(mem, cpu->hl++);
}

void ldi_hl_a(SM83_CPU* cpu, Memory* mem) {
    memory_write(mem, cpu->hl++, cpu->a);
}

void ldd_a_hl(SM83_CPU* cpu, Memory* mem) {
    cpu->a = memory_read(mem, cpu->hl--);
}

void ldd_hl_a(SM83_CPU* cpu, Memory* mem) {
    memory_write(mem, cpu->hl--, cpu->a);
}