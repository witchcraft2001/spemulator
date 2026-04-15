/*
 * SPEmulator — Mouse (stub)
 */
#include "input/mouse.h"
#include "machine.h"

void mouse_init(sp_machine_t *m) {
    (void)m;
}

void mouse_update(sp_machine_t *m, i16 dx, i16 dy, u8 buttons) {
    (void)m; (void)dx; (void)dy; (void)buttons;
    /* TODO: Kempston mouse protocol + RS-232 serial mouse */
}
