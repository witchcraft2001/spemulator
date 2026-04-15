/*
 * SPEmulator — Machine State
 */
#include "machine.h"
#include "memory/mmu.h"
#include "video/video.h"
#include "video/accel.h"
#include "input/keyboard.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

/* --- Memory callbacks for Z80 --- */

u8 machine_mem_read(void *ctx, u16 addr) {
    sp_machine_t *m = (sp_machine_t *)ctx;
    int win = addr >> 14;
    u8 page = m->page_reg[win];
    u32 phys;

    if (page >= 0x80) {
        /* ROM space */
        phys = ((u32)(page - 0x80) << 14) | (addr & 0x3FFF);
        if (phys < SP_ROM_SIZE)
            return m->rom[phys];
        return 0xFF;
    }

    /* RAM space */
    phys = ((u32)page << 14) | (addr & 0x3FFF);
    if (phys < SP_RAM_SIZE)
        return m->ram[phys];
    return 0xFF;
}

void machine_mem_write(void *ctx, u16 addr, u8 data) {
    sp_machine_t *m = (sp_machine_t *)ctx;
    int win = addr >> 14;
    u8 page = m->page_reg[win];

    /* ROM pages are write-protected */
    if (page >= 0x80) return;

    u32 phys = ((u32)page << 14) | (addr & 0x3FFF);

    /* Check if writing to VRAM pages */
    if (page >= 0x50 && page <= 0x5F) {
        u8 vram_mode = (page >> 2) & 0x03;
        u32 vram_offset = ((u32)(page & 0x03) << 14) | (addr & 0x3FFF);

        switch (vram_mode) {
        case 0: /* Normal: write to both VRAM and DRAM */
            if (vram_offset < SP_VRAM_SIZE)
                m->vram[vram_offset] = data;
            if (phys < SP_RAM_SIZE)
                m->ram[phys] = data;
            return;
        case 1: /* VRAM-only */
            if (vram_offset < SP_VRAM_SIZE)
                m->vram[vram_offset] = data;
            return;
        case 2: /* Transparent: skip 0xFF */
            if (data != 0xFF) {
                if (vram_offset < SP_VRAM_SIZE)
                    m->vram[vram_offset] = data;
                if (phys < SP_RAM_SIZE)
                    m->ram[phys] = data;
            }
            return;
        case 3: /* Sprite: transparent + VRAM-only */
            if (data != 0xFF && vram_offset < SP_VRAM_SIZE)
                m->vram[vram_offset] = data;
            return;
        }
    }

    if (phys < SP_RAM_SIZE)
        m->ram[phys] = data;
}

u8 machine_opcode_fetch(void *ctx, u16 addr) {
    /* Same as mem_read for now; M1 cycle detection can be added later */
    return machine_mem_read(ctx, addr);
}

u8 machine_port_read(void *ctx, u16 port) {
    sp_machine_t *m = (sp_machine_t *)ctx;

    /* Page register reads */
    u8 lo = port & 0xFF;
    if (lo == SP_PORT_PAGE0) return m->page_reg[0];
    if (lo == SP_PORT_PAGE1) return m->page_reg[1];
    if (lo == SP_PORT_PAGE2) return m->page_reg[2];
    if (lo == SP_PORT_PAGE3) return m->page_reg[3];

    /* Video port reads */
    if (lo == SP_PORT_ALLMODE) return m->video_mode;
    if (lo == SP_PORT_RGMOD)   return m->rgmod;

    /* Keyboard: port #FE (ZX matrix) */
    if ((port & 0xFF) == 0xFE) {
        u8 result = 0xFF;
        u8 rows = ~(port >> 8);
        for (int i = 0; i < 8; i++) {
            if (rows & (1 << i))
                result &= m->key_matrix[i];
        }
        return result;
    }

    /* Delegate to bus */
    return bus_port_read(&m->bus, port);
}

