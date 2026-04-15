/*
 * SPEmulator — Machine State
 *
 * Implements Sprinter SP2000 architecture following MAME's sprinter_state:
 * - DCP-based port decoding
 * - update_memory() recalculates 4 memory windows from registers
 * - VRAM access via PORT_Y * 1024 + (offset & 0x3FF)
 * - Two-phase boot: conf_loading → soft_reset → normal BIOS
 */
#include "machine.h"
#include "memory/mmu.h"
#include "memory/dcp.h"
#include "video/video.h"
#include "video/palette.h"
#include "video/accel.h"
#include "input/keyboard.h"
#include "cpu/ctc.h"
#include "cpu/sio.h"
#include "debug/trace.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

/* ================================================================
 * Default RAM page table (from MAME machine_start, 64 entries)
 * Indexed by virtual port 0xC0-0xFF → ram_pages[port - 0xC0]
 * ================================================================ */
static const u8 default_ram_pages[64] = {
    /* 0xC0-0xCF: system port copies */
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    /* 0xD0-0xDF: RAM pages */
    0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17,
    0x18, 0x19, 0x1A, 0x1B, 0x1C, 0x1D, 0x1E, 0x1F,
    /* 0xE0-0xEF: ROM/system pages */
    0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41,
    0x00, 0x05, 0x02, 0x41, 0xFF, 0x00, 0x00, 0x41,
    /* 0xF0-0xFF: RAM pages */
    0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,
    0x08, 0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F,
};

/* ================================================================
 * CTC IRQ callback
 * ================================================================ */
static void ctc_irq_handler(void *ctx, int channel) {
    sp_machine_t *m = (sp_machine_t *)ctx;
    u8 vector = m->ctc.vector + (u8)(channel * 2);
    z80_irq(&m->cpu, vector);
}

/* ================================================================
 * update_memory() — recalculate all 4 memory windows
 *
 * Following MAME sprinter_state::update_memory() logic:
 * WIN0: depends on conf_loading, rom_sys, cash_on, rom_rg
 * WIN1: ram_pages[0x29] (port 0xE9 entry)
 * WIN2: ram_pages[0x2A] (port 0xEA entry)
 * WIN3: page 0x40 during starting, else complex from PN/SC/CNF
 * ================================================================ */
static void set_win_rom(sp_machine_t *m, int win, u8 rom_page) {
    rom_page &= 0x0F;
    u32 offset = (u32)rom_page * SP_PAGE_SIZE;
    if (offset + SP_PAGE_SIZE <= SP_ROM_TOTAL)
        m->win_rd[win] = &m->rom[offset];
    else
        m->win_rd[win] = m->rom; /* fallback */
    m->win_wr[win] = NULL; /* ROM: read-only */
    m->win_page[win] = 0x80 + rom_page;
}

static void set_win_ram(sp_machine_t *m, int win, u8 ram_page) {
    u32 offset = (u32)ram_page * SP_PAGE_SIZE;
    if (offset + SP_PAGE_SIZE <= SP_RAM_SIZE) {
        m->win_rd[win] = &m->ram[offset];
        m->win_wr[win] = &m->ram[offset];
    } else {
        m->win_rd[win] = m->ram;
        m->win_wr[win] = m->ram;
    }
    m->win_page[win] = ram_page;
}

static void set_win_fastram(sp_machine_t *m, int win, u8 fr_page) {
    u32 offset = (u32)(fr_page & 3) * SP_PAGE_SIZE;
    if (offset + SP_PAGE_SIZE <= SP_FASTRAM_SIZE) {
        m->win_rd[win] = &m->fastram[offset];
        m->win_wr[win] = &m->fastram[offset];
    } else {
        m->win_rd[win] = m->fastram;
        m->win_wr[win] = m->fastram;
    }
    m->win_page[win] = 0xF0 + (fr_page & 3); /* pseudo-page for fastram */
}

