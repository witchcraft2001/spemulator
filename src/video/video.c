/*
 * SPEmulator — Video Controller
 *
 * Implements Sprinter tile-based video rendering following MAME's approach:
 *   - Mode descriptors in VRAM at offset 0x300 per 1KB line
 *   - Each 16×8 tile has a 4-byte descriptor
 *   - mode[0] bit 4 selects between tile (bitmap) and symbol (text) rendering
 *   - Palette stored in VRAM at offset ≥ 0x3E0, cached as ARGB32
 *   - Hardware scrolling via hold_x/hold_y
 */
#include "video/video.h"
#include "machine.h"
#include <string.h>

/*
 * Get pointer to 4-byte mode descriptor.
 *   col = tile column (0–55 in 896-wide space, but typically 0–39 visible)
 *   row = tile row (0–31 for 256 active lines / 8)
 *
 * VRAM address = (1 + col*2 + 0x80*(rgmod & 1)) * 1024 + 0x300 + row*4
 *
 * (From MAME as_mode(): line1 = (1 + a*2 + 0x80*(m_rgmod & 1)) * 1024)
 */
u8 *video_get_mode(sp_machine_t *m, int col, int row) {
    u32 line_addr = (u32)(1 + col * 2 + 0x80 * (m->rgmod & 1)) * SP_VRAM_LINE;
    u32 offset = line_addr + SP_MODE_OFFSET + (u32)row * 4;
    if (offset + 3 < SP_VRAM_SIZE)
        return &m->vram[offset];
    /* Fallback: return zeros */
    static u8 blank[4] = {0xFC, 0, 0, 0}; /* blank/border descriptor */
    return blank;
}

/*
 * Draw a bitmap tile (mode[0] bit 4 == 0).
 * Supports 8bpp and 4bpp modes, low-resolution pixel doubling.
 *
 * mode[0] bits 7-6: palette bank (0–3), shifted << 8 for pen index
 * mode[0] bit 5:    1=8bpp (1 pixel/byte), 0=4bpp (2 pixels/byte, nibbles)
 * mode[0] bits 3-0: tile X upper bits
 * mode[1] bits 2-0: tile X lower bits
 * mode[1] bits 7-3: tile Y offset
 * mode[2] bit 2:    lowres (pixel doubling)
 * mode[2] bits 1-0: lowres sub-offsets
 */
static void draw_tile(sp_machine_t *m, u8 *mode,
                       int clip_x, int clip_y, int clip_w, int clip_h) {
    u32 *fb = m->framebuffer;
    int fb_w = m->fb_width;

    u16 pal_base = (u16)((mode[0] >> 6) & 3) << 8;
    int tile_x = ((mode[0] & 0x0F) << 6) | ((mode[1] & 0x07) << 3);
    int tile_y = ((mode[1] >> 3) & 0x1F) << 3;
    bool is_8bpp = (mode[0] >> 5) & 1;
    bool lowres = (mode[2] >> 2) & 1;

    if (lowres) {
        tile_x += 4 * (mode[2] & 1);
        tile_y += 4 * ((mode[2] >> 1) & 1);
    }

    for (int dy = 0; dy < clip_h; dy++) {
        int screen_y = clip_y + dy;
        if (screen_y < 0 || screen_y >= m->fb_height) continue;

        int vy = (dy + (screen_y - m->hold_y)) & 7;
        if (lowres) vy >>= 1;
        int vram_y = tile_y + vy;

        for (int dx = 0; dx < clip_w; dx++) {
            int screen_x = clip_x + dx;
            if (screen_x < 0 || screen_x >= m->fb_width) continue;

            int vx = (dx + (screen_x - m->hold_x)) & 15;
            int byte_x = tile_x + (vx >> (1 + (lowres ? 1 : 0)));

            u32 vram_addr = (u32)vram_y * SP_VRAM_LINE + byte_x;
            if (vram_addr >= SP_VRAM_SIZE) continue;

            u8 color_byte = m->vram[vram_addr];
            u16 pen;

            if (is_8bpp) {
                pen = pal_base + color_byte;
            } else {
                /* 4bpp: even pixel = upper nibble, odd = lower */
                if ((screen_x - m->hold_x) & 1)
                    pen = pal_base + (color_byte & 0x0F);
                else
                    pen = pal_base + (color_byte >> 4);
            }

            if (pen < SP_PAL_TOTAL)
                fb[screen_y * fb_w + screen_x] = m->palette[pen];
        }
    }
}

