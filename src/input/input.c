#include "input.h"
#include <string.h>

/* Memory stores JOYP at io_registers[0x00] (0xFF00) — bits 4-5 are the select bits,
 * bits 0-3 are read as active-low input lines. We keep two internal masks for
 * direction and button groups (bits low-active): 1 = pressed, 0 = released.
 */

void input_init(Memory* mem) {
    if (!mem) return;
    /* Ensure JOYP default: both selects high (no group selected) */
    /* Default JOYP: all bits high (no buttons pressed, no group selected) */
    mem->io_registers[0x00] = 0xFF;
    /* We'll keep internal state in high bits of hram? Better add fields in Memory struct. */
}

/* Internal helper: check group visibility based on P14/P15 */
static bool is_group_visible(Memory* mem, bool is_buttons) {
    uint8_t reg = mem->io_registers[0x00];
    bool select_buttons = !(reg & (1 << 5)); /* P15 low means buttons selected */
    bool select_dirs = !(reg & (1 << 4));    /* P14 low means directions selected */
    if (!select_buttons && !select_dirs) return true;
    if (select_buttons && select_dirs) return true;
    return is_buttons ? select_buttons : select_dirs;
}

/* JOYP is updated by memory_update_joyp (in memory.c). */

void input_press(Memory* mem, Joypad_Button b) {
    if (!mem) return;
    bool is_button = false;
    uint8_t old_input = input_read_joyp(mem) & 0x0F;

    /* Map logical button into internal groups */
    switch (b) {
        case JP_A: mem->joypad_state_buttons |= 0x01; is_button = true; break;
        case JP_B: mem->joypad_state_buttons |= 0x02; is_button = true; break;
        case JP_SELECT: mem->joypad_state_buttons |= 0x04; is_button = true; break;
        case JP_START: mem->joypad_state_buttons |= 0x08; is_button = true; break;
        case JP_RIGHT: mem->joypad_state_dirs |= 0x01; break;
        case JP_LEFT: mem->joypad_state_dirs |= 0x02; break;
        case JP_UP: mem->joypad_state_dirs |= 0x04; break;
        case JP_DOWN: mem->joypad_state_dirs |= 0x08; break;
    }

    /* Update visible register and if this group is visible, trigger interrupt */
    memory_update_joyp(mem);
    if (is_group_visible(mem, is_button)) {
        uint8_t new_input = input_read_joyp(mem) & 0x0F;
        uint8_t fell = (old_input & ~new_input) & 0x0F;
        if (fell) mem->io_registers[0x0F] |= 0x10;
    }
}

void input_release(Memory* mem, Joypad_Button b) {
    if (!mem) return;
    switch (b) {
        case JP_A: mem->joypad_state_buttons &= ~0x01; break;
        case JP_B: mem->joypad_state_buttons &= ~0x02; break;
        case JP_SELECT: mem->joypad_state_buttons &= ~0x04; break;
        case JP_START: mem->joypad_state_buttons &= ~0x08; break;
        case JP_RIGHT: mem->joypad_state_dirs &= ~0x01; break;
        case JP_LEFT: mem->joypad_state_dirs &= ~0x02; break;
        case JP_UP: mem->joypad_state_dirs &= ~0x04; break;
        case JP_DOWN: mem->joypad_state_dirs &= ~0x08; break;
    }
    memory_update_joyp(mem);
}

void input_set_state(Memory* mem, uint8_t state_mask) {
    if (!mem) return;
    /* Lower nibble: directions (Right/Left/Up/Down), upper nibble of mask: buttons
     * We'll accept a mask where bits 0-3 map to directions and bits 4-7 map to buttons
     */
    mem->joypad_state_dirs = state_mask & 0x0F;
    mem->joypad_state_buttons = (state_mask >> 4) & 0x0F;
    memory_update_joyp(mem);
}

uint8_t input_read_joyp(Memory* mem) {
    if (!mem) return 0xFF;
    return mem->io_registers[0x00];
}
