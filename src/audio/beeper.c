/*
 * SPEmulator — Beeper
 */
#include "audio/beeper.h"
#include <string.h>

void beeper_init(sp_beeper_t *beep) {
    memset(beep, 0, sizeof(*beep));
}

void beeper_write(sp_beeper_t *beep, u8 val) {
    beep->state = (val & 0x10) != 0;
}
