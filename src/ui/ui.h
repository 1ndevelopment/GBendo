#ifndef GB_UI_H
#define GB_UI_H

#include <stdbool.h>
#include <SDL2/SDL.h>

struct Memory;

/* UI initialization and cleanup */
bool ui_init(SDL_Window* window, SDL_Renderer* renderer);
void ui_shutdown(void);

/* UI rendering and event handling */
void ui_begin_frame(void);
void ui_render(void);
void ui_render_logo(bool rom_loaded);  /* Render logo centered on screen when no ROM is loaded */
bool ui_handle_event(SDL_Event* event);

/* UI state queries */
bool ui_wants_keyboard(void);
bool ui_wants_mouse(void);
int ui_get_menu_height(void);

/* UI callbacks that need to be implemented */
void ui_on_reset(void);
void ui_on_save_state(void);
void ui_on_load_state(void);
bool ui_is_paused(void);
void ui_set_paused(bool paused);
bool ui_is_muted(void);
void ui_set_muted(bool muted);

/* Get selected ROM file path (returns NULL if no file selected) */
const char* ui_get_selected_rom(void);

/* Check if stop emulation was requested */
bool ui_get_stop_requested(void);

/* Notify UI that a ROM was loaded (for recent ROMs tracking) */
void ui_notify_rom_loaded(const char* rom_path);

/* Debug logging functions */
typedef enum {
    UI_DEBUG_PPU = 0,
    UI_DEBUG_APU = 1,
    UI_DEBUG_CPU = 2,
    UI_DEBUG_MEM = 3,
    UI_DEBUG_UI = 4
} UIDebugComponent;

bool ui_is_debug_enabled(UIDebugComponent component);
void ui_debug_log(UIDebugComponent component, const char* format, ...);

#endif /* GB_UI_H */
