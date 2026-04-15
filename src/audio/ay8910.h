/*
 * SPEmulator — AY-3-8910 Sound Chip
 */
#ifndef SPEMU_AY8910_H
#define SPEMU_AY8910_H

#include "types.h"

typedef struct {
    u8  regs[16];       /* AY registers R0-R15 */
    u8  selected_reg;   /* Currently selected register */
    /* Internal state */
    u16 tone_period[3]; /* Channel A/B/C tone period */
    u16 tone_counter[3];
    u8  tone_output[3];
    u16 noise_period;
    u16 noise_counter;
    u32 noise_shift;
    u16 env_period;
    u16 env_counter;
    u8  env_step;
    u8  env_volume;
    bool env_holding;
} sp_ay8910_t;

void ay8910_init(sp_ay8910_t *ay);
void ay8910_reset(sp_ay8910_t *ay);
void ay8910_write_reg(sp_ay8910_t *ay, u8 reg, u8 data);
u8   ay8910_read_reg(sp_ay8910_t *ay, u8 reg);
void ay8910_select_reg(sp_ay8910_t *ay, u8 reg);
/* Generate samples into buffer. Returns number of samples generated. */
int  ay8910_generate(sp_ay8910_t *ay, i16 *buffer, int num_samples, int clock_hz);

#endif
