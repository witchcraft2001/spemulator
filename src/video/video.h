/*
 * SPEmulator — Video Controller
 *
 * Sprinter video is a tile-based engine driven by mode descriptors in VRAM.
 * The FPGA scans 1024-byte VRAM lines, reading:
 *   - pixel/tile data at offsets 0x000–0x27F
 *   - mode descriptors at offset 0x300 (4 bytes per 16×8 tile)
 *   - palette RGB at offset 0x3E0
 *
 * Each 16×8 tile can independently be:
 *   - Bitmap tile (8bpp or 4bpp nibbles)
 *   - Symbol/text (8×8 font glyph from VRAM font table)
 *   - Border (solid color)
 *
 * Palette: 8 banks × 256 colors, stored in VRAM, cached as ARGB32.
 */
#ifndef SPEMU_VIDEO_H
#define SPEMU_VIDEO_H

#include "types.h"

typedef struct sp_machine sp_machine_t;

/* Render one full frame into machine->framebuffer */
void video_render_frame(sp_machine_t *m);

/* Called on VRAM write to update palette cache if address is in palette area */
void video_on_vram_write(sp_machine_t *m, u32 vram_offset, u8 data);

/* Get pointer to 4-byte mode descriptor for tile column `col` (0–55), row `row` (0–31) */
u8 *video_get_mode(sp_machine_t *m, int col, int row);

#endif /* SPEMU_VIDEO_H */
