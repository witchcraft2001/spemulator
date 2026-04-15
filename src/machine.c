/*
 * SPEmulator — Machine State
 *
 * Two-phase boot (following MAME):
 *   Phase 1 (conf_loading=true): CPU executes from ROM page 0x0C (config loader).
 *     The loader initializes hardware: writes to ports, sets up DCP table in RAM,
 *     copies BIOS code to FastRAM, etc. On completion it transitions to Phase 2.
 *   Phase 2 (conf_loading=false): Normal operation. Memory mapping via page registers.
 */
#include "machine.h"
#include "memory/mmu.h"
#include "video/video.h"
#include "video/palette.h"
#include "video/accel.h"
#include "input/keyboard.h"
#include "cpu/ctc.h"
#include "cpu/sio.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

/* CTC IRQ callback — fires when a CTC channel reaches zero */
static void ctc_irq_handler(void *ctx, int channel) {
    sp_machine_t *m = (sp_machine_t *)ctx;
    /* CTC vector = base_vector + channel*2 */
    u8 vector = m->ctc.vector + (u8)(channel * 2);
    z80_irq(&m->cpu, vector);
}

/* ROM layout: 256KB = 16 pages × 16KB */
#define ROM_TOTAL_SIZE   0x40000
#define ROM_PAGE_SIZE    SP_PAGE_SIZE
#define ROM_NUM_PAGES    (ROM_TOTAL_SIZE / ROM_PAGE_SIZE)
#define ROM_CONF_PAGE    0x0C  /* Configuration loader at page 12 */

/* FastRAM size */
#define FASTRAM_SIZE     0x10000  /* 64KB */

/* Default RAM page table (from MAME machine_start) */
static const u8 default_ram_pages[64] = {
    /* 0xC0-0xCF: SYS PORTS COPIES */
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    /* 0xD0-0xDF: RAM PAGES */
    0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17,
    0x18, 0x19, 0x1a, 0x1b, 0x1c, 0x1d, 0x1e, 0x1f,
    /* 0xE0-0xEF: ROM PAGES */
    0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41,
    0x00, 0x05, 0x02, 0x41, 0xff, 0x00, 0x00, 0x41,
    /* 0xF0-0xFF: RAM PAGES */
    0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,
    0x08, 0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f,
};

/* --- Memory callbacks for Z80 --- */

/*
 * Memory read: during conf_loading, WIN0 reads from ROM page ROM_CONF_PAGE.
 * After conf_loading, standard page-register-based mapping.
 */
u8 machine_mem_read(void *ctx, u16 addr) {
    sp_machine_t *m = (sp_machine_t *)ctx;
    int win = addr >> 14;

    /* During configuration loading, WIN0 reads from ROM conf page */
    if (win == 0 && m->conf_loading) {
        u32 rom_offset = (u32)ROM_CONF_PAGE * ROM_PAGE_SIZE + (addr & 0x3FFF);
        if (rom_offset < ROM_TOTAL_SIZE)
            return m->rom[rom_offset];
        return 0xFF;
    }

    u8 page = m->page_reg[win];

    /* ROM pages: 0x80+ maps to ROM */
    if (page >= 0x80) {
        u32 phys = ((u32)(page - 0x80) << 14) | (addr & 0x3FFF);
        if (phys < ROM_TOTAL_SIZE)
            return m->rom[phys];
        return 0xFF;
    }

    /* RAM pages */
    u32 phys = ((u32)page << 14) | (addr & 0x3FFF);
    if (phys < SP_RAM_SIZE)
        return m->ram[phys];
    return 0xFF;
}

void machine_mem_write(void *ctx, u16 addr, u8 data) {
    sp_machine_t *m = (sp_machine_t *)ctx;
    int win = addr >> 14;

    /* During configuration loading, WIN0 writes go to RAM page 0 (working area) */
    if (win == 0 && m->conf_loading) {
        u32 phys = addr & 0x3FFF;
        if (phys < SP_RAM_SIZE)
            m->ram[phys] = data;
        return;
    }

    u8 page = m->page_reg[win];

    /* ROM write-protected */
    if (page >= 0x80) return;

    u32 phys = ((u32)page << 14) | (addr & 0x3FFF);

    /* VRAM pages #50-#5F */
    if (page >= 0x50 && page <= 0x5F) {
        u8 transp = (page >> 3) & 1;
        u8 shadow = (page >> 2) & 1;
        u32 vram_offset = ((u32)(page & 0x03) << 14) | (addr & 0x3FFF);

        if (transp && data == 0xFF) return;

        if (vram_offset < SP_VRAM_SIZE) {
            m->vram[vram_offset] = data;
            video_on_vram_write(m, vram_offset, data);
        }
        if (!shadow && phys < SP_RAM_SIZE)
            m->ram[phys] = data;
        return;
    }

    if (phys < SP_RAM_SIZE)
        m->ram[phys] = data;
}

