/*
 * SPEmulator — Hardware Accelerator
 */
#include "video/accel.h"
#include "machine.h"

void accel_enable(sp_machine_t *m) {
    m->accel_enabled = true;
    m->accel_size = 0;
}

void accel_disable(sp_machine_t *m) {
    m->accel_enabled = false;
}
