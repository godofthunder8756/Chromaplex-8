/*
 * cx8_music.c — Chromaplex 8 Music Sequencer
 *
 * Tick-based pattern sequencer that drives the APU.
 */

#include "cx8_music.h"
#include "cx8_apu.h"
#include <string.h>
#include <stdio.h>
#include <math.h>

/* ─── Internal state ───────────────────────────────────────── */
static cx8_music_pattern_t s_patterns[CX8_MUSIC_MAX_PATTERNS];
static int   s_order[CX8_MUSIC_MAX_ORDER];
static int   s_order_len   = 0;
static int   s_bpm         = 120;
static bool  s_playing     = false;
static bool  s_paused      = false;
static int   s_cur_order   = 0;
static int   s_cur_row     = 0;
static float s_tick_accum  = 0.0f;

/* ─── Note-to-frequency table (A4 = 440 Hz, note 49) ──────── */

float cx8_music_note_freq(uint8_t note)
{
    if (note == 0) return 0.0f;
    /* Note 49 = A4 = 440 Hz.  Each semitone is 2^(1/12) apart. */
    return 440.0f * powf(2.0f, ((float)note - 49.0f) / 12.0f);
}

/* ─── Lifecycle ────────────────────────────────────────────── */

void cx8_music_init(void)
{
    memset(s_patterns, 0, sizeof(s_patterns));
    memset(s_order, -1, sizeof(s_order));
    s_order_len  = 0;
    s_bpm        = 120;
    s_playing    = false;
    s_paused     = false;
    s_cur_order  = 0;
    s_cur_row    = 0;
    s_tick_accum = 0.0f;
    printf("[CX8-MUSIC] Sequencer ready\n");
}

void cx8_music_shutdown(void)
{
    cx8_music_stop();
}

/* ─── Configuration ────────────────────────────────────────── */

void cx8_music_set_bpm(int bpm)
{
    if (bpm < 30)  bpm = 30;
    if (bpm > 300) bpm = 300;
    s_bpm = bpm;
}

int cx8_music_get_bpm(void) { return s_bpm; }

void cx8_music_set_order(const int *order, int length)
{
    if (length > CX8_MUSIC_MAX_ORDER) length = CX8_MUSIC_MAX_ORDER;
    for (int i = 0; i < length; i++)
        s_order[i] = order[i];
    s_order_len = length;
}

void cx8_music_set_note(int pattern, int row, int channel,
                        uint8_t note, uint8_t waveform, uint8_t volume)
{
    if (pattern < 0 || pattern >= CX8_MUSIC_MAX_PATTERNS) return;
    if (row < 0 || row >= CX8_MUSIC_PATTERN_ROWS) return;
    if (channel < 0 || channel >= CX8_MUSIC_MAX_CHANNELS) return;

    cx8_music_note_t *n = &s_patterns[pattern].notes[row][channel];
    n->note     = note;
    n->waveform = waveform;
    n->volume   = volume;
    n->effect   = 0;
}

/* ─── Playback control ─────────────────────────────────────── */

void cx8_music_play(int start_order)
{
    if (s_order_len == 0) return;
    if (start_order < 0) start_order = 0;
    if (start_order >= s_order_len) start_order = 0;
    s_cur_order  = start_order;
    s_cur_row    = 0;
    s_tick_accum = 0.0f;
    s_playing    = true;
    s_paused     = false;
}

void cx8_music_stop(void)
{
    s_playing = false;
    s_paused  = false;
    /* Silence the music channels */
    for (int ch = 0; ch < CX8_MUSIC_MAX_CHANNELS; ch++)
        cx8_apu_stop(ch);
}

void cx8_music_pause(void)  { s_paused = true; }
void cx8_music_resume(void) { s_paused = false; }
bool cx8_music_is_playing(void) { return s_playing && !s_paused; }

int cx8_music_current_order(void) { return s_cur_order; }
int cx8_music_current_row(void)   { return s_cur_row; }

/* ─── Play one row of the current pattern ──────────────────── */

static void play_row(void)
{
    if (s_cur_order >= s_order_len) {
        cx8_music_stop();
        return;
    }

    int pat_idx = s_order[s_cur_order];
    if (pat_idx < 0 || pat_idx >= CX8_MUSIC_MAX_PATTERNS) {
        cx8_music_stop();
        return;
    }

    cx8_music_pattern_t *pat = &s_patterns[pat_idx];

    for (int ch = 0; ch < CX8_MUSIC_MAX_CHANNELS; ch++) {
        cx8_music_note_t *n = &pat->notes[s_cur_row][ch];
        if (n->note == 0) continue;  /* no note this row */

        float freq = cx8_music_note_freq(n->note);
        float vol  = (float)n->volume / 255.0f;
        cx8_apu_play(ch, freq, vol, n->waveform, 0.5f);
    }
}

/* ─── Update (called each frame) ──────────────────────────── */

void cx8_music_update(void)
{
    if (!s_playing || s_paused) return;

    /* Rows per second = BPM * rows_per_beat / 60
     * With 1 row per beat: rows/sec = bpm/60 */
    float rows_per_sec = (float)s_bpm / 60.0f;
    float dt = 1.0f / 60.0f;  /* assuming 60 FPS */

    s_tick_accum += rows_per_sec * dt;

    while (s_tick_accum >= 1.0f) {
        s_tick_accum -= 1.0f;
        play_row();

        /* Advance */
        s_cur_row++;
        if (s_cur_row >= CX8_MUSIC_PATTERN_ROWS) {
            s_cur_row = 0;
            s_cur_order++;
            if (s_cur_order >= s_order_len) {
                /* Loop back to start */
                s_cur_order = 0;
            }
        }
    }
}