u8 machine_opcode_fetch(void *ctx, u16 addr) {
    return machine_mem_read(ctx, addr);
}

/* --- Port I/O --- */

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

    /* CTC ports (0x10-0x13) */
    if (lo >= 0x10 && lo <= 0x13)
        return ctc_read(&m->ctc, lo - 0x10);

    /* SIO ports: 0x18 = CH-A data (keyboard), 0x19 = CH-A ctrl,
     *            0x1A = CH-B data (serial), 0x1B = CH-B ctrl */
    if (lo == 0x18) return sio_read_data(&m->sio, 0);
    if (lo == 0x19) return sio_read_ctrl(&m->sio, 0);
    if (lo == 0x1A) return sio_read_data(&m->sio, 1);
    if (lo == 0x1B) return sio_read_ctrl(&m->sio, 1);

    /* CMOS/RTC: port 0x1C reads data at selected CMOS address.
     * The high byte of the port address is the CMOS register index
     * (set by previous OUT to port 0x1C where A=register). */
    if (lo == 0x1C) {
        u8 cmos_addr = (port >> 8) & 0xFF;
        /* Return sensible defaults for key CMOS registers */
        switch (cmos_addr) {
        case 0x0E: return 0x80;  /* Fast boot (skip RAM test), no buzzer */
        case 0x0F: return 0x10;  /* Keyboard delay/repeat default */
        case 0x10: return 0x02;  /* Boot device: IDE1 */
        case 0x11: return 0x01;  /* FDD/IDE config */
        case 0x1B: return 0x00;  /* Hardware config: normal speed */
        default: return 0x00;
        }
    }

    /* Port 0x00 with various high bytes: general port read */
    if (lo == 0x00) return 0xFF;

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

    /* IDE status — report not present */
    if ((port & 0xFF) == 0x53 && ((port >> 8) & 0xFF) == 0x40)
        return 0x00; /* BSY=0, RDY=0 → no device */

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
    if (lo == 0xCB) {
        m->scroll_reg = data;
        m->hold_x = (i16)((7 - (data & 0x0F)) * 2);
        m->hold_y = (i16)(7 - (data >> 4));
        return;
    }

    /* Port #FE: border + beeper */
    if (lo == 0xFE) { m->port_fe = data; return; }

    /* Port 0x3C: disable BIOS ROM / enable RAM at WIN0 */
    if (lo == 0x3C) {
        m->rom_sys = !(data & 0x40);
        if (m->conf_loading) {
            /* Configuration loader finished — transition to Phase 2 */
            m->conf_loading = false;
            /* Set up normal page mapping: WIN0 = ROM page 0 */
            m->page_reg[0] = 0x80; /* ROM page 0 */
            printf("Boot: Configuration phase complete, entering BIOS\n");
        }
        return;
    }

    /* Port 0x7C: enable BIOS ROM at WIN0 */
    if (lo == 0x7C) {
        m->rom_sys = true;
        return;
    }

    /* Port 0xFB: enable FastRAM/cache */
    if (lo == 0xFB) { m->cash_on = (port >> 7) & 1; return; }
    /* Port 0x7B: disable FastRAM */
    if (lo == 0x7B) { m->cash_on = false; return; }

    /* Pentagon page register #7FFD */
    if (port == 0x7FFD) { m->pn = data; return; }
    /* Scorpion page register #1FFD */
    if (port == 0x1FFD) { m->sc = data; return; }

    /* FPGA configuration port (0xEF via BC=xxEF OUT (C),r) —
     * Count writes; after enough bytes, skip to Phase 2 */
    if (lo == 0xEF && m->conf_loading) {
        m->conf_bytes++;
        /* Real ACEX 1K30 bitstream is ~40KB. After enough bytes, skip. */
        if (m->conf_bytes >= 40000) {
            m->conf_loading = false;
            /* Switch WIN0 to ROM page 0 for normal BIOS execution */
            m->page_reg[0] = 0x80;
            /* Reset CPU to start from 0x0000 (now ROM page 0) */
            z80_reset(&m->cpu);
            /* Set up standard post-config page mapping */
            m->page_reg[0] = 0x80; /* ROM page 0 */
            m->page_reg[1] = 0x02; /* RAM page 2 */
            m->page_reg[2] = 0x0A; /* RAM page 10 */
            m->page_reg[3] = 0x00; /* RAM page 0 */
            printf("Boot: Phase 2 — FPGA configured (%u bytes), entering BIOS\n",
                   m->conf_bytes);
        }
        return;
    }

    /* CTC ports (0x10-0x13) */
    if (lo >= 0x10 && lo <= 0x13) {
        ctc_write(&m->ctc, lo - 0x10, data);
        return;
    }

    /* SIO ports */
    if (lo == 0x18) { sio_write_data(&m->sio, 0, data); return; }
    if (lo == 0x19) { sio_write_ctrl(&m->sio, 0, data); return; }
    if (lo == 0x1A) { sio_write_data(&m->sio, 1, data); return; }
    if (lo == 0x1B) { sio_write_ctrl(&m->sio, 1, data); return; }

    /* CMOS: port 0x1C write = address (via OUT (n),A: hi byte = A, lo = 0x1C) */
    if (lo == 0x1C) return;  /* Address write, absorbed */
    /* CMOS: port 0x1D = address write, 0x1E = data write */
    if (lo == 0x1D || lo == 0x1E) return;

    /* Port 0xEE/0xEF: FPGA config — absorb */
    if (lo == 0xEE || lo == 0xEF) return;

    /* AY ports */
    if (lo == 0x8D || lo == 0x8E) return;

    bus_port_write(&m->bus, port, data);
}

