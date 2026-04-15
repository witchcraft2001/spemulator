/*
 * SPEmulator — Palette Management
 *
 * Rebuilds ARGB palette cache from VRAM.
 * VRAM palette: within each 1KB line, at offsets 0x3E0..0x3FF:
 *   pen = ((vram_offset >> 2) & 7) * 256 + (vram_offset >> 10)
 *   RGB triplet at (vram_offset & ~3), +1, +2
 */
#include "video/palette.h"
#include "machine.h"
#include <string.h>

void palette_rebuild(sp_machine_t *m) {
    /*
     * Scan all VRAM addresses in palette range and extract RGB.
     * Palette entries are scattered across VRAM lines:
     *   For each 1KB line L (0..255), offsets 0x3E0..0x3FF contain palette data.
     *   Each 4-byte group at (L*1024 + 0x3E0 + N*4) where N=0..7:
     *     pen_index = N * 256 + L
     *     R = vram[L*1024 + 0x3E0 + N*4 + 0]
     *     G = vram[L*1024 + 0x3E0 + N*4 + 1]
     *     B = vram[L*1024 + 0x3E0 + N*4 + 2]
     */
    for (int line = 0; line < SP_VRAM_LINES; line++) {
        u32 base = (u32)line * SP_VRAM_LINE + SP_PAL_OFFSET;
        for (int n = 0; n < SP_PAL_BANKS; n++) {
            u32 addr = base + (u32)n * 4;
            if (addr + 2 >= SP_VRAM_SIZE) continue;

            u16 pen = (u16)(n * SP_PAL_ENTRIES + line);
            if (pen >= SP_PAL_TOTAL) continue;

            u8 r = m->vram[addr];
            u8 g = m->vram[addr + 1];
            u8 b = m->vram[addr + 2];
            m->palette[pen] = ((u32)r << 16) | ((u32)g << 8) | b;
        }
    }
}

void palette_init_default(sp_machine_t *m) {
    /* Zero all palette entries */
    memset(m->palette, 0, sizeof(m->palette));

    /* Set ZX Spectrum standard colors in bank 0 (indices 0-15) */
    static const u32 zx_pal[16] = {
        0x000000, 0x0000C0, 0xC00000, 0xC000C0,
        0x00C000, 0x00C0C0, 0xC0C000, 0xC0C0C0,
        0x000000, 0x0000FF, 0xFF0000, 0xFF00FF,
        0x00FF00, 0x00FFFF, 0xFFFF00, 0xFFFFFF,
    };
    for (int i = 0; i < 16; i++)
        m->palette[i] = zx_pal[i];

    /* Also set text palette bank (0x400+) with same ZX colors for symbol mode */
    for (int i = 0; i < 16; i++)
        m->palette[0x400 + i] = zx_pal[i];

    /* Set some usable colors in text attribute space:
     * pen 0x400 + attr + 0x100*ink_on + 0x200*flash
     * For standard white-on-black text: attr=0x07 (white ink on black paper) */
    for (int attr = 0; attr < 256; attr++) {
        u8 paper = (attr >> 4) & 0x0F;
        u8 ink = attr & 0x0F;
        /* Paper color (no pixel set, no flash) */
        m->palette[0x400 + attr] = (paper < 16) ? zx_pal[paper] : 0;
        /* Ink color (pixel set) */
        m->palette[0x400 + attr + 0x100] = (ink < 16) ? zx_pal[ink] : 0xFFFFFF;
        /* Paper flash */
        m->palette[0x400 + attr + 0x200] = (ink < 16) ? zx_pal[ink] : 0xFFFFFF;
        /* Ink flash */
        m->palette[0x400 + attr + 0x300] = (paper < 16) ? zx_pal[paper] : 0;
    }
}
