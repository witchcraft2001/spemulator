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

/* Calculate WIN3 page index and page from registers (MAME lines 373-374) */
static u8 calc_pg3_idx(sp_machine_t *m) {
    u8 idx = (u8)(
        (((~m->pn >> 7) & 1) << 5) |
        0x10 |
        ((((m->sc >> 4) & 1) && !((m->cnf >> 7) & 1)) ||
         (((m->cnf >> 7) & 1) && ((m->pn >> 6) & 1))) << 3 |
        (m->pn & 0x07)
    );
    return idx & 0x3F;
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
            /* RAM/system mode — MAME update_memory() else branch */
            bool sc0   = (m->sc >> 0) & 1;
            bool sc_lc = !(sc0 && m->ram_sys);
            u8   dos   = m->dos_mode ? 1 : 0;
            u8   spr_  = ((m->sc >> 1) & 1) ? 0
                       : (u8)((dos << 1) | (((m->pn >> 4) & 1) || !dos));
            u8   pg0   = (u8)(0x20
                | ((sc0 || !m->ram_sys || !m->nmi_ena) << 3)
                | ((m->arom16 && !(sc0 && m->ram_sys)) << 2)
                | ((((spr_ >> 1) & 1 && sc_lc) || !m->ram_sys || !m->nmi_ena) << 1)
                | (((spr_ >> 0) & 1 && sc_lc) || !m->ram_sys || !m->nmi_ena));
            if (sc0 && m->ram_sys) {
                m->win_rd[0]  = NULL;
                m->win_wr[0]  = NULL;
                m->win_page[0] = 0xFF;
            } else {
                set_win_ram(m, 0, m->ram_pages[pg0]);
                m->win_wr[0] = NULL;  /* read-only */
            }
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

    /* WIN3: page 0x40 during starting, otherwise from ram_pages indexed by pg3_idx */
    m->pg3_idx = calc_pg3_idx(m);
    if (m->starting) {
        set_win_ram(m, 3, 0x40);
    } else {
        set_win_ram(m, 3, m->ram_pages[m->pg3_idx]);
    }
    TRACE_PAGE(m->cpu.total_tstates, m->cpu.pc.w, 3, m->win_page[3],
               m->starting ? "START" : "RW");
}

/* ================================================================
 * Soft reset: resets CPU and control registers only.
 * Preserves RAM pages and DCP table (page 0x40) for warm restart.
 * ================================================================ */
