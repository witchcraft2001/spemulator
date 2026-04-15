/*
 * SPEmulator — Machine State
 *
 * Architecture follows MAME's sprinter_state closely:
 * - DCP-based port decoding (256-entry LUT)
 * - update_memory() recalculates all 4 windows from registers
 * - VRAM access via PORT_Y * 1024 + (offset & 0x3FF)
 * - Two-phase boot: conf_loading → soft_reset → normal
 */
#ifndef SPEMU_MACHINE_H
#define SPEMU_MACHINE_H

#include "types.h"
#include "memory/dcp.h"
#include "bus.h"
#include "cpu/z80.h"
#include "cpu/ctc.h"
#include "cpu/sio.h"
#include "periph/rtc.h"
#include "config.h"

/* Screen constants (from MAME) */
#define SP_TOTAL_WIDTH    896
#define SP_TOTAL_HEIGHT   320
#define SP_BORDER_LEFT     48
#define SP_BORDER_RIGHT    48
#define SP_BORDER_TOP      16
#define SP_BORDER_BOTTOM   16
#define SP_ACTIVE_W       640
#define SP_ACTIVE_H       256
#define SP_VIS_W          (SP_BORDER_LEFT + SP_ACTIVE_W + SP_BORDER_RIGHT)
#define SP_VIS_H          (SP_BORDER_TOP + SP_ACTIVE_H + SP_BORDER_BOTTOM)

/* VRAM layout */
#define SP_VRAM_LINE      1024
#define SP_VRAM_LINES      256
#define SP_MODE_OFFSET    0x300
#define SP_PAL_OFFSET     0x3E0
#define SP_PAL_BANKS        8
#define SP_PAL_ENTRIES    256
#define SP_PAL_TOTAL      (SP_PAL_BANKS * SP_PAL_ENTRIES)

/* ROM: 256KB = 16 pages of 16KB */
#define SP_ROM_TOTAL      0x40000
#define SP_ROM_PAGES      16
/* FastRAM: 64KB */
#define SP_FASTRAM_SIZE   0x10000
/* Config loader ROM page */
#define SP_ROM_CONF_PAGE  0x0C

typedef struct sp_machine {
    /* CPU and peripherals */
    z80_t          cpu;
    sp_ctc_t       ctc;
    sp_sio_t       sio;
    sp_dcp_t       dcp;
    sp_bus_t       bus;
    sp_config_t   *config;

    /* === Memory === */
    u8            *ram;       /* 4MB */
    u8            *rom;       /* 256KB (16 × 16KB pages) */
    u8            *vram;      /* 256KB (256 × 1KB lines) */
    u8            *fastram;   /* 64KB */

    /* Computed memory window pointers (set by update_memory) */
    u8            *win_rd[4]; /* Read pointers for each 16KB window */
    u8            *win_wr[4]; /* Write pointers (NULL = read-only) */
    u8             win_page[4]; /* Effective page number per window */

    /* === System registers (from MAME) === */
    u8             pn;        /* Port #7FFD equivalent */
    u8             sc;        /* Port #1FFD equivalent */
    u8             cnf;       /* Config register (DCP bank select bits 3:2) */
    u8             rom_rg;    /* ROM page register (ports 0x5C, 0x8F) */
    bool           rom_sys;   /* ROM system mode (ports 0x3C/0x7C) */
    bool           cash_on;   /* FastRAM/cache enabled (port 0xFB/0x7B) */
    bool           dos_mode;  /* DOS mode: false=on, true=off */
    bool           ram_sys;   /* RAM system mode */
    u8             sys_pg;    /* System page flag for ROM XOR */
    bool           arom16;    /* AROM16 flag */
    bool           nmi_ena;   /* NMI enable (active low: 1=disabled) */

    /* RAM page table: 64 entries (virtual ports 0xC0-0xFF) */
    u8             ram_pages[64];

    /* Boot state */
    bool           conf_loading;  /* Phase 1: FPGA config loading */
    bool           starting;      /* True until first port read after reset */
    u32            conf_bytes;    /* Bytes written during conf_loading */

    /* === Video === */
    u8             port_y;     /* PORT_Y: VRAM Y-address (row for access) */
    u8             rgmod;      /* RGMOD register */
    u8             port_fe;    /* Port #FE (border, beeper) */
    u8             all_mode;   /* ALL_MODE (video mode select) */
    u8             scroll_reg; /* Scroll register (port 0xCB) */
    i16            hold_x;
    i16            hold_y;
    bool           conf_mode;  /* Game config mode */

    /* Framebuffer */
    u32           *framebuffer;
    int            fb_width;
    int            fb_height;

    /* Palette: 2048 ARGB entries */
    u32            palette[SP_PAL_TOTAL];

    /* Accelerator */
    bool           accel_enabled;
    u8             accel_size;

    /* Clock/timing */
    bool           turbo;
    u32            cpu_clock_hz;
    u64            frame_tstates;
    u64            tstates_in_frame;
    u32            frame_count;

    /* CMOS/RTC (DS12887) */
    sp_rtc_t       rtc;

    /* Keyboard (ZX matrix: 8 half-rows) */
    u8             key_matrix[8];

    /* State */
    bool           running;
    bool           paused;
    bool           debugger_active;
} sp_machine_t;

/* Lifecycle */
sp_machine_t *machine_create(sp_config_t *config);
void machine_destroy(sp_machine_t *machine);
void machine_reset(sp_machine_t *machine);
int  machine_run_frame(sp_machine_t *machine);

/* CPU callbacks */
u8   machine_mem_read(void *ctx, u16 addr);
void machine_mem_write(void *ctx, u16 addr, u8 data);
u8   machine_port_read(void *ctx, u16 port);
void machine_port_write(void *ctx, u16 port, u8 data);
u8   machine_opcode_fetch(void *ctx, u16 addr);
int  machine_accel_hook(void *ctx, u8 opcode);

/* Core memory mapping (called when registers change) */
void update_memory(sp_machine_t *m);

#endif /* SPEMU_MACHINE_H */