/* Calculate WIN3 page from registers (MAME lines 370-389) */
static u8 calc_win3_page(sp_machine_t *m) {
    u8 pg3_idx = ((~m->pn >> 7) & 1) << 5
               | 0x10
               | ((((m->sc >> 4) & 1) && !((m->cnf >> 7) & 1))
                 || (((m->cnf >> 7) & 1) && ((m->pn >> 6) & 1))) << 3
               | (m->pn & 0x07);
    /* pg3_idx is index into ram_pages, range 0x10-0x3F */
    pg3_idx &= 0x3F;
    return m->ram_pages[pg3_idx];
}

void update_memory(sp_machine_t *m) {
    /* WIN0 */
    if (m->conf_loading) {
        /* During config loading: ROM page 0x0C (config loader) */
        set_win_rom(m, 0, SP_ROM_CONF_PAGE);
    } else {
        bool pre_rom = m->rom_sys || m->cash_on;
        bool pre_cash = !m->cash_on;

        if (!pre_rom && pre_cash) {
            /* Normal ROM mode: rom_rg XOR sys_pg */
            u8 rom_page = (m->rom_rg & 0x0F) ^ (m->sys_pg ? 0 : 0x08);
            set_win_rom(m, 0, rom_page);
        } else if (pre_rom && !pre_cash) {
            /* FastRAM mode */
            set_win_fastram(m, 0, m->rom_rg & 3);
        } else {
            /* RAM mode (rom_sys=1, cash_on=0, or both) */
            /* Simplified: use ram_pages[0x28] (port 0xE8 default) */
            u8 pg0_idx = 0x28; /* This is a simplification; full calc uses PN/SC/DOS */
            set_win_ram(m, 0, m->ram_pages[pg0_idx]);
        }
    }

    /* WIN1 = ram_pages[0x29] (port 0xE9) */
    set_win_ram(m, 1, m->ram_pages[0x29]);

    /* WIN2 = ram_pages[0x2A] (port 0xEA) */
    set_win_ram(m, 2, m->ram_pages[0x2A]);

    TRACE_PAGE(m->cpu.total_tstates, m->cpu.pc.w, 0, m->win_page[0],
               m->win_wr[0] ? "RW" : "RO");
    TRACE_PAGE(m->cpu.total_tstates, m->cpu.pc.w, 1, m->win_page[1], "RW");
    TRACE_PAGE(m->cpu.total_tstates, m->cpu.pc.w, 2, m->win_page[2], "RW");

    /* WIN3: page 0x40 during starting, otherwise from ram_pages or PN/SC calc */
    if (m->starting) {
        set_win_ram(m, 3, 0x40);
    } else {
        /* Use ram_pages[0x2B] if directly set, otherwise calc from registers */
        u8 page3 = m->ram_pages[0x2B];
        if (page3 == 0xFF || page3 == 0x41) {
            /* Default/unset: calculate from PN/SC/CNF */
            page3 = calc_win3_page(m);
        }
        set_win_ram(m, 3, page3);
    }
    TRACE_PAGE(m->cpu.total_tstates, m->cpu.pc.w, 3, m->win_page[3],
               m->starting ? "START" : "RW");
}

/* ================================================================
 * Memory read/write with PORT_Y VRAM addressing
 * ================================================================ */
u8 machine_mem_read(void *ctx, u16 addr) {
    sp_machine_t *m = (sp_machine_t *)ctx;
    int win = addr >> 14;
    u16 offset = addr & 0x3FFF;
    u8 page = m->win_page[win];

    /* VRAM pages (0x50-0x5F): access via PORT_Y addressing */
    if ((page & 0xF0) == 0x50) {
        u32 vaddr = (u32)m->port_y * SP_VRAM_LINE + (offset & 0x3FF);
        if (vaddr < SP_VRAM_SIZE)
            return m->vram[vaddr];
        return 0xFF;
    }

    return m->win_rd[win] ? m->win_rd[win][offset] : 0xFF;
}

