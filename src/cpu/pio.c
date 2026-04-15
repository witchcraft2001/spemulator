/*
 * SPEmulator — Z80-PIO (stub)
 */
#include "cpu/pio.h"
#include <string.h>

void pio_init(sp_pio_t *pio) {
    memset(pio, 0, sizeof(*pio));
}

void pio_reset(sp_pio_t *pio) {
    pio_init(pio);
}

u8 pio_read_data(sp_pio_t *pio, int port) {
    if (port < 0 || port > 1) return 0xFF;
    return pio->data[port];
}

void pio_write_data(sp_pio_t *pio, int port, u8 data) {
    if (port < 0 || port > 1) return;
    pio->data[port] = data;
}

u8 pio_read_ctrl(sp_pio_t *pio, int port) {
    if (port < 0 || port > 1) return 0xFF;
    return pio->control[port];
}

void pio_write_ctrl(sp_pio_t *pio, int port, u8 data) {
    if (port < 0 || port > 1) return;
    pio->control[port] = data;
    /* TODO: Implement mode selection, interrupt control */
}
