/*
 * cx8_pixstretch.h — Chromaplex 8 PIXEL-STRETCH PRO Visual FX
 *
 * ┌──────────────────────────────────────────────────────────┐
 * │  PIXEL-STRETCH PRO  VisualFX Co.                        │
 * │  "Every pixel tells a story."                           │
 * │  Palette cycling, dithering, screen effects             │
 * │  Holographic rainbow shell                              │
 * └──────────────────────────────────────────────────────────┘
 *
 * Effects:
 *   fx_cycle(start, end, speed)  — Auto-cycle palette indices
 *   fx_dither(mode)              — Set dithering pattern
 *   fx_fade(target, speed)       — Screen fade (0=black, 1=full)
 *   fx_shake(amount, duration)   — Screen shake
 *   fx_tint(r, g, b, amount)    — Colour tint overlay
 *   fx_flash(col, frames)        — Flash screen a colour
 *   fx_wave(amp, freq)           — Scanline wave distortion
 *   fx_reset()                   — Clear all effects
 */

#ifndef CX8_PIXSTRETCH_H
#define CX8_PIXSTRETCH_H

#include "cx8.h"

/* ─── Dither modes ─────────────────────────────────────────── */
#define CX8_DITHER_NONE         0
#define CX8_DITHER_BAYER2       1   /* 2×2 ordered dithering   */
#define CX8_DITHER_BAYER4       2   /* 4×4 ordered dithering   */
#define CX8_DITHER_CHECKER      3   /* checkerboard            */
#define CX8_DITHER_HLINE        4   /* horizontal lines        */
#define CX8_DITHER_VLINE        5   /* vertical lines          */
#define CX8_DITHER_DIAG         6   /* diagonal lines          */
#define CX8_DITHER_MAX          7

/* ─── Palette cycle slot ───────────────────────────────────── */
#define CX8_FX_MAX_CYCLES       8   /* max simultaneous cycles */

typedef struct {
    int     start;          /* first palette index              */
    int     end;            /* last palette index               */
    float   speed;          /* cycles per second                */
    float   phase;          /* current phase (0-1)              */
    bool    active;
} cx8_fx_cycle_t;

/* ─── PIXEL-STRETCH PRO state ──────────────────────────────── */
typedef struct {
    bool            enabled;

    /* Palette cycling */
    cx8_fx_cycle_t  cycles[CX8_FX_MAX_CYCLES];
    int             cycle_count;

    /* Dithering */
    int             dither_mode;

    /* Fade */
    float           fade_level;     /* 0.0 = black, 1.0 = full */
    float           fade_target;
    float           fade_speed;     /* per second              */

    /* Screen shake */
    int             shake_x;
    int             shake_y;
    float           shake_amount;
    float           shake_time;     /* remaining seconds       */

    /* Colour tint */
    float           tint_r, tint_g, tint_b;
    float           tint_amount;    /* 0.0 = none, 1.0 = full  */

    /* Flash */
    uint8_t         flash_col;
    int             flash_frames;

    /* Scanline wave */
    float           wave_amp;       /* pixels                  */
    float           wave_freq;      /* cycles across screen    */
    float           wave_phase;
} cx8_fx_state_t;

/* ─── Lifecycle ────────────────────────────────────────────── */
void    cx8_fx_init(void);
void    cx8_fx_shutdown(void);

/* ─── Enable (called when module is loaded) ────────────────── */
void    cx8_fx_enable(void);
bool    cx8_fx_is_enabled(void);

/* ─── Effects API ──────────────────────────────────────────── */
void    cx8_fx_cycle(int start, int end, float speed);
void    cx8_fx_cycle_stop(int start);           /* stop by start index */
void    cx8_fx_dither(int mode);
int     cx8_fx_get_dither(void);
void    cx8_fx_fade(float target, float speed);
float   cx8_fx_get_fade(void);
void    cx8_fx_shake(float amount, float duration);
void    cx8_fx_tint(float r, float g, float b, float amount);
void    cx8_fx_flash(uint8_t col, int frames);
void    cx8_fx_wave(float amplitude, float frequency);
void    cx8_fx_reset(void);

/* ─── Per-frame update (call before present) ───────────────── */
void    cx8_fx_update(float dt);

/* ─── Apply effects to output pixels ──────────────────────── */
/*  Call after converting VRAM to ARGB but before presenting.
 *  Modifies pixel buffer in-place. */
void    cx8_fx_apply(uint32_t *pixels, int width, int height);

/* ─── Dither query (for draw-time dithering) ───────────────── */
/*  Returns true if pixel at (x,y) should be drawn at full opacity
 *  given the current dither pattern and threshold (0.0–1.0). */
bool    cx8_fx_dither_test(int x, int y, float threshold);

#endif /* CX8_PIXSTRETCH_H */
