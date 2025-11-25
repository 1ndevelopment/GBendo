#ifndef GB_WINDOW_H
#define GB_WINDOW_H

#include <stdint.h>
#include <stdbool.h>

struct Memory;

/* Initialize the video/window subsystem.
 * scale: integer scale factor for the 160x144 framebuffer (default: 3)
 * fullscreen: if true, create fullscreen window
 * vsync: if true, enable vsync (default: true)
 */
bool window_init(int scale, bool fullscreen, bool vsync);
void window_destroy(void);

/* Present a frame. framebuffer must be SCREEN_WIDTH * SCREEN_HEIGHT ARGB8888 pixels (uint32_t).
   This copies the pixels to the SDL texture and presents.
*/
void window_present(const uint32_t* framebuffer);

/* Poll window events and map keyboard to GB input. Returns true if user requested quit. */
bool window_poll_events(struct Memory* mem);

/* Audio functions */
bool audio_init(void);
void audio_cleanup(void);
void audio_queue_samples(const float* samples, uint32_t count);
void audio_set_muted(bool muted);

/* Emulator control functions */
void window_request_reset(void);
bool window_get_reset_requested(void);

/* ROM loaded state tracking */
void window_set_rom_loaded(bool loaded);

/* Video settings functions */
void window_toggle_fullscreen(void);
bool window_is_fullscreen(void);
int window_get_scale(void);
bool window_set_scaling_mode(const char* mode);  /* mode: "0"=nearest, "1"=linear, "2"=best */
int window_get_scaling_mode(void);  /* returns: 0=nearest, 1=linear, 2=best */

/* Save/load state functions */
bool window_get_save_state_requested(void);
bool window_get_load_state_requested(void);
void window_set_rom_path(const char* path);
const char* window_get_rom_path(void);

#endif /* GB_WINDOW_H */
