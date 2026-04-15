/*
 * SPEmulator — Debugger UI
 */
#ifndef SPEMU_DBG_UI_H
#define SPEMU_DBG_UI_H

#include "types.h"

typedef struct sp_machine sp_machine_t;

/* Process debugger keyboard input (terminal-based for now) */
void dbg_ui_process_input(sp_machine_t *m);

#endif
