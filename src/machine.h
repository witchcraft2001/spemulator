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

/* Machine state */
typedef struct sp_machine {
    /* Components */
    z80_t          cpu;
    sp_bus_t       bus;
    sp_config_t   *config;

    /* Memory (owned by machine, managed by MMU) */
    u8            *ram;         /* 4MB RAM */
    u8            *rom;         /* 512KB ROM */
    u8            *vram;        /* 256KB VRAM */

    /* Memory page registers */
    u8             page_reg[4]; /* Current page in each window (#82,#A2,#C2,#E2) */

    /* Video state */
    u8             video_mode;  /* 0=ZX, 1=320x256, 2=640x256 */
    u8             port_y;      /* PORT_Y register */
    u8             rgmod;       /* RGMOD register */
    u8             border_color;
    u32           *framebuffer; /* ARGB framebuffer for SDL */
    int            fb_width;
    int            fb_height;

    /* Palette: 256 entries, each is 0x00RRGGBB */
    u32            palette[256];

    /* Accelerator state */
    bool           accel_enabled;
    u8             accel_size;

    /* Turbo mode */
    bool           turbo;
    u32            cpu_clock_hz;

    /* Frame timing */
    u64            frame_tstates;     /* T-states per frame */
    u64            tstates_in_frame;  /* T-states elapsed in current frame */
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
