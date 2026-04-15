/*
 * SPEmulator — Beeper
 */
#ifndef SPEMU_BEEPER_H
#define SPEMU_BEEPER_H

#include "types.h"

typedef struct {
    bool state;  /* Current output state (port #FE bit 4) */
} sp_beeper_t;

void beeper_init(sp_beeper_t *beep);
void beeper_write(sp_beeper_t *beep, u8 val);

#endif
