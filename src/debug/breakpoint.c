/*
 * SPEmulator — Breakpoint Manager
 */
#include "debug/breakpoint.h"
#include <string.h>

void bp_init(sp_bp_manager_t *bpm) {
    memset(bpm, 0, sizeof(*bpm));
}

int bp_add(sp_bp_manager_t *bpm, u16 addr, sp_bp_type_t type) {
    for (int i = 0; i < MAX_BREAKPOINTS; i++) {
        if (!bpm->list[i].used) {
            bpm->list[i].addr = addr;
            bpm->list[i].type = type;
            bpm->list[i].enabled = true;
            bpm->list[i].used = true;
            bpm->count++;
            return i;
        }
    }
    return -1;
}

int bp_remove(sp_bp_manager_t *bpm, int index) {
    if (index < 0 || index >= MAX_BREAKPOINTS) return -1;
    if (!bpm->list[index].used) return -1;
    bpm->list[index].used = false;
    bpm->list[index].enabled = false;
    bpm->count--;
    return 0;
}

void bp_enable(sp_bp_manager_t *bpm, int index, bool enabled) {
    if (index >= 0 && index < MAX_BREAKPOINTS && bpm->list[index].used)
        bpm->list[index].enabled = enabled;
}

static bool bp_check(sp_bp_manager_t *bpm, u16 addr, sp_bp_type_t type) {
    for (int i = 0; i < MAX_BREAKPOINTS; i++) {
        if (bpm->list[i].used && bpm->list[i].enabled &&
            bpm->list[i].type == type && bpm->list[i].addr == addr)
            return true;
    }
    return false;
}

bool bp_check_exec(sp_bp_manager_t *bpm, u16 pc) {
    return bp_check(bpm, pc, BP_TYPE_EXEC);
}

bool bp_check_mem_read(sp_bp_manager_t *bpm, u16 addr) {
    return bp_check(bpm, addr, BP_TYPE_MEM_READ);
}

bool bp_check_mem_write(sp_bp_manager_t *bpm, u16 addr) {
    return bp_check(bpm, addr, BP_TYPE_MEM_WRITE);
}
