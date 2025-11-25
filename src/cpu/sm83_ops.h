#ifndef SM83_OPS_H
#define SM83_OPS_H

#include "sm83.h"

/* Forward declaration */
typedef struct Memory Memory;

/* 8-bit Load Instructions */
void ld_r_r(SM83_CPU* cpu, uint8_t* dest, uint8_t src);
void ld_r_n(SM83_CPU* cpu, uint8_t* reg, Memory* mem);
void ld_r_hl(SM83_CPU* cpu, uint8_t* reg, Memory* mem);
void ld_hl_r(SM83_CPU* cpu, uint8_t val, Memory* mem);
void ld_hl_n(SM83_CPU* cpu, Memory* mem);
void ld_a_bc(SM83_CPU* cpu, Memory* mem);
void ld_a_de(SM83_CPU* cpu, Memory* mem);
void ld_bc_a(SM83_CPU* cpu, Memory* mem);
void ld_de_a(SM83_CPU* cpu, Memory* mem);
void ld_a_nn(SM83_CPU* cpu, Memory* mem);
void ld_nn_a(SM83_CPU* cpu, Memory* mem);

/* Special A register instructions */
void ldh_a_n(SM83_CPU* cpu, Memory* mem);
void ldh_n_a(SM83_CPU* cpu, Memory* mem);
void ldh_a_c(SM83_CPU* cpu, Memory* mem);
void ldh_c_a(SM83_CPU* cpu, Memory* mem);
void ldi_a_hl(SM83_CPU* cpu, Memory* mem);
void ldi_hl_a(SM83_CPU* cpu, Memory* mem);
void ldd_a_hl(SM83_CPU* cpu, Memory* mem);
void ldd_hl_a(SM83_CPU* cpu, Memory* mem);

/* 16-bit Load Instructions */
void ld_sp_hl(SM83_CPU* cpu);
void ld_rr_nn(SM83_CPU* cpu, uint16_t* rp, Memory* mem);
void ld_nn_sp(SM83_CPU* cpu, Memory* mem);

/* 8-bit Arithmetic/Logic Instructions */
void add_a_r(SM83_CPU* cpu, uint8_t val);
void adc_a_r(SM83_CPU* cpu, uint8_t val);
void sub_a_r(SM83_CPU* cpu, uint8_t val);
void sbc_a_r(SM83_CPU* cpu, uint8_t val);
void and_a_r(SM83_CPU* cpu, uint8_t val);
void xor_a_r(SM83_CPU* cpu, uint8_t val);
void or_a_r(SM83_CPU* cpu, uint8_t val);
void cp_a_r(SM83_CPU* cpu, uint8_t val);
void inc_r(SM83_CPU* cpu, uint8_t* reg);
void dec_r(SM83_CPU* cpu, uint8_t* reg);

/* 16-bit Arithmetic Instructions */
void add_hl_rr(SM83_CPU* cpu, uint16_t val);
void inc_rr(uint16_t* rr);
void dec_rr(uint16_t* rr);
void add_sp_d(SM83_CPU* cpu, Memory* mem);
void ld_hl_sp_d(SM83_CPU* cpu, Memory* mem);

/* Rotate and Shift Instructions */
void rlca(SM83_CPU* cpu);
void rla(SM83_CPU* cpu);
void rrca(SM83_CPU* cpu);
void rra(SM83_CPU* cpu);

/* CB-prefixed Instructions */
void rlc_r(SM83_CPU* cpu, uint8_t* reg);
void rrc_r(SM83_CPU* cpu, uint8_t* reg);
void rl_r(SM83_CPU* cpu, uint8_t* reg);
void rr_r(SM83_CPU* cpu, uint8_t* reg);
void sla_r(SM83_CPU* cpu, uint8_t* reg);
void sra_r(SM83_CPU* cpu, uint8_t* reg);
void swap_r(SM83_CPU* cpu, uint8_t* reg);
void srl_r(SM83_CPU* cpu, uint8_t* reg);
void bit_n_r(SM83_CPU* cpu, uint8_t bit, uint8_t val);
void set_n_r(SM83_CPU* cpu, uint8_t bit, uint8_t* reg);
void res_n_r(SM83_CPU* cpu, uint8_t bit, uint8_t* reg);

/* Control Instructions */
void ccf(SM83_CPU* cpu);
void scf(SM83_CPU* cpu);
void nop(SM83_CPU* cpu);
void halt(SM83_CPU* cpu);
void stop(SM83_CPU* cpu);
void di(SM83_CPU* cpu);
void ei(SM83_CPU* cpu);

/* Jump Instructions */
void jp_hl(SM83_CPU* cpu);
void jp_nn(SM83_CPU* cpu, Memory* mem);
void jp_cc_nn(SM83_CPU* cpu, bool condition, Memory* mem);
void jr_d(SM83_CPU* cpu, Memory* mem);
void jr_cc_d(SM83_CPU* cpu, bool condition, Memory* mem);

/* Call and Return Instructions */
void call_nn(SM83_CPU* cpu, Memory* mem);
void call_cc_nn(SM83_CPU* cpu, bool condition, Memory* mem);
void ret(SM83_CPU* cpu, Memory* mem);
void ret_cc(SM83_CPU* cpu, bool condition, Memory* mem);
void reti(SM83_CPU* cpu, Memory* mem);
void rst_n(SM83_CPU* cpu, uint8_t vector, Memory* mem);

#endif /* SM83_OPS_H */