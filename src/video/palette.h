/*
 * SPEmulator — Palette Management
 *
 * Sprinter palette: 8 banks × 256 colors = 2048 entries.
 * Stored in VRAM at offset ≥ 0x3E0 within each 1KB line.
 * Each entry = 3 bytes (R, G, B). MAME stores them as raw 8-bit values.
 * The FPGA interprets them (likely 6-bit DAC, but stored as 8-bit in VRAM).
 */
#ifndef SPEMU_PALETTE_H
#define SPEMU_PALETTE_H

#include "types.h"

typedef struct sp_machine sp_machine_t;

/* Rebuild entire palette cache from current VRAM contents */
void palette_rebuild(sp_machine_t *m);

/* Initialize palette to default ZX Spectrum colors for bank 0 */
void palette_init_default(sp_machine_t *m);

#endif /* SPEMU_PALETTE_H */
