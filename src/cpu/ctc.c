/*
 * SPEmulator — Z80-CTC
 */
#include "cpu/ctc.h"
#include <string.h>

void ctc_init(sp_ctc_t *ctc) {
    memset(ctc, 0, sizeof(*ctc));
}

void ctc_reset(sp_ctc_t *ctc) {
    for (int i = 0; i < 4; i++) {
        ctc->control[i] = 0;
        ctc->time_const[i] = 0;
        ctc->counter[i] = 0;
        ctc->waiting_tc[i] = false;
    }
}

void ctc_write(sp_ctc_t *ctc, u8 channel, u8 data) {
    if (channel > 3) return;

    if (ctc->waiting_tc[channel]) {
        /* This byte is the time constant */
        ctc->time_const[channel] = data;
        ctc->counter[channel] = data ? data : 256;
        ctc->waiting_tc[channel] = false;
        return;
    }

    if (channel == 0 && !(data & 1)) {
        /* Interrupt vector (bit 0 = 0, only channel 0) */
        ctc->vector = data & 0xF8;
        return;
    }

    /* Control word */
    ctc->control[channel] = data;
    if (data & 0x04) {
        /* Time constant follows */
        ctc->waiting_tc[channel] = true;
    }
    if (data & 0x02) {
        /* Reset channel */
        ctc->counter[channel] = ctc->time_const[channel];
    }
}

u8 ctc_read(sp_ctc_t *ctc, u8 channel) {
    if (channel > 3) return 0xFF;
    return (u8)(ctc->counter[channel] & 0xFF);
}

void ctc_clock(sp_ctc_t *ctc, int tstates) {
    for (int ch = 0; ch < 4; ch++) {
        if (!(ctc->control[ch] & 0x80)) continue; /* Not running */
        if (ctc->control[ch] & 0x08) continue;    /* Counter mode, needs trigger */

        /* Timer mode: prescaler 16 or 256 */
        int prescaler = (ctc->control[ch] & 0x20) ? 256 : 16;
        /* Simplified: decrement by tstates / prescaler */
        int ticks = tstates / prescaler;
        if (ticks <= 0) ticks = 1;

        for (int t = 0; t < ticks; t++) {
            ctc->counter[ch]--;
            if (ctc->counter[ch] == 0) {
                ctc->counter[ch] = ctc->time_const[ch] ? ctc->time_const[ch] : 256;
                /* Fire interrupt if enabled */
                if ((ctc->control[ch] & 0x80) && ctc->irq_callback) {
                    ctc->irq_callback(ctc->irq_ctx, ch);
                }
            }
        }
    }
}
