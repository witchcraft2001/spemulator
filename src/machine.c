/*
 * SPEmulator — Machine State
 */
#include "machine.h"
#include "memory/mmu.h"
#include "video/video.h"
#include "video/palette.h"
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
        phys = ((u32)(page - 0x80) << 14) | (addr & 0x3FFF);
        if (phys < SP_ROM_SIZE)
            return m->rom[phys];
        return 0xFF;
    }

    phys = ((u32)page << 14) | (addr & 0x3FFF);
    if (phys < SP_RAM_SIZE)
        return m->ram[phys];
    return 0xFF;
}

void machine_mem_write(void *ctx, u16 addr, u8 data) {
    sp_machine_t *m = (sp_machine_t *)ctx;
    int win = addr >> 14;
    u8 page = m->page_reg[win];

    if (page >= 0x80) return; /* ROM write-protected */

    u32 phys = ((u32)page << 14) | (addr & 0x3FFF);

    /* VRAM pages #50-#5F */
    if (page >= 0x50 && page <= 0x5F) {
        /*
         * Page #5X: X = %TSPP
         *   T (bit 3): transparency (skip 0xFF)
         *   S (bit 2): shadow/VRAM-only (don't write DRAM)
         *   PP (bits 1-0): VRAM section offset
         */
        u8 transp = (page >> 3) & 1;
        u8 shadow = (page >> 2) & 1;
        u32 vram_offset = ((u32)(page & 0x03) << 14) | (addr & 0x3FFF);

        if (transp && data == 0xFF)
            return; /* Transparent: skip 0xFF bytes */

        if (vram_offset < SP_VRAM_SIZE) {
            m->vram[vram_offset] = data;
            video_on_vram_write(m, vram_offset, data);
        }
        if (!shadow && phys < SP_RAM_SIZE) {
            m->ram[phys] = data;
        }
        return;
    }

    if (phys < SP_RAM_SIZE)
        m->ram[phys] = data;
}

u8 machine_opcode_fetch(void *ctx, u16 addr) {
    return machine_mem_read(ctx, addr);
}

u8 machine_port_read(void *ctx, u16 port) {
    sp_machine_t *m = (sp_machine_t *)ctx;
    u8 lo = port & 0xFF;

    /* Page register reads */
    if (lo == SP_PORT_PAGE0) return m->page_reg[0];
    if (lo == SP_PORT_PAGE1) return m->page_reg[1];
    if (lo == SP_PORT_PAGE2) return m->page_reg[2];
    if (lo == SP_PORT_PAGE3) return m->page_reg[3];

    /* Video registers */
    if (lo == 0xC4 || lo == 0xCC) return m->port_y;
    if (lo == 0xC5 || lo == 0xCD) return m->rgmod;

    /* Keyboard: port #FE (ZX matrix) */
    if (lo == 0xFE) {
        u8 result = 0xFF;
        u8 rows = ~(port >> 8);
        for (int i = 0; i < 8; i++) {
            if (rows & (1 << i))
                result &= m->key_matrix[i];
        }
        return result;
    }

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

    /* Video registers */
    if (lo == 0xC4 || lo == 0xCC) { m->port_y = data; return; }
    if (lo == 0xC5 || lo == 0xCD) { m->rgmod = data; return; }

    /* Scroll register (port 0xCB) — from MAME: hold = {(7-(data&0xf))*2, 7-(data>>4)} */
    if (lo == 0xCB) {
        m->scroll_reg = data;
        m->hold_x = (i16)((7 - (data & 0x0F)) * 2);
        m->hold_y = (i16)(7 - (data >> 4));
        return;
    }

    /* Port #FE: border color + beeper */
    if (lo == 0xFE) {
        m->port_fe = data;
        return;
    }

    /* Pentagon page register #7FFD */
    if (port == 0x7FFD) {
        m->pn = data;
        return;
    }

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
        return count;
    }

    case 0x5B: { /* LD E,E — vertical fill */
        u16 addr = Z80_HL;
        u8 val = Z80_A;
        int count = Z80_A;
        for (int i = 0; i < count; i++) {
            machine_mem_write(m, addr, val);
            addr += 320;
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
        int count = Z80_A;
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
    m->vram = calloc(1, SP_VRAM_LINES * SP_VRAM_LINE);
    if (!m->ram || !m->rom || !m->vram) {
        fprintf(stderr, "Error: Failed to allocate memory\n");
        machine_destroy(m);
        return NULL;
    }

    /* Framebuffer: visible area */
    m->fb_width = SP_VIS_W;
    m->fb_height = SP_VIS_H;
    m->framebuffer = calloc((size_t)m->fb_width * m->fb_height, sizeof(u32));
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

    /* Initialize palette with defaults */
    palette_init_default(m);

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

    m->port_y = 0xC0;
    m->rgmod = 0;
    m->port_fe = 0;
    m->pn = 0;
    m->sc = 0;
    m->scroll_reg = 0x77; /* default: hold_x=0, hold_y=0 */
    m->hold_x = 0;
    m->hold_y = 0;
    m->conf_mode = false;
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
