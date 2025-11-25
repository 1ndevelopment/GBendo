#ifndef GB_INPUT_H
#define GB_INPUT_H

#include <stdint.h>
#include "../memory/memory.h"

/* Joypad buttons (logical) */
typedef enum {
    JP_RIGHT = 0x01,
    JP_LEFT  = 0x02,
    JP_UP    = 0x04,
    JP_DOWN  = 0x08,
    JP_A     = 0x10,
    JP_B     = 0x20,
    JP_SELECT= 0x40,
    JP_START = 0x80
} Joypad_Button;

/* Initialize input subsystem (attach to memory) */
void input_init(Memory* mem);

/* Press / release logical button */
void input_press(Memory* mem, Joypad_Button b);
void input_release(Memory* mem, Joypad_Button b);

/* Helper to set whole state (bits as Joypad_Button) */
void input_set_state(Memory* mem, uint8_t state_mask);

/* Read current visible JOYP register (returns 0..255 as stored in 0xFF00) */
uint8_t input_read_joyp(Memory* mem);

/* Note: JOYP updating is implemented in memory module; input calls memory_update_joyp via memory.h */
#endif /* GB_INPUT_H */
