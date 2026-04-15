/*
 * SPEmulator — Hardware Accelerator (Blitter)
 * Intercepts LD r,r opcodes for hardware-accelerated block operations.
 */
#ifndef SPEMU_ACCEL_H
#define SPEMU_ACCEL_H

#include "types.h"

typedef struct sp_machine sp_machine_t;

/* Enable accelerator (called when DI detected before accel sequence) */
void accel_enable(sp_machine_t *m);

/* Disable accelerator */
void accel_disable(sp_machine_t *m);

#endif /* SPEMU_ACCEL_H */
