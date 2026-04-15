/*
 * SPEmulator — DCP (Dynamic Configuration Port) Decoder
 *
 * The DCP is a 16K-entry lookup table in the FPGA that maps:
 *   {CNF[4:3], PN[5], DOS, R/W, A[15:14], A[13], A[7], A[6:5,2:0]} → dcpp
 *
 * The dcpp (decoded port) value determines which device handler processes
 * the I/O operation. Values 0xC0-0xEF map to ram_pages[] (system ports).
 *
 * Based on MAME's sprinter_state::dcp_r/dcp_w implementation.
 */
#ifndef SPEMU_DCP_H
#define SPEMU_DCP_H

#include "types.h"

/* DCP table size: 14-bit address = 16384 entries */
#define DCP_TABLE_SIZE  16384

typedef struct sp_machine sp_machine_t;

typedef struct {
    u8 table[DCP_TABLE_SIZE];  /* dcpp value for each 14-bit DCP offset */
} sp_dcp_t;

/* Initialize DCP table from built-in MIF data */
void dcp_init(sp_dcp_t *dcp);
void dcp_reset(sp_dcp_t *dcp);

/* Compute DCP offset from port address and machine state.
 * is_read: 1 for port read, 0 for port write */
u16 dcp_compute_offset(sp_machine_t *m, u16 port, int is_read);

/* Look up dcpp for a given port address */
u8 dcp_lookup_port(sp_dcp_t *dcp, sp_machine_t *m, u16 port, int is_read);

#endif
