/*
 * SPEmulator — Debugger Core
 */
#ifndef SPEMU_DEBUGGER_H
#define SPEMU_DEBUGGER_H

#include "types.h"
#include "debug/breakpoint.h"

typedef struct sp_machine sp_machine_t;

typedef enum {
    DBG_STATE_RUNNING,
    DBG_STATE_PAUSED,
    DBG_STATE_STEPPING,
    DBG_STATE_STEP_OVER,
} sp_dbg_state_t;

typedef struct {
    sp_bp_manager_t  breakpoints;
    sp_dbg_state_t   state;
    u16              step_over_addr; /* Address to stop at for step-over */
    bool             active;
} sp_debugger_t;

void debugger_init(sp_debugger_t *dbg);
void debugger_reset(sp_debugger_t *dbg);

/* Called before each CPU step. Returns true if execution should proceed. */
bool debugger_pre_step(sp_debugger_t *dbg, sp_machine_t *m);

/* Commands */
void debugger_break(sp_debugger_t *dbg);
void debugger_continue(sp_debugger_t *dbg);
void debugger_step(sp_debugger_t *dbg);
void debugger_step_over(sp_debugger_t *dbg, sp_machine_t *m);

/* Print state to stdout */
void debugger_print_regs(sp_debugger_t *dbg, sp_machine_t *m);
void debugger_print_disasm(sp_debugger_t *dbg, sp_machine_t *m, u16 addr, int lines);
void debugger_print_memdump(sp_machine_t *m, u8 page, u16 offset, int bytes);

#endif