void machine_mem_write(void *ctx, u16 addr, u8 data) {
    sp_machine_t *m = (sp_machine_t *)ctx;
    int win = addr >> 14;
    u16 offset = addr & 0x3FFF;
    u8 page = m->win_page[win];

    /* During conf_loading: writes to bootstrap area → fastram + counting */
    if (m->conf_loading) {
        /* All writes during conf_loading go to fastram */
        u32 fr_offset = addr & 0xFFFF;
        if (fr_offset < SP_FASTRAM_SIZE) {
            m->fastram[fr_offset] = data;
        }
        m->conf_bytes++;

        /* After 4096+ bytes: config complete → soft reset */
        if (m->conf_bytes > 0xFFF) {
            m->conf_loading = false;
            TRACE_BOOT(m->cpu.total_tstates, m->cpu.pc.w,
                       "FPGA config complete (%u bytes), soft reset", m->conf_bytes);
            printf("Boot: FPGA config complete (%u bytes), triggering soft reset\n",
                   m->conf_bytes);
            /* Soft reset: re-initialize with conf_loading=false */
            z80_reset(&m->cpu);
            m->starting = true;
            m->conf_bytes = 0;
            update_memory(m);
            printf("Boot: Phase 2 — BIOS from ROM page %d\n",
                   m->win_page[0] & 0x0F);
        }
        return;
    }

    /* VRAM pages (0x50-0x5F): PORT_Y addressing with mode bits */
    if ((page & 0xF0) == 0x50) {
        u8 transp = (page >> 3) & 1;  /* bit 3: transparency */
        u8 shadow = (page >> 2) & 1;  /* bit 2: VRAM-only (shadow) */

        if (transp && data == 0xFF) return; /* Skip transparent bytes */

        u32 vaddr = (u32)m->port_y * SP_VRAM_LINE + (offset & 0x3FF);

        /* Write to VRAM (always unless shadow says no) */
        if (vaddr < SP_VRAM_SIZE) {
            m->vram[vaddr] = data;
            video_on_vram_write(m, vaddr, data);
        }

        /* Also write to RAM (unless VRAM-only mode) */
        if (!shadow && m->win_wr[win]) {
            m->win_wr[win][offset] = data;
        }
        return;
    }

    /* Normal RAM write */
    if (m->win_wr[win]) {
        m->win_wr[win][offset] = data;
    }
}

u8 machine_opcode_fetch(void *ctx, u16 addr) {
    sp_machine_t *m = (sp_machine_t *)ctx;
    return machine_mem_read(ctx, addr);
}

/* ================================================================
 * Port I/O — DCP-based decoding
 *
 * MAME approach: ports 0x3C/0x7C are special-cased before DCP.
 * Then DCP LUT maps the port to a virtual number.
 * Virtual ports 0xC0-0xFF → ram_pages[] writes.
 * Other virtual ports → specific hardware handlers.
 *
 * Simplified: we decode ports by lo byte matching MAME's dcp_w/dcp_r
 * switch statements directly.
 * ================================================================ */

