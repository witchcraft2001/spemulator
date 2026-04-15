/*
 * SPEmulator — Z80-SIO
 */
#include "cpu/sio.h"
#include <string.h>

void sio_init(sp_sio_t *sio) {
    memset(sio, 0, sizeof(*sio));
}

void sio_reset(sp_sio_t *sio) {
    sio_init(sio);
}

void sio_receive(sp_sio_t *sio, int channel, u8 data) {
    if (channel < 0 || channel > 1) return;
    if (sio->ch[channel].rx_count >= SIO_BUF_SIZE) return;

    sio->ch[channel].rx_buf[sio->ch[channel].rx_tail] = data;
    sio->ch[channel].rx_tail = (sio->ch[channel].rx_tail + 1) % SIO_BUF_SIZE;
    sio->ch[channel].rx_count++;
    sio->ch[channel].rr[0] |= 0x01; /* RR0 bit 0 = Rx char available */
}

bool sio_rx_ready(sp_sio_t *sio, int channel) {
    if (channel < 0 || channel > 1) return false;
    return sio->ch[channel].rx_count > 0;
}

u8 sio_read_data(sp_sio_t *sio, int channel) {
    if (channel < 0 || channel > 1) return 0xFF;
    if (sio->ch[channel].rx_count == 0) return 0xFF;

    u8 data = sio->ch[channel].rx_buf[sio->ch[channel].rx_head];
    sio->ch[channel].rx_head = (sio->ch[channel].rx_head + 1) % SIO_BUF_SIZE;
    sio->ch[channel].rx_count--;
    if (sio->ch[channel].rx_count == 0)
        sio->ch[channel].rr[0] &= ~0x01;
    return data;
}

u8 sio_read_ctrl(sp_sio_t *sio, int channel) {
    if (channel < 0 || channel > 1) return 0xFF;
    u8 reg = sio->ch[channel].selected_reg;
    sio->ch[channel].selected_reg = 0; /* Auto-reset to RR0 */
    if (reg > 2) reg = 0;
    return sio->ch[channel].rr[reg];
}

void sio_write_data(sp_sio_t *sio, int channel, u8 data) {
    (void)sio; (void)channel; (void)data;
    /* TODO: Transmit data */
}

void sio_write_ctrl(sp_sio_t *sio, int channel, u8 data) {
    if (channel < 0 || channel > 1) return;

    if (sio->ch[channel].selected_reg == 0) {
        /* WR0: register pointer in bits 2-0 */
        sio->ch[channel].selected_reg = data & 0x07;
        /* Also handle reset commands in bits 5-3 */
    } else {
        u8 reg = sio->ch[channel].selected_reg;
        if (reg < 8)
            sio->ch[channel].wr[reg] = data;
        sio->ch[channel].selected_reg = 0;
    }
}
