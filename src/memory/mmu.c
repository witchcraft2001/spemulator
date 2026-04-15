/*
 * SPEmulator — Memory Management Unit
 */
#include "memory/mmu.h"
#include "machine.h"

void mmu_init(sp_machine_t *m) {
    mmu_reset(m);
}

void mmu_reset(sp_machine_t *m) {
    /* Default page mapping at reset:
     * WIN0 (0x0000-0x3FFF) = ROM page 0 (page# 0x80)
     * WIN1 (0x4000-0x7FFF) = RAM page 2
     * WIN2 (0x8000-0xBFFF) = RAM page 10
     * WIN3 (0xC000-0xFFFF) = RAM page 0
     */
    m->page_reg[0] = 0x80;
    m->page_reg[1] = 0x02;
    m->page_reg[2] = 0x0A;
    m->page_reg[3] = 0x00;
}
