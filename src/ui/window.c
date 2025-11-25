#include "window.h"
#include "ui.h"
#include "embedded_assets.h"
#include "../ppu/ppu.h"
#include "../input/input.h"
#include <SDL2/SDL.h>
#include <SDL2/SDL_image.h>
#include <stdio.h>
#include <string.h>

static SDL_Window* g_window = NULL;
static SDL_Renderer* g_renderer = NULL;
static SDL_Texture* g_texture = NULL;
static int g_scale = 1;
static bool g_reset_requested = false;
static bool g_rom_loaded = false;
static int g_scaling_mode = 0;  /* 0=nearest, 1=linear, 2=best */
static int g_windowed_w = 800;  /* Store windowed width */
static int g_windowed_h = 600;  /* Store windowed height */
static bool g_save_state_requested = false;
static bool g_load_state_requested = false;
static char g_current_rom_path[2048] = {0};

/* Audio state */
static SDL_AudioDeviceID g_audio_device = 0;
#define AUDIO_BUFFER_SIZE 4096
static float g_audio_buffer[AUDIO_BUFFER_SIZE];
static uint32_t g_audio_write_pos = 0;
static uint32_t g_audio_read_pos = 0;
static SDL_mutex* g_audio_mutex = NULL;
static bool g_audio_muted = false;

/* UI callback implementations */
void ui_on_reset(void) {
    g_reset_requested = true;
}

void ui_on_save_state(void) {
    g_save_state_requested = true;
}

void ui_on_load_state(void) {
    g_load_state_requested = true;
}

static void audio_callback(void* userdata, Uint8* stream, int len) {
    (void)userdata;
    int16_t* output = (int16_t*)stream;
    int samples_needed = len / sizeof(int16_t);
    
    /* If muted, output silence */
    if (g_audio_muted) {
        memset(output, 0, len);
        return;
    }
    
    SDL_LockMutex(g_audio_mutex);
    
    int samples_available = (g_audio_write_pos - g_audio_read_pos + AUDIO_BUFFER_SIZE) % AUDIO_BUFFER_SIZE;
    int samples_to_copy = samples_needed < samples_available ? samples_needed : samples_available;
    
    for (int i = 0; i < samples_to_copy; i++) {
        float sample = g_audio_buffer[g_audio_read_pos];
        /* Convert float [-1.0, 1.0] to int16_t */
        output[i] = (int16_t)(sample * 32767.0f);
        g_audio_read_pos = (g_audio_read_pos + 1) % AUDIO_BUFFER_SIZE;
    }
    
    /* Fill remaining with silence */
    for (int i = samples_to_copy; i < samples_needed; i++) {
        output[i] = 0;
    }
    
    SDL_UnlockMutex(g_audio_mutex);
}

