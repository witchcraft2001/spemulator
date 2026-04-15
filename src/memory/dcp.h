/*
 * SPEmulator — DCP (Dynamic Configuration Port) Decoder
 * 256x16-bit LUT mapping port addresses to device types and wait states.
 */
#ifndef SPEMU_DCP_H
#define SPEMU_DCP_H

#include "types.h"

typedef struct sp_machine sp_machine_t;

/* DCP entry: 16-bit value from lookup table */
typedef struct {
    u8  type;       /* bits 15-12: device type */
    u8  wait;       /* bits 14-12: wait state selector */
    u16 signals;    /* bits 11-0: MAX7000 control signals */
} sp_dcp_entry_t;

typedef struct {
    sp_dcp_entry_t table[256];  /* Indexed by low byte of port address */
} sp_dcp_t;

void dcp_init(sp_dcp_t *dcp);
void dcp_reset(sp_dcp_t *dcp);

/* Lookup DCP entry for given port */
sp_dcp_entry_t dcp_lookup(sp_dcp_t *dcp, u8 port_lo);

#endif