u8 machine_port_read(void *ctx, u16 port) {
    sp_machine_t *m = (sp_machine_t *)ctx;
    u8 lo = port & 0xFF;

    /* First port read clears starting flag (MAME: dcp_r line 579) */
    if (m->starting) {
        TRACE_BOOT(m->cpu.total_tstates, m->cpu.pc.w,
                   "starting cleared by port READ %04X", (unsigned)port);
        m->starting = false;
        update_memory(m);
    }

    /* === Direct-decoded ports (not through DCP) === */

    /* CTC (Z84C15 internal: 0x10-0x13) */
    if (lo >= 0x10 && lo <= 0x13) return ctc_read(&m->ctc, lo - 0x10);

    /* SIO (Z84C15 internal: 0x18-0x1B) */
    if (lo == 0x18) return sio_read_data(&m->sio, 0);
    if (lo == 0x19) return sio_read_ctrl(&m->sio, 0);
    if (lo == 0x1A) return sio_read_data(&m->sio, 1);
    if (lo == 0x1B) return sio_read_ctrl(&m->sio, 1);

    /* CMOS/RTC: port 0x1C = data read (DCP: CMOS_DAT_RD)
     * Only treat as CMOS when high byte suggests actual CMOS access.
     * For IN A,(#1C) the port is (A<<8)|0x1C — high byte varies.
     * For IN A,(C) with BC set to specific CMOS port — also handled. */
    if (lo == 0x1C) {
        u8 val = rtc_read(&m->rtc);
        TRACE_CMOS(m->cpu.total_tstates, m->cpu.pc.w,
                   "RD addr=%02X val=%02X", m->rtc.addr_reg, val);
        return val;
    }

    /* ISA CMOS ports: IN A,(C) with BC=0xFFBD → CMOS data read */
    if (lo == 0xBD && (port & 0xFF00) == 0xFF00) {
        u8 val = rtc_read(&m->rtc);
        TRACE_CMOS(m->cpu.total_tstates, m->cpu.pc.w,
                   "ISA RD addr=%02X val=%02X", m->rtc.addr_reg, val);
        return val;
    }

    /* Port #FE: keyboard matrix */
    if (lo == 0xFE) {
        u8 result = 0xFF;
        u8 rows = ~(port >> 8);
        for (int i = 0; i < 8; i++)
            if (rows & (1 << i))
                result &= m->key_matrix[i];
        return result;
    }

    /* === DCP-decoded ports (simplified matching MAME dcp_r switch) === */

    /* System port reads: ram_pages values */
    if (lo >= 0xC0) {
        u8 idx = lo - 0xC0;
        /* Some system ports return register values */
        switch (lo) {
        case 0xC0: return m->sc;        /* 1FFD */
        case 0xC1: return m->pn;        /* 7FFD */
        case 0xC3: return m->all_mode;  /* ALL_MODE */
        case 0xC4: case 0xCC: return m->port_y;
        case 0xC5: case 0xCD: return m->rgmod;
        default:
            if (idx < 64) return m->ram_pages[idx];
            return 0xFF;
        }
    }

    /* IDE: no device present */
    if (lo >= 0x50 && lo <= 0x57) return 0x00;

    /* FDD (WD1793): not ready */
    if (lo == 0x1F) return 0x80;
    if (lo == 0x3F || lo == 0x5F || lo == 0x7F) return 0x00;

    /* AY read */
    if (lo == 0x8D || lo == 0x8E) return 0xFF;

    /* cash_on control via port read (MAME: dcp_r line 585) */
    if ((lo & 0x7F) == 0x7B) {
        m->cash_on = (port >> 7) & 1;
        update_memory(m);
        return 0xFF;
    }

    {
        u8 val = bus_port_read(&m->bus, port);
        TRACE_PORT_R(m->cpu.total_tstates, m->cpu.pc.w, port, val);
        return val;
    }
}