bool window_init(int scale, bool fullscreen, bool vsync) {
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO) != 0) {
        fprintf(stderr, "SDL_Init error: %s\n", SDL_GetError());
        return false;
    }

    g_scale = (scale > 0) ? scale : 1;
    
    /* Set default scaling to integer (nearest neighbor) */
    SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "0");
    
    /* Always open at 800x600 resolution */
    int w = 800;
    int h = 600;

    Uint32 window_flags = SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE;
    if (fullscreen) {
        window_flags |= SDL_WINDOW_FULLSCREEN_DESKTOP;
    }

    g_window = SDL_CreateWindow("GBendo", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
                                w, h, window_flags);
    if (!g_window) {
        fprintf(stderr, "SDL_CreateWindow error: %s\n", SDL_GetError());
        SDL_Quit();
        return false;
    }
    
    /* Set minimum window size (GB screen + menu bar) */
    SDL_SetWindowMinimumSize(g_window, SCREEN_WIDTH, SCREEN_HEIGHT + 20);

    /* Set window icon from embedded data */
    SDL_RWops* icon_rw = SDL_RWFromMem(img_icon_png, img_icon_png_len);
    if (icon_rw) {
        SDL_Surface* icon = IMG_Load_RW(icon_rw, 1); /* 1 = free RWops automatically */
        if (icon) {
            SDL_SetWindowIcon(g_window, icon);
            SDL_FreeSurface(icon);
        } else {
            fprintf(stderr, "Warning: Could not load embedded window icon: %s\n", IMG_GetError());
        }
    } else {
        fprintf(stderr, "Warning: Could not create RWops for embedded icon data: %s\n", SDL_GetError());
    }

    Uint32 renderer_flags = SDL_RENDERER_ACCELERATED;
    if (vsync) {
        renderer_flags |= SDL_RENDERER_PRESENTVSYNC;
    }

    g_renderer = SDL_CreateRenderer(g_window, -1, renderer_flags);
    if (!g_renderer) {
        fprintf(stderr, "SDL_CreateRenderer error: %s\n", SDL_GetError());
        SDL_DestroyWindow(g_window);
        g_window = NULL;
        SDL_Quit();
        return false;
    }

    g_texture = SDL_CreateTexture(g_renderer, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STREAMING,
                                  SCREEN_WIDTH, SCREEN_HEIGHT);
    if (!g_texture) {
        fprintf(stderr, "SDL_CreateTexture error: %s\n", SDL_GetError());
        SDL_DestroyRenderer(g_renderer);
        SDL_DestroyWindow(g_window);
        g_renderer = NULL;
        g_window = NULL;
        SDL_Quit();
        return false;
    }

    /* Initialize UI system */
    if (!ui_init(g_window, g_renderer)) {
        fprintf(stderr, "Failed to initialize UI\n");
        SDL_DestroyTexture(g_texture);
        SDL_DestroyRenderer(g_renderer);
        SDL_DestroyWindow(g_window);
        g_texture = NULL;
        g_renderer = NULL;
        g_window = NULL;
        SDL_Quit();
        return false;
    }

    return true;
}

void window_destroy(void) {
    audio_cleanup();
    
    ui_shutdown();
    if (g_texture) SDL_DestroyTexture(g_texture);
    if (g_renderer) SDL_DestroyRenderer(g_renderer);
    if (g_window) SDL_DestroyWindow(g_window);
    g_texture = NULL;
    g_renderer = NULL;
    g_window = NULL;
    SDL_Quit();
}

bool audio_init(void) {
    SDL_AudioSpec desired, obtained;
    
    desired.freq = 44100;
    desired.format = AUDIO_S16SYS;
    desired.channels = 1;
    desired.samples = 512;
    desired.callback = audio_callback;
    desired.userdata = NULL;
    
    g_audio_mutex = SDL_CreateMutex();
    if (!g_audio_mutex) {
        fprintf(stderr, "Failed to create audio mutex: %s\n", SDL_GetError());
        return false;
    }
    
    g_audio_device = SDL_OpenAudioDevice(NULL, 0, &desired, &obtained, 0);
    if (g_audio_device == 0) {
        fprintf(stderr, "Failed to open audio device: %s\n", SDL_GetError());
        SDL_DestroyMutex(g_audio_mutex);
        g_audio_mutex = NULL;
        return false;
    }
    
    memset(g_audio_buffer, 0, sizeof(g_audio_buffer));
    g_audio_write_pos = 0;
    g_audio_read_pos = 0;
    
    SDL_PauseAudioDevice(g_audio_device, 0);
    return true;
}

void audio_cleanup(void) {
    if (g_audio_device != 0) {
        SDL_CloseAudioDevice(g_audio_device);
        g_audio_device = 0;
    }
    if (g_audio_mutex) {
        SDL_DestroyMutex(g_audio_mutex);
        g_audio_mutex = NULL;
    }
}

void audio_queue_samples(const float* samples, uint32_t count) {
    if (!g_audio_mutex || g_audio_device == 0) return;
    
    SDL_LockMutex(g_audio_mutex);
    
    for (uint32_t i = 0; i < count; i++) {
        uint32_t next_pos = (g_audio_write_pos + 1) % AUDIO_BUFFER_SIZE;
        /* Don't overwrite unread samples */
        if (next_pos == g_audio_read_pos) {
            /* Buffer full, skip this sample */
            break;
        }
        g_audio_buffer[g_audio_write_pos] = samples[i];
        g_audio_write_pos = next_pos;
    }
    
    SDL_UnlockMutex(g_audio_mutex);
}

