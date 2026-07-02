/*
 * cx8_music.h — Chromaplex 8 Music Sequencer
 *
 * Pattern-based music system built on top of the APU.
 * Supports up to 32 patterns, each with 32 rows of note data.
 * Patterns are arranged in a song order list.
 */

#ifndef CX8_MUSIC_H
#define CX8_MUSIC_H

#include "cx8.h"

#define CX8_MUSIC_MAX_PATTERNS  32
#define CX8_MUSIC_PATTERN_ROWS  32
#define CX8_MUSIC_MAX_CHANNELS  4
#define CX8_MUSIC_MAX_ORDER     64

/* A single note in the pattern */
typedef struct {
    uint8_t  note;       /* 0=off, 1-96 = C-0..B-7       */
    uint8_t  waveform;   /* CX8_WAVE_*                    */
    uint8_t  volume;     /* 0-255 (mapped to 0.0-1.0)     */
    uint8_t  effect;     /* 0=none, future expansion      */
} cx8_music_note_t;

/* A pattern (32 rows × 4 channels) */
typedef struct {
    cx8_music_note_t notes[CX8_MUSIC_PATTERN_ROWS][CX8_MUSIC_MAX_CHANNELS];
} cx8_music_pattern_t;

/* Initialise the music system */
void cx8_music_init(void);
void cx8_music_shutdown(void);

/* Set BPM (beats per minute, each beat = 1 row at default speed) */
void cx8_music_set_bpm(int bpm);
int  cx8_music_get_bpm(void);

/* Set the song order list (array of pattern indices, -1 terminated) */
void cx8_music_set_order(const int *order, int length);

/* Set a note in a pattern */
void cx8_music_set_note(int pattern, int row, int channel,
                        uint8_t note, uint8_t waveform, uint8_t volume);

/* Playback control */
void cx8_music_play(int start_order);   /* start from order index  */
void cx8_music_stop(void);
void cx8_music_pause(void);
void cx8_music_resume(void);
bool cx8_music_is_playing(void);

/* Get current playback position */
int  cx8_music_current_order(void);
int  cx8_music_current_row(void);

/* Must be called each frame to advance the sequencer */
void cx8_music_update(void);

/* Convert note number (1-96) to frequency in Hz */
float cx8_music_note_freq(uint8_t note);

#endif /* CX8_MUSIC_H */
