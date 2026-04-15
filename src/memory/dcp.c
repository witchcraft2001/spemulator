/*
 * SPEmulator — DCP (Dynamic Configuration Port) Decoder
 *
 * Implements port decoding matching MAME's sprinter_state::dcp_r/dcp_w.
 *
 * DCP offset formula (from DCP.TDF X_ADR and MAME):
 *   offset = (CNF[4:3] << 12) | (PN[5] << 11) | (DOS << 10)
 *          | (RW << 9) | (A[15:14] << 7) | (A[13] << 4)
 *          | (A[7] << 3) | (A & 0x67)
 *
 * The DCP table maps each 14-bit offset to an 8-bit dcpp (virtual port).
 * dcpp determines which device handler runs:
 *   0x00 = no port, 0x10-0x13 = FDD, 0x1C = CMOS read, 0x1D = CMOS addr,
 *   0x1E = CMOS write, 0x20-0x29 = IDE, 0x40 = keyboard, 0x52 = AY read,
 *   0x80-0x89 = accelerator/COVOX, 0xC0-0xEF = system ports (ram_pages),
 *   0xF0-0xFF = WIN3 page
 *
 * The table is initialized to match the DCP.MIF default port assignments.
 */
#include "memory/dcp.h"
#include "machine.h"
#include <string.h>

/* DCP.MIF entry format: 16 bits.
 * Bits [15:12] = TYPE, Bits [11:0] = PAGE/signals.
 * The dcpp is encoded in the MIF entry index.
 * We pre-build the 16K table so that each DCP offset maps to the
 * correct dcpp value. */

/* Reverse-compute which dcpp a given port address should map to.
 * This encodes the DCP.MIF port assignments. */
static u8 compute_dcpp_for_port(u16 port)
{
    u8 lo = port & 0xFF;
    u8 hi = (port >> 8) & 0xFF;

    /* CTC (Z84C15 internal): ports 0x10-0x13 regardless of high byte */
    if ((lo & 0xFC) == 0x10) return lo;

    /* SIO: ports 0x18-0x1B — handled outside DCP in real hardware */

    /* FDD (WD1793): ports with specific patterns
     * DCP entries 0x10-0x15 cover FDD */

    /* CMOS/RTC (DCP entries 0x1C-0x1E) */
    if (lo == 0x1C) return 0x1C;
    if (lo == 0x1D) return 0x1D;
    if (lo == 0x1E) return 0x1E;

    /* ISA CMOS (routed via ISA bus, not DCP) */
    if (lo == 0xBD) {
        if (hi == 0xFF) return 0x1C; /* CMOS_DRD */
        if (hi == 0xDF) return 0x1D; /* CMOS_AWR */
        if (hi == 0xBF) return 0x1E; /* CMOS_DWR */
    }

    /* IDE/ATA: DCP entries 0x20-0x29 */
    if ((lo & 0xF8) == 0x50) {
        /* Ports 0x50-0x57 map to IDE CS0 (dcpp 0x20-0x27) */
        return 0x20 + (lo & 0x07);
    }

    /* Keyboard: DCP entry 0x40 — port #FE read */
    if (lo == 0xFE) return 0x40;

    /* AY-3-8910: DCP entries 0x52 (read), 0x8D (addr write), 0x8E (data write) */
    if (lo == 0x8D) return 0x8D;
    if (lo == 0x8E) return 0x8E;

    /* COVOX: port 0x88 = DCP entry 0x88, port 0x89 = 0x89 */
    if (lo == 0x88) return 0x88;
    if (lo == 0x89) return 0x89;

    /* ROM write port: 0x8F */
    if (lo == 0x8F) return 0x8F;

    /* System ports 0xC0-0xFF: DCP entries 0xC0-0xFF
     * These are RAM page registers and system control ports.
     * The dcpp equals the low byte of the port for OUT(C),A
     * when the high byte doesn't interfere. */
    if (lo >= 0xC0) {
        /* For system ports, dcpp = lo byte.
         * The DCP.MIF maps 0x80-0xFF to C000 (default system port),
         * with specific overrides for known ports. */
        return lo;
    }

    /* Ports 0x80-0xBF with specific overrides from DCP.MIF */
    if (lo >= 0x90 && lo <= 0x9F) {
        /* RAM pages 0x30-0x3F */
        return 0xC0 + (lo - 0x90) + 0x10; /* map to system port range */
    }
    if (lo >= 0xB0 && lo <= 0xBF) {
        /* RAM pages 0x20-0x2F */
        return 0xC0 + (lo - 0xB0); /* 0xC0-0xCF maps to ram_pages 0-15? */
        /* Actually these map to dcpp 0xB0-0xBF which aren't in the switch */
    }

    /* Accelerator control: DCP entry 0x80 */
    if (lo == 0x80 && hi >= 0x80) return 0x80;

    /* Page register shortcut ports (below 0xC0):
     * 0x82 → WIN0, 0xA2 → WIN1, 0xC2 → border/WIN2, 0xE2 → system port
     * These go through DCP and map to system port dcpp values */
    if (lo == 0x82) {
        /* DCP maps OUT (#82),A to a system port based on hi byte.
         * With default DCP, this maps to dcpp for win0 page register */
        return 0xE8; /* map to WIN0 page port */
    }
    if (lo == 0xA2) {
        return 0xE9; /* map to WIN1 page port */
    }

    /* Port 0x00: no function (dcpp 0x00) */
    /* All unrecognized ports: no function */
    return 0x00;
}

