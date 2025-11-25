#include "ui.h"
#include "window.h"
#include "embedded_assets.h"
#include "../ppu/ppu.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <dirent.h>
#include <sys/stat.h>
#include <unistd.h>
#include <stdarg.h>
#include <SDL2/SDL_image.h>

/* Simple immediate mode UI state */
static struct {
    SDL_Renderer* renderer;
    SDL_Window* window;
    SDL_Texture* logo_texture;
    
    /* Menu bar state */
    bool menu_bar_hovered;
    int active_menu;  /* -1 if none */
    int hovered_item;
    
    /* UI state */
    bool paused;
    bool muted;
    bool show_about;
    bool show_controls;
    bool show_file_browser;
    bool show_video_settings;
    bool show_debug;
    bool show_settings;
    
    /* Mouse state */
    int mouse_x, mouse_y;
    bool mouse_down;
    
    /* File browser state */
    char current_dir[2048];
    char** file_list;
    int file_count;
    int selected_file;
    int scroll_offset;
    char selected_rom_path[2048];
    bool rom_selected;
    bool stop_requested;
    
    /* Directory path editing */
    bool editing_path;
    char path_edit_buffer[2048];
    int cursor_pos;
    int selection_start;
    int selection_end;
    bool mouse_selecting;
    
    /* Scrollbar dragging */
    bool dragging_scrollbar;
    int drag_start_y;
    int drag_start_offset;
    
    /* Recent ROMs */
    char recent_roms[5][512];
    int recent_rom_count;
    
    /* Directory bookmarks */
    char bookmarked_dirs[10][512];
    int bookmark_count;
    bool show_bookmark_menu;
    
    /* Video settings dropdown state */
    int active_video_dropdown;  /* -1 if none, 0=scaling */
    
    /* Settings window state */
    int active_settings_tab;  /* 0=Video, 1=Audio, 2=Input, 3=Debug, 4=Palette */
    int active_settings_dropdown;  /* -1 if none, 0=scaling, 1=audio_device, 2=palette, etc */
    
    /* Palette settings state */
    int selected_palette;  /* Currently selected palette index */
    
    /* Debug window state */
    bool debug_enabled;
    bool debug_ppu;
    bool debug_apu;
    bool debug_cpu;
    bool debug_mem;
    bool debug_ui;
    int active_debug_dropdown;  /* -1 if none, 0=component selector */
    int selected_debug_component;  /* 0=PPU, 1=APU, 2=CPU, 3=MEM, 4=UI */
    char debug_buffer[8192];  /* Circular buffer for debug messages */
    int debug_buffer_offset;
    int debug_scroll_offset;
} ui_state = {0};

/* Menu constants */
#define MENU_HEIGHT 20
#define MENU_ITEM_HEIGHT 20
#define MENU_WIDTH 180
#define MAX_RECENT_ROMS 5
#define MAX_BOOKMARKS 10

/* Simple 5x7 bitmap font data (ASCII 32-126) */
static const unsigned char font_5x7[][5] = {
    {0x00, 0x00, 0x00, 0x00, 0x00}, // 32 (space)
    {0x00, 0x00, 0x5F, 0x00, 0x00}, // 33 !
    {0x00, 0x07, 0x00, 0x07, 0x00}, // 34 "
    {0x14, 0x7F, 0x14, 0x7F, 0x14}, // 35 #
    {0x24, 0x2A, 0x7F, 0x2A, 0x12}, // 36 $
    {0x23, 0x13, 0x08, 0x64, 0x62}, // 37 %
    {0x36, 0x49, 0x55, 0x22, 0x50}, // 38 &
    {0x00, 0x05, 0x03, 0x00, 0x00}, // 39 '
    {0x00, 0x1C, 0x22, 0x41, 0x00}, // 40 (
    {0x00, 0x41, 0x22, 0x1C, 0x00}, // 41 )
    {0x14, 0x08, 0x3E, 0x08, 0x14}, // 42 *
    {0x08, 0x08, 0x3E, 0x08, 0x08}, // 43 +
    {0x00, 0x50, 0x30, 0x00, 0x00}, // 44 ,
    {0x08, 0x08, 0x08, 0x08, 0x08}, // 45 -
    {0x00, 0x60, 0x60, 0x00, 0x00}, // 46 .
    {0x20, 0x10, 0x08, 0x04, 0x02}, // 47 /
    {0x3E, 0x51, 0x49, 0x45, 0x3E}, // 48 0
    {0x00, 0x42, 0x7F, 0x40, 0x00}, // 49 1
    {0x42, 0x61, 0x51, 0x49, 0x46}, // 50 2
    {0x21, 0x41, 0x45, 0x4B, 0x31}, // 51 3
    {0x18, 0x14, 0x12, 0x7F, 0x10}, // 52 4
    {0x27, 0x45, 0x45, 0x45, 0x39}, // 53 5
    {0x3C, 0x4A, 0x49, 0x49, 0x30}, // 54 6
    {0x01, 0x71, 0x09, 0x05, 0x03}, // 55 7
    {0x36, 0x49, 0x49, 0x49, 0x36}, // 56 8
    {0x06, 0x49, 0x49, 0x29, 0x1E}, // 57 9
    {0x00, 0x36, 0x36, 0x00, 0x00}, // 58 :
    {0x00, 0x56, 0x36, 0x00, 0x00}, // 59 ;
    {0x08, 0x14, 0x22, 0x41, 0x00}, // 60 <
    {0x14, 0x14, 0x14, 0x14, 0x14}, // 61 =
    {0x00, 0x41, 0x22, 0x14, 0x08}, // 62 >
    {0x02, 0x01, 0x51, 0x09, 0x06}, // 63 ?
    {0x32, 0x49, 0x79, 0x41, 0x3E}, // 64 @
    {0x7E, 0x11, 0x11, 0x11, 0x7E}, // 65 A
    {0x7F, 0x49, 0x49, 0x49, 0x36}, // 66 B
    {0x3E, 0x41, 0x41, 0x41, 0x22}, // 67 C
    {0x7F, 0x41, 0x41, 0x22, 0x1C}, // 68 D
    {0x7F, 0x49, 0x49, 0x49, 0x41}, // 69 E
    {0x7F, 0x09, 0x09, 0x09, 0x01}, // 70 F
    {0x3E, 0x41, 0x49, 0x49, 0x7A}, // 71 G
    {0x7F, 0x08, 0x08, 0x08, 0x7F}, // 72 H
    {0x00, 0x41, 0x7F, 0x41, 0x00}, // 73 I
    {0x20, 0x40, 0x41, 0x3F, 0x01}, // 74 J
    {0x7F, 0x08, 0x14, 0x22, 0x41}, // 75 K
    {0x7F, 0x40, 0x40, 0x40, 0x40}, // 76 L
    {0x7F, 0x02, 0x0C, 0x02, 0x7F}, // 77 M
    {0x7F, 0x04, 0x08, 0x10, 0x7F}, // 78 N
    {0x3E, 0x41, 0x41, 0x41, 0x3E}, // 79 O
    {0x7F, 0x09, 0x09, 0x09, 0x06}, // 80 P
    {0x3E, 0x41, 0x51, 0x21, 0x5E}, // 81 Q
    {0x7F, 0x09, 0x19, 0x29, 0x46}, // 82 R
    {0x46, 0x49, 0x49, 0x49, 0x31}, // 83 S
    {0x01, 0x01, 0x7F, 0x01, 0x01}, // 84 T
    {0x3F, 0x40, 0x40, 0x40, 0x3F}, // 85 U
    {0x1F, 0x20, 0x40, 0x20, 0x1F}, // 86 V
    {0x3F, 0x40, 0x38, 0x40, 0x3F}, // 87 W
    {0x63, 0x14, 0x08, 0x14, 0x63}, // 88 X
    {0x07, 0x08, 0x70, 0x08, 0x07}, // 89 Y
    {0x61, 0x51, 0x49, 0x45, 0x43}, // 90 Z
    {0x00, 0x7F, 0x41, 0x41, 0x00}, // 91 [
    {0x02, 0x04, 0x08, 0x10, 0x20}, // 92 backslash
    {0x00, 0x41, 0x41, 0x7F, 0x00}, // 93 ]
    {0x04, 0x02, 0x01, 0x02, 0x04}, // 94 ^
    {0x40, 0x40, 0x40, 0x40, 0x40}, // 95 _
    {0x00, 0x01, 0x02, 0x04, 0x00}, // 96 `
    {0x20, 0x54, 0x54, 0x54, 0x78}, // 97 a
    {0x7F, 0x48, 0x44, 0x44, 0x38}, // 98 b
    {0x38, 0x44, 0x44, 0x44, 0x20}, // 99 c
    {0x38, 0x44, 0x44, 0x48, 0x7F}, // 100 d
    {0x38, 0x54, 0x54, 0x54, 0x18}, // 101 e
    {0x08, 0x7E, 0x09, 0x01, 0x02}, // 102 f
    {0x0C, 0x52, 0x52, 0x52, 0x3E}, // 103 g
    {0x7F, 0x08, 0x04, 0x04, 0x78}, // 104 h
    {0x00, 0x44, 0x7D, 0x40, 0x00}, // 105 i
    {0x20, 0x40, 0x44, 0x3D, 0x00}, // 106 j
    {0x7F, 0x10, 0x28, 0x44, 0x00}, // 107 k
    {0x00, 0x41, 0x7F, 0x40, 0x00}, // 108 l
    {0x7C, 0x04, 0x18, 0x04, 0x78}, // 109 m
    {0x7C, 0x08, 0x04, 0x04, 0x78}, // 110 n
    {0x38, 0x44, 0x44, 0x44, 0x38}, // 111 o
    {0x7C, 0x14, 0x14, 0x14, 0x08}, // 112 p
    {0x08, 0x14, 0x14, 0x18, 0x7C}, // 113 q
    {0x7C, 0x08, 0x04, 0x04, 0x08}, // 114 r
    {0x48, 0x54, 0x54, 0x54, 0x20}, // 115 s
    {0x04, 0x3F, 0x44, 0x40, 0x20}, // 116 t
    {0x3C, 0x40, 0x40, 0x20, 0x7C}, // 117 u
    {0x1C, 0x20, 0x40, 0x20, 0x1C}, // 118 v
    {0x3C, 0x40, 0x30, 0x40, 0x3C}, // 119 w
    {0x44, 0x28, 0x10, 0x28, 0x44}, // 120 x
    {0x0C, 0x50, 0x50, 0x50, 0x3C}, // 121 y
    {0x44, 0x64, 0x54, 0x4C, 0x44}, // 122 z
    {0x00, 0x08, 0x36, 0x41, 0x00}, // 123 {
    {0x00, 0x00, 0x7F, 0x00, 0x00}, // 124 |
    {0x00, 0x41, 0x36, 0x08, 0x00}, // 125 }
    {0x08, 0x04, 0x08, 0x10, 0x08}, // 126 ~
};

/* Render bitmap text */
static void draw_text(const char* text, int x, int y, SDL_Color color) {
    SDL_SetRenderDrawColor(ui_state.renderer, color.r, color.g, color.b, color.a);
    
    int char_x = x;
    for (const char* p = text; *p; p++) {
        unsigned char c = (unsigned char)*p;
        
        /* Only render printable ASCII */
        if (c >= 32 && c <= 126) {
            const unsigned char* glyph = font_5x7[c - 32];
            
            /* Draw each column of the character */
            for (int col = 0; col < 5; col++) {
                unsigned char column_data = glyph[col];
                
                /* Draw each pixel in the column */
                for (int row = 0; row < 7; row++) {
                    if (column_data & (1 << row)) {
                        SDL_Rect pixel = {char_x + col, y + row, 1, 1};
                        SDL_RenderFillRect(ui_state.renderer, &pixel);
                    }
                }
            }
        }
        
        char_x += 6; /* 5 pixels + 1 pixel spacing */
    }
}

static void draw_rect_filled(int x, int y, int w, int h, SDL_Color color) {
    SDL_SetRenderDrawColor(ui_state.renderer, color.r, color.g, color.b, color.a);
    SDL_Rect r = {x, y, w, h};
    SDL_RenderFillRect(ui_state.renderer, &r);
}

static void draw_rect_outline(int x, int y, int w, int h, SDL_Color color) {
    SDL_SetRenderDrawColor(ui_state.renderer, color.r, color.g, color.b, color.a);
    SDL_Rect r = {x, y, w, h};
    SDL_RenderDrawRect(ui_state.renderer, &r);
}

/* Forward declarations */
static void load_recent_roms(void);
static void save_recent_roms(void);
static void add_recent_rom(const char* rom_path);
static void rebuild_file_menu(void);
static void load_bookmarks(void);
static void save_bookmarks(void);
static void add_bookmark(const char* dir_path);
static void remove_bookmark(int index);
static void free_file_list(void);
static void load_palette_setting(void);
static void save_palette_setting(void);

bool ui_init(SDL_Window* window, SDL_Renderer* renderer) {
    memset(&ui_state, 0, sizeof(ui_state));
    ui_state.renderer = renderer;
    ui_state.window = window;
    ui_state.active_menu = -1;
    ui_state.hovered_item = -1;
    ui_state.active_video_dropdown = -1;
    ui_state.active_debug_dropdown = -1;
    ui_state.active_settings_dropdown = -1;
    ui_state.selected_debug_component = 0;
    ui_state.debug_scroll_offset = 0;
    ui_state.debug_buffer_offset = 0;
    load_recent_roms();
    load_bookmarks();
    load_palette_setting();
    rebuild_file_menu();
    
    /* Load logo texture from embedded data */
    SDL_RWops* logo_rw = SDL_RWFromMem(img_logo_png, img_logo_png_len);
    if (logo_rw) {
        SDL_Surface* logo_surface = IMG_Load_RW(logo_rw, 1); /* 1 = free RWops automatically */
        if (logo_surface) {
            ui_state.logo_texture = SDL_CreateTextureFromSurface(renderer, logo_surface);
            SDL_FreeSurface(logo_surface);
            if (!ui_state.logo_texture) {
                fprintf(stderr, "Failed to create logo texture: %s\n", SDL_GetError());
            }
        } else {
            fprintf(stderr, "Failed to load embedded logo image: %s\n", IMG_GetError());
        }
    } else {
        fprintf(stderr, "Failed to create RWops for embedded logo data: %s\n", SDL_GetError());
    }
    
    return true;
}