void machine_port_write(void *ctx, u16 port, u8 data) {
    sp_machine_t *m = (sp_machine_t *)ctx;
    u8 lo = port & 0xFF;

    /* Page registers */
    if (lo == SP_PORT_PAGE0) { m->page_reg[0] = data; return; }
    if (lo == SP_PORT_PAGE1) { m->page_reg[1] = data; return; }
    if (lo == SP_PORT_PAGE2) { m->page_reg[2] = data; return; }
    if (lo == SP_PORT_PAGE3) { m->page_reg[3] = data; return; }

    /* Video mode */
    if (lo == SP_PORT_ALLMODE) { m->video_mode = data & 0x03; return; }
    if (lo == SP_PORT_RGMOD)   { m->rgmod = data; return; }
    if (lo == SP_PORT_Y)       { m->port_y = data; return; }

    /* Border color (port #FE) */
    if ((port & 0xFF) == 0xFE) {
        m->border_color = data & 0x07;
        return;
    }

    /* Delegate to bus */
    bus_port_write(&m->bus, port, data);
}

/* --- Accelerator hook --- */

int machine_accel_hook(void *ctx, u8 opcode) {
    sp_machine_t *m = (sp_machine_t *)ctx;
    if (!m->accel_enabled) return 0;

    z80_t *cpu = &m->cpu;

    switch (opcode) {
    case 0x40: /* LD B,B — disable accelerator */
        m->accel_enabled = false;
        return 0;

    case 0x52: /* LD D,D — set block size */
        m->accel_size = Z80_A;
        return 0;

    case 0x49: { /* LD C,C — fill block */
        u16 addr = Z80_HL;
        u8 val = Z80_A;
        int count = m->accel_size ? m->accel_size : 256;
        for (int i = 0; i < count; i++) {
            machine_mem_write(m, addr, val);
            addr++;
        }
        Z80_HL = addr;
        /* At 42MHz, each byte takes ~1 cycle = ~6 CPU cycles at 7MHz */
        return count; /* approximate T-states */
    }

    case 0x5B: { /* LD E,E — vertical fill */
        u16 addr = Z80_HL;
        u8 val = Z80_A;
        int count = Z80_A; /* height */
        for (int i = 0; i < count; i++) {
            machine_mem_write(m, addr, val);
            addr += 320; /* next line in 320-mode */
        }
        return count * 2;
    }

    case 0x6D: { /* LD L,L — copy row (HL→DE) */
        u16 src = Z80_HL;
        u16 dst = Z80_DE;
        int count = m->accel_size ? m->accel_size : 256;
        for (int i = 0; i < count; i++) {
            u8 v = machine_mem_read(m, src++);
            machine_mem_write(m, dst++, v);
        }
        Z80_HL = src;
        Z80_DE = dst;
        return count;
    }

    case 0x7F: { /* LD A,A — copy vertical */
        u16 src = Z80_HL;
        u16 dst = Z80_DE;
        int count = Z80_A; /* height */
        for (int i = 0; i < count; i++) {
            u8 v = machine_mem_read(m, src);
            machine_mem_write(m, dst, v);
            src += 320;
            dst += 320;
        }
        return count * 2;
    }
    }

    return 0;
}

/* --- Machine lifecycle --- */

static int load_rom(sp_machine_t *m, const char *path) {
    if (!path || !path[0]) {
        fprintf(stderr, "Warning: No ROM file specified\n");
        return -1;
    }
    FILE *f = fopen(path, "rb");
    if (!f) {
        fprintf(stderr, "Error: Cannot open ROM file: %s\n", path);
        return -1;
    }
    size_t n = fread(m->rom, 1, SP_ROM_SIZE, f);
    fclose(f);
    printf("Loaded ROM: %s (%zu bytes)\n", path, n);
    return 0;
}

