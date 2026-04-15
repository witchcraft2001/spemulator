/*
 * SPEmulator — Audio Mixer
 * Mixes AY, Covox, Beeper into stereo output for SDL3 audio.
 */
#ifndef SPEMU_AUDIO_H
#define SPEMU_AUDIO_H

#include "types.h"

typedef struct sp_machine sp_machine_t;

typedef struct {
    i16   *buffer;       /* Stereo interleaved buffer */
    int    buffer_size;  /* Total samples (L+R pairs) */
    int    sample_rate;
    int    write_pos;
    bool   enabled;
} sp_audio_t;

int  audio_init(sp_audio_t *audio, int sample_rate);
void audio_destroy(sp_audio_t *audio);
void audio_generate_frame(sp_audio_t *audio, sp_machine_t *m);

#endif