void ui_shutdown(void) {
    /* Clean up allocated memory before zeroing state */
    free_file_list();
    
    if (ui_state.logo_texture) {
        SDL_DestroyTexture(ui_state.logo_texture);
        ui_state.logo_texture = NULL;
    }
    
    memset(&ui_state, 0, sizeof(ui_state));
}

void ui_begin_frame(void) {
    /* Reset per-frame state */
    ui_state.menu_bar_hovered = false;
}

bool ui_wants_keyboard(void) {
    return ui_state.active_menu >= 0 || ui_state.editing_path;
}

bool ui_wants_mouse(void) {
    return ui_state.menu_bar_hovered || ui_state.active_menu >= 0;
}

int ui_get_menu_height(void) {
    return MENU_HEIGHT;
}

bool ui_is_paused(void) {
    return ui_state.paused;
}

void ui_set_paused(bool paused) {
    ui_state.paused = paused;
}

bool ui_is_muted(void) {
    return ui_state.muted;
}

void ui_set_muted(bool muted) {
    ui_state.muted = muted;
}

const char* ui_get_selected_rom(void) {
    if (ui_state.rom_selected) {
        ui_state.rom_selected = false;  /* Reset flag after retrieval */
        return ui_state.selected_rom_path;
    }
    return NULL;
}

bool ui_get_stop_requested(void) {
    if (ui_state.stop_requested) {
        ui_state.stop_requested = false;  /* Reset flag after retrieval */
        return true;
    }
    return false;
}

void ui_notify_rom_loaded(const char* rom_path) {
    add_recent_rom(rom_path);
    rebuild_file_menu();
}

/* File browser functions */
static bool build_rom_path(const char* dir, const char* filename) {
    /* Always use snprintf with buffer size - let it handle truncation safely */
    int result = snprintf(ui_state.selected_rom_path, sizeof(ui_state.selected_rom_path), 
                         "%s/%s", dir, filename);
    
    /* Check if truncation occurred */
    if (result >= (int)sizeof(ui_state.selected_rom_path)) {
        printf("Warning: ROM path truncated (length %d, buffer %zu)\n", 
               result, sizeof(ui_state.selected_rom_path));
        return false;
    }
    
    return true;
}

static void free_file_list(void) {
    if (ui_state.file_list) {
        for (int i = 0; i < ui_state.file_count; i++) {
            if (ui_state.file_list[i]) {
                free(ui_state.file_list[i]);
                ui_state.file_list[i] = NULL;
            }
        }
        free(ui_state.file_list);
        ui_state.file_list = NULL;
    }
    ui_state.file_count = 0;
}

static int compare_strings(const void* a, const void* b) {
    return strcmp(*(const char**)a, *(const char**)b);
}

static void scan_directory(const char* path) {
    free_file_list();
    
    DIR* dir = opendir(path);
    if (!dir) return;
    
    /* Count files first */
    int capacity = 16;
    ui_state.file_list = malloc(sizeof(char*) * capacity);
    if (!ui_state.file_list) {
        closedir(dir);
        return;
    }
    
    /* Always add parent directory */
    char* parent_dir = strdup("..");
    if (!parent_dir) {
        free_file_list();
        closedir(dir);
        return;
    }
    ui_state.file_list[ui_state.file_count++] = parent_dir;
    
    struct dirent* entry;
    while ((entry = readdir(dir)) != NULL) {
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
            continue;
        }
        
        char full_path[4096];
        snprintf(full_path, sizeof(full_path), "%s/%s", path, entry->d_name);
        
        struct stat st;
        if (stat(full_path, &st) == 0) {
            bool is_dir = S_ISDIR(st.st_mode);
            bool is_rom = false;
            
            if (!is_dir) {
                const char* ext = strrchr(entry->d_name, '.');
                if (ext && (strcmp(ext, ".gb") == 0 || strcmp(ext, ".gbc") == 0 || strcmp(ext, ".GB") == 0 || strcmp(ext, ".GBC") == 0)) {
                    is_rom = true;
                }
            }
            
            if (is_dir || is_rom) {
                if (ui_state.file_count >= capacity) {
                    capacity *= 2;
                    char** new_list = realloc(ui_state.file_list, sizeof(char*) * capacity);
                    if (!new_list) {
                        /* Realloc failed - clean up and return */
                        free_file_list();
                        closedir(dir);
                        return;
                    }
                    ui_state.file_list = new_list;
                }
                
                char* new_entry = NULL;
                if (is_dir) {
                    char dir_name[512];
                    snprintf(dir_name, sizeof(dir_name), "[%s]", entry->d_name);
                    new_entry = strdup(dir_name);
                } else {
                    new_entry = strdup(entry->d_name);
                }
                
                if (!new_entry) {
                    /* strdup failed - continue with other entries */
                    continue;
                }
                
                ui_state.file_list[ui_state.file_count++] = new_entry;
            }
        }
    }
    
    closedir(dir);
    
    /* Sort the list (keeping .. at the top) */
    if (ui_state.file_count > 1) {
        qsort(ui_state.file_list + 1, ui_state.file_count - 1, sizeof(char*), compare_strings);
    }
    
    ui_state.selected_file = 0;
    ui_state.scroll_offset = 0;
}

static void open_file_browser(void) {
    if (!ui_state.show_file_browser) {
        getcwd(ui_state.current_dir, sizeof(ui_state.current_dir));
        scan_directory(ui_state.current_dir);
        ui_state.show_file_browser = true;
    }
}

/* Recent ROMs management */
static void load_recent_roms(void) {
    const char* home = getenv("HOME");
    if (!home) return;
    
    char config_path[1024];
    snprintf(config_path, sizeof(config_path), "%s/.gbendo_recent_roms", home);
    
    FILE* f = fopen(config_path, "r");
    if (!f) return;
    
    ui_state.recent_rom_count = 0;
    while (ui_state.recent_rom_count < MAX_RECENT_ROMS && 
           fgets(ui_state.recent_roms[ui_state.recent_rom_count], 
                 sizeof(ui_state.recent_roms[0]), f)) {
        /* Remove newline */
        size_t len = strlen(ui_state.recent_roms[ui_state.recent_rom_count]);
        if (len > 0 && ui_state.recent_roms[ui_state.recent_rom_count][len - 1] == '\n') {
            ui_state.recent_roms[ui_state.recent_rom_count][len - 1] = '\0';
        }
        ui_state.recent_rom_count++;
    }
    
    fclose(f);
}

static void save_recent_roms(void) {
    const char* home = getenv("HOME");
    if (!home) return;
    
    char config_path[1024];
    snprintf(config_path, sizeof(config_path), "%s/.gbendo_recent_roms", home);
    
    FILE* f = fopen(config_path, "w");
    if (!f) return;
    
    for (int i = 0; i < ui_state.recent_rom_count; i++) {
        fprintf(f, "%s\n", ui_state.recent_roms[i]);
    }
    
    fclose(f);
}

static void add_recent_rom(const char* rom_path) {
    /* Check if already in list */
    for (int i = 0; i < ui_state.recent_rom_count; i++) {
        if (strcmp(ui_state.recent_roms[i], rom_path) == 0) {
            /* Move to front */
            if (i > 0) {
                char temp[512];
                strncpy(temp, ui_state.recent_roms[i], sizeof(temp));
                temp[sizeof(temp) - 1] = '\0';
                
                memmove(&ui_state.recent_roms[1], &ui_state.recent_roms[0], 
                       sizeof(ui_state.recent_roms[0]) * i);
                strncpy(ui_state.recent_roms[0], temp, sizeof(ui_state.recent_roms[0]));
                ui_state.recent_roms[0][sizeof(ui_state.recent_roms[0]) - 1] = '\0';
            }
            save_recent_roms();
            return;
        }
    }
    
    /* Add to front */
    if (ui_state.recent_rom_count < MAX_RECENT_ROMS) {
        memmove(&ui_state.recent_roms[1], &ui_state.recent_roms[0], 
               sizeof(ui_state.recent_roms[0]) * ui_state.recent_rom_count);
        ui_state.recent_rom_count++;
    } else {
        memmove(&ui_state.recent_roms[1], &ui_state.recent_roms[0], 
               sizeof(ui_state.recent_roms[0]) * (MAX_RECENT_ROMS - 1));
    }
    
    strncpy(ui_state.recent_roms[0], rom_path, sizeof(ui_state.recent_roms[0]));
    ui_state.recent_roms[0][sizeof(ui_state.recent_roms[0]) - 1] = '\0';
    
    save_recent_roms();
}

/* Directory bookmarks management */
static void load_bookmarks(void) {
    const char* home = getenv("HOME");
    if (!home) return;
    
    char config_path[1024];
    snprintf(config_path, sizeof(config_path), "%s/.gbendo_bookmarks", home);
    
    FILE* f = fopen(config_path, "r");
    if (!f) return;
    
    ui_state.bookmark_count = 0;
    while (ui_state.bookmark_count < MAX_BOOKMARKS && 
           fgets(ui_state.bookmarked_dirs[ui_state.bookmark_count], 
                 sizeof(ui_state.bookmarked_dirs[0]), f)) {
        /* Remove newline */
        size_t len = strlen(ui_state.bookmarked_dirs[ui_state.bookmark_count]);
        if (len > 0 && ui_state.bookmarked_dirs[ui_state.bookmark_count][len - 1] == '\n') {
            ui_state.bookmarked_dirs[ui_state.bookmark_count][len - 1] = '\0';
        }
        ui_state.bookmark_count++;
    }
    
    fclose(f);
}

static void save_bookmarks(void) {
    const char* home = getenv("HOME");
    if (!home) return;
    
    char config_path[1024];
    snprintf(config_path, sizeof(config_path), "%s/.gbendo_bookmarks", home);
    
    FILE* f = fopen(config_path, "w");
    if (!f) return;
    
    for (int i = 0; i < ui_state.bookmark_count; i++) {
        fprintf(f, "%s\n", ui_state.bookmarked_dirs[i]);
    }
    
    fclose(f);
}

static void add_bookmark(const char* dir_path) {
    /* Check if already bookmarked */
    for (int i = 0; i < ui_state.bookmark_count; i++) {
        if (strcmp(ui_state.bookmarked_dirs[i], dir_path) == 0) {
            return; /* Already exists */
        }
    }
    
    /* Add new bookmark if there's space */
    if (ui_state.bookmark_count < MAX_BOOKMARKS) {
        strncpy(ui_state.bookmarked_dirs[ui_state.bookmark_count], dir_path, 
                sizeof(ui_state.bookmarked_dirs[0]) - 1);
        ui_state.bookmarked_dirs[ui_state.bookmark_count][sizeof(ui_state.bookmarked_dirs[0]) - 1] = '\0';
        ui_state.bookmark_count++;
        save_bookmarks();
    }
}

static void remove_bookmark(int index) {
    if (index < 0 || index >= ui_state.bookmark_count) return;
    
    /* Shift remaining bookmarks down */
    for (int i = index; i < ui_state.bookmark_count - 1; i++) {
        strncpy(ui_state.bookmarked_dirs[i], ui_state.bookmarked_dirs[i + 1], 
                sizeof(ui_state.bookmarked_dirs[0]));
    }
    ui_state.bookmark_count--;
    save_bookmarks();
}

/* Palette settings management */
static void load_palette_setting(void) {
    const char* home = getenv("HOME");
    if (!home) {
        ui_state.selected_palette = 0;
        ppu_set_palette(0);
        return;
    }
    
    char config_path[1024];
    snprintf(config_path, sizeof(config_path), "%s/.gbendo_palette", home);
    
    FILE* f = fopen(config_path, "r");
    if (!f) {
        ui_state.selected_palette = 0;
        ppu_set_palette(0);
        return;
    }
    
    int palette_index = 0;
    if (fscanf(f, "%d", &palette_index) == 1) {
        if (palette_index >= 0 && palette_index < ppu_get_palette_count()) {
            ui_state.selected_palette = palette_index;
            ppu_set_palette(palette_index);
        } else {
            ui_state.selected_palette = 0;
            ppu_set_palette(0);
        }
    } else {
        ui_state.selected_palette = 0;
        ppu_set_palette(0);
    }
    
    fclose(f);
}

static void save_palette_setting(void) {
    const char* home = getenv("HOME");
    if (!home) return;
    
    char config_path[1024];
    snprintf(config_path, sizeof(config_path), "%s/.gbendo_palette", home);
    
    FILE* f = fopen(config_path, "w");
    if (!f) return;
    
    fprintf(f, "%d\n", ui_state.selected_palette);
    
    fclose(f);
}

/* Menu item structure */
typedef struct {
    const char* label;
    const char* shortcut;
    bool is_separator;
    bool is_checkbox;
    bool* checked;
    void (*callback)(void);
} MenuItem;

/* Callback implementations */
static void cb_open_rom(void) {
    open_file_browser();
}

/* Callbacks for recent ROMs - need to store which one was clicked */
static int clicked_recent_index = -1;

static void cb_recent_rom_0(void) { clicked_recent_index = 0; }
static void cb_recent_rom_1(void) { clicked_recent_index = 1; }
static void cb_recent_rom_2(void) { clicked_recent_index = 2; }
static void cb_recent_rom_3(void) { clicked_recent_index = 3; }
static void cb_recent_rom_4(void) { clicked_recent_index = 4; }

static void (*recent_rom_callbacks[])(void) = {
    cb_recent_rom_0, cb_recent_rom_1, cb_recent_rom_2, cb_recent_rom_3, cb_recent_rom_4
};

static void cb_exit(void) {
    SDL_Event quit = {.type = SDL_QUIT};
    SDL_PushEvent(&quit);
}

static void cb_pause(void) {
    ui_state.paused = !ui_state.paused;
}

static void cb_reset(void) {
    ui_on_reset();
}

static void cb_stop(void) {
    ui_state.stop_requested = true;
    ui_state.paused = false;  /* Unpause when stopping */
}

static void cb_mute(void) {
    ui_state.muted = !ui_state.muted;
    audio_set_muted(ui_state.muted);
}

static void cb_settings(void) {
    ui_state.show_settings = true;
    ui_state.active_settings_tab = 0;  /* Default to Video tab */
    ui_state.active_settings_dropdown = -1;  /* Close any open dropdowns */
}

static void cb_clear_recent_roms(void) {
    ui_state.recent_rom_count = 0;
    save_recent_roms();
    rebuild_file_menu();
}

