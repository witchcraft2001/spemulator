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
#include "debug/trace.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(int argc, char **argv) {
    printf("SPEmulator — Sprinter SP2000 Emulator\n");
    printf("Platform: %s\n\n", platform_name());

    /* Initialize trace system */
    trace_init(&g_trace);

    sp_config_t *config = config_create();
    if (!config) { fprintf(stderr, "Failed to create config\n"); return 1; }

    config_load_file(config, platform_default_config_path());
    if (config_parse_args(config, argc, argv) < 0) {
        config_destroy(config);
        return 1;
    }

    /* Set up trace if requested */
    if (config->trace_enabled) {
        g_trace.enabled = true;
        trace_parse_spec(&g_trace, config->trace_spec);
        fprintf(stderr, "Trace enabled: mask=0x%04X spec=%s\n",
                g_trace.mask, config->trace_spec);
    }

    sp_machine_t *machine = machine_create(config);
    if (!machine) {
        fprintf(stderr, "Failed to create machine\n");
        config_destroy(config);
        return 1;
    }

    sp_debugger_t debugger;
    debugger_init(&debugger);
    if (config->start_debugger) {
        debugger.active = true;
        debugger_break(&debugger);
    }

    sp_sdl_t sdl;
    memset(&sdl, 0, sizeof(sdl));
    if (sdl_init(&sdl, machine->fb_width, machine->fb_height,
                 config->scale, config->fullscreen) < 0) {
        machine_destroy(machine);
        config_destroy(config);
        return 1;
    }

    sp_audio_t audio;
    memset(&audio, 0, sizeof(audio));
    if (config->audio_enabled) {
        if (audio_init(&audio, config->sample_rate) == 0)
            sdl_audio_init(&sdl, config->sample_rate);
    }

    printf("Machine initialized. CPU=%dMHz, RAM=%dKB\n",
           config->cpu_speed_mhz, config->ram_size_kb);
    if (config->rom_path[0]) printf("ROM: %s\n", config->rom_path);
    printf("Video: %dx%d @ %dx scale\n",
           machine->fb_width, machine->fb_height, config->scale);
    printf("\nControls: F11=Fullscreen, F12=Debugger, Ctrl+F10=Quit\n\n");

    /* Main loop */
    u64 last_frame = sdl_get_ticks();
    int render_skip = 0;

    while (machine->running) {
        /* Process SDL events (less frequent during boot) */
        if (render_skip == 0 || machine->cpu.iff1) {
            if (!sdl_poll_events(&sdl, machine)) {
                machine->running = false;
                break;
            }
        }

        /* Debugger */
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

            /* Boot trace */
            if (machine->frame_count == 10000 ||
                machine->frame_count == 50000 ||
                machine->frame_count == 70000 ||
                machine->frame_count == 100000 ||
                machine->frame_count == 200000) {
                printf("F%5d: PC=%04X SP=%04X IFF=%d Pg=%02X/%02X/%02X/%02X ts=%lluM\n",
                       machine->frame_count,
                       machine->cpu.pc.w, machine->cpu.sp.w,
                       machine->cpu.iff1,
                       machine->win_page[0], machine->win_page[1],
                       machine->win_page[2], machine->win_page[3],
                       (unsigned long long)(machine->cpu.total_tstates / 1000000));
                fflush(stdout);
            }
        }

        /* Render video — minimal during boot */
        render_skip++;
        if (render_skip >= 500 || machine->frame_count > 100000) {
            render_skip = 0;
            if (machine->frame_count > 100000) {
                video_render_frame(machine);
            }
            sdl_present(&sdl, machine->framebuffer, machine->fb_width, machine->fb_height);
        }

        /* Frame timing: only after boot complete (IFF=1) */
        if (machine->cpu.iff1) {
            u64 now = sdl_get_ticks();
            u64 elapsed = now - last_frame;
            u64 frame_time_ms = 1000 / SP_FRAME_RATE_PAL;
            if (elapsed < frame_time_ms)
                sdl_delay((u32)(frame_time_ms - elapsed));
            last_frame = sdl_get_ticks();
        }
    }

    if (config->audio_enabled) audio_destroy(&audio);
    sdl_destroy(&sdl);
    machine_destroy(machine);
    config_destroy(config);

    printf("SPEmulator terminated.\n");
    return 0;
}
