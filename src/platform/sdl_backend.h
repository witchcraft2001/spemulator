/*
 * SPEmulator — SDL3 Backend
 * Window management, rendering, audio output, input events.
 */
#ifndef SPEMU_SDL_BACKEND_H
#define SPEMU_SDL_BACKEND_H

#include "types.h"

typedef struct sp_machine sp_machine_t;

typedef struct {
    void *window;       /* SDL_Window* */
    void *renderer;     /* SDL_Renderer* */
    void *texture;      /* SDL_Texture* */
    void *audio_stream; /* SDL_AudioStream* */
    int   win_width;
    int   win_height;
    int   scale;
    bool  fullscreen;
} sp_sdl_t;

/* Initialize SDL3, create window and renderer */
int sdl_init(sp_sdl_t *sdl, int fb_width, int fb_height, int scale, bool fullscreen);

/* Destroy SDL resources */
void sdl_destroy(sp_sdl_t *sdl);

/* Upload framebuffer to texture and present */
void sdl_present(sp_sdl_t *sdl, const u32 *framebuffer, int fb_width, int fb_height);

/* Process SDL events. Returns false if quit requested. */
bool sdl_poll_events(sp_sdl_t *sdl, sp_machine_t *machine);

/* Get timestamp in milliseconds */
u64 sdl_get_ticks(void);

/* Delay for given ms */
void sdl_delay(u32 ms);

/* Initialize audio output */
int sdl_audio_init(sp_sdl_t *sdl, int sample_rate);

/* Queue audio samples */
void sdl_audio_queue(sp_sdl_t *sdl, const i16 *samples, int count);

#endif /* SPEMU_SDL_BACKEND_H */