static void cb_debug(void) {
    ui_state.show_debug = true;
}

static void cb_controls(void) {
    ui_state.show_controls = true;
}

static void cb_about(void) {
    ui_state.show_about = true;
}

static void cb_save_state(void) {
    ui_on_save_state();
}

static void cb_load_state(void) {
    ui_on_load_state();
}

/* Menu definitions */
static MenuItem emulation_menu[] = {
    {"Pause", "P", false, true, &ui_state.paused, cb_pause},
    {"Reset", "Ctrl+R", false, false, NULL, cb_reset},
    {"Stop", NULL, false, false, NULL, cb_stop},
    {"", NULL, true, false, NULL, NULL},
    {"Save State", "F5", false, false, NULL, cb_save_state},
    {"Load State", "F7", false, false, NULL, cb_load_state},
    {"", NULL, true, false, NULL, NULL},
    {"Mute Sound", "M", false, true, &ui_state.paused, cb_mute},
};

static MenuItem settings_menu[] = {
    {"Settings...", NULL, false, false, NULL, cb_settings},
    {"", NULL, true, false, NULL, NULL},
    {"Clear Recent ROMs", NULL, false, false, NULL, cb_clear_recent_roms},
    {"", NULL, true, false, NULL, NULL},
    {"Enable debug", NULL, false, false, NULL, cb_debug},
};

static MenuItem help_menu[] = {
    {"Controls", "F1", false, false, NULL, cb_controls},
    {"", NULL, true, false, NULL, NULL},
    {"About", NULL, false, false, NULL, cb_about},
};

/* Dynamic file menu that includes recent ROMs */
static MenuItem file_menu_items[10];  /* Open ROM + separator + 5 recent + separator + Exit = 9 max */
static int file_menu_count = 0;

typedef struct {
    const char* title;
    MenuItem* items;
    int count;
} Menu;

static Menu menus[] = {
    {"File", file_menu_items, 0},  /* count will be updated dynamically */
    {"Emulation", emulation_menu, 8},
    {"Settings", settings_menu, 5},
    {"Help", help_menu, 3},
};
static const int menu_count = 4;

static void rebuild_file_menu(void) {
    int idx = 0;
    
    /* Open ROM... */
    file_menu_items[idx++] = (MenuItem){"Open ROM...", "Ctrl+O", false, false, NULL, cb_open_rom};
    
    /* Add recent ROMs if any */
    if (ui_state.recent_rom_count > 0) {
        file_menu_items[idx++] = (MenuItem){"", NULL, true, false, NULL, NULL};  /* Separator */
        
        for (int i = 0; i < ui_state.recent_rom_count; i++) {
            /* Get just the filename from the full path */
            const char* filename = strrchr(ui_state.recent_roms[i], '/');
            filename = filename ? filename + 1 : ui_state.recent_roms[i];
            
            file_menu_items[idx++] = (MenuItem){filename, NULL, false, false, NULL, recent_rom_callbacks[i]};
        }
    }
    
    /* Separator and Exit */
    file_menu_items[idx++] = (MenuItem){"", NULL, true, false, NULL, NULL};
    file_menu_items[idx++] = (MenuItem){"Exit", "Alt+F4", false, false, NULL, cb_exit};
    
    file_menu_count = idx;
    menus[0].count = file_menu_count;  /* Update the File menu count */
}

static bool is_point_in_rect(int px, int py, int x, int y, int w, int h) {
    return px >= x && px < x + w && py >= y && py < y + h;
}

void ui_render_logo(bool rom_loaded) {
    /* Only render logo when no ROM is loaded */
    if (rom_loaded || !ui_state.logo_texture) return;
    
    /* Get window size */
    int win_w, win_h;
    SDL_GetWindowSize(ui_state.window, &win_w, &win_h);
    
    /* Get original texture size */
    int logo_w, logo_h;
    SDL_QueryTexture(ui_state.logo_texture, NULL, NULL, &logo_w, &logo_h);
    
    /* Scale logo to fit nicely - max 40% of window height, maintaining aspect ratio */
    int menu_height = ui_get_menu_height();
    int available_h = win_h - menu_height;
    int max_height = (int)(available_h * 0.4f);
    int max_width = (int)(win_w * 0.6f);
    
    float scale;
    if (logo_h > max_height || logo_w > max_width) {
        float scale_h = (float)max_height / (float)logo_h;
        float scale_w = (float)max_width / (float)logo_w;
        scale = scale_h < scale_w ? scale_h : scale_w;
    } else {
        /* Keep original size if it's already small enough */
        scale = 1.0f;
    }
    
    int scaled_w = (int)(logo_w * scale);
    int scaled_h = (int)(logo_h * scale);
    
    /* Calculate centered position (accounting for menu bar) */
    int x = (win_w - scaled_w) / 2;
    int y = menu_height + (available_h - scaled_h) / 2;
    
    /* Render logo */
    SDL_Rect dst = {x, y, scaled_w, scaled_h};
    SDL_RenderCopy(ui_state.renderer, ui_state.logo_texture, NULL, &dst);
}