/*
 * Draw a symbol/character tile (mode[0] bit 4 == 1).
 * Text mode: renders 8×8 font glyphs from VRAM font table.
 *
 * Two 8-pixel-wide characters fit in one 16-pixel-wide tile.
 * Character code read from VRAM at address derived from mode descriptor.
 * Attribute (ink/paper color) from separate VRAM location.
 * Font glyph bytes from VRAM font area.
 *
 * From MAME draw_symbol():
 *   attr = vram[(mode[2]<<10) | (mode[0]&0x0F)<<6 | (pn>>3&1)<<5 | 0x18 | (mode[0]>>6)&3]
 *   symb = vram[(mode[1]<<10) | (mode[0]&0x0F)<<6 | (pn>>3&1)<<5 | (mode[0]>>6&3)<<3 | (dy&7)]
 */
static void draw_symbol(sp_machine_t *m, u8 *mode,
                          int clip_x, int clip_y, int clip_w, int clip_h,
                          bool flash) {
    u32 *fb = m->framebuffer;
    int fb_w = m->fb_width;

    /* Check for blank tile: if (mode[0] & 0xFC) == 0xFC → border */
    bool is_blank = ((mode[0] & 0xFC) == 0xFC);
    bool is_border = (!is_blank) && (((mode[0] >> 5) & 7) == 7) &&
                     ((mode[0] & 0x0C) != 0x0C);

    /* Read attribute byte */
    u8 attr = 0;
    if (!is_blank) {
        u32 attr_addr = ((u32)mode[2] << 10)
                      | ((u32)(mode[0] & 0x0F) << 6)
                      | ((u32)((m->pn >> 3) & 1) << 5)
                      | 0x18
                      | ((mode[0] >> 6) & 3);
        if (attr_addr < SP_VRAM_SIZE)
            attr = m->vram[attr_addr];
    }

    /* Render the 16-pixel wide tile (two 8-pixel characters) */
    /* Process left half (first char) then right half (second char, mode+1024) */
    for (int half = 0; half < 2; half++) {
        u8 *cur_mode = mode;
        int half_start_x = clip_x + half * 8;
        int char_w = 8;

        /* The right half uses mode descriptor 1024 bytes ahead in VRAM */
        if (half == 1) {
            u32 right_offset = (u32)(cur_mode - m->vram) + SP_VRAM_LINE;
            if (right_offset + 3 < SP_VRAM_SIZE)
                cur_mode = &m->vram[right_offset];
        }

        /* Skip if mode[0] bit 5 is set and we need sub-character alignment */
        if (!(mode[0] & 0x20) && half == 1 && clip_w <= 8)
            continue;

        for (int dy = 0; dy < clip_h; dy++) {
            int screen_y = clip_y + dy;
            if (screen_y < 0 || screen_y >= m->fb_height) continue;

            int font_row = (screen_y - m->hold_y) & 7;

            /* Read font glyph byte */
            u8 symb = 0;
            if (!is_blank) {
                u32 symb_addr = ((u32)cur_mode[1] << 10)
                              | ((u32)(cur_mode[0] & 0x0F) << 6)
                              | ((u32)((m->pn >> 3) & 1) << 5)
                              | (((cur_mode[0] >> 6) & 3) << 3)
                              | font_row;
                if (symb_addr < SP_VRAM_SIZE)
                    symb = m->vram[symb_addr];
            }

            for (int dx = 0; dx < char_w; dx++) {
                int screen_x = half_start_x + dx;
                if (screen_x < 0 || screen_x >= m->fb_width) continue;
                if (screen_x < clip_x || screen_x >= clip_x + clip_w) continue;

                u16 pen;
                if (is_border) {
                    /* Border: use port #FE color */
                    u8 bc = m->port_fe & 0x07;
                    pen = 0x400 + (bc << 3) + bc;
                } else if (is_blank) {
                    pen = 0x400; /* black */
                } else {
                    /* Symbol rendering: bit set = ink, bit clear = paper */
                    int shift = (mode[0] & 0x20) ? 0 : 1;
                    u8 bit_mask = 1 << (7 - ((dx >> shift) & 7));
                    bool pixel_on = (symb & bit_mask) != 0;
                    pen = attr + 0x400
                        + (pixel_on ? 0x100 : 0)
                        + (flash ? 0x200 : 0);
                }

                if (pen < SP_PAL_TOTAL)
                    fb[screen_y * fb_w + screen_x] = m->palette[pen];
                else
                    fb[screen_y * fb_w + screen_x] = 0x000000;
            }
        }
    }
}

