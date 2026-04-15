/*
 * SPEmulator — Audio Mixer (stub)
 */
#include "audio/audio.h"
#include "machine.h"
#include <stdlib.h>
#include <string.h>

int audio_init(sp_audio_t *audio, int sample_rate) {
    memset(audio, 0, sizeof(*audio));
    audio->sample_rate = sample_rate;
    /* Allocate buffer for one frame (~20ms at 50Hz) */
    audio->buffer_size = sample_rate / 50;
    audio->buffer = calloc(audio->buffer_size * 2, sizeof(i16));
    audio->enabled = true;
    return audio->buffer ? 0 : -1;
}

void audio_destroy(sp_audio_t *audio) {
    free(audio->buffer);
    audio->buffer = NULL;
}

void audio_generate_frame(sp_audio_t *audio, sp_machine_t *m) {
    (void)m;
    if (!audio->enabled || !audio->buffer) return;
    /* TODO: Mix AY + Covox + Beeper samples for the frame */
    memset(audio->buffer, 0, audio->buffer_size * 2 * sizeof(i16));
}
