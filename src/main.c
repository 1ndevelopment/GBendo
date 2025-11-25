#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <time.h>
#include <SDL2/SDL.h>
#include "gbendo.h"
#include "ui/window.h"
#include "ui/ui.h"
#include "cpu/sm83_optimized.h"
#include "error_handling.h"
#include "profiler.h"

static void print_usage(const char* prog_name) {
    printf("Usage: %s [OPTIONS] [rom_file]\n", prog_name);
    printf("\nBy default, GBendo launches in GUI mode. Specify a ROM file to load it directly.\n");
    printf("\nOptions:\n");
    printf("  -s, --scale N       Set window scale factor (default: 3)\n");
    printf("  -f, --fullscreen    Run in fullscreen mode\n");
    printf("  --vsync             Enable vsync (default: enabled)\n");
    printf("  --no-vsync          Disable vsync\n");
    printf("  -v, --verbose       Enable verbose debug output\n");
    printf("  --profile           Enable performance profiling\n");
    printf("  -h, --help          Show this help message\n");
}

int main(int argc, char* argv[]) {
    int scale = 3;
    bool fullscreen = false;
    bool vsync = true;
    bool verbose = false;
    bool profiling = false;
    bool gui_mode = true;  /* GUI mode is now the default */
    const char* rom_file = NULL;

    /* Parse command-line arguments */
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "--help") == 0) {
            print_usage(argv[0]);
            return 0;
        } else if (strcmp(argv[i], "-s") == 0 || strcmp(argv[i], "--scale") == 0) {
            if (i + 1 < argc) {
                scale = atoi(argv[++i]);
                if (scale <= 0) {
                    fprintf(stderr, "Error: Scale must be a positive integer\n");
                    return 1;
                }
            } else {
                fprintf(stderr, "Error: --scale requires a value\n");
                return 1;
            }
        } else if (strcmp(argv[i], "-f") == 0 || strcmp(argv[i], "--fullscreen") == 0) {
            fullscreen = true;
        } else if (strcmp(argv[i], "--vsync") == 0) {
            vsync = true;
        } else if (strcmp(argv[i], "--no-vsync") == 0) {
            vsync = false;
        } else if (strcmp(argv[i], "-v") == 0 || strcmp(argv[i], "--verbose") == 0) {
            verbose = true;
        } else if (strcmp(argv[i], "--profile") == 0) {
            profiling = true;
        } else if (argv[i][0] == '-') {
            fprintf(stderr, "Error: Unknown option: %s\n", argv[i]);
            print_usage(argv[0]);
            return 1;
        } else {
            rom_file = argv[i];
            gui_mode = false;  /* Disable GUI mode when ROM file is provided */
        }
    }

    /* Initialize error handling and profiling */
    error_init();
    profiler_init();
    if (profiling) {
        profiler_enable(true);
        printf("Performance profiling enabled\n");
    }

    /* No validation needed - GUI mode is default if no ROM specified */

    GBEmulator gb;
    gb_init(&gb);

    /* Enable debug mode if verbose flag is set */
    if (verbose) {
        gb_enable_debug(&gb);
        printf("[DEBUG] Verbose mode enabled\n");
    }

    /* Load ROM if provided */
    bool rom_loaded = false;
    if (rom_file) {
        if (!gb_load_rom(&gb, rom_file)) {
            printf("Failed to load ROM: %s\n", rom_file);
            return 1;
        }
        
        if (verbose) {
            printf("[DEBUG] ROM loaded: %s\n", rom_file);
        }
        
        /* Reset CPU to proper initial state after ROM load */
        gb_reset(&gb);
        
        if (verbose) {
            printf("[DEBUG] CPU reset - PC=0x%04X, AF=0x%04X, BC=0x%04X, DE=0x%04X, HL=0x%04X, SP=0x%04X\n",
                   gb.cpu.pc, gb.cpu.af, gb.cpu.bc, gb.cpu.de, gb.cpu.hl, gb.cpu.sp);
            printf("[DEBUG] LCDC=0x%02X (LCD %s)\n", gb.ppu.lcdc, 
                   (gb.ppu.lcdc & 0x80) ? "enabled" : "disabled");
        }
        
        /* Notify UI of ROM load for recent ROMs tracking */
        ui_notify_rom_loaded(rom_file);
        window_set_rom_loaded(true);
        window_set_rom_path(rom_file);  /* Track ROM path for save states */
        rom_loaded = true;
    } else if (gui_mode) {
        printf("Launching in GUI mode - use File > Open ROM to load a game\n");
    }

    /* Initialize window with user-specified options. Requires SDL2 installed. */
    if (!window_init(scale, fullscreen, vsync)) {
        fprintf(stderr, "Failed to initialize window. Continuing without video.\n");
    }

    /* Initialize audio */
    if (!audio_init()) {
        fprintf(stderr, "Failed to initialize audio. Continuing without sound.\n");
    }

    /* Main emulation loop with optimized timing */
    bool quit = false;
    uint32_t frame_count = 0;
    
    /* High-resolution timing for consistent frame rate */
    struct timespec frame_start, frame_end, sleep_time;
    const long target_frame_time_ns = 1000000000L / 60;  /* 60 FPS target */
    
    /* Initialize optimized CPU dispatch */
    sm83_init_jump_tables();
    
    /* Create blank framebuffer for GUI-only mode - dark green/grey to match menu */
    uint32_t blank_framebuffer[160 * 144];
    uint32_t bg_color = 0xFF0F190F;  /* ARGB: RGB(15, 25, 15) - dark green-grey */
    for (int i = 0; i < 160 * 144; i++) {
        blank_framebuffer[i] = bg_color;
    }
    
    while (!quit) {
        clock_gettime(CLOCK_MONOTONIC, &frame_start);
        
        if (rom_loaded) {
            /* Run emulation if ROM is loaded and not paused */
            if (!ui_is_paused()) {
                PROFILE_START(FRAME_RENDER);
                gb_run_frame_optimized(&gb);
                PROFILE_END(FRAME_RENDER);
                
                profiler_increment_frame_count();
                profiler_update_metrics();

                /* Handle frame completion: present PPU framebuffer to window */
                if (gb.frame_complete) {
                    frame_count++;
                    if (verbose && (frame_count % 60 == 0)) {
                        printf("[DEBUG] Frame %u - Cycles: %u, PC: 0x%04X, LY: %u, LCDC: 0x%02X\n",
                               frame_count, gb.cycles, gb.cpu.pc, gb.ppu.ly, gb.ppu.lcdc);
                    }
                    /* Only present if LCD is currently enabled - avoids showing VRAM during tile uploads */
                    if (gb.ppu.lcdc & 0x80) {  /* LCDC_DISPLAY_ENABLE */
                        window_present(gb.ppu.framebuffer);
                    }
                    gb.frame_complete = false;
                    
                    /* Queue audio samples from APU buffer */
                    float audio_samples[735];  /* SAMPLE_RATE / 60 */
                    uint32_t sample_count = apu_get_samples(&gb.apu, audio_samples, sizeof(audio_samples) / sizeof(float));
                    if (sample_count > 0) {
                        audio_queue_samples(audio_samples, sample_count);
                    }
                }
            } else {
                /* When paused, keep displaying the last frame and handle UI events */
                window_present(gb.ppu.framebuffer);
                SDL_Delay(16);  /* ~60 FPS to keep UI responsive */
            }
            
            /* Check if reset was requested */
            if (window_get_reset_requested()) {
                printf("Resetting emulator...\n");
                gb_reset(&gb);
                if (verbose) {
                    printf("[DEBUG] Emulator reset\n");
                }
            }
            
            /* Check if stop was requested */
            if (ui_get_stop_requested()) {
                printf("Stopping emulation...\n");
                gb_unload_rom(&gb);  /* Properly clean up ROM data */
                window_set_rom_loaded(false);
                window_set_rom_path(NULL);  /* Clear ROM path */
                rom_loaded = false;
                if (verbose) {
                    printf("[DEBUG] Emulation stopped - returning to GUI mode\n");
                }
            }
            
            /* Check if save state was requested */
            if (window_get_save_state_requested()) {
                const char* rom_path = window_get_rom_path();
                if (rom_path) {
                    char save_path[2048];
                    snprintf(save_path, sizeof(save_path), "%s.gbstate", rom_path);
                    if (gb_save_state(&gb, save_path)) {
                        printf("State saved successfully\n");
                    } else {
                        fprintf(stderr, "Failed to save state\n");
                    }
                } else {
                    fprintf(stderr, "Cannot save state: no ROM loaded\n");
                }
            }
            
            /* Check if load state was requested */
            if (window_get_load_state_requested()) {
                const char* rom_path = window_get_rom_path();
                if (rom_path) {
                    char load_path[2048];
                    snprintf(load_path, sizeof(load_path), "%s.gbstate", rom_path);
                    if (gb_load_state(&gb, load_path)) {
                        printf("State loaded successfully\n");
                    } else {
                        fprintf(stderr, "Failed to load state\n");
                    }
                } else {
                    fprintf(stderr, "Cannot load state: no ROM loaded\n");
                }
            }
            
            /* Poll window events and allow user to quit */
            if (window_poll_events(&gb.memory)) quit = true;
        } else {
            /* GUI-only mode: just show blank screen and handle events */
            window_present(blank_framebuffer);
            
            /* Check if a ROM was selected from the file browser */
            const char* selected_rom = ui_get_selected_rom();
            if (selected_rom) {
                printf("Loading ROM: %s\n", selected_rom);
                if (gb_load_rom(&gb, selected_rom)) {
                    gb_reset(&gb);
                    ui_notify_rom_loaded(selected_rom);  /* Add to recent ROMs */
                    window_set_rom_loaded(true);
                    window_set_rom_path(selected_rom);  /* Track ROM path for save states */
                    rom_loaded = true;
                    if (verbose) {
                        printf("[DEBUG] ROM loaded successfully\n");
                    }
                } else {
                    fprintf(stderr, "Failed to load ROM: %s\n", selected_rom);
                }
            }
            
            /* Poll window events (pass NULL for memory since no ROM is loaded) */
            if (window_poll_events(NULL)) quit = true;
            
            /* Precise timing control */
            clock_gettime(CLOCK_MONOTONIC, &frame_end);
            long elapsed_ns = (frame_end.tv_sec - frame_start.tv_sec) * 1000000000L + 
                             (frame_end.tv_nsec - frame_start.tv_nsec);
            
            if (elapsed_ns < target_frame_time_ns) {
                sleep_time.tv_sec = 0;
                sleep_time.tv_nsec = target_frame_time_ns - elapsed_ns;
                nanosleep(&sleep_time, NULL);
            }
        }
    }

    /* Print profiling report if enabled */
    if (profiling) {
        printf("\n=== Final Performance Report ===\n");
        profiler_print_report();
        profiler_print_memory_stats();
    }

    window_destroy();
    gb_cleanup(&gb);
    return 0;
}