void ui_render(void) {
    /* Dark green Game Boy inspired theme */
    SDL_Color bg = {15, 25, 15, 255};           /* Very dark green-black */
    SDL_Color text = {155, 255, 155, 255};      /* Bright Game Boy green */
    SDL_Color hover = {25, 55, 25, 255};        /* Dark green hover */
    SDL_Color active = {35, 75, 35, 255};       /* Medium dark green */
    SDL_Color sep = {45, 85, 45, 255};          /* Green separator */
    
    /* Draw menu bar background */
    draw_rect_filled(0, 0, 800, MENU_HEIGHT, bg);
    
    /* Draw menu titles */
    int x = 0;
    for (int i = 0; i < menu_count; i++) {
        int width = strlen(menus[i].title) * 6 + 16;
        
        /* Check if hovered */
        bool hovered = is_point_in_rect(ui_state.mouse_x, ui_state.mouse_y, 
                                        x, 0, width, MENU_HEIGHT);
        if (hovered) ui_state.menu_bar_hovered = true;
        
        /* Highlight active or hovered */
        if (i == ui_state.active_menu) {
            draw_rect_filled(x, 0, width, MENU_HEIGHT, active);
        } else if (hovered && ui_state.active_menu < 0) {
            draw_rect_filled(x, 0, width, MENU_HEIGHT, hover);
        }
        
        draw_text(menus[i].title, x + 8, 7, text);
        x += width;
    }
    
    /* Draw active dropdown menu */
    if (ui_state.active_menu >= 0 && ui_state.active_menu < menu_count) {
        Menu* menu = &menus[ui_state.active_menu];
        
        /* Calculate menu position */
        int menu_x = 0;
        for (int i = 0; i < ui_state.active_menu; i++) {
            menu_x += strlen(menus[i].title) * 6 + 16;
        }
        
        int menu_y = MENU_HEIGHT;
        int menu_h = menu->count * MENU_ITEM_HEIGHT;
        
        /* Draw menu background and border */
        draw_rect_filled(menu_x, menu_y, MENU_WIDTH, menu_h, bg);
        draw_rect_outline(menu_x, menu_y, MENU_WIDTH, menu_h, (SDL_Color){55, 120, 55, 255});
        
        /* Draw menu items */
        ui_state.hovered_item = -1;
        for (int i = 0; i < menu->count; i++) {
            MenuItem* item = &menu->items[i];
            int item_y = menu_y + i * MENU_ITEM_HEIGHT;
            
            if (item->is_separator) {
                /* Draw separator line */
                draw_rect_filled(menu_x + 8, item_y + 9, MENU_WIDTH - 16, 1, sep);
            } else {
                /* Check hover */
                bool hovered = is_point_in_rect(ui_state.mouse_x, ui_state.mouse_y,
                                               menu_x, item_y, MENU_WIDTH, MENU_ITEM_HEIGHT);
                if (hovered) {
                    ui_state.hovered_item = i;
                    draw_rect_filled(menu_x, item_y, MENU_WIDTH, MENU_ITEM_HEIGHT, hover);
                }
                
                /* Draw checkbox */
                if (item->is_checkbox && item->checked && *item->checked) {
                    draw_text("*", menu_x + 8, item_y + 7, text);
                }
                
                /* Draw label - always align at same position */
                int label_x = menu_x + 24;
                draw_text(item->label, label_x, item_y + 7, text);
                
                /* Draw shortcut */
                if (item->shortcut) {
                    int sc_x = menu_x + MENU_WIDTH - strlen(item->shortcut) * 6 - 8;
                    draw_text(item->shortcut, sc_x, item_y + 7, (SDL_Color){90, 160, 90, 255});
                }
            }
        }
    }
    
    /* Draw dialogs */
    if (ui_state.show_about) {
        int dw = 300, dh = 120;
        int win_w, win_h;
        SDL_GetWindowSize(ui_state.window, &win_w, &win_h);
        int dx = (win_w - dw) / 2;
        int dy = (win_h - dh) / 2;
        
        draw_rect_filled(dx, dy, dw, dh, bg);
        draw_rect_outline(dx, dy, dw, dh, (SDL_Color){55, 120, 55, 255});
        draw_text("GBendo - v0.5", dx + 20, dy + 20, text);
        draw_text("Game Boy Emulator", dx + 20, dy + 40, text);
        draw_text("written by 1ndevelopment", dx + 20, dy + 60, text);
        draw_text("Click anywhere to close", dx + 20, dy + 80, (SDL_Color){90, 160, 90, 255});
    }
    
    if (ui_state.show_controls) {
        int dw = 300, dh = 200;
        int win_w, win_h;
        SDL_GetWindowSize(ui_state.window, &win_w, &win_h);
        int dx = (win_w - dw) / 2;
        int dy = (win_h - dh) / 2;
        
        draw_rect_filled(dx, dy, dw, dh, bg);
        draw_rect_outline(dx, dy, dw, dh, (SDL_Color){55, 120, 55, 255});
        draw_text("Controls", dx + 20, dy + 20, text);
        draw_text("Arrow Keys: D-Pad", dx + 20, dy + 50, text);
        draw_text("Z: A Button", dx + 20, dy + 70, text);
        draw_text("X: B Button", dx + 20, dy + 90, text);
        draw_text("Enter: Start", dx + 20, dy + 110, text);
        draw_text("Shift: Select", dx + 20, dy + 130, text);
        draw_text("Click anywhere to close", dx + 20, dy + 160, (SDL_Color){90, 160, 90, 255});
    }
    
    
    /* Debug window */
    if (ui_state.show_debug) {
        int dw = 600, dh = 500;
        int win_w, win_h;
        SDL_GetWindowSize(ui_state.window, &win_w, &win_h);
        int dx = (win_w - dw) / 2;
        int dy = (win_h - dh) / 2;
        
        draw_rect_filled(dx, dy, dw, dh, bg);
        draw_rect_outline(dx, dy, dw, dh, (SDL_Color){55, 120, 55, 255});
        
        /* Title */
        draw_text("Debug Console", dx + 10, dy + 10, text);
        
        /* Component enable toggles */
        int y_offset = dy + 35;
        draw_text("Enable Components:", dx + 20, y_offset, text);
        y_offset += 20;
        
        /* Checkboxes for each component */
        const char* component_names[] = {"PPU", "APU", "CPU", "MEM", "UI"};
        int checkbox_x = dx + 20;
        bool* debug_flags[] = {&ui_state.debug_ppu, &ui_state.debug_apu, &ui_state.debug_cpu, 
                                &ui_state.debug_mem, &ui_state.debug_ui};
        
        for (int i = 0; i < 5; i++) {
            int cb_x = checkbox_x + i * 100;
            
            /* Draw checkbox */
            draw_rect_filled(cb_x, y_offset - 2, 14, 14, (SDL_Color){25, 55, 25, 255});
            draw_rect_outline(cb_x, y_offset - 2, 14, 14, (SDL_Color){55, 120, 55, 255});
            
            if (*debug_flags[i]) {
                /* Draw checkmark */
                draw_text("X", cb_x + 3, y_offset + 1, text);
            }
            
            /* Draw label */
            draw_text(component_names[i], cb_x + 18, y_offset, text);
        }
        
        y_offset += 25;
        
        /* Separator */
        draw_rect_filled(dx + 15, y_offset, dw - 30, 1, sep);
        y_offset += 10;
        
        /* Debug output area */
        int output_y = y_offset;
        int output_h = dh - (y_offset - dy) - 50;
        int output_w = dw - 50;
        
        /* Output background */
        draw_rect_filled(dx + 10, output_y, output_w, output_h, (SDL_Color){10, 20, 10, 255});
        draw_rect_outline(dx + 10, output_y, output_w, output_h, (SDL_Color){55, 120, 55, 255});
        
        /* Render debug buffer contents */
        if (ui_state.debug_buffer[0] != '\0') {
            int line_y = output_y + 5;
            int max_lines = (output_h - 10) / 8;  /* 8 pixels per line of text */
            
            /* Parse buffer and display lines */
            char* line_start = ui_state.debug_buffer;
            int line_count = 0;
            int skip_lines = ui_state.debug_scroll_offset;
            
            while (*line_start && line_count < max_lines + skip_lines) {
                char* line_end = strchr(line_start, '\n');
                if (!line_end) line_end = line_start + strlen(line_start);
                
                if (line_count >= skip_lines) {
                    /* Display this line */
                    int line_len = line_end - line_start;
                    if (line_len > 0) {
                        char line_buf[512];
                        int copy_len = line_len < 511 ? line_len : 511;
                        strncpy(line_buf, line_start, copy_len);
                        line_buf[copy_len] = '\0';
                        draw_text(line_buf, dx + 15, line_y, (SDL_Color){155, 255, 155, 255});
                    }
                    line_y += 8;
                }
                
                line_count++;
                if (*line_end == '\n') line_end++;
                if (*line_end == '\0') break;
                line_start = line_end;
            }
        } else {
            /* No debug output yet */
            SDL_Color hint_color = {90, 160, 90, 255};
            draw_text("Enable components to see debug output...", dx + 20, output_y + 10, hint_color);
        }
        
        /* Scrollbar */
        int scroll_x = dx + output_w + 15;
        int scroll_y = output_y;
        
        /* Scroll up button */
        draw_rect_filled(scroll_x, scroll_y, 20, 20, (SDL_Color){25, 55, 25, 255});
        draw_rect_outline(scroll_x, scroll_y, 20, 20, (SDL_Color){55, 120, 55, 255});
        draw_text("^", scroll_x + 7, scroll_y + 6, text);
        
        /* Scroll down button */
        int scroll_down_y = scroll_y + output_h - 20;
        draw_rect_filled(scroll_x, scroll_down_y, 20, 20, (SDL_Color){25, 55, 25, 255});
        draw_rect_outline(scroll_x, scroll_down_y, 20, 20, (SDL_Color){55, 120, 55, 255});
        draw_text("v", scroll_x + 7, scroll_down_y + 6, text);
        
        /* Buttons */
        int btn_y = dy + dh - 35;
        
        /* Clear button */
        draw_rect_filled(dx + 10, btn_y, 80, 20, (SDL_Color){25, 55, 25, 255});
        draw_rect_outline(dx + 10, btn_y, 80, 20, (SDL_Color){55, 120, 55, 255});
        draw_text("Clear", dx + 26, btn_y + 6, text);
        
        /* Close button */
        draw_rect_filled(dx + 100, btn_y, 80, 20, (SDL_Color){25, 55, 25, 255});
        draw_rect_outline(dx + 100, btn_y, 80, 20, (SDL_Color){55, 120, 55, 255});
        draw_text("Close", dx + 116, btn_y + 6, text);
        
        /* Close instruction */
        SDL_Color note_color = {90, 160, 90, 255};
        draw_text("Click outside to close", dx + dw - 150, btn_y + 6, note_color);
    }
    
    /* Settings window */
    if (ui_state.show_settings) {
        int dw = 600, dh = 500;
        int win_w, win_h;
        SDL_GetWindowSize(ui_state.window, &win_w, &win_h);
        int dx = (win_w - dw) / 2;
        int dy = (win_h - dh) / 2;
        
        draw_rect_filled(dx, dy, dw, dh, bg);
        draw_rect_outline(dx, dy, dw, dh, (SDL_Color){55, 120, 55, 255});
        
        /* Title */
        draw_text("Settings", dx + 10, dy + 10, text);
        
        /* Tab buttons */
        const char* tab_names[] = {"Video", "Audio", "Input", "Debug", "Palette"};
        int tab_count = 5;
        int tab_width = 80;
        int tab_y = dy + 35;
        
        for (int i = 0; i < tab_count; i++) {
            int tab_x = dx + 10 + i * (tab_width + 5);
            bool is_active = (i == ui_state.active_settings_tab);
            bool is_hovered = (ui_state.mouse_x >= tab_x && ui_state.mouse_x < tab_x + tab_width &&
                              ui_state.mouse_y >= tab_y && ui_state.mouse_y < tab_y + 25);
            
            SDL_Color tab_bg = is_active ? active : (is_hovered ? hover : (SDL_Color){25, 55, 25, 255});
            draw_rect_filled(tab_x, tab_y, tab_width, 25, tab_bg);
            draw_rect_outline(tab_x, tab_y, tab_width, 25, (SDL_Color){55, 120, 55, 255});
            
            int text_x = tab_x + (tab_width - strlen(tab_names[i]) * 6) / 2;
            draw_text(tab_names[i], text_x, tab_y + 9, text);
        }
        
        /* Tab content area */
        int content_y = tab_y + 30;
        
        /* Draw content based on active tab */
        switch (ui_state.active_settings_tab) {
            case 0: /* Video */
                {
                    int y_offset = content_y + 10;
                    int dropdown_x = dx + 200;
                    int dropdown_w = 140;
                    
                    /* Display Mode */
                    draw_text("Display Mode:", dx + 20, y_offset, text);
                    bool btn_hover = (ui_state.mouse_x >= dropdown_x && ui_state.mouse_x < dropdown_x + 80 &&
                                      ui_state.mouse_y >= y_offset - 3 && ui_state.mouse_y < y_offset + 15);
                    SDL_Color btn_bg = btn_hover ? hover : (SDL_Color){25, 55, 25, 255};
                    draw_rect_filled(dropdown_x, y_offset - 3, 80, 18, btn_bg);
                    draw_rect_outline(dropdown_x, y_offset - 3, 80, 18, (SDL_Color){55, 120, 55, 255});
                    const char* mode_text = window_is_fullscreen() ? "Fullscreen" : "Windowed";
                    draw_text(mode_text, dropdown_x + 8, y_offset + 2, text);
                    y_offset += 28;
                    
                    /* Window size */
                    draw_text("Window Size:", dx + 20, y_offset, text);
                    char size_str[64];
                    snprintf(size_str, sizeof(size_str), "%dx%d", win_w, win_h);
                    draw_text(size_str, dropdown_x, y_offset, (SDL_Color){90, 160, 90, 255});
                    y_offset += 25;
                    
                    /* Scale factor */
                    draw_text("Scale Factor:", dx + 20, y_offset, text);
                    char scale_str[32];
                    snprintf(scale_str, sizeof(scale_str), "%dx", window_get_scale());
                    draw_text(scale_str, dropdown_x, y_offset, (SDL_Color){90, 160, 90, 255});
                    y_offset += 25;
                    
                    /* Scaling method with dropdown */
                    draw_text("Scaling Method:", dx + 20, y_offset, text);
                    int scaling_mode = window_get_scaling_mode();
                    const char* scaling = (scaling_mode == 0) ? "Integer" : (scaling_mode == 1) ? "Linear" : "Best";
                    
                    SDL_Color scaling_bg = (ui_state.active_settings_dropdown == 0) ? active : (SDL_Color){25, 55, 25, 255};
                    draw_rect_filled(dropdown_x, y_offset - 3, dropdown_w, 18, scaling_bg);
                    draw_rect_outline(dropdown_x, y_offset - 3, dropdown_w, 18, (SDL_Color){55, 120, 55, 255});
                    draw_text(scaling, dropdown_x + 8, y_offset + 2, text);
                    draw_text("v", dropdown_x + dropdown_w - 14, y_offset + 2, (SDL_Color){90, 160, 90, 255});
                    
                    /* Dropdown menu for scaling */
                    if (ui_state.active_settings_dropdown == 0) {
                        int dd_y = y_offset + 16;
                        draw_rect_filled(dropdown_x, dd_y, dropdown_w, 54, bg);
                        draw_rect_outline(dropdown_x, dd_y, dropdown_w, 54, (SDL_Color){55, 120, 55, 255});
                        
                        bool hover_int = (ui_state.mouse_x >= dropdown_x && ui_state.mouse_x < dropdown_x + dropdown_w &&
                                          ui_state.mouse_y >= dd_y && ui_state.mouse_y < dd_y + 18);
                        bool hover_linear = (ui_state.mouse_x >= dropdown_x && ui_state.mouse_x < dropdown_x + dropdown_w &&
                                             ui_state.mouse_y >= dd_y + 18 && ui_state.mouse_y < dd_y + 36);
                        bool hover_best = (ui_state.mouse_x >= dropdown_x && ui_state.mouse_x < dropdown_x + dropdown_w &&
                                           ui_state.mouse_y >= dd_y + 36 && ui_state.mouse_y < dd_y + 54);
                        
                        if (hover_int) draw_rect_filled(dropdown_x, dd_y, dropdown_w, 18, hover);
                        draw_text("Integer", dropdown_x + 8, dd_y + 4, text);
                        
                        if (hover_linear) draw_rect_filled(dropdown_x, dd_y + 18, dropdown_w, 18, hover);
                        draw_text("Linear", dropdown_x + 8, dd_y + 22, text);
                        
                        if (hover_best) draw_rect_filled(dropdown_x, dd_y + 36, dropdown_w, 18, hover);
                        draw_text("Best", dropdown_x + 8, dd_y + 40, text);
                    }
                    y_offset += 35;
                    
                    /* Separator */
                    draw_rect_filled(dx + 15, y_offset, dw - 30, 1, sep);
                    y_offset += 15;
                    
                    /* Information notes */
                    SDL_Color note_color = {90, 160, 90, 255};
                    draw_text("Keyboard Shortcuts:", dx + 20, y_offset, text);
                    y_offset += 20;
                    draw_text("  F11 / Alt+Enter: Toggle Fullscreen", dx + 30, y_offset, note_color);
                    y_offset += 18;
                    draw_text("  VSync: Set at startup (--vsync)", dx + 30, y_offset, note_color);
                    y_offset += 18;
                    draw_text("  Scale: Set at startup (-s flag)", dx + 30, y_offset, note_color);
                }
                break;
                
            case 1: /* Audio */
                {
                    int y_offset = content_y + 10;
                    
                    draw_text("Audio Settings", dx + 20, y_offset, text);
                    y_offset += 30;
                    
                    /* Mute checkbox */
                    draw_text("Mute Sound:", dx + 20, y_offset, text);
                    int checkbox_x = dx + 200;
                    draw_rect_filled(checkbox_x, y_offset - 2, 14, 14, (SDL_Color){25, 55, 25, 255});
                    draw_rect_outline(checkbox_x, y_offset - 2, 14, 14, (SDL_Color){55, 120, 55, 255});
                    if (ui_state.muted) {
                        draw_text("X", checkbox_x + 3, y_offset + 1, text);
                    }
                    y_offset += 25;
                    
                    /* Volume info */
                    draw_text("Volume:", dx + 20, y_offset, text);
                    draw_text("100%", dx + 200, y_offset, (SDL_Color){90, 160, 90, 255});
                    y_offset += 25;
                    
                    /* Sample rate info */
                    draw_text("Sample Rate:", dx + 20, y_offset, text);
                    draw_text("44100 Hz", dx + 200, y_offset, (SDL_Color){90, 160, 90, 255});
                    y_offset += 25;
                    
                    /* Channels info */
                    draw_text("Channels:", dx + 20, y_offset, text);
                    draw_text("Stereo", dx + 200, y_offset, (SDL_Color){90, 160, 90, 255});
                    y_offset += 35;
                    
                    /* Separator */
                    draw_rect_filled(dx + 15, y_offset, dw - 30, 1, sep);
                    y_offset += 15;
                    
                    /* Information notes */
                    SDL_Color note_color = {90, 160, 90, 255};
                    draw_text("Keyboard Shortcuts:", dx + 20, y_offset, text);
                    y_offset += 20;
                    draw_text("  M: Toggle Mute", dx + 30, y_offset, note_color);
                }
                break;
                
            case 2: /* Input */
                {
                    int y_offset = content_y + 10;
                    
                    draw_text("Input Settings", dx + 20, y_offset, text);
                    y_offset += 30;
                    
                    /* Control mappings */
                    draw_text("Game Boy Controls:", dx + 20, y_offset, text);
                    y_offset += 25;
                    
                    const char* controls[][2] = {
                        {"D-Pad:", "Arrow Keys"},
                        {"A Button:", "Z Key"},
                        {"B Button:", "X Key"},
                        {"Start:", "Enter Key"},
                        {"Select:", "Shift Key"}
                    };
                    
                    for (int i = 0; i < 5; i++) {
                        draw_text(controls[i][0], dx + 30, y_offset, text);
                        draw_text(controls[i][1], dx + 200, y_offset, (SDL_Color){90, 160, 90, 255});
                        y_offset += 20;
                    }
                    
                    y_offset += 15;
                    
                    /* Separator */
                    draw_rect_filled(dx + 15, y_offset, dw - 30, 1, sep);
                    y_offset += 15;
                    
                    /* Information notes */
                    SDL_Color note_color = {90, 160, 90, 255};
                    draw_text("Note: Control mappings are currently fixed.", dx + 20, y_offset, note_color);
                    y_offset += 18;
                    draw_text("Custom key mapping will be added in a future version.", dx + 20, y_offset, note_color);
                }
                break;
                
            case 3: /* Debug */
                {
                    int y_offset = content_y + 10;
                    
                    draw_text("Debug Settings", dx + 20, y_offset, text);
                    y_offset += 30;
                    
                    /* Debug enable checkboxes */
                    draw_text("Enable Debug Components:", dx + 20, y_offset, text);
                    y_offset += 25;
                    
                    const char* component_names[] = {"PPU", "APU", "CPU", "MEM", "UI"};
                    bool* debug_flags[] = {&ui_state.debug_ppu, &ui_state.debug_apu, &ui_state.debug_cpu, 
                                            &ui_state.debug_mem, &ui_state.debug_ui};
                    
                    for (int i = 0; i < 5; i++) {
                        int cb_x = dx + 30;
                        int cb_y = y_offset + i * 25;
                        
                        /* Draw checkbox */
                        draw_rect_filled(cb_x, cb_y - 2, 14, 14, (SDL_Color){25, 55, 25, 255});
                        draw_rect_outline(cb_x, cb_y - 2, 14, 14, (SDL_Color){55, 120, 55, 255});
                        
                        if (*debug_flags[i]) {
                            draw_text("X", cb_x + 3, cb_y + 1, text);
                        }
                        
                        /* Draw label */
                        draw_text(component_names[i], cb_x + 20, cb_y, text);
                    }
                    
                    y_offset += 5 * 25 + 15;
                    
                    /* Debug console button */
                    bool console_hover = (ui_state.mouse_x >= dx + 30 && ui_state.mouse_x < dx + 150 &&
                                         ui_state.mouse_y >= y_offset && ui_state.mouse_y < y_offset + 25);
                    SDL_Color console_bg = console_hover ? hover : (SDL_Color){25, 55, 25, 255};
                    draw_rect_filled(dx + 30, y_offset, 120, 25, console_bg);
                    draw_rect_outline(dx + 30, y_offset, 120, 25, (SDL_Color){55, 120, 55, 255});
                    draw_text("Open Console", dx + 45, y_offset + 9, text);
                    y_offset += 35;
                    
                    /* Separator */
                    draw_rect_filled(dx + 15, y_offset, dw - 30, 1, sep);
                    y_offset += 15;
                    
                    /* Information notes */
                    SDL_Color note_color = {90, 160, 90, 255};
                    draw_text("Debug output will be shown in the debug console.", dx + 20, y_offset, note_color);
                    y_offset += 18;
                    draw_text("Enable components to see detailed logging information.", dx + 20, y_offset, note_color);
                }
                break;
                
            case 4: /* Palette */
                {
                    int y_offset = content_y + 10;
                    int dropdown_x = dx + 200;
                    int dropdown_w = 180;
                    
                    draw_text("Palette Settings", dx + 20, y_offset, text);
                    y_offset += 30;
                    
                    /* Palette selection dropdown */
                    draw_text("Color Palette:", dx + 20, y_offset, text);
                    
                    const char* current_palette_name = ppu_get_palette_name(ui_state.selected_palette);
                    SDL_Color palette_bg = (ui_state.active_settings_dropdown == 2) ? active : (SDL_Color){25, 55, 25, 255};
                    draw_rect_filled(dropdown_x, y_offset - 3, dropdown_w, 18, palette_bg);
                    draw_rect_outline(dropdown_x, y_offset - 3, dropdown_w, 18, (SDL_Color){55, 120, 55, 255});
                    draw_text(current_palette_name, dropdown_x + 8, y_offset + 2, text);
                    draw_text("v", dropdown_x + dropdown_w - 14, y_offset + 2, (SDL_Color){90, 160, 90, 255});
                    y_offset += 35;
                    
                    /* Color preview */
                    draw_text("Preview:", dx + 20, y_offset, text);
                    y_offset += 20;
                    
                    const uint32_t* palette_colors = ppu_get_palette_colors(ui_state.selected_palette);
                    int preview_x = dx + 30;
                    int preview_size = 40;
                    int preview_spacing = 10;
                    
                    for (int i = 0; i < 4; i++) {
                        int px = preview_x + i * (preview_size + preview_spacing);
                        uint32_t color = palette_colors[i];
                        SDL_Color sdl_color = {
                            (color >> 16) & 0xFF,
                            (color >> 8) & 0xFF,
                            color & 0xFF,
                            255
                        };
                        draw_rect_filled(px, y_offset, preview_size, preview_size, sdl_color);
                        draw_rect_outline(px, y_offset, preview_size, preview_size, (SDL_Color){55, 120, 55, 255});
                    }
                    y_offset += preview_size + 20;
                    
                    /* Separator */
                    draw_rect_filled(dx + 15, y_offset, dw - 30, 1, sep);
                    y_offset += 15;
                    
                    /* Information notes */
                    SDL_Color note_color = {90, 160, 90, 255};
                    draw_text("Available Palettes:", dx + 20, y_offset, text);
                    y_offset += 20;
                    draw_text("  Authentic DMG: Original Game Boy colors", dx + 30, y_offset, note_color);
                    y_offset += 18;
                    draw_text("  Grayscale: Classic black and white", dx + 30, y_offset, note_color);
                    y_offset += 18;
                    draw_text("  BGB Emulator: Modern emulator palette", dx + 30, y_offset, note_color);
                    y_offset += 18;
                    draw_text("  Game Boy Pocket: DMG Pocket colors", dx + 30, y_offset, note_color);
                    y_offset += 18;
                    draw_text("  Game Boy Light: DMG Light colors", dx + 30, y_offset, note_color);
                }
                break;
        }
        
        /* Close button */
        int btn_y = dy + dh - 35;
        bool close_hover = (ui_state.mouse_x >= dx + dw - 90 && ui_state.mouse_x < dx + dw - 10 &&
                           ui_state.mouse_y >= btn_y && ui_state.mouse_y < btn_y + 25);
        SDL_Color close_bg = close_hover ? hover : (SDL_Color){25, 55, 25, 255};
        draw_rect_filled(dx + dw - 90, btn_y, 80, 25, close_bg);
        draw_rect_outline(dx + dw - 90, btn_y, 80, 25, (SDL_Color){55, 120, 55, 255});
        draw_text("Close", dx + dw - 66, btn_y + 9, text);
        
        /* Close instruction */
        SDL_Color note_color = {90, 160, 90, 255};
        draw_text("Click outside to close", dx + 10, btn_y + 9, note_color);
        
        /* Render palette dropdown on top of everything else */
        if (ui_state.active_settings_tab == 4 && ui_state.active_settings_dropdown == 2) {
            int content_y = dy + 35 + 30;  /* tab_y + 30 */
            int y_offset = content_y + 10 + 30;  /* Skip title and label */
            int dropdown_x = dx + 200;
            int dropdown_w = 180;
            
            int palette_count = ppu_get_palette_count();
            int dd_y = y_offset + 16;
            int dd_h = palette_count * 18;
            draw_rect_filled(dropdown_x, dd_y, dropdown_w, dd_h, bg);
            draw_rect_outline(dropdown_x, dd_y, dropdown_w, dd_h, (SDL_Color){55, 120, 55, 255});
            
            for (int i = 0; i < palette_count; i++) {
                bool hover_palette = (ui_state.mouse_x >= dropdown_x && ui_state.mouse_x < dropdown_x + dropdown_w &&
                                     ui_state.mouse_y >= dd_y + i * 18 && ui_state.mouse_y < dd_y + (i + 1) * 18);
                
                if (hover_palette) draw_rect_filled(dropdown_x, dd_y + i * 18, dropdown_w, 18, hover);
                draw_text(ppu_get_palette_name(i), dropdown_x + 8, dd_y + i * 18 + 4, text);
            }
        }
    }
    
    /* File browser dialog */
    if (ui_state.show_file_browser) {
        int dw = 500, dh = 400;
        int win_w, win_h;
        SDL_GetWindowSize(ui_state.window, &win_w, &win_h);
        int dx = (win_w - dw) / 2;
        int dy = (win_h - dh) / 2;
        
        draw_rect_filled(dx, dy, dw, dh, bg);
        draw_rect_outline(dx, dy, dw, dh, (SDL_Color){55, 120, 55, 255});
        
        /* Title */
        draw_text("Open ROM File", dx + 10, dy + 10, text);
        
        /* Editable directory path field */
        int path_field_y = dy + 28;
        int path_field_w = dw - 70;
        
        /* Draw path input box */
        SDL_Color path_bg = ui_state.editing_path ? (SDL_Color){35, 75, 35, 255} : (SDL_Color){25, 55, 25, 255};
        draw_rect_filled(dx + 10, path_field_y, path_field_w, 18, path_bg);
        draw_rect_outline(dx + 10, path_field_y, path_field_w, 18, (SDL_Color){55, 120, 55, 255});
        
        /* Display current path or editing buffer */
        const char* display_path = ui_state.editing_path ? ui_state.path_edit_buffer : ui_state.current_dir;
        int text_start_x = dx + 15;
        int text_y = path_field_y + 6;
        
        if (ui_state.editing_path && ui_state.selection_start != ui_state.selection_end) {
            /* Draw text with selection highlighting */
            int sel_start = ui_state.selection_start < ui_state.selection_end ? ui_state.selection_start : ui_state.selection_end;
            int sel_end = ui_state.selection_start > ui_state.selection_end ? ui_state.selection_start : ui_state.selection_end;
            
            /* Draw text before selection */
            if (sel_start > 0) {
                char before_text[512];
                strncpy(before_text, display_path, sel_start);
                before_text[sel_start] = '\0';
                draw_text(before_text, text_start_x, text_y, text);
            }
            
            /* Draw selection highlight */
            int sel_x = text_start_x + sel_start * 6;
            int sel_width = (sel_end - sel_start) * 6;
            draw_rect_filled(sel_x, path_field_y + 4, sel_width, 10, (SDL_Color){80, 140, 80, 255});
            
            /* Draw selected text */
            if (sel_end > sel_start) {
                char sel_text[512];
                strncpy(sel_text, display_path + sel_start, sel_end - sel_start);
                sel_text[sel_end - sel_start] = '\0';
                draw_text(sel_text, sel_x, text_y, (SDL_Color){0, 0, 0, 255});  /* Black text on selection */
            }
            
            /* Draw text after selection */
            if (sel_end < (int)strlen(display_path)) {
                draw_text(display_path + sel_end, text_start_x + sel_end * 6, text_y, text);
            }
        } else {
            /* No selection - draw normal text */
            draw_text(display_path, text_start_x, text_y, text);
        }
        
        /* Draw cursor if editing and no selection */
        if (ui_state.editing_path && ui_state.selection_start == ui_state.selection_end && (SDL_GetTicks() / 500) % 2 == 0) {
            int cursor_x = text_start_x + ui_state.cursor_pos * 6;
            draw_rect_filled(cursor_x, path_field_y + 5, 1, 8, text);
        }
        
        /* Go button */
        int go_btn_x = dx + path_field_w + 15;
        draw_rect_filled(go_btn_x, path_field_y, 45, 18, (SDL_Color){25, 55, 25, 255});
        draw_rect_outline(go_btn_x, path_field_y, 45, 18, (SDL_Color){55, 120, 55, 255});
        draw_text("Go", go_btn_x + 13, path_field_y + 5, text);
        
        /* Bookmarks section */
        int bookmarks_y = dy + 50;
        draw_text("Bookmarks:", dx + 10, bookmarks_y, text);
        
        /* Add bookmark button */
        int add_btn_x = dx + 85;
        bool add_hover = (ui_state.mouse_x >= add_btn_x && ui_state.mouse_x < add_btn_x + 20 &&
                         ui_state.mouse_y >= bookmarks_y - 3 && ui_state.mouse_y < bookmarks_y + 15);
        SDL_Color add_bg = add_hover ? hover : (SDL_Color){25, 55, 25, 255};
        draw_rect_filled(add_btn_x, bookmarks_y - 3, 20, 18, add_bg);
        draw_rect_outline(add_btn_x, bookmarks_y - 3, 20, 18, (SDL_Color){55, 120, 55, 255});
        draw_text("+", add_btn_x + 8, bookmarks_y + 2, text);
        
        /* Bookmarks dropdown button */
        int bookmarks_btn_x = dx + 110;
        bool bookmarks_hover = (ui_state.mouse_x >= bookmarks_btn_x && ui_state.mouse_x < bookmarks_btn_x + 60 &&
                               ui_state.mouse_y >= bookmarks_y - 3 && ui_state.mouse_y < bookmarks_y + 15);
        SDL_Color bookmarks_bg = (ui_state.show_bookmark_menu || bookmarks_hover) ? hover : (SDL_Color){25, 55, 25, 255};
        draw_rect_filled(bookmarks_btn_x, bookmarks_y - 3, 60, 18, bookmarks_bg);
        draw_rect_outline(bookmarks_btn_x, bookmarks_y - 3, 60, 18, (SDL_Color){55, 120, 55, 255});
        draw_text("Saved", bookmarks_btn_x + 8, bookmarks_y + 2, text);
        draw_text("v", bookmarks_btn_x + 48, bookmarks_y + 2, (SDL_Color){90, 160, 90, 255});
        
        
        /* File list */
        int list_y = dy + 75;  /* Moved down to make room for bookmarks */
        int list_h = dh - 115; /* Adjusted height */
        int visible_items = list_h / 18;
        int max_scroll = ui_state.file_count - visible_items;
        if (max_scroll < 0) max_scroll = 0;
        
        /* Scroll up button */
        int scroll_btn_x = dx + dw - 25;
        int scroll_up_y = list_y;
        draw_rect_filled(scroll_btn_x, scroll_up_y, 20, 20, (SDL_Color){25, 55, 25, 255});
        draw_rect_outline(scroll_btn_x, scroll_up_y, 20, 20, (SDL_Color){55, 120, 55, 255});
        draw_text("^", scroll_btn_x + 7, scroll_up_y + 6, text);
        
        /* Scroll down button */
        int scroll_down_y = dy + dh - 60;
        draw_rect_filled(scroll_btn_x, scroll_down_y, 20, 20, (SDL_Color){25, 55, 25, 255});
        draw_rect_outline(scroll_btn_x, scroll_down_y, 20, 20, (SDL_Color){55, 120, 55, 255});
        draw_text("v", scroll_btn_x + 7, scroll_down_y + 6, text);
        
        /* Scrollbar track between buttons */
        int track_y = scroll_up_y + 20;
        int track_h = scroll_down_y - track_y;
        draw_rect_filled(scroll_btn_x, track_y, 20, track_h, (SDL_Color){15, 25, 15, 255});
        draw_rect_outline(scroll_btn_x, track_y, 20, track_h, (SDL_Color){55, 120, 55, 255});
        
        /* Scrollbar thumb */
        if (max_scroll > 0) {
            float thumb_ratio = (float)visible_items / (float)ui_state.file_count;
            int thumb_h = (int)(track_h * thumb_ratio);
            if (thumb_h < 20) thumb_h = 20;  /* Minimum thumb size */
            
            float scroll_ratio = (float)ui_state.scroll_offset / (float)max_scroll;
            int thumb_y = track_y + (int)((track_h - thumb_h) * scroll_ratio);
            
            draw_rect_filled(scroll_btn_x + 2, thumb_y, 16, thumb_h, (SDL_Color){35, 75, 35, 255});
            draw_rect_outline(scroll_btn_x + 2, thumb_y, 16, thumb_h, (SDL_Color){90, 160, 90, 255});
        }
        
        for (int i = 0; i < visible_items && (i + ui_state.scroll_offset) < ui_state.file_count; i++) {
            int idx = i + ui_state.scroll_offset;
            int item_y = list_y + i * 18;
            
            /* Highlight selected item */
            if (idx == ui_state.selected_file) {
                draw_rect_filled(dx + 5, item_y, dw - 35, 16, active);
            }
            
            /* Check if mouse is hovering over this item */
            if (ui_state.mouse_x >= dx + 5 && ui_state.mouse_x < dx + dw - 30 &&
                ui_state.mouse_y >= item_y && ui_state.mouse_y < item_y + 16) {
                if (idx != ui_state.selected_file) {
                    draw_rect_filled(dx + 5, item_y, dw - 35, 16, hover);
                }
            }
            
            draw_text(ui_state.file_list[idx], dx + 10, item_y + 4, text);
        }
        
        /* Buttons */
        int btn_y = dy + dh - 30;
        draw_rect_filled(dx + 10, btn_y, 80, 20, (SDL_Color){25, 55, 25, 255});
        draw_rect_outline(dx + 10, btn_y, 80, 20, (SDL_Color){55, 120, 55, 255});
        draw_text("Open", dx + 30, btn_y + 6, text);
        
        draw_rect_filled(dx + 100, btn_y, 80, 20, (SDL_Color){25, 55, 25, 255});
        draw_rect_outline(dx + 100, btn_y, 80, 20, (SDL_Color){55, 120, 55, 255});
        draw_text("Cancel", dx + 110, btn_y + 6, text);
        
        /* Bookmarks dropdown menu - rendered last to appear on top */
        if (ui_state.show_bookmark_menu) {
            int bookmarks_y = dy + 50;
            int bookmarks_btn_x = dx + 110;
            int menu_y = bookmarks_y + 16;
            int menu_h = ui_state.bookmark_count > 0 ? ui_state.bookmark_count * 18 + 4 : 22;
            draw_rect_filled(bookmarks_btn_x, menu_y, 200, menu_h, bg);
            draw_rect_outline(bookmarks_btn_x, menu_y, 200, menu_h, (SDL_Color){55, 120, 55, 255});
            
            if (ui_state.bookmark_count > 0) {
                for (int i = 0; i < ui_state.bookmark_count; i++) {
                    int item_y = menu_y + 2 + i * 18;
                    bool item_hover = (ui_state.mouse_x >= bookmarks_btn_x && ui_state.mouse_x < bookmarks_btn_x + 200 &&
                                      ui_state.mouse_y >= item_y && ui_state.mouse_y < item_y + 16);
                    
                    if (item_hover) {
                        draw_rect_filled(bookmarks_btn_x + 2, item_y, 196, 16, hover);
                    }
                    
                    /* Get just the directory name for display */
                    const char* dir_name = strrchr(ui_state.bookmarked_dirs[i], '/');
                    dir_name = dir_name ? dir_name + 1 : ui_state.bookmarked_dirs[i];
                    if (strlen(dir_name) == 0) dir_name = "/";
                    
                    /* Truncate long names */
                    char display_name[30];
                    if (strlen(dir_name) > 25) {
                        strncpy(display_name, dir_name, 22);
                        display_name[22] = '\0';
                        strcat(display_name, "...");
                    } else {
                        strncpy(display_name, dir_name, sizeof(display_name) - 1);
                        display_name[sizeof(display_name) - 1] = '\0';
                    }
                    
                    draw_text(display_name, bookmarks_btn_x + 6, item_y + 4, text);
                    
                    /* Delete button */
                    int del_x = bookmarks_btn_x + 180;
                    bool del_hover = (ui_state.mouse_x >= del_x && ui_state.mouse_x < del_x + 16 &&
                                     ui_state.mouse_y >= item_y && ui_state.mouse_y < item_y + 16);
                    if (del_hover) {
                        draw_rect_filled(del_x, item_y, 16, 16, (SDL_Color){60, 25, 25, 255});
                    }
                    draw_text("X", del_x + 6, item_y + 4, del_hover ? (SDL_Color){255, 200, 200, 255} : (SDL_Color){160, 90, 90, 255});
                }
            } else {
                draw_text("No bookmarks", bookmarks_btn_x + 6, menu_y + 6, (SDL_Color){90, 160, 90, 255});
            }
        }
    }
}

