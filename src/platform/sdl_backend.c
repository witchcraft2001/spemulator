/*
 * SPEmulator — SDL3 Backend
 */
#include "platform/sdl_backend.h"
#include "machine.h"
#include "input/keyboard.h"

#include <SDL3/SDL.h>
#include <stdio.h>

int sdl_init(sp_sdl_t *sdl, int fb_width, int fb_height, int scale, bool fullscreen) {
    if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_EVENTS)) {
        fprintf(stderr, "SDL_Init failed: %s\n", SDL_GetError());
        return -1;
    }

    sdl->scale = scale;
    sdl->fullscreen = fullscreen;
    sdl->win_width = fb_width * scale;
    sdl->win_height = fb_height * scale;

    SDL_WindowFlags flags = 0;
    if (fullscreen) flags |= SDL_WINDOW_FULLSCREEN;
    flags |= SDL_WINDOW_RESIZABLE;

    SDL_Window *window = SDL_CreateWindow(
        "SPEmulator — Sprinter SP2000",
        sdl->win_width, sdl->win_height,
        flags);
    if (!window) {
        fprintf(stderr, "SDL_CreateWindow failed: %s\n", SDL_GetError());
        return -1;
    }
    sdl->window = window;

    SDL_Renderer *renderer = SDL_CreateRenderer(window, NULL);
    if (!renderer) {
        fprintf(stderr, "SDL_CreateRenderer failed: %s\n", SDL_GetError());
        return -1;
    }
    sdl->renderer = renderer;

    /* Set logical presentation for scaling */
    SDL_SetRenderLogicalPresentation(renderer, fb_width, fb_height,
                                     SDL_LOGICAL_PRESENTATION_LETTERBOX);

    SDL_Texture *texture = SDL_CreateTexture(renderer,
        SDL_PIXELFORMAT_ARGB8888,
        SDL_TEXTUREACCESS_STREAMING,
        fb_width, fb_height);
    if (!texture) {
        fprintf(stderr, "SDL_CreateTexture failed: %s\n", SDL_GetError());
        return -1;
    }
    SDL_SetTextureScaleMode(texture, SDL_SCALEMODE_NEAREST);
    sdl->texture = texture;

    return 0;
}

void sdl_destroy(sp_sdl_t *sdl) {
    if (sdl->texture)  SDL_DestroyTexture((SDL_Texture *)sdl->texture);
    if (sdl->renderer) SDL_DestroyRenderer((SDL_Renderer *)sdl->renderer);
    if (sdl->window)   SDL_DestroyWindow((SDL_Window *)sdl->window);
    if (sdl->audio_stream) SDL_DestroyAudioStream((SDL_AudioStream *)sdl->audio_stream);
    SDL_Quit();
}

void sdl_present(sp_sdl_t *sdl, const u32 *framebuffer, int fb_width, int fb_height) {
    SDL_Texture *tex = (SDL_Texture *)sdl->texture;
    SDL_Renderer *ren = (SDL_Renderer *)sdl->renderer;

    SDL_UpdateTexture(tex, NULL, framebuffer, fb_width * sizeof(u32));
    SDL_RenderClear(ren);
    SDL_RenderTexture(ren, tex, NULL, NULL);
    SDL_RenderPresent(ren);
}

bool sdl_poll_events(sp_sdl_t *sdl, sp_machine_t *machine) {
    SDL_Event event;
    while (SDL_PollEvent(&event)) {
        switch (event.type) {
        case SDL_EVENT_QUIT:
            return false;

        case SDL_EVENT_KEY_DOWN:
            if (event.key.key == SDLK_F12) {
                machine->debugger_active = !machine->debugger_active;
                break;
            }
            if (event.key.key == SDLK_F11) {
                /* Toggle fullscreen */
                sdl->fullscreen = !sdl->fullscreen;
                SDL_SetWindowFullscreen((SDL_Window *)sdl->window,
                    sdl->fullscreen);
                break;
            }
            if (event.key.key == SDLK_F10 && (event.key.mod & SDL_KMOD_CTRL)) {
                return false; /* Ctrl+F10 = quit */
            }
            keyboard_key_down(machine, event.key.scancode);
            break;

        case SDL_EVENT_KEY_UP:
            keyboard_key_up(machine, event.key.scancode);
            break;
        }
    }
    return true;
}

u64 sdl_get_ticks(void) {
    return SDL_GetTicks();
}

void sdl_delay(u32 ms) {
    SDL_Delay(ms);
}

int sdl_audio_init(sp_sdl_t *sdl, int sample_rate) {
    SDL_AudioSpec spec;
    spec.freq = sample_rate;
    spec.format = SDL_AUDIO_S16;
    spec.channels = 2;

    SDL_AudioStream *stream = SDL_OpenAudioDeviceStream(
        SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, &spec, NULL, NULL);
    if (!stream) {
        fprintf(stderr, "SDL audio init failed: %s\n", SDL_GetError());
        return -1;
    }
    sdl->audio_stream = stream;
    SDL_ResumeAudioStreamDevice(stream);
    return 0;
}

void sdl_audio_queue(sp_sdl_t *sdl, const i16 *samples, int count) {
    if (sdl->audio_stream) {
        SDL_PutAudioStreamData((SDL_AudioStream *)sdl->audio_stream,
                               samples, count * 2 * sizeof(i16));
    }
}
