/*
 * SPEmulator — DCP Decoder (stub)
 */
#include "memory/dcp.h"
#include <string.h>

void dcp_init(sp_dcp_t *dcp) {
    memset(dcp, 0, sizeof(*dcp));
    /* TODO: Load DCP.MIF table or build from known port mappings */
}

void dcp_reset(sp_dcp_t *dcp) {
    dcp_init(dcp);
}

sp_dcp_entry_t dcp_lookup(sp_dcp_t *dcp, u8 port_lo) {
    return dcp->table[port_lo];
}
