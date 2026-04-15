/*
 * SPEmulator — Covox Blaster (PCM DAC)
 */
#ifndef SPEMU_COVOX_H
#define SPEMU_COVOX_H

#include "types.h"

typedef struct {
    u8   sample;        /* Current sample value */
    u8   mode;          /* Mode register */
    bool enabled;
    bool stereo;
    bool is_16bit;
    u8   freq_code;     /* Frequency code 0-F */
    int  sample_rate;   /* Derived sample rate */
} sp_covox_t;

void covox_init(sp_covox_t *cov);
void covox_reset(sp_covox_t *cov);
void covox_write_data(sp_covox_t *cov, u8 data);
void covox_write_mode(sp_covox_t *cov, u8 mode);

#endif
