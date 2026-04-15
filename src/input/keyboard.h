/*
 * SPEmulator — Keyboard
 * Maps SDL scancodes to ZX Spectrum matrix and PS/2 scancodes.
 */
#ifndef SPEMU_KEYBOARD_H
#define SPEMU_KEYBOARD_H

#include "types.h"

typedef struct sp_machine sp_machine_t;

/* Process key press (SDL scancode) */
void keyboard_key_down(sp_machine_t *m, u32 scancode);

/* Process key release (SDL scancode) */
void keyboard_key_up(sp_machine_t *m, u32 scancode);

/* Reset keyboard state */
void keyboard_reset(sp_machine_t *m);

#endif /* SPEMU_KEYBOARD_H */
