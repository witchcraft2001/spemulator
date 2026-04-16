/*
 * SPEmulator — DCP (Dynamic Configuration Port) Decoder
 *
 * The DCP is a lookup table stored in RAM page 0x40 (offset 0x100000).
 * It maps a 14-bit key derived from the Z80 port address and machine
 * state to an 8-bit dcpp (decoded port) value:
 *
 *   key = (CNF[4:3]<<12)|(PN[5]<<11)|(DOS<<10)|(RW<<9)
 *       | (A[15:14]<<7)|(A[13]<<4)|(A[7]<<3)|(port&0x67)
 *
 * The dcpp selects the device handler:
 *   0x00        = no port
 *   0x10-0x15   = FDD (WD1793)
 *   0x1B        = ISA control
 *   0x1C        = CMOS data read
 *   0x1D        = CMOS addr write
 *   0x1E        = CMOS data write
 *   0x20-0x29   = IDE/ATA
 *   0x2E        = soft reset
 *   0x40        = keyboard
 *   0x52        = AY read
 *   0x88-0x89   = COVOX
 *   0x8F        = ROM/fastram page register
 *   0x90        = AY address write
 *   0x91        = AY data write
 *   0xC0-0xEF   = system ports (ram_pages[dcpp-0xC0])
 *   0xF0-0xFF   = WIN3 page register
 *
 * The table is initialized with defaults in dcp_init_default_ram() and
 * overwritten by the BIOS during the starting phase via memory writes to
 * WIN3 (0xC000-0xFFFF = RAM page 0x40).
 */
#ifndef SPEMU_DCP_H
#define SPEMU_DCP_H

#include "types.h"

/* DCP table: 14-bit key = 16384 entries */
#define DCP_TABLE_SIZE  16384
/* DCP table lives at RAM page 0x40 */
#define DCP_RAM_PAGE    0x40

typedef struct sp_machine sp_machine_t;

typedef struct {
    /* No state needed: DCP table is in RAM page 0x40 */
    u8 _pad;
} sp_dcp_t;

/* Initialize DCP table (stub — actual init via dcp_init_default_ram) */
void dcp_init(sp_dcp_t *dcp);
void dcp_reset(sp_dcp_t *dcp);

/* Populate RAM page 0x40 with default DCPP values based on port patterns.
 * Called from machine_reset(). The BIOS will overwrite during starting. */
void dcp_init_default_ram(u8 *ram);

/* Compute 14-bit DCP key from port address and machine state */
u16 dcp_compute_offset(sp_machine_t *m, u16 port, int is_read);

/* Look up dcpp from RAM page 0x40 */
u8 dcp_lookup_port(sp_dcp_t *dcp, sp_machine_t *m, u16 port, int is_read);

#endif
