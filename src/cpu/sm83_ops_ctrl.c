#include "sm83_ops.h"
#include "sm83.h"
#include "../memory/memory.h"
#include "../gbendo.h"
#include <stdio.h>

/* Control Instructions */
void ccf(SM83_CPU* cpu) {
    cpu->f &= FLAG_Z;  /* Preserve zero flag */
    if (!(cpu->f & FLAG_C)) cpu->f |= FLAG_C;
}

void scf(SM83_CPU* cpu) {
    cpu->f &= FLAG_Z;  /* Preserve zero flag */
    cpu->f |= FLAG_C;
}

void nop(SM83_CPU* cpu) {
    (void)cpu;  /* Parameter not needed for this operation */
    /* Do nothing */
}

void halt(SM83_CPU* cpu) {
    /* Implement HALT bug if necessary */
    cpu->halted = true;
}

void stop(SM83_CPU* cpu) {
    cpu->stopped = true;
}

void di(SM83_CPU* cpu) {
    cpu->ime = false;
    if (gb_is_debug_enabled()) {
        GBEmulator* gb = gb_get_debug_gb();
        if (gb) {
            printf("[CPU] DI - Interrupts disabled at PC=0x%04X\n", cpu->pc - 1);
        }
    }
}

void ei(SM83_CPU* cpu) {
    /* EI actually enables interrupts after the next instruction */
    cpu->ei_delay = true;
    if (gb_is_debug_enabled()) {
        GBEmulator* gb = gb_get_debug_gb();
        if (gb) {
            printf("[CPU] EI - Interrupts will be enabled after next instruction at PC=0x%04X\n", cpu->pc - 1);
        }
    }
}

/* Jump Instructions */
void jp_nn(SM83_CPU* cpu, Memory* mem) {
    uint8_t low = memory_read(mem, cpu->pc++);
    uint8_t high = memory_read(mem, cpu->pc++);
    cpu->pc = (high << 8) | low;
}

void jp_cc_nn(SM83_CPU* cpu, bool condition, Memory* mem) {
    uint8_t low = memory_read(mem, cpu->pc++);
    uint8_t high = memory_read(mem, cpu->pc++);
    
    if (condition) {
        cpu->pc = (high << 8) | low;
        cpu->cycles += 4;  /* Extra cycles if jump taken */
    }
}

void jp_hl(SM83_CPU* cpu) {
    cpu->pc = cpu->hl;
}

void jr_d(SM83_CPU* cpu, Memory* mem) {
    int8_t offset = (int8_t)memory_read(mem, cpu->pc++);
    cpu->pc += offset;
}

void jr_cc_d(SM83_CPU* cpu, bool condition, Memory* mem) {
    int8_t offset = (int8_t)memory_read(mem, cpu->pc++);
    
    if (condition) {
        cpu->pc += offset;
        cpu->cycles += 4;  /* Extra cycles if jump taken */
    }
}

/* Call and Return Instructions */
void call_nn(SM83_CPU* cpu, Memory* mem) {
    uint16_t addr = memory_read(mem, cpu->pc++);
    addr |= memory_read(mem, cpu->pc++) << 8;
    
    cpu->sp -= 2;
    memory_write(mem, cpu->sp + 1, cpu->pc >> 8);
    memory_write(mem, cpu->sp, cpu->pc & 0xFF);
    
    cpu->pc = addr;
}

void call_cc_nn(SM83_CPU* cpu, bool condition, Memory* mem) {
    uint16_t addr = memory_read(mem, cpu->pc++);
    addr |= memory_read(mem, cpu->pc++) << 8;
    
    if (condition) {
        cpu->sp -= 2;
        memory_write(mem, cpu->sp + 1, cpu->pc >> 8);
        memory_write(mem, cpu->sp, cpu->pc & 0xFF);
        
        cpu->pc = addr;
        cpu->cycles += 12;  /* Extra cycles if call taken */
    }
}

void ret(SM83_CPU* cpu, Memory* mem) {
    cpu->pc = memory_read(mem, cpu->sp++);
    cpu->pc |= memory_read(mem, cpu->sp++) << 8;
}

void ret_cc(SM83_CPU* cpu, bool condition, Memory* mem) {
    if (condition) {
        cpu->pc = memory_read(mem, cpu->sp++);
        cpu->pc |= memory_read(mem, cpu->sp++) << 8;
        cpu->cycles += 12;  /* Extra cycles if return taken */
    }
}

void reti(SM83_CPU* cpu, Memory* mem) {
    ret(cpu, mem);
    cpu->ime = true;  /* Enable interrupts immediately */
}

void rst_n(SM83_CPU* cpu, uint8_t vector, Memory* mem) {
    cpu->sp -= 2;
    memory_write(mem, cpu->sp + 1, cpu->pc >> 8);
    memory_write(mem, cpu->sp, cpu->pc & 0xFF);
    
    cpu->pc = vector;
}