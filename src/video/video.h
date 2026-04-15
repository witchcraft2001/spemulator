/*
 * SPEmulator — Video Controller
 * Renders ZX Spectrum and Sprinter native video modes to framebuffer.
 */
#ifndef SPEMU_VIDEO_H
#define SPEMU_VIDEO_H

#include "types.h"

typedef struct sp_machine sp_machine_t;

/* Render current frame to machine->framebuffer based on video_mode */
void video_render_frame(sp_machine_t *m);

/* Render ZX Spectrum 256x192 mode */
void video_render_zx(sp_machine_t *m);

/* Render Sprinter 320x256 8bpp mode */
void video_render_320(sp_machine_t *m);

/* Render Sprinter 640x256 4bpp mode */
void video_render_640(sp_machine_t *m);

#endif /* SPEMU_VIDEO_H */