void window_present(const uint32_t* framebuffer) {
    if (!g_texture || !g_renderer) return;

    /* Update the texture with raw ARGB8888 pixels. Pitch = width * 4 */
    SDL_UpdateTexture(g_texture, NULL, framebuffer, SCREEN_WIDTH * sizeof(uint32_t));

    /* Set background color to match UI dark green theme */
    SDL_SetRenderDrawColor(g_renderer, 15, 25, 15, 255);
    SDL_RenderClear(g_renderer);
    
    /* Get UI menu height */
    int menu_height = ui_get_menu_height();
    
    /* Get current window size */
    int win_w, win_h;
    SDL_GetWindowSize(g_window, &win_w, &win_h);
    
    /* Calculate Game Boy screen area with proper aspect ratio (160:144) */
    int available_h = win_h - menu_height;
    float target_ratio = 160.0f / 144.0f;
    float available_ratio = (float)win_w / (float)available_h;
    
    SDL_Rect dst;
    dst.y = menu_height;
    
    if (available_ratio > target_ratio) {
        /* Window is wider than needed - add pillarboxing (black bars on sides) */
        dst.h = available_h;
        dst.w = (int)(dst.h * target_ratio);
        dst.x = (win_w - dst.w) / 2;
    } else {
        /* Window is taller than needed - add letterboxing (black bars on top/bottom) */
        dst.w = win_w;
        dst.h = (int)(dst.w / target_ratio);
        dst.x = 0;
        dst.y = menu_height + (available_h - dst.h) / 2;
    }
    
    SDL_RenderCopy(g_renderer, g_texture, NULL, &dst);
    
    /* Render UI on top */
    ui_begin_frame();
    ui_render_logo(g_rom_loaded);  /* Render logo only when no ROM is loaded */
    ui_render();
    
    SDL_RenderPresent(g_renderer);
}

bool window_poll_events(struct Memory* mem) {
    SDL_Event ev;
    while (SDL_PollEvent(&ev)) {
        /* Let UI handle the event first */
        if (ui_handle_event(&ev)) {
            continue; /* Event was consumed by UI */
        }
        
        if (ev.type == SDL_QUIT) return true;
        
        /* Handle window resize to maintain aspect ratio */
        if (ev.type == SDL_WINDOWEVENT) {
            if (ev.window.event == SDL_WINDOWEVENT_RESIZED) {
                int new_w = ev.window.data1;
                int new_h = ev.window.data2;
                int menu_h = ui_get_menu_height();
                
                /* Calculate Game Boy screen area (excluding menu bar) */
                int screen_h = new_h - menu_h;
                
                /* Game Boy aspect ratio is 160:144 (10:9) */
                float target_ratio = 160.0f / 144.0f;
                float current_ratio = (float)new_w / (float)screen_h;
                
                /* Adjust to maintain aspect ratio */
                if (current_ratio > target_ratio) {
                    /* Too wide - constrain width */
                    new_w = (int)(screen_h * target_ratio);
                } else {
                    /* Too tall - constrain height */
                    screen_h = (int)(new_w / target_ratio);
                    new_h = screen_h + menu_h;
                }
                
                SDL_SetWindowSize(g_window, new_w, new_h);
            }
        }
        
        if (ev.type == SDL_KEYDOWN) {
            /* ESC quits if UI not using it */
            if (ev.key.keysym.sym == SDLK_ESCAPE && !ui_wants_keyboard()) {
                return true;
            }
            
            /* Game input only when not paused and UI not using keyboard */
            if (mem && !ui_is_paused() && !ui_wants_keyboard()) {
                switch (ev.key.keysym.sym) {
                    case SDLK_RIGHT: input_press(mem, JP_RIGHT); break;
                    case SDLK_LEFT:  input_press(mem, JP_LEFT);  break;
                    case SDLK_UP:    input_press(mem, JP_UP);    break;
                    case SDLK_DOWN:  input_press(mem, JP_DOWN);  break;
                    case SDLK_z:     input_press(mem, JP_A);     break; /* A */
                    case SDLK_x:     input_press(mem, JP_B);     break; /* B */
                    case SDLK_RETURN:input_press(mem, JP_START); break;
                    case SDLK_RSHIFT:input_press(mem, JP_SELECT);break;
                    case SDLK_LSHIFT:input_press(mem, JP_SELECT);break;
                    default: break;
                }
            }
        } else if (ev.type == SDL_KEYUP) {
            if (mem && !ui_is_paused() && !ui_wants_keyboard()) {
                switch (ev.key.keysym.sym) {
                    case SDLK_RIGHT: input_release(mem, JP_RIGHT); break;
                    case SDLK_LEFT:  input_release(mem, JP_LEFT);  break;
                    case SDLK_UP:    input_release(mem, JP_UP);    break;
                    case SDLK_DOWN:  input_release(mem, JP_DOWN);  break;
                    case SDLK_z:     input_release(mem, JP_A);     break;
                    case SDLK_x:     input_release(mem, JP_B);     break;
                    case SDLK_RETURN:input_release(mem, JP_START); break;
                    case SDLK_RSHIFT:input_release(mem, JP_SELECT);break;
                    case SDLK_LSHIFT:input_release(mem, JP_SELECT);break;
                    default: break;
                }
            }
        }
    }
    return false;
}

