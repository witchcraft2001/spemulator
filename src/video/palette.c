/*
 * SPEmulator — Palette Management
 */
#include "video/palette.h"
#include "machine.h"

/* Convert 6-bit color component to 8-bit */
static inline u8 expand6to8(u8 val6) {
    val6 &= 0x3F;
    return (u8)((val6 << 2) | (val6 >> 4));
}

void palette_set(sp_machine_t *m, u8 index, u8 r6, u8 g6, u8 b6) {
    u8 r = expand6to8(r6);
    u8 g = expand6to8(g6);
    u8 b = expand6to8(b6);
    m->palette[index] = ((u32)r << 16) | ((u32)g << 8) | b;
}

void palette_init_default(sp_machine_t *m) {
    /* Standard ZX Spectrum colors (indices 0-15) */
    static const u32 zx_pal[16] = {
        0x000000, 0x0000C0, 0xC00000, 0xC000C0,
        0x00C000, 0x00C0C0, 0xC0C000, 0xC0C0C0,
        0x000000, 0x0000FF, 0xFF0000, 0xFF00FF,
        0x00FF00, 0x00FFFF, 0xFFFF00, 0xFFFFFF,
    };
    for (int i = 0; i < 16; i++)
        m->palette[i] = zx_pal[i];

    /* Fill 16-255 with a 6-6-6 RGB ramp */
    int idx = 16;
    for (int r = 0; r < 6 && idx < 256; r++)
        for (int g = 0; g < 6 && idx < 256; g++)
            for (int b = 0; b < 6 && idx < 256; b++)
                m->palette[idx++] = (expand6to8(r * 12) << 16)
                                  | (expand6to8(g * 12) << 8)
                                  |  expand6to8(b * 12);
}
