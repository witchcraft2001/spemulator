/*
 * SPEmulator — Machine State
 * Central machine state holding all components.
 */
#ifndef SPEMU_MACHINE_H
#define SPEMU_MACHINE_H

#include "types.h"
#include "bus.h"
#include "cpu/z80.h"
#include "config.h"

/* Forward declarations */
typedef struct sp_mmu sp_mmu_t;
typedef struct sp_video sp_video_t;
typedef struct sp_keyboard sp_keyboard_t;

/*
 * Sprinter VRAM layout:
 *   Total 256KB organized as lines of 1024 bytes each (256 lines).
 *   Within each 1KB line:
 *     [0x000..0x13F] Screen A pixel data (320 bytes)
 *     [0x140..0x27F] Screen B pixel data (320 bytes)
 *     [0x280..0x2BF] (reserved / game scroll data)
 *     [0x2C0..0x2FF] Character font data (for symbol mode)
 *     [0x300..0x39F] Mode descriptors (4 bytes × 40 tile columns)
 *     [0x3A0..0x3DF] (reserved / interrupt config)
 *     [0x3E0..0x3FF] Palette RGB data (8 entries × 4 bytes)
 *
 *   Mode descriptor (4 bytes per 16×8 tile):
 *     mode[0]: bits 7-6 = palette bank (0-3)
 *              bit  5   = color mode (1=8bpp, 0=4bpp nibbles)
 *              bit  4   = symbol/tile select (1=symbol, 0=tile)
 *              bits 3-0 = tile X offset upper bits
 *     mode[1]: bits 7-3 = tile Y offset
 *              bits 2-0 = tile X offset lower bits
 *     mode[2]: bit  2   = lowres flag (pixel doubling)
 *              bits 1-0 = lowres X/Y suboffset
 *     mode[3]: bits 7-4 = scroll Y, bits 3-0 = scroll X (game mode)
 *
 *   Palette at offset ≥ 0x3E0 within each 1KB line:
 *     pen_index = bits[2:0_of_offset]*256 + (offset >> 10)
 *     3 bytes per entry: R, G, B (each stored as-is, MAME treats as 8-bit)
 */

/* Screen constants matching MAME */
#define SP_TOTAL_WIDTH    896     /* Total pixels per line (incl. blanking) */
#define SP_TOTAL_HEIGHT   320     /* Total lines (incl. blanking) */
#define SP_BORDER_LEFT     48
#define SP_BORDER_RIGHT    48
#define SP_BORDER_TOP      16
#define SP_BORDER_BOTTOM   16
#define SP_ACTIVE_W       640     /* Max active width */
#define SP_ACTIVE_H       256     /* Active height */
#define SP_VIS_W          (SP_BORDER_LEFT + SP_ACTIVE_W + SP_BORDER_RIGHT)   /* 736 */
#define SP_VIS_H          (SP_BORDER_TOP + SP_ACTIVE_H + SP_BORDER_BOTTOM)   /* 288 */

/* VRAM line stride */
#define SP_VRAM_LINE      1024
/* VRAM total */
#define SP_VRAM_LINES      256
/* Mode descriptor offsets within 1KB line */
#define SP_MODE_OFFSET    0x300
/* Palette offset within 1KB line */
#define SP_PAL_OFFSET     0x3E0

/* Number of palette banks × entries */
#define SP_PAL_BANKS        8
#define SP_PAL_ENTRIES    256
#define SP_PAL_TOTAL      (SP_PAL_BANKS * SP_PAL_ENTRIES)  /* 2048 */

/* Machine state */
typedef struct sp_machine {
    /* Components */
    z80_t          cpu;
    sp_bus_t       bus;
    sp_config_t   *config;

    /* Memory (owned by machine, managed by MMU) */
    u8            *ram;         /* 4MB RAM */
    u8            *rom;         /* 256KB ROM (16 × 16KB pages) */
    u8            *vram;        /* 256KB VRAM (SP_VRAM_LINES * SP_VRAM_LINE) */
    u8            *fastram;     /* 64KB FastRAM (cache) */

    /* Boot state: two-phase boot like real Sprinter */
    bool           conf_loading;  /* Phase 1: config loader running from ROM page 0x0C */
    u32            conf_bytes;    /* Counter: bytes written to FPGA config port */
    u8             rom_rg;        /* ROM register: selects which ROM page at WIN0 */
    bool           rom_sys;       /* ROM system mode */
    bool           cash_on;       /* FastRAM/cache enabled */
    bool           dos_mode;      /* DOS active (0=on, 1=off), init=1 */

    /* Memory page registers — port-based (Sprinter native) */
    u8             page_reg[4]; /* Current page in each window (#82,#A2,#C2,#E2) */
    /* RAM page table (64 entries, indexed by port hi-nibble) */
    u8             ram_pages[64];

    /* Pentagon/Scorpion compat registers */
    u8             pn;          /* Pentagon page register (#7FFD) */
    u8             sc;          /* Scorpion page register (#1FFD) */

    /* Video state */
    u8             port_y;      /* PORT_Y register (0xC4/0xCC) */
    u8             rgmod;       /* RGMOD register (0xC5/0xCD): bit0=screen select */
    u8             port_fe;     /* Port #FE data (border, beeper, tape) */
    u8             scroll_reg;  /* Scroll register (port 0xCB) */
    i16            hold_x;      /* Horizontal scroll offset (derived from scroll_reg) */
    i16            hold_y;      /* Vertical scroll offset (derived from scroll_reg) */
    bool           conf_mode;   /* Game configuration mode (Thunder in the Deep) */

    /* Framebuffer for SDL */
    u32           *framebuffer;
    int            fb_width;    /* = SP_VIS_W */
    int            fb_height;   /* = SP_VIS_H */

    /* Palette cache: 2048 ARGB entries (8 banks × 256 colors)
     * Rebuilt from VRAM palette area on writes */
    u32            palette[SP_PAL_TOTAL];

    /* Accelerator state */
    bool           accel_enabled;
    u8             accel_size;

    /* Turbo mode */
    bool           turbo;
    u32            cpu_clock_hz;

    /* Frame timing */
    u64            frame_tstates;
    u64            tstates_in_frame;
    u32            frame_count;

    /* Keyboard state (ZX matrix: 8 half-rows) */
    u8             key_matrix[8];

    /* Running state */
    bool           running;
    bool           paused;

    /* Debugger active */
    bool           debugger_active;
} sp_machine_t;

/* Create/destroy machine */
sp_machine_t *machine_create(sp_config_t *config);
void machine_destroy(sp_machine_t *machine);

/* Reset machine */
void machine_reset(sp_machine_t *machine);

/* Run one frame (returns actual T-states executed) */
int machine_run_frame(sp_machine_t *machine);

/* Memory access (used as CPU callbacks) */
u8   machine_mem_read(void *ctx, u16 addr);
void machine_mem_write(void *ctx, u16 addr, u8 data);
u8   machine_port_read(void *ctx, u16 port);
void machine_port_write(void *ctx, u16 port, u8 data);
u8   machine_opcode_fetch(void *ctx, u16 addr);

/* Accelerator hook */
int  machine_accel_hook(void *ctx, u8 opcode);

#endif /* SPEMU_MACHINE_H */
