/*
 * SPEmulator — Mouse (Kempston + serial)
 */
#ifndef SPEMU_MOUSE_H
#define SPEMU_MOUSE_H

#include "types.h"

typedef struct sp_machine sp_machine_t;

void mouse_init(sp_machine_t *m);
void mouse_update(sp_machine_t *m, i16 dx, i16 dy, u8 buttons);

#endif