void machine_port_write(void *ctx, u16 port, u8 data) {
    sp_machine_t *m = (sp_machine_t *)ctx;
    u8 lo = port & 0xFF;
    TRACE_PORT_W(m->cpu.total_tstates, m->cpu.pc.w, port, data);

    /* During starting, ALL I/O writes are ignored (MAME: dcp_w returns immediately).
     * The starting flag is only cleared by port READS, not writes.
     * This matches the real DCP hardware behavior. */
    if (m->starting) {
        TRACE_BOOT(m->cpu.total_tstates, m->cpu.pc.w,
                   "WRITE %04X=%02X IGNORED (starting)",
                   (unsigned)port, (unsigned)data);
        return;
    }

    /* === Special-cased ports (before DCP): 0x3C/0x7C === */
    if ((lo & 0xBF) == 0x3C) {
        /* Port 0x3C: rom_sys=1 (RAM/FastRAM at WIN0)
         * Port 0x7C: rom_sys=0 (ROM at WIN0) */
        m->rom_sys = !((lo >> 6) & 1);  /* bit6: 0→rom_sys=1, 1→rom_sys=0 */
        if (!(data & 0x02))
            m->sys_pg = ((m->rom_rg >> 4) & 1) || (data & 1);
        update_memory(m);
        return;
    }

    /* Port 0x5C: ROM register (only when rom_sys=0) */
    if (lo == 0x5C && !m->rom_sys) {
        m->rom_rg = data;
        m->sys_pg |= (m->rom_rg >> 4) & 1;
        update_memory(m);
        return;
    }

    /* === Direct-decoded ports === */

    /* CTC */
    if (lo >= 0x10 && lo <= 0x13) { ctc_write(&m->ctc, lo - 0x10, data); return; }
    /* SIO */
    if (lo == 0x18) { sio_write_data(&m->sio, 0, data); return; }
    if (lo == 0x19) { sio_write_ctrl(&m->sio, 0, data); return; }
    if (lo == 0x1A) { sio_write_data(&m->sio, 1, data); return; }
    if (lo == 0x1B) { sio_write_ctrl(&m->sio, 1, data); return; }
    /* CMOS ports (DCP: 0x1C=DAT_RD, 0x1D=ADR_WR, 0x1E=DAT_WR)
     * Port 0x1C is CMOS_DAT_RD — writes to it are NOT CMOS operations.
     * The BIOS uses OUT(#1C),A with various A values for DCP page config,
     * not for CMOS access. CMOS address is set via port 0x1D only.
     * CMOS data writes go through port 0x1E. */
    if (lo == 0x1C) {
        /* NOT a CMOS operation — DCP page configuration or no-op.
         * On real hardware, OUT(#1C),A goes through DCP which may
         * reconfigure memory pages based on the full 16-bit port address.
         * TODO: implement DCP page switching for different A values. */
        TRACE(TR_PORT_W, m->cpu.total_tstates, m->cpu.pc.w,
              "PW %04X=%02X (port 1C write, not CMOS)", (unsigned)port, (unsigned)data);
        return;
    }
    if (lo == 0x1D) {
        TRACE_CMOS(m->cpu.total_tstates, m->cpu.pc.w,
                   "ADR=%02X", data);
        rtc_write_addr(&m->rtc, data);
        return;
    }
    if (lo == 0x1E) {
        TRACE_CMOS(m->cpu.total_tstates, m->cpu.pc.w,
                   "WR addr=%02X val=%02X", m->rtc.addr_reg, data);
        rtc_write_data(&m->rtc, data);
        return;
    }

    /* ISA CMOS ports: OUT(C),A with BC=0xDFBD → address, 0xBFBD → data write */
    if (lo == 0xBD) {
        u8 hi = (port >> 8) & 0xFF;
        if (hi == 0xDF || (hi & 0xE0) == 0xC0) {
            /* CMOS_AWR: address write */
            TRACE_CMOS(m->cpu.total_tstates, m->cpu.pc.w,
                       "ISA ADR=%02X", data);
            rtc_write_addr(&m->rtc, data);
            return;
        }
        if (hi == 0xBF || (hi & 0xE0) == 0xA0) {
            /* CMOS_DWR: data write */
            TRACE_CMOS(m->cpu.total_tstates, m->cpu.pc.w,
                       "ISA WR addr=%02X val=%02X", m->rtc.addr_reg, data);
            rtc_write_data(&m->rtc, data);
            return;
        }
    }
    /* Port #FE */
    if (lo == 0xFE) { m->port_fe = data; return; }

    /* === Sprinter page register ports (DCP-decoded in real HW) ===
     * Ports 0x82/0xA2 are below 0xC0, need explicit handlers.
     * They map through DCP to modify WIN0/WIN1 page registers.
     * Ports 0xC2/0xE2 are in the 0xC0+ system port range and are
     * handled by the system port default handler via ram_pages[lo-0xC0].
     * update_memory() reads from ram_pages[0x28-0x2B] for WIN0-WIN3. */
    if (lo == 0x82) { m->ram_pages[0x28] = data; update_memory(m); return; } /* WIN0 (when in RAM mode) */
    if (lo == 0xA2) { m->ram_pages[0x29] = data; update_memory(m); return; } /* WIN1 */

    /* === DCP-decoded system ports 0xC0-0xFF → ram_pages writes === */
    if (lo >= 0xC0) {
        u8 idx = lo - 0xC0;
        switch (lo) {
        case 0xC0: /* 1FFD (SC register) */
            m->sc = data;
            update_memory(m);
            return;
        case 0xC1: /* 7FFD (PN register) */
            m->pn = data;
            update_memory(m);
            return;
        case 0xC2: /* Border / ZX video */
            m->port_fe = (m->port_fe & 0xF8) | (data & 0x07);
            return;
        case 0xC3: /* ALL_MODE */
            m->all_mode = data;
            return;
        case 0xC4: case 0xCC: /* PORT_Y */
            m->port_y = data;
            return;
        case 0xC5: case 0xCD: /* RGMOD */
            m->rgmod = data;
            return;
        case 0xC6: case 0xCE: /* CNF/SYS */
            m->ram_sys = !((lo >> 6) & 1);
            if (data & 0x04) m->cnf = data;
            if (data & 0x02) m->turbo = data & 1;
            else m->arom16 = data & 1;
            update_memory(m);
            return;
        case 0xCB: /* Scroll */
            m->scroll_reg = data;
            m->hold_x = (i16)((7 - (data & 0x0F)) * 2);
            m->hold_y = (i16)(7 - (data >> 4));
            return;
        default:
            /* All other 0xC0-0xFF: write to ram_pages table */
            if (idx < 64) {
                m->ram_pages[idx] = data;
                update_memory(m);
            }
            return;
        }
    }

    /* Port 0x8F: ROM/FastRAM page register (alternate) */
    if (lo == 0x8F) {
        m->rom_rg = data;
        update_memory(m);
        return;
    }

    /* Port 0x89: PORT_Y (also used for COVOX mode) */
    if (lo == 0x89) { m->port_y = data; return; }

    /* FPGA config ports */
    if (lo == 0xEE || lo == 0xEF) return;
    /* IDE write ports */
    if (lo >= 0x50 && lo <= 0x57) return;
    /* FDD write ports */
    if (lo == 0x1F || lo == 0x3F || lo == 0x5F || lo == 0x7F || lo == 0xFF) return;
    /* AY ports */
    if (lo == 0x8D || lo == 0x8E) return;

    bus_port_write(&m->bus, port, data);
}

