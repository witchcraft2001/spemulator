/*
 * SPEmulator — Z80-SIO (Serial I/O)
 * 2 channels: A = PS/2 keyboard, B = RS-232 serial (mouse)
 */
#ifndef SPEMU_SIO_H
#define SPEMU_SIO_H

#include "types.h"

#define SIO_BUF_SIZE 64

typedef struct {
    /* Per-channel state */
    struct {
        u8  wr[8];          /* Write registers WR0-WR7 */
        u8  rr[3];          /* Read registers RR0-RR2 */
        u8  rx_buf[SIO_BUF_SIZE];
        int rx_head;
        int rx_tail;
        int rx_count;
        u8  selected_reg;
    } ch[2];
} sp_sio_t;

void sio_init(sp_sio_t *sio);
void sio_reset(sp_sio_t *sio);

/* Read/write SIO registers */
u8   sio_read_data(sp_sio_t *sio, int channel);
u8   sio_read_ctrl(sp_sio_t *sio, int channel);
void sio_write_data(sp_sio_t *sio, int channel, u8 data);
void sio_write_ctrl(sp_sio_t *sio, int channel, u8 data);

/* Push a byte into receive buffer (from external source) */
void sio_receive(sp_sio_t *sio, int channel, u8 data);

/* Check if data is available in receive buffer */
bool sio_rx_ready(sp_sio_t *sio, int channel);

#endif