void audio_set_muted(bool muted) {
    g_audio_muted = muted;
}

void window_request_reset(void) {
    g_reset_requested = true;
}

bool window_get_reset_requested(void) {
    bool requested = g_reset_requested;
    g_reset_requested = false;  /* Clear flag after reading */
    return requested;
}

void window_set_rom_loaded(bool loaded) {
    g_rom_loaded = loaded;
}

void window_toggle_fullscreen(void) {
    if (!g_window) return;
    
    Uint32 flags = SDL_GetWindowFlags(g_window);
    bool is_fullscreen = (flags & (SDL_WINDOW_FULLSCREEN | SDL_WINDOW_FULLSCREEN_DESKTOP)) != 0;
    
    if (is_fullscreen) {
        /* Exiting fullscreen - restore windowed size */
        SDL_SetWindowFullscreen(g_window, 0);
        SDL_SetWindowSize(g_window, g_windowed_w, g_windowed_h);
    } else {
        /* Entering fullscreen - save current windowed size */
        SDL_GetWindowSize(g_window, &g_windowed_w, &g_windowed_h);
        SDL_SetWindowFullscreen(g_window, SDL_WINDOW_FULLSCREEN_DESKTOP);
    }
}

bool window_is_fullscreen(void) {
    if (!g_window) return false;
    
    Uint32 flags = SDL_GetWindowFlags(g_window);
    return (flags & (SDL_WINDOW_FULLSCREEN | SDL_WINDOW_FULLSCREEN_DESKTOP)) != 0;
}

int window_get_scale(void) {
    return g_scale;
}

bool window_set_scaling_mode(const char* mode) {
    if (!g_renderer) return false;
    
    /* Set the hint */
    SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, mode);
    
    /* Update tracking variable */
    g_scaling_mode = atoi(mode);
    
    /* Recreate the texture with the new scaling mode */
    if (g_texture) {
        SDL_DestroyTexture(g_texture);
    }
    
    g_texture = SDL_CreateTexture(g_renderer, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STREAMING,
                                  SCREEN_WIDTH, SCREEN_HEIGHT);
    if (!g_texture) {
        fprintf(stderr, "SDL_CreateTexture error: %s\n", SDL_GetError());
        return false;
    }
    
    return true;
}

int window_get_scaling_mode(void) {
    return g_scaling_mode;
}

bool window_get_save_state_requested(void) {
    bool requested = g_save_state_requested;
    g_save_state_requested = false;  /* Clear flag after reading */
    return requested;
}

bool window_get_load_state_requested(void) {
    bool requested = g_load_state_requested;
    g_load_state_requested = false;  /* Clear flag after reading */
    return requested;
}

void window_set_rom_path(const char* path) {
    if (path) {
        strncpy(g_current_rom_path, path, sizeof(g_current_rom_path) - 1);
        g_current_rom_path[sizeof(g_current_rom_path) - 1] = '\0';
    } else {
        g_current_rom_path[0] = '\0';
    }
}

const char* window_get_rom_path(void) {
    return g_current_rom_path[0] ? g_current_rom_path : NULL;
}