/* ================================================================
 * Accelerator hook
 * ================================================================ */
int machine_accel_hook(void *ctx, u8 opcode) {
    sp_machine_t *m = (sp_machine_t *)ctx;
    if (!m->accel_enabled) return 0;
    z80_t *cpu = &m->cpu;

    switch (opcode) {
    case 0x40: m->accel_enabled = false; return 0;
    case 0x52: m->accel_size = Z80_A; return 0;
    case 0x49: { /* Fill */
        u16 a = Z80_HL; u8 v = Z80_A;
        int n = m->accel_size ? m->accel_size : 256;
        for (int i = 0; i < n; i++) machine_mem_write(m, a++, v);
        Z80_HL = a; return n;
    }
    case 0x5B: { /* Vertical fill */
        u16 a = Z80_HL; u8 v = Z80_A; int n = Z80_A;
        for (int i = 0; i < n; i++) { machine_mem_write(m, a, v); a += 320; }
        return n * 2;
    }
    case 0x6D: { /* Copy row */
        u16 s = Z80_HL, d = Z80_DE;
        int n = m->accel_size ? m->accel_size : 256;
        for (int i = 0; i < n; i++) machine_mem_write(m, d++, machine_mem_read(m, s++));
        Z80_HL = s; Z80_DE = d; return n;
    }
    case 0x7F: { /* Copy vertical */
        u16 s = Z80_HL, d = Z80_DE; int n = Z80_A;
        for (int i = 0; i < n; i++) {
            machine_mem_write(m, d, machine_mem_read(m, s));
            s += 320; d += 320;
        }
        return n * 2;
    }
    }
    return 0;
}

/* ================================================================
 * Machine lifecycle
 * ================================================================ */

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
    if (sz > (long)SP_ROM_TOTAL) sz = SP_ROM_TOTAL;
    size_t n = fread(m->rom, 1, (size_t)sz, f);
    fclose(f);
    printf("Loaded ROM: %s (%zu bytes, %zu pages)\n", path, n, n / SP_PAGE_SIZE);
    return 0;
}