/*
 * Main rendering: iterate visible area in 16×8 tile blocks.
 * For each tile, read mode descriptor and dispatch to draw_tile or draw_symbol.
 * Following MAME's screen_update_graph().
 */
static void render_graph_mode(sp_machine_t *m) {
    bool flash = (m->frame_count >> 4) & 1;

    for (int vpos = 0; vpos < m->fb_height; ) {
        int b8 = (SP_TOTAL_HEIGHT + vpos - SP_BORDER_TOP - m->hold_y) % SP_TOTAL_HEIGHT;

        for (int hpos = 0; hpos < m->fb_width; ) {
            int a16 = (SP_TOTAL_WIDTH + hpos - SP_BORDER_LEFT - m->hold_x) % SP_TOTAL_WIDTH;

            /* Get mode descriptor for this tile */
            u8 *mode = video_get_mode(m, a16 >> 4, b8 >> 3);

            /* Calculate tile clip rectangle */
            int tile_w = 16 - (a16 & 15);
            int tile_h = 8 - (b8 & 7);
            if (hpos + tile_w > m->fb_width) tile_w = m->fb_width - hpos;
            if (vpos + tile_h > m->fb_height) tile_h = m->fb_height - vpos;

            if (mode[0] & 0x10) {
                /* Symbol/text mode */
                draw_symbol(m, mode, hpos, vpos, tile_w, tile_h, flash);
            } else {
                /* Bitmap/tile mode */
                draw_tile(m, mode, hpos, vpos, tile_w, tile_h);
            }

            hpos += tile_w;
        }
        vpos += 8 - (b8 & 7);
    }
}

/*
 * Update palette cache from VRAM.
 * Called on every VRAM write to addresses >= 0x3E0 within a 1KB line.
 *
 * From MAME:
 *   pen = BIT(offset, 2, 3) * 256 + (offset >> 10)
 *   p_red = offset & ~0x3  → RGB triplet at [p_red], [p_red+1], [p_red+2]
 */
void video_on_vram_write(sp_machine_t *m, u32 vram_offset, u8 data) {
    (void)data;
    u16 laddr = vram_offset & 0x3FF;

    if (laddr >= SP_PAL_OFFSET) {
        /* Palette area — recalculate this pen */
        u16 pen = ((vram_offset >> 2) & 7) * 256 + (vram_offset >> 10);
        u32 p_base = vram_offset & ~(u32)3;

        if (p_base + 2 < SP_VRAM_SIZE && pen < SP_PAL_TOTAL) {
            u8 r = m->vram[p_base];
            u8 g = m->vram[p_base + 1];
            u8 b = m->vram[p_base + 2];
            m->palette[pen] = ((u32)r << 16) | ((u32)g << 8) | b;
        }
    }
}

/*
 * Main entry: render frame to framebuffer.
 */
void video_render_frame(sp_machine_t *m) {
    /* Clear framebuffer to black */
    memset(m->framebuffer, 0, (size_t)m->fb_width * m->fb_height * sizeof(u32));

    if (m->conf_mode) {
        /* Game configuration mode — simplified for now, same as graph */
        render_graph_mode(m);
    } else {
        render_graph_mode(m);
    }
}
