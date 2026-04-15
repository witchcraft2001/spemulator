/*
 * SPEmulator — Z80-PIO (Parallel I/O)
 */
#ifndef SPEMU_PIO_H
#define SPEMU_PIO_H

#include "types.h"

typedef struct {
    u8  data[2];     /* Port A and B data */
    u8  control[2];  /* Control registers */
    u8  mode[2];     /* Operating mode */
    u8  io_mask[2];  /* I/O direction mask (bit mode) */
} sp_pio_t;

void pio_init(sp_pio_t *pio);
void pio_reset(sp_pio_t *pio);
u8   pio_read_data(sp_pio_t *pio, int port);
void pio_write_data(sp_pio_t *pio, int port, u8 data);
u8   pio_read_ctrl(sp_pio_t *pio, int port);
void pio_write_ctrl(sp_pio_t *pio, int port, u8 data);

#endif
