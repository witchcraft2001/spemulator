/*
 * SPEmulator — Sprinter SP2000 Emulator
 * Main entry point.
 */
#include "config.h"
#include "machine.h"
#include "video/video.h"
#include "audio/audio.h"
#include "platform/sdl_backend.h"
#include "platform/platform.h"
#include "debug/debugger.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(int argc, char **argv) {
    printf("SPEmulator — Sprinter SP2000 Emulator\n");
    printf("Platform: %s\n\n", platform_name());

    /* Parse configuration */
    sp_config_t *config = config_create();
    if (!config) {
        fprintf(stderr, "Failed to create config\n");
        return 1;
    }

    /* Try loading default config file */
    config_load_file(config, platform_default_config_path());

    /* Command line overrides config file */
    if (config_parse_args(config, argc, argv) < 0) {
        config_destroy(config);
        return 1;
    }

    /* Create machine */
    sp_machine_t *machine = machine_create(config);
    if (!machine) {
        fprintf(stderr, "Failed to create machine\n");
        config_destroy(config);
        return 1;
    }

    /* Initialize debugger */
    sp_debugger_t debugger;
    debugger_init(&debugger);
    if (config->start_debugger) {
        debugger.active = true;
        debugger_break(&debugger);
        printf("Debugger active. Press F12 to toggle.\n");
    }

    /* Initialize SDL3 */
    sp_sdl_t sdl;
    memset(&sdl, 0, sizeof(sdl));

    if (sdl_init(&sdl, machine->fb_width, machine->fb_height,
                 config->scale, config->fullscreen) < 0) {
        fprintf(stderr, "Failed to initialize SDL\n");
        machine_destroy(machine);
        config_destroy(config);
        return 1;
    }

    /* Initialize audio */
    sp_audio_t audio;
    if (config->audio_enabled) {
        if (audio_init(&audio, config->sample_rate) == 0) {
            sdl_audio_init(&sdl, config->sample_rate);
        }
    }

    printf("Machine initialized. CPU=%dMHz, RAM=%dKB\n",
           config->cpu_speed_mhz, config->ram_size_kb);
    if (config->rom_path[0])
        printf("ROM: %s\n", config->rom_path);
    printf("Video: %dx%d @ %dx scale\n",
           machine->fb_width, machine->fb_height, config->scale);
    printf("\nControls: F11=Fullscreen, F12=Debugger, Ctrl+F10=Quit\n\n");

    /* Main loop */
    u64 frame_time_ms = 1000 / SP_FRAME_RATE_PAL;
    u64 last_frame = sdl_get_ticks();

    while (machine->running) {
        u64 now = sdl_get_ticks();

        /* Process SDL events */
        if (!sdl_poll_events(&sdl, machine)) {
            machine->running = false;
            break;
        }

        /* Debugger check */
        if (machine->debugger_active != debugger.active) {
            debugger.active = machine->debugger_active;
            if (debugger.active) {
                debugger_break(&debugger);
                debugger_print_regs(&debugger, machine);
                debugger_print_disasm(&debugger, machine, machine->cpu.pc.w, 10);
            } else {
                debugger_continue(&debugger);
            }
        }

        /* Run CPU for one frame */
        if (!debugger.active || debugger.state == DBG_STATE_RUNNING) {
            machine_run_frame(machine);

            /* Boot status at key frames */
            if (machine->frame_count == 50 || machine->frame_count == 200 ||
                machine->frame_count == 500 || machine->frame_count == 1000) {
                printf("F%3d: PC=%04X SP=%04X IFF=%d IM=%d Pg=%02X/%02X/%02X/%02X\n",
                       machine->frame_count,
                       machine->cpu.pc.w, machine->cpu.sp.w,
                       machine->cpu.iff1, machine->cpu.im,
                       machine->page_reg[0], machine->page_reg[1],
                       machine->page_reg[2], machine->page_reg[3]);
                int vram_nz = 0;
                for (int i = 0; i < SP_VRAM_LINES * SP_VRAM_LINE; i++)
                    if (machine->vram[i]) vram_nz++;
                printf("  VRAM: %d non-zero bytes\n", vram_nz);
                fflush(stdout);
            }
        }

        /* Render video */
        video_render_frame(machine);
        sdl_present(&sdl, machine->framebuffer, machine->fb_width, machine->fb_height);

        /* Generate and queue audio */
        if (config->audio_enabled) {
            audio_generate_frame(&audio, machine);
            sdl_audio_queue(&sdl, audio.buffer, audio.buffer_size);
        }

        /* Frame timing */
        u64 elapsed = sdl_get_ticks() - last_frame;
        if (elapsed < frame_time_ms) {
            sdl_delay((u32)(frame_time_ms - elapsed));
        }
        last_frame = sdl_get_ticks();
    }

    /* Cleanup */
    if (config->audio_enabled)
        audio_destroy(&audio);
    sdl_destroy(&sdl);
    machine_destroy(machine);
    config_destroy(config);

    printf("SPEmulator terminated.\n");
    return 0;
}
