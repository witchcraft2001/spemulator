/*
 * SPEmulator — AY-3-8910 Sound Chip
 */
#include "audio/ay8910.h"
#include <string.h>

void ay8910_init(sp_ay8910_t *ay) {
    memset(ay, 0, sizeof(*ay));
    ay->noise_shift = 1;
}

void ay8910_reset(sp_ay8910_t *ay) {
    ay8910_init(ay);
}

void ay8910_select_reg(sp_ay8910_t *ay, u8 reg) {
    ay->selected_reg = reg & 0x0F;
}

void ay8910_write_reg(sp_ay8910_t *ay, u8 reg, u8 data) {
    reg &= 0x0F;
    ay->regs[reg] = data;

    switch (reg) {
    case 0: case 1: ay->tone_period[0] = (ay->regs[1] & 0x0F) << 8 | ay->regs[0]; break;
    case 2: case 3: ay->tone_period[1] = (ay->regs[3] & 0x0F) << 8 | ay->regs[2]; break;
    case 4: case 5: ay->tone_period[2] = (ay->regs[5] & 0x0F) << 8 | ay->regs[4]; break;
    case 6: ay->noise_period = data & 0x1F; break;
    case 11: case 12: ay->env_period = ay->regs[12] << 8 | ay->regs[11]; break;
    case 13:
        ay->env_step = 0;
        ay->env_counter = 0;
        ay->env_holding = false;
        break;
    }
}

u8 ay8910_read_reg(sp_ay8910_t *ay, u8 reg) {
    return ay->regs[reg & 0x0F];
}

int ay8910_generate(sp_ay8910_t *ay, i16 *buffer, int num_samples, int clock_hz) {
    (void)clock_hz;
    /* TODO: Proper tone/noise/envelope generation */
    memset(buffer, 0, num_samples * sizeof(i16));
    return num_samples;
}
