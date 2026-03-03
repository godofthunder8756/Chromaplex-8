/*
 * cx8_apu.h — Chromaplex 8 WAVE-4 Audio Processing Unit
 *
 * 4 channels (expandable to 16 via SYNTHWAVE-16 module)
 * Waveforms: square, triangle, sawtooth, noise, pulse, sine
 * ADSR envelopes per channel
 */

#ifndef CX8_APU_H
#define CX8_APU_H

#include "cx8.h"

/* Initialise the APU (call before SDL audio is started) */
void cx8_apu_init(void);
void cx8_apu_shutdown(void);

/* Expand channels (called by SYNTHWAVE-16 module) */
void cx8_apu_set_channels(int count);
int  cx8_apu_get_channels(void);

/* Play a note on a channel */
void cx8_apu_play(int channel, float frequency, float volume,
                  uint8_t waveform, float duty);

/* Set ADSR envelope for a channel */
void cx8_apu_envelope(int channel, float attack, float decay,
                      float sustain, float release);

/* Stop a channel */
void cx8_apu_stop(int channel);

/* Stop all channels */
void cx8_apu_stop_all(void);

/* SDL audio callback – fills the output buffer */
void cx8_apu_callback(void *userdata, uint8_t *stream, int len);

#endif /* CX8_APU_H */
