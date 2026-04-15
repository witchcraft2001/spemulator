/*
 * SPEmulator — Memory Management Unit
 */
#ifndef SPEMU_MMU_H
#define SPEMU_MMU_H

#include "types.h"

typedef struct sp_machine sp_machine_t;

/* Initialize MMU with default page mapping */
void mmu_init(sp_machine_t *m);

/* Reset to power-on page mapping */
void mmu_reset(sp_machine_t *m);

#endif