sp_machine_t *machine_create(sp_config_t *config) {
    sp_machine_t *m = calloc(1, sizeof(sp_machine_t));
    if (!m) return NULL;
    m->config = config;

    /* Allocate memory */
    m->ram = calloc(1, SP_RAM_SIZE);
    m->rom = malloc(SP_ROM_TOTAL);
    m->vram = calloc(1, SP_VRAM_LINES * SP_VRAM_LINE);
    m->fastram = calloc(1, SP_FASTRAM_SIZE);
    if (!m->ram || !m->rom || !m->vram || !m->fastram) {
        fprintf(stderr, "Error: Failed to allocate memory\n");
        machine_destroy(m);
        return NULL;
    }
    memset(m->rom, 0xFF, SP_ROM_TOTAL);

    /* Framebuffer */
    m->fb_width = SP_VIS_W;
    m->fb_height = SP_VIS_H;
    m->framebuffer = calloc((size_t)m->fb_width * m->fb_height, sizeof(u32));
    if (!m->framebuffer) {
        machine_destroy(m);
        return NULL;
    }

    /* Initialize DCP port decode table */
    dcp_init(&m->dcp);

    /* Initialize bus, CPU, peripherals */
    bus_init(&m->bus, m);
    z80_init(&m->cpu);
    m->cpu.mem_read = machine_mem_read;
    m->cpu.mem_write = machine_mem_write;
    m->cpu.port_read = machine_port_read;
    m->cpu.port_write = machine_port_write;
    m->cpu.opcode_fetch = machine_opcode_fetch;
    m->cpu.callback_ctx = m;
    m->cpu.accel_hook = machine_accel_hook;
    m->cpu.accel_ctx = m;

    ctc_init(&m->ctc);
    m->ctc.irq_callback = ctc_irq_handler;
    m->ctc.irq_ctx = m;
    sio_init(&m->sio);
    rtc_init(&m->rtc);

    memset(m->key_matrix, 0xFF, sizeof(m->key_matrix));

    /* Load ROM */
    load_rom(m, config->rom_path);

    /* Initialize palette */
    palette_init_default(m);

    /* Clock */
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

    /* Boot state: Phase 1 = config loading */
    m->conf_loading = true;
    m->starting = true;
    m->conf_bytes = 0;

    /* Register defaults (MAME machine_reset) */
    m->rom_rg = 0x00;
    m->rom_sys = false;
    m->cash_on = false;
    m->dos_mode = true; /* DOS off */
    m->ram_sys = false;
    m->sys_pg = 0;
    m->arom16 = false;
    m->nmi_ena = true; /* NMI disabled */
    m->pn = 0x00;
    m->sc = 0x00;
    m->cnf = 0x00;

    /* Initialize RAM page table */
    memcpy(m->ram_pages, default_ram_pages, sizeof(default_ram_pages));

    /* RTC/CMOS reset */
    rtc_reset(&m->rtc);

    /* Video */
    m->port_y = 0;
    m->rgmod = 0;
    m->port_fe = 0;
    m->all_mode = 0;
    m->scroll_reg = 0;
    m->hold_x = 0;
    m->hold_y = 0;
    m->conf_mode = false;
    m->accel_enabled = false;
    m->accel_size = 0;
    m->frame_count = 0;
    m->tstates_in_frame = 0;

    /* Reset peripherals */
    ctc_reset(&m->ctc);
    sio_reset(&m->sio);

    /* Calculate initial memory mapping */
    update_memory(m);

    printf("Boot: Phase 1 — config loader from ROM page %d\n",
           m->win_page[0] & 0x0F);

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

    /* Clock CTC once per frame */
    ctc_clock(&m->ctc, frame_ts);

    /* Tick RTC (UIP simulation) */
    rtc_tick(&m->rtc);

    /* VSync IRQ (IM1: vector 0xFF) */
    if (m->cpu.iff1 && !m->cpu.irq_pending) {
        TRACE_IRQ(m->cpu.total_tstates, m->cpu.pc.w, 0xFF);
        z80_irq(&m->cpu, 0xFF);
    }

    m->tstates_in_frame = 0;
    m->frame_count++;

    bus_end_frame(&m->bus);
    return executed;
}
