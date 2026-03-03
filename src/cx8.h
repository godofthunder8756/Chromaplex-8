/*
 * ╔═══════════════════════════════════════════════════════════════╗
 * ║              CHROMAPLEX 8 — Fantasy Game Console              ║
 * ║                  Hardware Specification v1.0                   ║
 * ╚═══════════════════════════════════════════════════════════════╝
 *
 *  CPU:  CX8-A   (virtual, 60 Hz tick)
 *  GPU:  PRISM-64 (256×144, 64-colour palette)
 *  APU:  WAVE-4   (4 channels, 6 waveforms, ADSR envelopes)
 *  RAM:  128 KB   (expandable via modules)
 *  CART: 64 KB    (no token limit)
 *
 *  Scripting: Lua 5.4  ·  PickUp (optional)
 */

#ifndef CX8_H
#define CX8_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include <string.h>
#include <math.h>

/* ─── Display ──────────────────────────────────────────────── */
#define CX8_SCREEN_W        256
#define CX8_SCREEN_H        144
#define CX8_WINDOW_SCALE    4        /* default pixel scaling   */

/* ─── Palette ──────────────────────────────────────────────── */
#define CX8_PALETTE_SIZE    64

/* ─── Sprites ──────────────────────────────────────────────── */
#define CX8_SPRITE_COUNT    256      /* sprite slots            */
#define CX8_SPRITE_W        8        /* pixels per sprite       */
#define CX8_SPRITE_H        8
#define CX8_SPRITESHEET_W   128      /* 16 sprites across       */
#define CX8_SPRITESHEET_H   128      /* 16 sprites down         */

/* ─── Tile map ─────────────────────────────────────────────── */
#define CX8_MAP_W           256
#define CX8_MAP_H           256

/* ─── Memory ───────────────────────────────────────────────── */
#define CX8_BASE_RAM        (128 * 1024)   /* 128 KB            */
#define CX8_CART_SIZE       (64  * 1024)   /*  64 KB            */

/* ─── Audio ────────────────────────────────────────────────── */
#define CX8_AUDIO_CHANNELS  4
#define CX8_MAX_CHANNELS    16       /* with SYNTHWAVE-16 mod   */
#define CX8_SAMPLE_RATE     44100
#define CX8_AUDIO_BUFSIZE   1024

/* ─── Frame rate ───────────────────────────────────────────── */
#define CX8_FPS             60

/* ─── Font ─────────────────────────────────────────────────── */
#define CX8_GLYPH_W         4
#define CX8_GLYPH_H         6
#define CX8_FONT_CHARS      95       /* ASCII 32-126            */

/* ─── Input buttons ────────────────────────────────────────── */
#define CX8_BTN_LEFT        0
#define CX8_BTN_RIGHT       1
#define CX8_BTN_UP          2
#define CX8_BTN_DOWN        3
#define CX8_BTN_A           4
#define CX8_BTN_B           5
#define CX8_BTN_X           6
#define CX8_BTN_Y           7
#define CX8_BTN_COUNT       8

/* ─── Waveform types ───────────────────────────────────────── */
#define CX8_WAVE_SQUARE     0
#define CX8_WAVE_TRIANGLE   1
#define CX8_WAVE_SAW        2
#define CX8_WAVE_NOISE      3
#define CX8_WAVE_PULSE      4
#define CX8_WAVE_SINE       5

/* ─── Module IDs ───────────────────────────────────────────── */
#define CX8_MOD_TURBO_RAM     0      /* NovaByte Industries     */
#define CX8_MOD_SYNTHWAVE16   1      /* AudioLux Labs           */
#define CX8_MOD_PIXSTRETCH    2      /* VisualFX Co.            */
#define CX8_MOD_CART_DOUBLER  3      /* MegaMedia               */
#define CX8_MOD_NETLINK       4      /* CyberConnect            */
#define CX8_MOD_MAX           8

/* ─── Types ────────────────────────────────────────────────── */

/* RGBA colour */
typedef struct {
    uint8_t r, g, b, a;
} cx8_color_t;

/* Audio channel */
typedef struct {
    uint8_t  waveform;       /* CX8_WAVE_*              */
    float    frequency;      /* Hz                      */
    float    volume;         /* 0.0 – 1.0               */
    float    phase;          /* 0.0 – 1.0               */
    float    duty;           /* pulse duty cycle         */
    /* ADSR envelope (seconds) */
    float    attack;
    float    decay;
    float    sustain;        /* sustain level 0-1        */
    float    release;
    float    env_level;      /* current envelope level   */
    float    env_time;       /* time in current stage    */
    uint8_t  env_stage;      /* 0=off 1=A 2=D 3=S 4=R   */
    bool     active;
    uint32_t noise_state;    /* LFSR for noise           */
} cx8_channel_t;

/* Camera state */
typedef struct {
    float x, y;
    float zoom;              /* 1.0 = no zoom            */
} cx8_camera_t;

/* Clipping rectangle */
typedef struct {
    int x, y, w, h;
} cx8_clip_t;

/* Expansion module descriptor */
typedef struct {
    int         id;
    const char *name;
    const char *manufacturer;
    const char *description;
    const char *flavor;      /* aesthetic tagline        */
    bool        loaded;
} cx8_module_t;

/* Forward declaration */
typedef struct cx8_state cx8_state_t;

/* ─── Utility macros ───────────────────────────────────────── */
#define CX8_MIN(a,b)  ((a) < (b) ? (a) : (b))
#define CX8_MAX(a,b)  ((a) > (b) ? (a) : (b))
#define CX8_CLAMP(v,lo,hi) CX8_MAX((lo), CX8_MIN((v),(hi)))

#endif /* CX8_H */
