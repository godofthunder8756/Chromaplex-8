/*
 * cx8_apu.c — Chromaplex 8 WAVE-4 Audio Processing Unit
 *
 * Real-time waveform synthesis with ADSR envelopes.
 * Generates audio via SDL callback.
 */

#include "cx8_apu.h"
#include <stdlib.h>
#include <stdio.h>
#include <math.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

/* ─── Internal state ───────────────────────────────────────── */
static cx8_channel_t s_channels[CX8_MAX_CHANNELS];
static int           s_num_channels = CX8_AUDIO_CHANNELS;

/* ─── Lifecycle ────────────────────────────────────────────── */

void cx8_apu_init(void)
{
    memset(s_channels, 0, sizeof(s_channels));
    s_num_channels = CX8_AUDIO_CHANNELS;
    for (int i = 0; i < CX8_MAX_CHANNELS; i++) {
        s_channels[i].noise_state = 0xACE1u;
        s_channels[i].duty = 0.5f;
    }
    printf("[CX8-APU] WAVE-%d initialised (%d Hz, %d channels)\n",
           s_num_channels, CX8_SAMPLE_RATE, s_num_channels);
}

void cx8_apu_shutdown(void)
{
    cx8_apu_stop_all();
}

/* ─── Channel management ──────────────────────────────────── */

void cx8_apu_set_channels(int count)
{
    if (count < 1) count = 1;
    if (count > CX8_MAX_CHANNELS) count = CX8_MAX_CHANNELS;
    s_num_channels = count;
    printf("[CX8-APU] Expanded to %d channels\n", s_num_channels);
}

int cx8_apu_get_channels(void)
{
    return s_num_channels;
}

void cx8_apu_play(int channel, float frequency, float volume,
                  uint8_t waveform, float duty)
{
    if (channel < 0 || channel >= s_num_channels) return;
    cx8_channel_t *ch = &s_channels[channel];
    ch->frequency = frequency;
    ch->volume    = CX8_CLAMP(volume, 0.0f, 1.0f);
    ch->waveform  = waveform;
    ch->duty      = CX8_CLAMP(duty, 0.05f, 0.95f);
    ch->phase     = 0.0f;
    ch->env_time  = 0.0f;
    ch->env_level = 0.0f;
    ch->env_stage = (ch->attack > 0.0f) ? 1 : 3; /* skip to sustain if no ADSR */
    ch->active    = true;
}

void cx8_apu_envelope(int channel, float attack, float decay,
                      float sustain, float release)
{
    if (channel < 0 || channel >= s_num_channels) return;
    s_channels[channel].attack  = attack;
    s_channels[channel].decay   = decay;
    s_channels[channel].sustain = CX8_CLAMP(sustain, 0.0f, 1.0f);
    s_channels[channel].release = release;
}

void cx8_apu_stop(int channel)
{
    if (channel < 0 || channel >= s_num_channels) return;
    cx8_channel_t *ch = &s_channels[channel];
    if (ch->active && ch->release > 0.0f) {
        ch->env_stage = 4; /* release */
        ch->env_time  = 0.0f;
    } else {
        ch->active = false;
    }
}

void cx8_apu_stop_all(void)
{
    for (int i = 0; i < CX8_MAX_CHANNELS; i++)
        s_channels[i].active = false;
}

/* ─── Waveform generation ──────────────────────────────────── */

static float generate_sample(cx8_channel_t *ch)
{
    float t = ch->phase;
    float sample = 0.0f;

    switch (ch->waveform) {
    case CX8_WAVE_SQUARE:
        sample = (t < 0.5f) ? 1.0f : -1.0f;
        break;
    case CX8_WAVE_TRIANGLE:
        sample = (t < 0.5f) ? (4.0f * t - 1.0f) : (3.0f - 4.0f * t);
        break;
    case CX8_WAVE_SAW:
        sample = 2.0f * t - 1.0f;
        break;
    case CX8_WAVE_NOISE: {
        /* 16-bit LFSR */
        uint32_t bit = ((ch->noise_state >> 0) ^ (ch->noise_state >> 2) ^
                        (ch->noise_state >> 3) ^ (ch->noise_state >> 5)) & 1;
        ch->noise_state = (ch->noise_state >> 1) | (bit << 15);
        sample = ((float)(ch->noise_state & 0xFFFF) / 32768.0f) - 1.0f;
        break;
    }
    case CX8_WAVE_PULSE:
        sample = (t < ch->duty) ? 1.0f : -1.0f;
        break;
    case CX8_WAVE_SINE:
        sample = sinf(t * 2.0f * (float)M_PI);
        break;
    default:
        break;
    }
    return sample;
}

/* ─── ADSR envelope processing ─────────────────────────────── */

static float process_envelope(cx8_channel_t *ch, float dt)
{
    switch (ch->env_stage) {
    case 0: /* off */
        return 0.0f;
    case 1: /* attack */
        ch->env_time += dt;
        if (ch->attack <= 0.0f || ch->env_time >= ch->attack) {
            ch->env_level = 1.0f;
            ch->env_stage = 2;
            ch->env_time  = 0.0f;
        } else {
            ch->env_level = ch->env_time / ch->attack;
        }
        break;
    case 2: /* decay */
        ch->env_time += dt;
        if (ch->decay <= 0.0f || ch->env_time >= ch->decay) {
            ch->env_level = ch->sustain;
            ch->env_stage = 3;
            ch->env_time  = 0.0f;
        } else {
            ch->env_level = 1.0f - (1.0f - ch->sustain) * (ch->env_time / ch->decay);
        }
        break;
    case 3: /* sustain */
        ch->env_level = ch->sustain;
        /* stays here until stop is called */
        break;
    case 4: /* release */
        ch->env_time += dt;
        if (ch->release <= 0.0f || ch->env_time >= ch->release) {
            ch->env_level = 0.0f;
            ch->active = false;
            ch->env_stage = 0;
        } else {
            ch->env_level = ch->sustain * (1.0f - ch->env_time / ch->release);
        }
        break;
    }
    return ch->env_level;
}

/* ─── SDL audio callback ──────────────────────────────────── */

void cx8_apu_callback(void *userdata, uint8_t *stream, int len)
{
    (void)userdata;
    float *out = (float *)stream;
    int    samples = len / (int)sizeof(float);
    float  dt = 1.0f / (float)CX8_SAMPLE_RATE;

    for (int i = 0; i < samples; i++) {
        float mix = 0.0f;
        int active_count = 0;

        for (int c = 0; c < s_num_channels; c++) {
            cx8_channel_t *ch = &s_channels[c];
            if (!ch->active) continue;

            float env = process_envelope(ch, dt);
            float sample = generate_sample(ch);
            mix += sample * ch->volume * env;
            active_count++;

            /* Advance phase */
            ch->phase += ch->frequency * dt;
            while (ch->phase >= 1.0f) ch->phase -= 1.0f;
        }

        /* Simple limiter */
        if (active_count > 0) mix /= (float)CX8_MAX(active_count, 1);
        mix = CX8_CLAMP(mix, -1.0f, 1.0f);

        /* Master volume reduction */
        out[i] = mix * 0.3f;
    }
}
