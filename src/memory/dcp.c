/*
 * SPEmulator — DCP (Dynamic Configuration Port) Decoder
 *
 * The DCP table lives in RAM at page 0x40 (offset DCP_RAM_PAGE * SP_PAGE_SIZE).
 * It is initialized with defaults by dcp_init_default_ram(), then overwritten
 * by the BIOS during the starting phase via WIN3 memory writes (0xC000-0xFFFF).
 *
 * DCP key formula (14 bits):
 *   key = (CNF[4:3]<<12)|(PN[5]<<11)|(DOS<<10)|(RW<<9)
 *       | (A[15:14]<<7)|(A[13]<<4)|(A[7]<<3)|(port&0x67)
 */
#include "memory/dcp.h"
#include "machine.h"
#include <string.h>

/* DCP table offset in RAM */
#define DCP_RAM_OFFSET ((u32)DCP_RAM_PAGE * SP_PAGE_SIZE)

/* Compute default DCPP for a given port address and direction.
 * Used to pre-populate the DCP table before BIOS init. */
static u8 compute_default_dcpp(u16 port, int is_read)
{
    u8 lo = port & 0xFF;
    u8 hi = (port >> 8) & 0xFF;

    /* System ports lo=0xC0-0xFF: dcpp = lo */
    if (lo >= 0xC0) return lo;

    /* Port 0xFE: keyboard (read) or border (write) */
    if (lo == 0xFE) return is_read ? 0x40 : 0xC2;

    /* CMOS direct ports (low hi byte only) */
    if (lo == 0x1C && is_read  && hi < 0x80) return 0x1C;
    if (lo == 0x1D && !is_read && hi < 0x80) return 0x1D;
    if (lo == 0x1E && !is_read && hi < 0x80) return 0x1E;

    /* ISA CMOS: lo=0xBD with specific hi bytes */
    if (lo == 0xBD) {
        if ( is_read && hi == 0xFF) return 0x1C;
        if (!is_read && hi == 0xDF) return 0x1D;
        if (!is_read && hi == 0xBF) return 0x1E;
    }

    /* AY (ZX128 standard): lo=0xFD with hi=0xFF or 0xBF */
    if (lo == 0xFD) {
        if (hi == 0xFF) return is_read ? 0x52 : 0x90;  /* addr wr / data rd */
        if (hi == 0xBF && !is_read) return 0x91;       /* data write */
    }

    /* WIN0/WIN1 page registers (lo byte only, low hi) */
    if (lo == 0x82 && !is_read && hi < 0x80) return 0xE8;
    if (lo == 0xA2 && !is_read && hi < 0x80) return 0xE9;

    /* IDE CS0: lo=0x50-0x57 */
    if (lo >= 0x50 && lo <= 0x57) return 0x20 + (lo - 0x50);

    /* FDD (WD1793) */
    if (lo == 0x1F) return 0x10;
    if (lo == 0x3F) return 0x11;
    if (lo == 0x5F) return 0x12;
    if (lo == 0x7F) return 0x13;

    /* COVOX */
    if (lo == 0x88) return 0x88;
    if (lo == 0x89) return 0x89;

    /* ROM/fastram page */
    if (lo == 0x8F) return 0x8F;

    return 0x00;  /* no port */
}

void dcp_init(sp_dcp_t *dcp)
{
    (void)dcp;
    /* DCP table is in RAM — call dcp_init_default_ram(m->ram) from machine_reset */
}

void dcp_reset(sp_dcp_t *dcp)
{
    (void)dcp;
}

void dcp_init_default_ram(u8 *ram)
{
    /* Clear entire DCP table region */
    memset(ram + DCP_RAM_OFFSET, 0x00, DCP_TABLE_SIZE);

    /* Fill default DCPP values for all combinations of CNF/PN/DOS/RW and port */
    for (int cnf43 = 0; cnf43 < 4; cnf43++) {
        for (int pn5 = 0; pn5 < 2; pn5++) {
            for (int dos = 0; dos < 2; dos++) {
                for (int rw = 0; rw < 2; rw++) {
                    for (u32 p = 0; p <= 0xFFFF; p++) {
                        u16 port = (u16)p;
                        u16 offset = (u16)(
                            (cnf43 << 12) | (pn5 << 11) | (dos << 10) | (rw << 9) |
                            (((port >> 14) & 3) << 7) | (((port >> 13) & 1) << 4) |
                            (((port >> 7)  & 1) << 3) | (port & 0x67)
                        );
                        u8 dcpp = compute_default_dcpp(port, rw);
                        if (dcpp)
                            ram[DCP_RAM_OFFSET + offset] = dcpp;
                    }
                }
            }
        }
    }
}

u16 dcp_compute_offset(sp_machine_t *m, u16 port, int is_read)
{
    u16 offset =
        (((m->cnf >> 3) & 3) << 12) |
        (((m->pn  >> 5) & 1) << 11) |
        ((m->dos_mode ? 1 : 0) << 10) |
        ((is_read ? 1 : 0) << 9) |
        (((port >> 14) & 3) << 7) |
        (((port >> 13) & 1) << 4) |
        (((port >>  7) & 1) << 3) |
        (port & 0x67);

    return offset & (DCP_TABLE_SIZE - 1);
}

u8 dcp_lookup_port(sp_dcp_t *dcp, sp_machine_t *m, u16 port, int is_read)
{
    (void)dcp;
    u16 offset = dcp_compute_offset(m, port, is_read);
    return m->ram[DCP_RAM_OFFSET + offset];
}
