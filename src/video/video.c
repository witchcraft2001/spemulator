/*
 * SPEmulator — Video Controller
 */
#include "video/video.h"
#include "machine.h"
#include <string.h>

/* ZX Spectrum non-linear pixel address calculation */
static inline u32 zx_pixel_addr(int x, int y) {
    /* Standard ZX Spectrum screen layout:
       Address = ((y & 0xC0) << 5) | ((y & 0x07) << 8) | ((y & 0x38) << 2) | (x >> 3) */
    return (u32)(((y & 0xC0) << 5) | ((y & 0x07) << 8) | ((y & 0x38) << 2) | (x >> 3));
}

static inline u32 zx_attr_addr(int x, int y) {
    return (u32)(6144 + (y >> 3) * 32 + (x >> 3));
}

void video_render_zx(sp_machine_t *m) {
    int bw = SP_BORDER_SIZE;
    int fb_w = m->fb_width;
    u32 border_rgb = m->palette[m->border_color & 0x07];

    /* VRAM for ZX mode is in pages #50-#54 mapped to VRAM at offset 0 */
    u8 *scr = m->vram;  /* Screen data starts at VRAM offset 0 */

    /* Fill entire framebuffer with border */
    for (int i = 0; i < fb_w * m->fb_height; i++)
        m->framebuffer[i] = border_rgb;

    /* Render 256x192 pixels */
    bool flash_on = (m->frame_count >> 4) & 1; /* ~50/16 = ~3Hz blink */

    for (int y = 0; y < SP_SCREEN_H_ZX; y++) {
        for (int col = 0; col < 32; col++) {
            int x = col * 8;
            u32 paddr = zx_pixel_addr(x, y);
            u32 aaddr = zx_attr_addr(x, y);
            u8 pixels = scr[paddr];
            u8 attr = scr[aaddr];

            u8 ink = attr & 0x07;
            u8 paper = (attr >> 3) & 0x07;
            bool bright = (attr & 0x40) != 0;
            bool flash = (attr & 0x80) != 0;

            if (bright) { ink += 8; paper += 8; }
            if (flash && flash_on) { u8 tmp = ink; ink = paper; paper = tmp; }

            u32 ink_rgb = m->palette[ink];
            u32 paper_rgb = m->palette[paper];

            int fb_y = y + bw;
            int fb_x = x + bw;
            u32 *row = &m->framebuffer[fb_y * fb_w + fb_x];

            for (int bit = 7; bit >= 0; bit--) {
                *row++ = (pixels & (1 << bit)) ? ink_rgb : paper_rgb;
            }
        }
    }
}

void video_render_320(sp_machine_t *m) {
    int bw = SP_BORDER_SIZE;
    int fb_w = m->fb_width;
    u32 border_rgb = m->palette[m->border_color];

    /* Determine active screen buffer based on RGMOD bit 0 */
    /* Screen A: VRAM offset 0, Screen B: VRAM offset 5*16384 = 81920 */
    u32 scr_offset = (m->rgmod & 1) ? 0 : (5 * SP_PAGE_SIZE);
    u8 *scr = m->vram + scr_offset;

    /* Fill border */
    for (int i = 0; i < fb_w * m->fb_height; i++)
        m->framebuffer[i] = border_rgb;

    /* Render 320x256 pixels, 1 byte per pixel */
    for (int y = 0; y < SP_SCREEN_H_320; y++) {
        int fb_y = y + bw;
        u32 *row = &m->framebuffer[fb_y * fb_w + bw];
        u8 *src = &scr[y * 320];

        for (int x = 0; x < SP_SCREEN_W_320; x++) {
            *row++ = m->palette[*src++];
        }
    }
}

void video_render_640(sp_machine_t *m) {
    int bw = SP_BORDER_SIZE;
    int fb_w = m->fb_width;
    u32 border_rgb = m->palette[m->border_color];

    u32 scr_offset = (m->rgmod & 1) ? 0 : (5 * SP_PAGE_SIZE);
    u8 *scr = m->vram + scr_offset;

    /* Fill border */
    for (int i = 0; i < fb_w * m->fb_height; i++)
        m->framebuffer[i] = border_rgb;

    /* Render 640x256 pixels, 4bpp (2 pixels per byte) */
    for (int y = 0; y < SP_SCREEN_H_640; y++) {
        int fb_y = y + bw;
        u32 *row = &m->framebuffer[fb_y * fb_w + bw];
        u8 *src = &scr[y * 320]; /* 640/2 = 320 bytes per line */

        for (int x = 0; x < 320; x++) {
            u8 byte = *src++;
            *row++ = m->palette[(byte >> 4) & 0x0F];
            *row++ = m->palette[byte & 0x0F];
        }
    }
}

void video_render_frame(sp_machine_t *m) {
    switch (m->video_mode) {
    case SP_VMODE_ZX:
        video_render_zx(m);
        break;
    case SP_VMODE_320:
        video_render_320(m);
        break;
    case SP_VMODE_640:
        video_render_640(m);
        break;
    default:
        video_render_zx(m);
        break;
    }
}