bool ui_handle_event(SDL_Event* event) {
    if (event->type == SDL_MOUSEMOTION) {
        ui_state.mouse_x = event->motion.x;
        ui_state.mouse_y = event->motion.y;
        
        /* Handle scrollbar dragging */
        if (ui_state.dragging_scrollbar && ui_state.show_file_browser) {
            int dw = 500, dh = 400;
            int win_w, win_h;
            SDL_GetWindowSize(ui_state.window, &win_w, &win_h);
            __attribute__((unused)) int dx = (win_w - dw) / 2;
            int dy = (win_h - dh) / 2;
            
            int list_y = dy + 75;  /* Updated to match new file list position */
            int list_h = dh - 115; /* Updated to match new file list height */
            int visible_items = list_h / 18;
            int max_scroll = ui_state.file_count - visible_items;
            if (max_scroll < 0) max_scroll = 0;
            
            int scroll_up_y = list_y;
            int scroll_down_y = dy + dh - 60;
            int track_y = scroll_up_y + 20;
            int track_h = scroll_down_y - track_y;
            
            float thumb_ratio = (float)visible_items / (float)ui_state.file_count;
            int thumb_h = (int)(track_h * thumb_ratio);
            if (thumb_h < 20) thumb_h = 20;
            
            /* Calculate new scroll position based on mouse Y */
            int mouse_delta = event->motion.y - ui_state.drag_start_y;
            float scroll_delta = (float)mouse_delta / (float)(track_h - thumb_h);
            int new_offset = ui_state.drag_start_offset + (int)(scroll_delta * max_scroll);
            
            if (new_offset < 0) new_offset = 0;
            if (new_offset > max_scroll) new_offset = max_scroll;
            
            ui_state.scroll_offset = new_offset;
        }
        
        /* Handle mouse drag for text selection */
        if (ui_state.mouse_selecting && ui_state.editing_path && ui_state.show_file_browser) {
            int dw = 500, dh = 400;
            int win_w, win_h;
            SDL_GetWindowSize(ui_state.window, &win_w, &win_h);
            int dx = (win_w - dw) / 2;
            int dy = (win_h - dh) / 2;
            int path_field_y = dy + 28;
            int path_field_w = dw - 70;
            
            if (event->motion.x >= dx + 10 && event->motion.x < dx + 10 + path_field_w &&
                event->motion.y >= path_field_y && event->motion.y < path_field_y + 18) {
                int drag_offset = (event->motion.x - (dx + 15)) / 6;
                int text_len = strlen(ui_state.path_edit_buffer);
                drag_offset = drag_offset < 0 ? 0 : (drag_offset > text_len ? text_len : drag_offset);
                ui_state.selection_end = drag_offset;
                ui_state.cursor_pos = drag_offset;
            }
        }
        
        /* Auto-open menus when hovering with a menu already open */
        if (ui_state.active_menu >= 0 && event->motion.y < MENU_HEIGHT) {
            int x = 0;
            for (int i = 0; i < menu_count; i++) {
                int width = strlen(menus[i].title) * 6 + 16;
                if (event->motion.x >= x && event->motion.x < x + width) {
                    if (i != ui_state.active_menu) {
                        ui_state.active_menu = i;
                        ui_state.hovered_item = -1;
                        return true;
                    }
                    break;
                }
                x += width;
            }
        }
        
        return ui_wants_mouse();
    }
    
    if (event->type == SDL_MOUSEBUTTONDOWN) {
        ui_state.mouse_down = true;
        
        
        /* Handle settings window */
        if (ui_state.show_settings) {
            int dw = 600, dh = 500;
            int win_w, win_h;
            SDL_GetWindowSize(ui_state.window, &win_w, &win_h);
            int dx = (win_w - dw) / 2;
            int dy = (win_h - dh) / 2;
            
            /* Check if clicking inside the dialog */
            if (event->button.x >= dx && event->button.x < dx + dw &&
                event->button.y >= dy && event->button.y < dy + dh) {
                
                /* Handle tab clicks */
                int tab_width = 80;
                int tab_y = dy + 35;
                for (int i = 0; i < 5; i++) {
                    int tab_x = dx + 10 + i * (tab_width + 5);
                    if (event->button.x >= tab_x && event->button.x < tab_x + tab_width &&
                        event->button.y >= tab_y && event->button.y < tab_y + 25) {
                        ui_state.active_settings_tab = i;
                        ui_state.active_settings_dropdown = -1;  /* Close any open dropdowns */
                        return true;
                    }
                }
                
                /* Handle content based on active tab */
                int content_y = tab_y + 30;
                
                switch (ui_state.active_settings_tab) {
                    case 0: /* Video */
                        {
                            int y_offset = content_y + 10;
                            int dropdown_x = dx + 200;
                            int dropdown_w = 140;
                            
                            /* Display Mode button */
                            if (event->button.x >= dropdown_x && event->button.x < dropdown_x + 80 &&
                                event->button.y >= y_offset - 3 && event->button.y < y_offset + 15) {
                                window_toggle_fullscreen();
                                return true;
                            }
                            y_offset += 28 + 25 + 25;  /* Skip to Scaling Method */
                            
                            /* Scaling Method dropdown */
                            if (event->button.x >= dropdown_x && event->button.x < dropdown_x + dropdown_w &&
                                event->button.y >= y_offset - 3 && event->button.y < y_offset + 15) {
                                ui_state.active_settings_dropdown = (ui_state.active_settings_dropdown == 0) ? -1 : 0;
                                return true;
                            }
                            
                            /* Scaling Method dropdown menu items */
                            if (ui_state.active_settings_dropdown == 0) {
                                int dd_y = y_offset + 16;
                                if (event->button.x >= dropdown_x && event->button.x < dropdown_x + dropdown_w) {
                                    if (event->button.y >= dd_y && event->button.y < dd_y + 18) {
                                        window_set_scaling_mode("0");
                                        ui_state.active_settings_dropdown = -1;
                                        return true;
                                    } else if (event->button.y >= dd_y + 18 && event->button.y < dd_y + 36) {
                                        window_set_scaling_mode("1");
                                        ui_state.active_settings_dropdown = -1;
                                        return true;
                                    } else if (event->button.y >= dd_y + 36 && event->button.y < dd_y + 54) {
                                        window_set_scaling_mode("2");
                                        ui_state.active_settings_dropdown = -1;
                                        return true;
                                    }
                                }
                            }
                        }
                        break;
                        
                    case 1: /* Audio */
                        {
                            int y_offset = content_y + 10 + 30;  /* Skip title */
                            int checkbox_x = dx + 200;
                            
                            /* Mute checkbox */
                            if (event->button.x >= checkbox_x && event->button.x < checkbox_x + 14 &&
                                event->button.y >= y_offset - 2 && event->button.y < y_offset + 12) {
                                ui_state.muted = !ui_state.muted;
                                audio_set_muted(ui_state.muted);
                                return true;
                            }
                        }
                        break;
                        
                    case 3: /* Debug */
                        {
                            int y_offset = content_y + 10 + 30 + 25;  /* Skip title and "Enable Debug Components:" */
                            
                            /* Debug enable checkboxes */
                            bool* debug_flags[] = {&ui_state.debug_ppu, &ui_state.debug_apu, &ui_state.debug_cpu, 
                                                    &ui_state.debug_mem, &ui_state.debug_ui};
                            
                            for (int i = 0; i < 5; i++) {
                                int cb_x = dx + 30;
                                int cb_y = y_offset + i * 25;
                                if (event->button.x >= cb_x && event->button.x < cb_x + 14 &&
                                    event->button.y >= cb_y - 2 && event->button.y < cb_y + 12) {
                                    *debug_flags[i] = !*debug_flags[i];
                                    return true;
                                }
                            }
                            
                            /* Debug console button */
                            y_offset += 5 * 25 + 15;
                            if (event->button.x >= dx + 30 && event->button.x < dx + 150 &&
                                event->button.y >= y_offset && event->button.y < y_offset + 25) {
                                ui_state.show_debug = true;
                                return true;
                            }
                        }
                        break;
                    
                    case 4: /* Palette */
                        {
                            int y_offset = content_y + 10;
                            y_offset += 30;  /* Skip "Palette Settings" label */
                            
                            int dropdown_x = dx + 200;
                            int dropdown_w = 180;
                            
                            /* Check palette dropdown click */
                            if (event->button.x >= dropdown_x && event->button.x < dropdown_x + dropdown_w &&
                                event->button.y >= y_offset - 3 && event->button.y < y_offset + 15) {
                                /* Toggle palette dropdown */
                                ui_state.active_settings_dropdown = (ui_state.active_settings_dropdown == 2) ? -1 : 2;
                                return true;
                            }
                            
                            /* Check palette dropdown menu items */
                            if (ui_state.active_settings_dropdown == 2) {
                                int palette_count = ppu_get_palette_count();
                                int dd_y = y_offset + 16;
                                
                                for (int i = 0; i < palette_count; i++) {
                                    if (event->button.x >= dropdown_x && event->button.x < dropdown_x + dropdown_w &&
                                        event->button.y >= dd_y + i * 18 && event->button.y < dd_y + (i + 1) * 18) {
                                        ui_state.selected_palette = i;
                                        ppu_set_palette(i);
                                        save_palette_setting();
                                        ui_state.active_settings_dropdown = -1;
                                        return true;
                                    }
                                }
                            }
                        }
                        break;
                }
                
                /* Close button */
                int btn_y = dy + dh - 35;
                if (event->button.x >= dx + dw - 90 && event->button.x < dx + dw - 10 &&
                    event->button.y >= btn_y && event->button.y < btn_y + 25) {
                    ui_state.show_settings = false;
                    ui_state.active_settings_dropdown = -1;
                    return true;
                }
                
                /* Clicking inside but not on controls - keep dialog open */
                return true;
            }
            
            /* Click outside closes the dialog */
            ui_state.show_settings = false;
            ui_state.active_settings_dropdown = -1;
            return true;
        }
        
        /* Handle debug window */
        if (ui_state.show_debug) {
            int dw = 600, dh = 500;
            int win_w, win_h;
            SDL_GetWindowSize(ui_state.window, &win_w, &win_h);
            int dx = (win_w - dw) / 2;
            int dy = (win_h - dh) / 2;
            
            /* Check if clicking inside the dialog */
            if (event->button.x >= dx && event->button.x < dx + dw &&
                event->button.y >= dy && event->button.y < dy + dh) {
                
                int y_offset = dy + 35;
                y_offset += 20;  /* Skip "Enable Components:" label */
                
                /* Check component enable checkboxes */
                int checkbox_x = dx + 20;
                bool* debug_flags[] = {&ui_state.debug_ppu, &ui_state.debug_apu, &ui_state.debug_cpu, 
                                        &ui_state.debug_mem, &ui_state.debug_ui};
                
                for (int i = 0; i < 5; i++) {
                    int cb_x = checkbox_x + i * 100;
                    if (event->button.x >= cb_x && event->button.x < cb_x + 14 &&
                        event->button.y >= y_offset - 2 && event->button.y < y_offset + 12) {
                        *debug_flags[i] = !*debug_flags[i];
                        return true;
                    }
                }
                
                /* Check scroll buttons */
                y_offset += 25;
                y_offset += 10;  /* After separator */
                int output_y = y_offset;
                int output_h = dh - (y_offset - dy) - 50;
                int output_w = dw - 50;
                int scroll_x = dx + output_w + 15;
                int scroll_y = output_y;
                int scroll_down_y = scroll_y + output_h - 20;
                
                /* Scroll up button */
                if (event->button.x >= scroll_x && event->button.x < scroll_x + 20 &&
                    event->button.y >= scroll_y && event->button.y < scroll_y + 20) {
                    if (ui_state.debug_scroll_offset > 0) {
                        ui_state.debug_scroll_offset--;
                    }
                    return true;
                }
                
                /* Scroll down button */
                if (event->button.x >= scroll_x && event->button.x < scroll_x + 20 &&
                    event->button.y >= scroll_down_y && event->button.y < scroll_down_y + 20) {
                    ui_state.debug_scroll_offset++;
                    return true;
                }
                
                /* Check buttons */
                int btn_y = dy + dh - 35;
                
                /* Clear button */
                if (event->button.x >= dx + 10 && event->button.x < dx + 90 &&
                    event->button.y >= btn_y && event->button.y < btn_y + 20) {
                    memset(ui_state.debug_buffer, 0, sizeof(ui_state.debug_buffer));
                    ui_state.debug_buffer_offset = 0;
                    ui_state.debug_scroll_offset = 0;
                    return true;
                }
                
                /* Close button */
                if (event->button.x >= dx + 100 && event->button.x < dx + 180 &&
                    event->button.y >= btn_y && event->button.y < btn_y + 20) {
                    ui_state.show_debug = false;
                    return true;
                }
                
                /* Clicking inside but not on controls - keep dialog open */
                return true;
            }
            
            /* Click outside closes the dialog */
            ui_state.show_debug = false;
            return true;
        }
        
        /* Close dialogs */
        if (ui_state.show_about || ui_state.show_controls) {
            ui_state.show_about = false;
            ui_state.show_controls = false;
            return true;
        }
        
        /* Handle file browser */
        if (ui_state.show_file_browser) {
            int dw = 500, dh = 400;
            int win_w, win_h;
            SDL_GetWindowSize(ui_state.window, &win_w, &win_h);
            int dx = (win_w - dw) / 2;
            int dy = (win_h - dh) / 2;
            
            /* Check if click is inside dialog */
            if (event->button.x >= dx && event->button.x < dx + dw &&
                event->button.y >= dy && event->button.y < dy + dh) {
                
                /* Handle path field clicks */
                int path_field_y = dy + 28;
                int path_field_w = dw - 70;
                int go_btn_x = dx + path_field_w + 15;
                
                /* Check if clicking path input field */
                if (event->button.button == SDL_BUTTON_LEFT &&
                    event->button.x >= dx + 10 && event->button.x < dx + 10 + path_field_w &&
                    event->button.y >= path_field_y && event->button.y < path_field_y + 18) {
                    if (!ui_state.editing_path) {
                        ui_state.editing_path = true;
                        strncpy(ui_state.path_edit_buffer, ui_state.current_dir, sizeof(ui_state.path_edit_buffer));
                        ui_state.path_edit_buffer[sizeof(ui_state.path_edit_buffer) - 1] = '\0';
                        SDL_StartTextInput();  /* Enable text input mode */
                    }
                    /* Calculate cursor position from mouse click */
                    int click_offset = (event->button.x - (dx + 15)) / 6;
                    int text_len = strlen(ui_state.path_edit_buffer);
                    ui_state.cursor_pos = click_offset < 0 ? 0 : (click_offset > text_len ? text_len : click_offset);
                    ui_state.selection_start = ui_state.selection_end = ui_state.cursor_pos;
                    ui_state.mouse_selecting = true;
                    return true;
                }
                
                /* Check if clicking Go button */
                if (event->button.x >= go_btn_x && event->button.x < go_btn_x + 45 &&
                    event->button.y >= path_field_y && event->button.y < path_field_y + 18) {
                    if (ui_state.editing_path) {
                        /* Apply the path change */
                        char resolved[4096];
                        if (realpath(ui_state.path_edit_buffer, resolved)) {
                            strncpy(ui_state.current_dir, resolved, sizeof(ui_state.current_dir) - 1);
                            ui_state.current_dir[sizeof(ui_state.current_dir) - 1] = '\0';
                            scan_directory(ui_state.current_dir);
                            ui_state.editing_path = false;
                            SDL_StopTextInput();
                        } else {
                            /* Invalid path - stay in edit mode */
                            printf("Invalid path: %s\n", ui_state.path_edit_buffer);
                        }
                    }
                    return true;
                }
                
                /* Handle bookmark controls */
                int bookmarks_y = dy + 50;
                int add_btn_x = dx + 85;
                int bookmarks_btn_x = dx + 110;
                
                /* Add bookmark button */
                if (event->button.x >= add_btn_x && event->button.x < add_btn_x + 20 &&
                    event->button.y >= bookmarks_y - 3 && event->button.y < bookmarks_y + 15) {
                    add_bookmark(ui_state.current_dir);
                    return true;
                }
                
                /* Bookmarks dropdown button */
                if (event->button.x >= bookmarks_btn_x && event->button.x < bookmarks_btn_x + 60 &&
                    event->button.y >= bookmarks_y - 3 && event->button.y < bookmarks_y + 15) {
                    ui_state.show_bookmark_menu = !ui_state.show_bookmark_menu;
                    return true;
                }
                
                /* Handle bookmark menu clicks */
                if (ui_state.show_bookmark_menu) {
                    int menu_y = bookmarks_y + 16;
                    int menu_h = ui_state.bookmark_count > 0 ? ui_state.bookmark_count * 18 + 4 : 22;
                    
                    if (event->button.x >= bookmarks_btn_x && event->button.x < bookmarks_btn_x + 200 &&
                        event->button.y >= menu_y && event->button.y < menu_y + menu_h) {
                        
                        if (ui_state.bookmark_count > 0) {
                            int clicked_bookmark = (event->button.y - menu_y - 2) / 18;
                            if (clicked_bookmark >= 0 && clicked_bookmark < ui_state.bookmark_count) {
                                int del_x = bookmarks_btn_x + 180;
                                int item_y = menu_y + 2 + clicked_bookmark * 18;
                                
                                /* Check if clicking delete button */
                                if (event->button.x >= del_x && event->button.x < del_x + 16 &&
                                    event->button.y >= item_y && event->button.y < item_y + 16) {
                                    remove_bookmark(clicked_bookmark);
                                } else {
                                    /* Navigate to bookmarked directory */
                                    strncpy(ui_state.current_dir, ui_state.bookmarked_dirs[clicked_bookmark], 
                                           sizeof(ui_state.current_dir) - 1);
                                    ui_state.current_dir[sizeof(ui_state.current_dir) - 1] = '\0';
                                    scan_directory(ui_state.current_dir);
                                    ui_state.show_bookmark_menu = false;
                                }
                            }
                        }
                        return true;
                    } else {
                        /* Click outside bookmark menu - close it */
                        ui_state.show_bookmark_menu = false;
                    }
                }
                
                /* Handle scrollbar and scroll button clicks */
                int scroll_btn_x = dx + dw - 25;
                int scroll_up_y = dy + 75;  /* Updated to match new file list position */
                int scroll_down_y = dy + dh - 60;
                int list_y = dy + 75;  /* Updated to match new file list position */
                int list_h = dh - 115; /* Updated to match new file list height */
                int visible_items = list_h / 18;
                int max_scroll = ui_state.file_count - visible_items;
                if (max_scroll < 0) max_scroll = 0;
                
                int track_y = scroll_up_y + 20;
                int track_h = scroll_down_y - track_y;
                
                /* Check if clicking scrollbar thumb */
                if (max_scroll > 0) {
                    float thumb_ratio = (float)visible_items / (float)ui_state.file_count;
                    int thumb_h = (int)(track_h * thumb_ratio);
                    if (thumb_h < 20) thumb_h = 20;
                    
                    float scroll_ratio = (float)ui_state.scroll_offset / (float)max_scroll;
                    int thumb_y = track_y + (int)((track_h - thumb_h) * scroll_ratio);
                    
                    if (event->button.x >= scroll_btn_x + 2 && event->button.x < scroll_btn_x + 18 &&
                        event->button.y >= thumb_y && event->button.y < thumb_y + thumb_h) {
                        ui_state.dragging_scrollbar = true;
                        ui_state.drag_start_y = event->button.y;
                        ui_state.drag_start_offset = ui_state.scroll_offset;
                        return true;
                    }
                }
                
                /* Check scroll up button */
                if (event->button.x >= scroll_btn_x && event->button.x < scroll_btn_x + 20 &&
                    event->button.y >= scroll_up_y && event->button.y < scroll_up_y + 20) {
                    if (ui_state.scroll_offset > 0) {
                        ui_state.scroll_offset--;
                    }
                    return true;
                }
                
                /* Check scroll down button */
                if (event->button.x >= scroll_btn_x && event->button.x < scroll_btn_x + 20 &&
                    event->button.y >= scroll_down_y && event->button.y < scroll_down_y + 20) {
                    if (ui_state.scroll_offset < max_scroll) {
                        ui_state.scroll_offset++;
                    }
                    return true;
                }
                
                /* Handle file list clicks (but not on scroll buttons) */
                if (event->button.x >= dx + 5 && event->button.x < dx + dw - 30 &&
                    event->button.y >= list_y && event->button.y < list_y + list_h) {
                    int clicked_idx = (event->button.y - list_y) / 18 + ui_state.scroll_offset;
                    if (clicked_idx >= 0 && clicked_idx < ui_state.file_count) {
                        if (clicked_idx == ui_state.selected_file && event->button.clicks == 2) {
                            /* Double-click: open file or directory */
                            const char* name = ui_state.file_list[clicked_idx];
                            
                            if (name[0] == '[') {
                                /* Directory */
                                char dir_name[1024];
                                strncpy(dir_name, name + 1, sizeof(dir_name) - 1);
                                dir_name[sizeof(dir_name) - 1] = '\0';  /* Ensure null termination */
                                if (strlen(dir_name) > 0) dir_name[strlen(dir_name) - 1] = '\0';  /* Remove trailing ] */
                                
                                char new_path[4096];
                                snprintf(new_path, sizeof(new_path), "%s/%s", ui_state.current_dir, dir_name);
                                
                                char resolved[4096];
                                if (realpath(new_path, resolved)) {
                                    strncpy(ui_state.current_dir, resolved, sizeof(ui_state.current_dir) - 1);
                                    ui_state.current_dir[sizeof(ui_state.current_dir) - 1] = '\0';
                                    scan_directory(ui_state.current_dir);
                                }
                            } else if (strcmp(name, "..") == 0) {
                                /* Parent directory */
                                char* last_slash = strrchr(ui_state.current_dir, '/');
                                if (last_slash && last_slash != ui_state.current_dir) {
                                    *last_slash = '\0';
                                    scan_directory(ui_state.current_dir);
                                }
                            } else {
                                /* ROM file - select it */
                                build_rom_path(ui_state.current_dir, name);
                                ui_state.rom_selected = true;
                                ui_state.show_file_browser = false;
                                free_file_list();
                            }
                        } else {
                            /* Single click: select */
                            ui_state.selected_file = clicked_idx;
                        }
                    }
                }
                
                /* Handle button clicks */
                int btn_y = dy + dh - 30;
                if (event->button.y >= btn_y && event->button.y < btn_y + 20) {
                    /* Open button */
                    if (event->button.x >= dx + 10 && event->button.x < dx + 90) {
                        const char* name = ui_state.file_list[ui_state.selected_file];
                        
                        if (name[0] != '[' && strcmp(name, "..") != 0) {
                            /* ROM file */
                            build_rom_path(ui_state.current_dir, name);
                            ui_state.rom_selected = true;
                            ui_state.show_file_browser = false;
                            free_file_list();
                        }
                    }
                    /* Cancel button */
                    else if (event->button.x >= dx + 100 && event->button.x < dx + 180) {
                        ui_state.show_file_browser = false;
                        free_file_list();
                    }
                }
                
                return true;
            }
        }
        
        /* Handle menu bar clicks */
        if (event->button.y < MENU_HEIGHT) {
            int x = 0;
            for (int i = 0; i < menu_count; i++) {
                int width = strlen(menus[i].title) * 6 + 16;
                if (event->button.x >= x && event->button.x < x + width) {
                    ui_state.active_menu = (ui_state.active_menu == i) ? -1 : i;
                    return true;
                }
                x += width;
            }
        }
        
        /* Handle menu item clicks */
        if (ui_state.active_menu >= 0) {
            Menu* menu = &menus[ui_state.active_menu];
            int menu_x = 0;
            for (int i = 0; i < ui_state.active_menu; i++) {
                menu_x += strlen(menus[i].title) * 6 + 16;
            }
            
            int menu_y = MENU_HEIGHT;
            
            if (event->button.x >= menu_x && event->button.x < menu_x + MENU_WIDTH &&
                event->button.y >= menu_y && event->button.y < menu_y + menu->count * MENU_ITEM_HEIGHT) {
                
                int idx = (event->button.y - menu_y) / MENU_ITEM_HEIGHT;
                if (idx >= 0 && idx < menu->count && !menu->items[idx].is_separator) {
                    if (menu->items[idx].callback) {
                        menu->items[idx].callback();
                        
                        /* Check if a recent ROM was clicked */
                        if (clicked_recent_index >= 0 && clicked_recent_index < ui_state.recent_rom_count) {
                            strncpy(ui_state.selected_rom_path, ui_state.recent_roms[clicked_recent_index], 
                                   sizeof(ui_state.selected_rom_path));
                            ui_state.selected_rom_path[sizeof(ui_state.selected_rom_path) - 1] = '\0';
                            ui_state.rom_selected = true;
                            clicked_recent_index = -1;  /* Reset */
                        }
                    }
                    if (!menu->items[idx].is_checkbox) {
                        ui_state.active_menu = -1;
                    }
                }
                return true;
            } else {
                /* Click outside menu */
                ui_state.active_menu = -1;
                return false;
            }
        }
    }
    
    if (event->type == SDL_MOUSEBUTTONUP) {
        ui_state.mouse_down = false;
        ui_state.mouse_selecting = false;
        ui_state.dragging_scrollbar = false;
    }
    
    if (event->type == SDL_MOUSEWHEEL) {
        /* Handle mouse wheel scrolling in file browser */
        if (ui_state.show_file_browser) {
            int dw = 500, dh = 400;
            int win_w, win_h;
            SDL_GetWindowSize(ui_state.window, &win_w, &win_h);
            int dx = (win_w - dw) / 2;
            int dy = (win_h - dh) / 2;
            
            /* Check if mouse is inside file browser dialog */
            if (ui_state.mouse_x >= dx && ui_state.mouse_x < dx + dw &&
                ui_state.mouse_y >= dy && ui_state.mouse_y < dy + dh) {
                
                int list_h = dh - 115;
                int visible_items = list_h / 18;
                int max_scroll = ui_state.file_count - visible_items;
                if (max_scroll < 0) max_scroll = 0;
                
                if (event->wheel.y > 0) {
                    /* Scroll up */
                    if (ui_state.scroll_offset > 0) {
                        ui_state.scroll_offset--;
                    }
                } else if (event->wheel.y < 0) {
                    /* Scroll down */
                    if (ui_state.scroll_offset < max_scroll) {
                        ui_state.scroll_offset++;
                    }
                }
                return true;
            }
        }
        
        /* Handle mouse wheel scrolling in debug window */
        if (ui_state.show_debug) {
            int dw = 600, dh = 500;
            int win_w, win_h;
            SDL_GetWindowSize(ui_state.window, &win_w, &win_h);
            int dx = (win_w - dw) / 2;
            int dy = (win_h - dh) / 2;
            
            /* Check if mouse is inside debug window dialog */
            if (ui_state.mouse_x >= dx && ui_state.mouse_x < dx + dw &&
                ui_state.mouse_y >= dy && ui_state.mouse_y < dy + dh) {
                
                if (event->wheel.y > 0) {
                    /* Scroll up */
                    if (ui_state.debug_scroll_offset > 0) {
                        ui_state.debug_scroll_offset--;
                    }
                } else if (event->wheel.y < 0) {
                    /* Scroll down */
                    ui_state.debug_scroll_offset++;
                }
                return true;
            }
        }
    }
    
    if (event->type == SDL_TEXTINPUT) {
        /* Handle text input when editing directory path */
        if (ui_state.editing_path) {
            /* Delete selection if any */
            if (ui_state.selection_start != ui_state.selection_end) {
                int sel_start = ui_state.selection_start < ui_state.selection_end ? ui_state.selection_start : ui_state.selection_end;
                int sel_end = ui_state.selection_start > ui_state.selection_end ? ui_state.selection_start : ui_state.selection_end;
                int len = strlen(ui_state.path_edit_buffer);
                memmove(ui_state.path_edit_buffer + sel_start, ui_state.path_edit_buffer + sel_end, len - sel_end + 1);
                ui_state.cursor_pos = sel_start;
                ui_state.selection_start = ui_state.selection_end = sel_start;
            }
            
            int len = strlen(ui_state.path_edit_buffer);
            int input_len = strlen(event->text.text);
            if (len + input_len < (int)sizeof(ui_state.path_edit_buffer) - 1) {
                /* Insert at cursor position */
                memmove(ui_state.path_edit_buffer + ui_state.cursor_pos + input_len,
                        ui_state.path_edit_buffer + ui_state.cursor_pos,
                        len - ui_state.cursor_pos + 1);
                memcpy(ui_state.path_edit_buffer + ui_state.cursor_pos, event->text.text, input_len);
                ui_state.cursor_pos += input_len;
                ui_state.selection_start = ui_state.selection_end = ui_state.cursor_pos;
            }
            return true;
        }
    }
    
    if (event->type == SDL_KEYDOWN) {
        SDL_Keymod mod = SDL_GetModState();
        
        /* F11 or Alt+Enter: Toggle fullscreen */
        if (event->key.keysym.sym == SDLK_F11 || 
            ((mod & KMOD_ALT) && event->key.keysym.sym == SDLK_RETURN)) {
            window_toggle_fullscreen();
            return true;
        }
        
        /* Handle text editing keys when editing directory path */
        if (ui_state.editing_path) {
            mod = SDL_GetModState();
            
            /* Ctrl+A: Select all */
            if ((mod & KMOD_CTRL) && event->key.keysym.sym == SDLK_a) {
                ui_state.selection_start = 0;
                ui_state.selection_end = strlen(ui_state.path_edit_buffer);
                ui_state.cursor_pos = ui_state.selection_end;
                return true;
            }
            
            if (event->key.keysym.sym == SDLK_BACKSPACE) {
                if (ui_state.selection_start != ui_state.selection_end) {
                    /* Delete selection */
                    int sel_start = ui_state.selection_start < ui_state.selection_end ? ui_state.selection_start : ui_state.selection_end;
                    int sel_end = ui_state.selection_start > ui_state.selection_end ? ui_state.selection_start : ui_state.selection_end;
                    int len = strlen(ui_state.path_edit_buffer);
                    memmove(ui_state.path_edit_buffer + sel_start, ui_state.path_edit_buffer + sel_end, len - sel_end + 1);
                    ui_state.cursor_pos = sel_start;
                    ui_state.selection_start = ui_state.selection_end = sel_start;
                } else if (ui_state.cursor_pos > 0) {
                    /* Delete character before cursor */
                    int len = strlen(ui_state.path_edit_buffer);
                    memmove(ui_state.path_edit_buffer + ui_state.cursor_pos - 1,
                            ui_state.path_edit_buffer + ui_state.cursor_pos,
                            len - ui_state.cursor_pos + 1);
                    ui_state.cursor_pos--;
                    ui_state.selection_start = ui_state.selection_end = ui_state.cursor_pos;
                }
                return true;
            }
            
            if (event->key.keysym.sym == SDLK_RETURN) {
                /* Apply the path change */
                char resolved[4096];
                if (realpath(ui_state.path_edit_buffer, resolved)) {
                    strncpy(ui_state.current_dir, resolved, sizeof(ui_state.current_dir) - 1);
                    ui_state.current_dir[sizeof(ui_state.current_dir) - 1] = '\0';
                    scan_directory(ui_state.current_dir);
                    ui_state.editing_path = false;
                    SDL_StopTextInput();
                } else {
                    /* Invalid path - stay in edit mode */
                    printf("Invalid path: %s\n", ui_state.path_edit_buffer);
                }
                return true;
            }
            
            if (event->key.keysym.sym == SDLK_ESCAPE) {
                /* Cancel editing */
                ui_state.editing_path = false;
                SDL_StopTextInput();
                return true;
            }
            
            /* Arrow key navigation */
            if (event->key.keysym.sym == SDLK_LEFT) {
                if (ui_state.cursor_pos > 0) {
                    ui_state.cursor_pos--;
                    ui_state.selection_start = ui_state.selection_end = ui_state.cursor_pos;
                }
                return true;
            }
            
            if (event->key.keysym.sym == SDLK_RIGHT) {
                if (ui_state.cursor_pos < (int)strlen(ui_state.path_edit_buffer)) {
                    ui_state.cursor_pos++;
                    ui_state.selection_start = ui_state.selection_end = ui_state.cursor_pos;
                }
                return true;
            }
            
            if (event->key.keysym.sym == SDLK_HOME) {
                ui_state.cursor_pos = 0;
                ui_state.selection_start = ui_state.selection_end = 0;
                return true;
            }
            
            if (event->key.keysym.sym == SDLK_END) {
                ui_state.cursor_pos = strlen(ui_state.path_edit_buffer);
                ui_state.selection_start = ui_state.selection_end = ui_state.cursor_pos;
                return true;
            }
            
            return true;
        }
        
        /* Handle arrow keys in file browser */
        if (ui_state.show_file_browser && !ui_state.editing_path) {
            if (event->key.keysym.sym == SDLK_UP) {
                if (ui_state.selected_file > 0) {
                    ui_state.selected_file--;
                    
                    /* Scroll up if selected item is above visible area */
                    if (ui_state.selected_file < ui_state.scroll_offset) {
                        ui_state.scroll_offset = ui_state.selected_file;
                    }
                }
                return true;
            }
            
            if (event->key.keysym.sym == SDLK_DOWN) {
                if (ui_state.selected_file < ui_state.file_count - 1) {
                    ui_state.selected_file++;
                    
                    /* Scroll down if selected item is below visible area */
                    __attribute__((unused)) int dw = 500, dh = 400;
                    int list_h = dh - 90;
                    int visible_items = list_h / 18;
                    if (ui_state.selected_file >= ui_state.scroll_offset + visible_items) {
                        ui_state.scroll_offset = ui_state.selected_file - visible_items + 1;
                    }
                }
                return true;
            }
            
            /* Handle Enter key to open selected file/directory */
            if (event->key.keysym.sym == SDLK_RETURN) {
                const char* name = ui_state.file_list[ui_state.selected_file];
                
                if (name[0] == '[') {
                    /* Directory */
                    char dir_name[1024];
                    strncpy(dir_name, name + 1, sizeof(dir_name) - 1);
                    dir_name[sizeof(dir_name) - 1] = '\0';  /* Ensure null termination */
                    if (strlen(dir_name) > 0) dir_name[strlen(dir_name) - 1] = '\0';  /* Remove trailing ] */
                    
                    char new_path[4096];
                    snprintf(new_path, sizeof(new_path), "%s/%s", ui_state.current_dir, dir_name);
                    
                    char resolved[4096];
                    if (realpath(new_path, resolved)) {
                        strncpy(ui_state.current_dir, resolved, sizeof(ui_state.current_dir) - 1);
                        ui_state.current_dir[sizeof(ui_state.current_dir) - 1] = '\0';
                        scan_directory(ui_state.current_dir);
                    }
                } else if (strcmp(name, "..") == 0) {
                    /* Parent directory */
                    char* last_slash = strrchr(ui_state.current_dir, '/');
                    if (last_slash && last_slash != ui_state.current_dir) {
                        *last_slash = '\0';
                        scan_directory(ui_state.current_dir);
                    }
                } else {
                    /* ROM file - select it */
                    build_rom_path(ui_state.current_dir, name);
                    ui_state.rom_selected = true;
                    ui_state.show_file_browser = false;
                    free_file_list();
                }
                return true;
            }
            
            /* Handle Backspace to go to parent directory */
            if (event->key.keysym.sym == SDLK_BACKSPACE) {
                /* Navigate to parent directory */
                char* last_slash = strrchr(ui_state.current_dir, '/');
                if (last_slash && last_slash != ui_state.current_dir) {
                    *last_slash = '\0';
                    scan_directory(ui_state.current_dir);
                }
                return true;
            }
            
            /* Close file browser on ESC */
            if (event->key.keysym.sym == SDLK_ESCAPE) {
                ui_state.show_file_browser = false;
                free_file_list();
                return true;
            }
        }
        
        /* Close menu on ESC */
        if (event->key.keysym.sym == SDLK_ESCAPE && ui_state.active_menu >= 0) {
            ui_state.active_menu = -1;
            return true;
        }
        
        /* Keyboard shortcuts */
        mod = SDL_GetModState();
        if ((mod & KMOD_CTRL) && event->key.keysym.sym == SDLK_o) {
            cb_open_rom();
            return true;
        }
        if ((mod & KMOD_CTRL) && event->key.keysym.sym == SDLK_r) {
            cb_reset();
            return true;
        }
        if (event->key.keysym.sym == SDLK_p) {
            cb_pause();
            return true;
        }
        if (event->key.keysym.sym == SDLK_m) {
            cb_mute();
            return true;
        }
        if (event->key.keysym.sym == SDLK_F1) {
            cb_controls();
            return true;
        }
        if (event->key.keysym.sym == SDLK_F5) {
            cb_save_state();
            return true;
        }
        if (event->key.keysym.sym == SDLK_F7) {
            cb_load_state();
            return true;
        }
    }
    
    return false;
}

