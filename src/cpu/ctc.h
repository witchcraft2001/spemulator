/*
 * SPEmulator — Z80-CTC (Counter/Timer Circuit)
 * 4 channels, integrated in Z84C15.
 */
#ifndef SPEMU_CTC_H
#define SPEMU_CTC_H

#include "types.h"

typedef void (*ctc_irq_fn)(void *ctx, int channel);

typedef struct {
    u8   control[4];     /* Control word per channel */
    u8   time_const[4];  /* Time constant per channel */
    u16  counter[4];     /* Down counter */
    bool waiting_tc[4];  /* Waiting for time constant */
    u8   vector;         /* Interrupt vector (channel 0) */
    ctc_irq_fn irq_callback;
    void *irq_ctx;
} sp_ctc_t;

void ctc_init(sp_ctc_t *ctc);
void ctc_reset(sp_ctc_t *ctc);
void ctc_write(sp_ctc_t *ctc, u8 channel, u8 data);
u8   ctc_read(sp_ctc_t *ctc, u8 channel);
/* Clock the CTC by given number of CPU T-states */
void ctc_clock(sp_ctc_t *ctc, int tstates);

#endif
