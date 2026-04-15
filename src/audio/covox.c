/*
 * SPEmulator — Covox Blaster (stub)
 */
#include "audio/covox.h"
#include <string.h>

static const int covox_freq_table[16] = {
    16000, 0, 0, 0, 0, 0, 0, 0,
    7812, 10937, 0, 0, 31250, 0, 0, 109375
};

void covox_init(sp_covox_t *cov) {
    memset(cov, 0, sizeof(*cov));
}

void covox_reset(sp_covox_t *cov) {
    covox_init(cov);
}

void covox_write_data(sp_covox_t *cov, u8 data) {
    cov->sample = data;
}

void covox_write_mode(sp_covox_t *cov, u8 mode) {
    cov->mode = mode;
    cov->enabled = (mode & 0x80) != 0;
    cov->stereo = (mode & 0x40) != 0;
    cov->is_16bit = (mode & 0x20) != 0;
    cov->freq_code = mode & 0x0F;
    cov->sample_rate = covox_freq_table[cov->freq_code];
}