/* Debug logging functions */
bool ui_is_debug_enabled(UIDebugComponent component) {
    if (!ui_state.show_debug) return false;
    
    switch (component) {
        case UI_DEBUG_PPU: return ui_state.debug_ppu;
        case UI_DEBUG_APU: return ui_state.debug_apu;
        case UI_DEBUG_CPU: return ui_state.debug_cpu;
        case UI_DEBUG_MEM: return ui_state.debug_mem;
        case UI_DEBUG_UI:  return ui_state.debug_ui;
        default: return false;
    }
}

void ui_debug_log(UIDebugComponent component, const char* format, ...) {
    if (!ui_is_debug_enabled(component)) return;
    
    va_list args;
    va_start(args, format);
    
    /* Format the message */
    char temp_buffer[512];
    vsnprintf(temp_buffer, sizeof(temp_buffer), format, args);
    va_end(args);
    
    /* Ensure message ends with newline */
    int msg_len = strlen(temp_buffer);
    bool needs_newline = (msg_len == 0 || temp_buffer[msg_len - 1] != '\n');
    
    /* Calculate space needed */
    int space_needed = msg_len + (needs_newline ? 1 : 0);
    int buffer_size = sizeof(ui_state.debug_buffer);
    
    /* If message is too long for buffer, keep most recent data */
    if (space_needed >= buffer_size) {
        /* Message is huge, just take the end */
        int start_pos = space_needed - buffer_size + 100; /* Leave some room */
        strncpy(ui_state.debug_buffer, temp_buffer + start_pos, buffer_size - 1);
        ui_state.debug_buffer[buffer_size - 1] = '\0';
        ui_state.debug_buffer_offset = strlen(ui_state.debug_buffer);
        return;
    }
    
    /* Check if we need to make room */
    int available = buffer_size - ui_state.debug_buffer_offset - 1;
    if (space_needed > available) {
        /* Shift buffer to make room - keep most recent half */
        int keep_size = buffer_size / 2;
        int discard = ui_state.debug_buffer_offset - keep_size;
        if (discard > 0) {
            memmove(ui_state.debug_buffer, 
                   ui_state.debug_buffer + discard, 
                   keep_size);
            ui_state.debug_buffer_offset = keep_size;
            ui_state.debug_buffer[ui_state.debug_buffer_offset] = '\0';
        }
    }
    
    /* Append the message */
    strncat(ui_state.debug_buffer, temp_buffer, buffer_size - ui_state.debug_buffer_offset - 1);
    ui_state.debug_buffer_offset += msg_len;
    
    if (needs_newline && ui_state.debug_buffer_offset < buffer_size - 1) {
        ui_state.debug_buffer[ui_state.debug_buffer_offset++] = '\n';
        ui_state.debug_buffer[ui_state.debug_buffer_offset] = '\0';
    }
}

/* Placeholder implementations - to be defined in window.c */
void __attribute__((weak)) ui_on_open_rom(void) {
    printf("Open ROM requested\n");
}

void __attribute__((weak)) ui_on_reset(void) {
    printf("Reset requested\n");
}

void __attribute__((weak)) ui_on_save_state(void) {
    printf("Save state requested\n");
}

void __attribute__((weak)) ui_on_load_state(void) {
    printf("Load state requested\n");
}