/* --- Accelerator hook --- */

int machine_accel_hook(void *ctx, u8 opcode) {
    sp_machine_t *m = (sp_machine_t *)ctx;
    if (!m->accel_enabled) return 0;

    z80_t *cpu = &m->cpu;

    switch (opcode) {
    case 0x40: m->accel_enabled = false; return 0;
    case 0x52: m->accel_size = Z80_A; return 0;

    case 0x49: { /* Fill block */
        u16 addr = Z80_HL;
        u8 val = Z80_A;
        int count = m->accel_size ? m->accel_size : 256;
        for (int i = 0; i < count; i++)
            machine_mem_write(m, addr++, val);
        Z80_HL = addr;
        return count;
    }
    case 0x5B: { /* Vertical fill */
        u16 addr = Z80_HL;
        u8 val = Z80_A;
        int count = Z80_A;
        for (int i = 0; i < count; i++) {
            machine_mem_write(m, addr, val);
            addr += 320;
        }
        return count * 2;
    }
    case 0x6D: { /* Copy row */
        u16 src = Z80_HL, dst = Z80_DE;
        int count = m->accel_size ? m->accel_size : 256;
        for (int i = 0; i < count; i++) {
            u8 v = machine_mem_read(m, src++);
            machine_mem_write(m, dst++, v);
        }
        Z80_HL = src; Z80_DE = dst;
        return count;
    }
    case 0x7F: { /* Copy vertical */
        u16 src = Z80_HL, dst = Z80_DE;
        int count = Z80_A;
        for (int i = 0; i < count; i++) {
            machine_mem_write(m, dst, machine_mem_read(m, src));
            src += 320; dst += 320;
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
    fseek(f, 0, SEEK_END);
    long sz = ftell(f);
    fseek(f, 0, SEEK_SET);

    if (sz > ROM_TOTAL_SIZE) sz = ROM_TOTAL_SIZE;
    size_t n = fread(m->rom, 1, (size_t)sz, f);
    fclose(f);
    printf("Loaded ROM: %s (%zu bytes, %zu pages)\n", path, n, n / ROM_PAGE_SIZE);
    return 0;
}

sp_machine_t *machine_create(sp_config_t *config) {
    sp_machine_t *m = calloc(1, sizeof(sp_machine_t));
    if (!m) return NULL;

    m->config = config;

    /* Allocate memory */
    m->ram = calloc(1, SP_RAM_SIZE);
    m->rom = malloc(ROM_TOTAL_SIZE);
    m->vram = calloc(1, SP_VRAM_LINES * SP_VRAM_LINE);
    m->fastram = calloc(1, FASTRAM_SIZE);
    if (!m->ram || !m->rom || !m->vram || !m->fastram) {
        fprintf(stderr, "Error: Failed to allocate memory\n");
        machine_destroy(m);
        return NULL;
    }

    /* Fill ROM with 0xFF (erased flash state) */
    memset(m->rom, 0xFF, ROM_TOTAL_SIZE);

    /* Framebuffer */
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

    memset(m->key_matrix, 0xFF, sizeof(m->key_matrix));

    /* Initialize CTC and SIO */
    ctc_init(&m->ctc);
    m->ctc.irq_callback = ctc_irq_handler;
    m->ctc.irq_ctx = m;
    sio_init(&m->sio);

    /* Load ROM */
    load_rom(m, config->rom_path);

    /* Initialize palette */
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
    free(m->fastram);
    free(m->framebuffer);
    free(m);
}

void machine_reset(sp_machine_t *m) {
    z80_reset(&m->cpu);

    /*
     * Skip Phase 1 (FPGA bitstream upload). The config loader only writes
     * bitstream data to memory-mapped FPGA port (0xFE00) — it doesn't
     * set up RAM. Real data initialization happens in BIOS (ROM page 8).
     *
     * Start directly with ROM page 8 which is the real BIOS entry point.
     */
    m->conf_loading = false;
    m->conf_bytes = 0;
    m->rom_rg = 0;
    m->rom_sys = false;
    m->cash_on = false;
    m->dos_mode = true;

    /* Post-configuration page mapping:
     * WIN0 = ROM page 8 (BIOS entry: JP 0x00B0 → DI; IM 1; init)
     * WIN1 = RAM page 5
     * WIN2 = RAM page 2
     * WIN3 = RAM page 0
     */
    m->page_reg[0] = 0x88;  /* ROM page 8 */
    m->page_reg[1] = 0x05;  /* RAM page 5 */
    m->page_reg[2] = 0x02;  /* RAM page 2 */
    m->page_reg[3] = 0x00;  /* RAM page 0 */

    /* Initialize RAM page table */
    memcpy(m->ram_pages, default_ram_pages, sizeof(default_ram_pages));

    m->port_y = 0;
    m->rgmod = 0;
    m->port_fe = 0;
    m->pn = 0;
    m->sc = 0;
    m->scroll_reg = 0;
    m->hold_x = 0;
    m->hold_y = 0;
    m->conf_mode = false;
    m->accel_enabled = false;
    m->accel_size = 0;
    m->frame_count = 0;
    m->tstates_in_frame = 0;

    /*
     * Pre-load BIOS resident code into RAM page 0.
     * The config loader would normally do this, but we skip it.
     * Copy from ROM page 8 (0x3FD0-0x3FFF) to RAM page 0 (0x3FD0-0x3FFF).
     * This contains RST vector handlers that switch ROM in/out.
     * Also copy RST 08 and RST 38 handlers.
     */
    if (m->rom) {
        u32 rom8_base = 8 * ROM_PAGE_SIZE;  /* ROM page 8 offset */
        /* Copy resident code block: ROM page 8 at 0x3FD0-0x3FFF → RAM page 0 */
        for (int i = 0x3F00; i < 0x4000; i++) {
            u8 b = m->rom[rom8_base + i];
            if (b != 0xFF) /* Skip erased flash bytes */
                m->ram[i] = b;
        }
        /* Also set up RST 08 vector in RAM page 0: JP to BIOS RST8 handler */
        /* RST 08 at 0x0008: F5 3E 00 D3 7C F1 C9 (push af, LD A,0, OUT 7C, pop af, ret) */
        /* Actually just copy the first 0x100 bytes from ROM page 8 for all RST vectors */
        for (int i = 0; i < 0x100; i++) {
            u8 b = m->rom[rom8_base + i];
            if (b != 0xFF)
                m->ram[i] = b;
        }
        printf("Boot: Pre-loaded BIOS resident code into RAM page 0\n");
    }

    /* Reset peripherals */
    ctc_reset(&m->ctc);
    sio_reset(&m->sio);

    printf("Boot: Starting BIOS from ROM page 8\n");

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

        /* Clock CTC periodically (every ~100 T-states for efficiency) */
        m->tstates_in_frame += t;
        if (m->tstates_in_frame >= 100) {
            ctc_clock(&m->ctc, (int)m->tstates_in_frame);
            m->tstates_in_frame = 0;
        }
    }

    /* VSync: generate interrupt at end of frame.
     * In IM1, RST 38 vector = 0xFF (hardware default).
     * In IM2, CTC provides the vector via irq_callback.
     * Also signal CTC channel 3 (VSync source). */
    if (m->cpu.iff1) {
        z80_irq(&m->cpu, 0xFF);
    }

    m->tstates_in_frame = 0;
    m->frame_count++;

    bus_end_frame(&m->bus);
    return executed;
}