static void machine_soft_reset(sp_machine_t *m) {
    z80_reset(&m->cpu);
    m->starting = true;
    m->rom_rg  = 0x00;
    m->rom_sys = false;
    m->cash_on = false;
    m->dos_mode = true;
    m->ram_sys = false;
    m->sys_pg  = 0;
    m->arom16  = false;
    m->nmi_ena = true;
    m->pn = 0x00;
    m->sc = 0x00;
    m->cnf = 0x00;
    m->isa_addr_ext = 0;
    update_memory(m);
    TRACE_BOOT(m->cpu.total_tstates, 0, "soft reset (RAM/DCP preserved)");
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

    /* Warm restart trigger: sc=0x10, WIN3=page 0xA0, write to WIN3.
     * MAME: if (Bank==3 && sc==0x10 && pages[3]==(BANK_RAM_MASK|0xa0)) soft_reset */
    if (win == 3 && m->sc == 0x10 && m->win_page[3] == 0xA0) {
        TRACE_BOOT(m->cpu.total_tstates, m->cpu.pc.w,
                   "warm restart trigger (WIN3 write sc=0x10 page=0xA0)");
        machine_soft_reset(m);
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

    /* First port read clears starting flag */
    if (m->starting) {
        TRACE_BOOT(m->cpu.total_tstates, m->cpu.pc.w,
                   "starting cleared by port READ %04X", (unsigned)port);
        m->starting = false;
        update_memory(m);
        /* Dump DCPP for key ports after DCP_INIT */
        static const struct { u16 port; int rw; const char *name; } key_ports[] = {
            {0x0082, 0, "PAGE0 wr"}, {0x00A2, 0, "PAGE1 wr"},
            {0x00C2, 0, "PAGE2 wr"}, {0x00E2, 0, "PAGE3 wr"},
            {0x00E2, 1, "PAGE3 rd"},
            {0x1FFD, 0, "SC wr"},    {0x7FFD, 0, "PN wr"},
            {0xFFBD, 1, "CMOS rd"},  {0xDFBD, 0, "CMOS adr"}, {0xBFBD, 0, "CMOS dat"},
        };
        for (int i = 0; i < (int)(sizeof(key_ports)/sizeof(key_ports[0])); i++) {
            u16 off = dcp_compute_offset(m, key_ports[i].port, key_ports[i].rw);
            u8 dcpp = m->ram[0x100000 + off];
            printf("  DCPP[%s port=%04X off=%04X] = %02X\n",
                   key_ports[i].rw?"R":"W", key_ports[i].port, off, dcpp);
        }
    }

    /* Z84C15 internal: CTC and SIO are decoded before DCP */
    if (lo >= 0x10 && lo <= 0x13) return ctc_read(&m->ctc, lo - 0x10);
    if (lo == 0x18) return sio_read_data(&m->sio, 0);
    if (lo == 0x19) return sio_read_ctrl(&m->sio, 0);
    if (lo == 0x1A) return sio_read_data(&m->sio, 1);
    if (lo == 0x1B) return sio_read_ctrl(&m->sio, 1);

    /* cash_on: special-cased before DCP */
    if ((lo & 0x7F) == 0x7B) {
        m->cash_on = (port >> 7) & 1;
        update_memory(m);
    }

    /* DCP lookup */
    u8 dcpp = dcp_lookup_port(&m->dcp, m, port, 1);
    u8 data = 0xFF;

    switch (dcpp) {
    case 0x00:  /* no port */
        break;

    /* FDD (WD1793) */
    case 0x10: data = 0x80; break;  /* status: not ready */
    case 0x11: case 0x12: case 0x13: case 0x15:
        data = 0x00; break;

    /* CMOS/RTC */
    case 0x1C:
        data = rtc_read(&m->rtc);
        TRACE_CMOS(m->cpu.total_tstates, m->cpu.pc.w,
                   "RD addr=%02X val=%02X", m->rtc.addr_reg, data);
        break;

    /* IDE: no device */
    case 0x20: case 0x21: case 0x22: case 0x23:
    case 0x24: case 0x25: case 0x26: case 0x27:
    case 0x28: case 0x29:
        data = 0x00;
        break;

    /* Keyboard */
    case 0x40: {
        u8 result = 0xFF;
        u8 rows = (u8)(~(port >> 8));
        for (int i = 0; i < 8; i++)
            if (rows & (1 << i))
                result &= m->key_matrix[i];
        data = result;
        break;
    }

    /* AY read */
    case 0x52:
        data = 0xFF;
        break;

    /* System ports: specific registers */
    case 0xC0: case 0xC8:
        data = m->sc; break;
    case 0xC1: case 0xC9:
        data = m->pn; break;
    case 0xC3:
        data = m->all_mode; break;
    case 0xC4: case 0xCC:
        data = m->port_y; break;
    case 0xC5: case 0xCD:
        data = m->rgmod; break;

    default:
        if (dcpp >= 0xC0 && dcpp <= 0xEF) {
            data = m->ram_pages[dcpp - 0xC0];
        } else if (dcpp >= 0xF0) {
            /* WIN3 page read */
            data = m->ram_pages[m->pg3_idx];
        } else {
            data = bus_port_read(&m->bus, port);
        }
        break;
    }

    TRACE_PORT_R(m->cpu.total_tstates, m->cpu.pc.w, port, data);
    return data;
}

void machine_port_write(void *ctx, u16 port, u8 data) {
    sp_machine_t *m = (sp_machine_t *)ctx;
    u8 lo = port & 0xFF;
    TRACE_PORT_W(m->cpu.total_tstates, m->cpu.pc.w, port, data);

    /* During starting, ALL I/O writes are ignored */
    if (m->starting) {
        TRACE_BOOT(m->cpu.total_tstates, m->cpu.pc.w,
                   "WRITE %04X=%02X IGNORED (starting)",
                   (unsigned)port, (unsigned)data);
        return;
    }

    /* Special-cased before DCP: ROM control ports 0x3C/0x7C */
    if ((lo & 0xBF) == 0x3C) {
        m->rom_sys = !((lo >> 6) & 1);
        if (!(data & 0x02))
            m->sys_pg = ((m->rom_rg >> 4) & 1) || (data & 1);
        update_memory(m);
        return;
    }
    if (lo == 0x5C && !m->rom_sys) {
        m->rom_rg = data;
        m->sys_pg |= (m->rom_rg >> 4) & 1;
        update_memory(m);
        return;
    }

    /* Z84C15 internal: CTC and SIO before DCP */
    if (lo >= 0x10 && lo <= 0x13) { ctc_write(&m->ctc, lo - 0x10, data); return; }
    if (lo == 0x18) { sio_write_data(&m->sio, 0, data); return; }
    if (lo == 0x19) { sio_write_ctrl(&m->sio, 0, data); return; }
    if (lo == 0x1A) { sio_write_data(&m->sio, 1, data); return; }
    if (lo == 0x1B) { sio_write_ctrl(&m->sio, 1, data); return; }

    /* DCP lookup */
    u8 dcpp = dcp_lookup_port(&m->dcp, m, port, 0);

    /* System ports 0xC0-0xEF: always write to ram_pages */
    if (dcpp >= 0xC0 && dcpp <= 0xEF)
        m->ram_pages[dcpp - 0xC0] = data;

    switch (dcpp) {
    case 0x00:  /* no port */
        break;

    /* FDD */
    case 0x10: case 0x11: case 0x12: case 0x13: case 0x14:
    case 0x16: case 0x17:
        break;  /* TODO: FDD */

    /* ISA control */
    case 0x1B:
        m->isa_addr_ext = data & 0x3F;
        break;

    /* CMOS/RTC */
    case 0x1D:
        TRACE_CMOS(m->cpu.total_tstates, m->cpu.pc.w, "ADR=%02X", data);
        rtc_write_addr(&m->rtc, data);
        break;
    case 0x1E:
        TRACE_CMOS(m->cpu.total_tstates, m->cpu.pc.w,
                   "WR addr=%02X val=%02X", m->rtc.addr_reg, data);
        rtc_write_data(&m->rtc, data);
        break;

    /* IDE: no device */
    case 0x20: case 0x21: case 0x22: case 0x23:
    case 0x24: case 0x25: case 0x26: case 0x27:
    case 0x28: case 0x29:
        break;

    /* Soft reset (BIOS reload) */
    case 0x2E:
        m->conf_loading = true;
        machine_soft_reset(m);
        break;

    /* COVOX / PORT_Y */
    case 0x88: case 0x89:
        m->port_y = data;
        break;

    /* ROM/fastram page */
    case 0x8F:
        m->rom_rg = data;
        m->sys_pg = (u8)(((m->rom_rg >> 4) & 1) || (data & 1));
        update_memory(m);
        break;

    /* AY address write */
    case 0x8D: case 0x90:
        break;  /* TODO: AY */

    /* AY data write */
    case 0x8E: case 0x91:
        break;  /* TODO: AY */

    /* System port handlers */
    case 0xC0: case 0xC8:   /* SC (1FFD) */
        m->sc = data;
        update_memory(m);
        break;
    case 0xC1: case 0xC9:   /* PN (7FFD) */
        m->pn = data;
        update_memory(m);
        break;
    case 0xC2:              /* Border / ZX ULA */
        m->port_fe = (m->port_fe & 0xF8) | (data & 0x07);
        break;
    case 0xC3:              /* ALL_MODE */
        m->all_mode = data;
        break;
    case 0xCB:              /* Scroll */
        m->scroll_reg = data;
        m->hold_x = (i16)((7 - (data & 0x0F)) * 2);
        m->hold_y = (i16)(7 - (data >> 4));
        break;
    case 0xC4: case 0xCC:   /* PORT_Y */
        m->port_y = data;
        break;
    case 0xC5: case 0xCD:   /* RGMOD */
        m->rgmod = data;
        break;
    case 0xC6: case 0xCE:   /* CNF/SYS */
        m->ram_sys = !((lo >> 6) & 1);
        if (data & 0x02) m->turbo = data & 1;
        else m->arom16 = data & 1;
        if (data & 0x04) m->cnf = data;
        update_memory(m);
        break;
    case 0xC7: case 0xCF:   /* alternate accelerator */
        break;

    /* RAM page registers: ram_pages already written above, just update memory */
    case 0xD0: case 0xD1: case 0xD2: case 0xD3:
    case 0xD4: case 0xD5: case 0xD6: case 0xD7:
    case 0xD8: case 0xD9: case 0xDA: case 0xDB:
    case 0xDC: case 0xDD: case 0xDE: case 0xDF:
    case 0xE0: case 0xE1: case 0xE2: case 0xE3:
    case 0xE4: case 0xE5: case 0xE6: case 0xE7:
    case 0xE8: case 0xE9: case 0xEA: case 0xEB:
    case 0xEC: case 0xED: case 0xEE: case 0xEF:
        update_memory(m);
        break;

    /* WIN3 page register */
    case 0xF0: case 0xF1: case 0xF2: case 0xF3:
    case 0xF4: case 0xF5: case 0xF6: case 0xF7:
    case 0xF8: case 0xF9: case 0xFA: case 0xFB:
    case 0xFC: case 0xFD: case 0xFE: case 0xFF:
        m->ram_pages[m->pg3_idx] = data;
        update_memory(m);
        break;

    default:
        bus_port_write(&m->bus, port, data);
        break;
    }
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

    /* Initialize DCP stub (table in RAM, populated in machine_reset) */
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

    /* Initialize DCP table in RAM page 0x40 with default port mappings */
    dcp_init_default_ram(m->ram);

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
