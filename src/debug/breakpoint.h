/*
 * SPEmulator — Breakpoint Manager
 */
#ifndef SPEMU_BREAKPOINT_H
#define SPEMU_BREAKPOINT_H

#include "types.h"

#define MAX_BREAKPOINTS 64

typedef enum {
    BP_TYPE_EXEC,       /* Break on execution (PC) */
    BP_TYPE_MEM_READ,   /* Break on memory read */
    BP_TYPE_MEM_WRITE,  /* Break on memory write */
    BP_TYPE_PORT_READ,  /* Break on port read */
    BP_TYPE_PORT_WRITE, /* Break on port write */
} sp_bp_type_t;

typedef struct {
    u16          addr;
    sp_bp_type_t type;
    bool         enabled;
    bool         used;
} sp_breakpoint_t;

typedef struct {
    sp_breakpoint_t list[MAX_BREAKPOINTS];
    int             count;
} sp_bp_manager_t;

void bp_init(sp_bp_manager_t *bpm);
int  bp_add(sp_bp_manager_t *bpm, u16 addr, sp_bp_type_t type);
int  bp_remove(sp_bp_manager_t *bpm, int index);
void bp_enable(sp_bp_manager_t *bpm, int index, bool enabled);
bool bp_check_exec(sp_bp_manager_t *bpm, u16 pc);
bool bp_check_mem_read(sp_bp_manager_t *bpm, u16 addr);
bool bp_check_mem_write(sp_bp_manager_t *bpm, u16 addr);

#endif