sp_machine_t *machine_create(sp_config_t *config) {
    sp_machine_t *m = calloc(1, sizeof(sp_machine_t));
    if (!m) return NULL;

    m->config = config;

    /* Allocate memory */
    m->ram = calloc(1, SP_RAM_SIZE);
    m->rom = calloc(1, SP_ROM_SIZE);
    m->vram = calloc(1, SP_VRAM_SIZE);
    if (!m->ram || !m->rom || !m->vram) {
        fprintf(stderr, "Error: Failed to allocate memory\n");
        machine_destroy(m);
        return NULL;
    }

    /* Framebuffer: max resolution 640+border*2 x 256+border*2 */
    m->fb_width = SP_SCREEN_W_640 + SP_BORDER_SIZE * 2;
    m->fb_height = SP_SCREEN_H_640 + SP_BORDER_SIZE * 2;
    m->framebuffer = calloc(m->fb_width * m->fb_height, sizeof(u32));
    if (!m->framebuffer) {
        fprintf(stderr, "Error: Failed to allocate framebuffer\n");
        machine_destroy(m);
        return NULL;
    }

    /* Initialize bus */
    bus_init(&m->bus, m);

    /* Initialize Z80 */
    z80_init(&m->cpu);
    m->cpu.mem_read = machine_mem_read;
    m->cpu.mem_write = machine_mem_write;
    m->cpu.port_read = machine_port_read;
    m->cpu.port_write = machine_port_write;
    m->cpu.opcode_fetch = machine_opcode_fetch;
    m->cpu.callback_ctx = m;
    m->cpu.accel_hook = machine_accel_hook;
    m->cpu.accel_ctx = m;

    /* Initialize keyboard to all keys released */
    memset(m->key_matrix, 0xFF, sizeof(m->key_matrix));

    /* Load ROM */
    load_rom(m, config->rom_path);

    /* Initialize default palette (ZX Spectrum colors + grayscale ramp) */
    static const u32 zx_colors[16] = {
        0x000000, 0x0000C0, 0xC00000, 0xC000C0,
        0x00C000, 0x00C0C0, 0xC0C000, 0xC0C0C0,
        0x000000, 0x0000FF, 0xFF0000, 0xFF00FF,
        0x00FF00, 0x00FFFF, 0xFFFF00, 0xFFFFFF,
    };
    for (int i = 0; i < 16; i++)
        m->palette[i] = zx_colors[i];
    /* Fill remaining with grayscale ramp */
    for (int i = 16; i < 256; i++) {
        u8 g = (u8)i;
        m->palette[i] = (g << 16) | (g << 8) | g;
    }

    /* Set clock */
    m->turbo = (config->cpu_speed_mhz >= 21);
    m->cpu_clock_hz = m->turbo ? SP_CPU_TURBO_HZ : SP_CPU_NORMAL_HZ;
    m->frame_tstates = m->cpu_clock_hz / SP_FRAME_RATE_PAL;

    m->running = true;

    machine_reset(m);
    return m;
}

void machine_destroy(sp_machine_t *m) {
    if (!m) return;
    bus_destroy(&m->bus);
    free(m->ram);
    free(m->rom);
    free(m->vram);
    free(m->framebuffer);
    free(m);
}

void machine_reset(sp_machine_t *m) {
    z80_reset(&m->cpu);

    /* Default page mapping at reset */
    m->page_reg[0] = 0x80;  /* WIN0 = ROM page 0 */
    m->page_reg[1] = 0x02;  /* WIN1 = RAM page 2 */
    m->page_reg[2] = 0x0A;  /* WIN2 = RAM page 10 */
    m->page_reg[3] = 0x00;  /* WIN3 = RAM page 0 */

    m->video_mode = SP_VMODE_ZX;
    m->port_y = 0xC0;
    m->rgmod = 0;
    m->border_color = 0;
    m->accel_enabled = false;
    m->accel_size = 0;
    m->frame_count = 0;
    m->tstates_in_frame = 0;

    bus_reset_all(&m->bus);
}

int machine_run_frame(sp_machine_t *m) {
    if (m->paused) return 0;

    bus_begin_frame(&m->bus);

    int frame_ts = (int)m->frame_tstates;
    int executed = 0;

    while (executed < frame_ts) {
        int t = z80_step(&m->cpu);
        executed += t;
        m->cpu.total_tstates += t;
    }

    /* VSync interrupt */
    if (m->cpu.iff1) {
        z80_irq(&m->cpu, 0xFF);
    }

    m->tstates_in_frame = 0;
    m->frame_count++;

    bus_end_frame(&m->bus);
    return executed;
}
