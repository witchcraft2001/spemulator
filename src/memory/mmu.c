/*
 * SPEmulator — Memory Management Unit
 * Now delegated to update_memory() in machine.c
 */
#include "memory/mmu.h"
#include "machine.h"

void mmu_init(sp_machine_t *m) {
    mmu_reset(m);
}

void mmu_reset(sp_machine_t *m) {
    /* Memory mapping is handled by update_memory() */
    update_memory(m);
}