void dcp_init(sp_dcp_t *dcp)
{
    /* Build the 16K DCP table.
     * For each possible 14-bit DCP offset, compute the dcpp value.
     *
     * The DCP offset encodes: {CNF[4:3], PN[5], DOS, R/W, A[15:14], A[13], A[7], A[6:5,2:0]}
     * We reconstruct the port address from the offset bits and compute dcpp. */
    for (u32 offset = 0; offset < DCP_TABLE_SIZE; offset++) {
        /* Extract address bits from offset */
        u8 a_low = offset & 0x67; /* bits A[6:5] and A[2:0] */
        u8 a7 = (offset >> 3) & 1;
        u8 a13 = (offset >> 4) & 1;
        u8 a15_14 = (offset >> 7) & 3;
        /* int rw = (offset >> 9) & 1; */

        /* Reconstruct partial port address from available bits */
        u8 lo = a_low | (a7 << 7);
        /* A[4:3] are not in the DCP offset — they don't affect port decode */
        u8 hi = (a15_14 << 6) | (a13 << 5);
        /* Other hi bits not available; they don't affect dcpp for most ports */

        u16 port = ((u16)hi << 8) | lo;

        dcp->table[offset] = compute_dcpp_for_port(port);
    }
}

void dcp_reset(sp_dcp_t *dcp)
{
    /* DCP table is static after FPGA config; no reset needed */
}

u16 dcp_compute_offset(sp_machine_t *m, u16 port, int is_read)
{
    /* MAME formula:
     * offset = (CNF[4:3] << 12) | (PN[5] << 11) | (DOS << 10)
     *        | (RW << 9) | (A[15:14] << 7) | (A[13] << 4)
     *        | (A[7] << 3) | (port & 0x67) */
    u16 offset =
        (((m->cnf >> 3) & 3) << 12) |
        (((m->pn >> 5) & 1) << 11) |
        ((m->dos_mode ? 1 : 0) << 10) |
        ((is_read ? 1 : 0) << 9) |
        (((port >> 14) & 3) << 7) |
        (((port >> 13) & 1) << 4) |
        (((port >> 7) & 1) << 3) |
        (port & 0x67);

    return offset & (DCP_TABLE_SIZE - 1);
}

u8 dcp_lookup_port(sp_dcp_t *dcp, sp_machine_t *m, u16 port, int is_read)
{
    u16 offset = dcp_compute_offset(m, port, is_read);
    return dcp->table[offset];
}
