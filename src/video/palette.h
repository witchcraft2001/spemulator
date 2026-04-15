/*
 * SPEmulator — Palette Management
 * Sprinter: 256 colors, 6-6-6 RGB encoding.
 */
#ifndef SPEMU_PALETTE_H
#define SPEMU_PALETTE_H

#include "types.h"

typedef struct sp_machine sp_machine_t;

/* Set palette entry: index, 6-bit R, G, B */
void palette_set(sp_machine_t *m, u8 index, u8 r6, u8 g6, u8 b6);

/* Initialize default Sprinter palette */
void palette_init_default(sp_machine_t *m);

#endif /* SPEMU_PALETTE_H */
