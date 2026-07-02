/*
 * cx8_crt.c — Chromaplex 8 CRT Post-Processing
 *
 * Software-rendered CRT effects without requiring OpenGL/shaders.
 * Applied as a final pass on the ARGB32 framebuffer.
 */

#include "cx8_crt.h"
#include <math.h>
#include <stdio.h>
#include <string.h>

/* ─── State ────────────────────────────────────────────────── */

static int   s_mode = CX8_CRT_OFF;
static float s_scanline_intensity = 0.35f;  /* how dark the scanlines are */
static float s_vignette_strength  = 0.25f;  /* edge darkening             */
static float s_curvature          = 0.03f;  /* barrel distortion amount   */
static float s_bloom              = 0.15f;  /* bright pixel bleed         */

/* Lookup tables for speed */
static uint8_t s_scanline_lut[256];   /* pre-computed dimmed values */
static float   s_vignette_map[CX8_SCREEN_H]; /* per-row vignette factor */
static bool    s_luts_dirty = true;

/* ─── LUT Rebuilding ──────────────────────────────────────── */

static void rebuild_luts(void)
{
    /* Scanline dimming LUT */
    float dim = 1.0f - s_scanline_intensity;
    for (int i = 0; i < 256; i++) {
        s_scanline_lut[i] = (uint8_t)(i * dim);
    }

    /* Vignette: stronger at top/bottom edges */
    float half_h = (float)CX8_SCREEN_H * 0.5f;
    for (int y = 0; y < CX8_SCREEN_H; y++) {
        float dy = (y - half_h) / half_h;  /* -1 to +1 */
        float v = 1.0f - (dy * dy) * s_vignette_strength;
        if (v < 0.3f) v = 0.3f;
        s_vignette_map[y] = v;
    }

    s_luts_dirty = false;
}

/* ─── Helpers ──────────────────────────────────────────────── */

static inline uint8_t clamp8(int v)
{
    return (uint8_t)(v < 0 ? 0 : (v > 255 ? 255 : v));
}

static inline void pixel_components(uint32_t px, uint8_t *r, uint8_t *g, uint8_t *b)
{
    *r = (uint8_t)((px >> 16) & 0xFF);
    *g = (uint8_t)((px >> 8)  & 0xFF);
    *b = (uint8_t)(px & 0xFF);
}

static inline uint32_t make_pixel(uint8_t r, uint8_t g, uint8_t b)
{
    return (0xFFu << 24) | ((uint32_t)r << 16) | ((uint32_t)g << 8) | (uint32_t)b;
}

/* ─── Effect Passes ───────────────────────────────────────── */

/* Pass 1: Scanlines — darken every other row */
static void apply_scanlines(uint32_t *pixels, int w, int h)
{
    for (int y = 0; y < h; y += 2) {
        /* Darken even rows for that CRT gap look */
        uint32_t *row = pixels + (y + 1) * w;
        if (y + 1 >= h) break;

        for (int x = 0; x < w; x++) {
            uint8_t r, g, b;
            pixel_components(row[x], &r, &g, &b);
            row[x] = make_pixel(s_scanline_lut[r], s_scanline_lut[g], s_scanline_lut[b]);
        }
    }
}

/* Pass 2: Vignette — darken edges */
static void apply_vignette(uint32_t *pixels, int w, int h)
{
    float half_w = (float)w * 0.5f;

    for (int y = 0; y < h; y++) {
        float vy = s_vignette_map[y];
        uint32_t *row = pixels + y * w;

        for (int x = 0; x < w; x++) {
            float dx = (x - half_w) / half_w;  /* -1 to +1 */
            float vx = 1.0f - (dx * dx) * s_vignette_strength;
            if (vx < 0.3f) vx = 0.3f;
            float v = vx * vy;

            uint8_t r, g, b;
            pixel_components(row[x], &r, &g, &b);
            row[x] = make_pixel(
                (uint8_t)(r * v),
                (uint8_t)(g * v),
                (uint8_t)(b * v)
            );
        }
    }
}

/* Pass 3: Simple bloom — bright pixels bleed into neighbours */
static void apply_bloom(uint32_t *pixels, int w, int h)
{
    /* Single-pass horizontal bloom approximation */
    static uint32_t temp[CX8_SCREEN_W * CX8_SCREEN_H];
    memcpy(temp, pixels, (size_t)(w * h) * sizeof(uint32_t));

    float blend = s_bloom * 0.5f;

    for (int y = 0; y < h; y++) {
        uint32_t *src = temp + y * w;
        uint32_t *dst = pixels + y * w;

        for (int x = 1; x < w - 1; x++) {
            uint8_t r, g, b;
            uint8_t rl, gl, bl;
            uint8_t rr, gr, br;

            pixel_components(src[x], &r, &g, &b);
            pixel_components(src[x - 1], &rl, &gl, &bl);
            pixel_components(src[x + 1], &rr, &gr, &br);

            /* Only bloom from bright pixels */
            int brightness = (r + g + b) / 3;
            if (brightness > 128) {
                int nr = (int)r + (int)((rl + rr) * blend);
                int ng = (int)g + (int)((gl + gr) * blend);
                int nb = (int)b + (int)((bl + br) * blend);
                dst[x] = make_pixel(clamp8(nr), clamp8(ng), clamp8(nb));
            }
        }
    }
}

/* ─── Public API ──────────────────────────────────────────── */

void cx8_crt_init(void)
{
    s_mode = CX8_CRT_OFF;
    s_luts_dirty = true;
    printf("[CX8-CRT] Post-processing ready (F9 to cycle modes)\n");
}

void cx8_crt_set_mode(int mode)
{
    if (mode < CX8_CRT_OFF || mode > CX8_CRT_HEAVY) mode = CX8_CRT_OFF;
    s_mode = mode;
    s_luts_dirty = true;

    static const char *names[] = { "OFF", "SCANLINES", "FULL", "HEAVY" };
    printf("[CX8-CRT] Mode: %s\n", names[mode]);
}

int cx8_crt_get_mode(void)
{
    return s_mode;
}

void cx8_crt_cycle(void)
{
    int next = (s_mode + 1) % (CX8_CRT_HEAVY + 1);
    cx8_crt_set_mode(next);
}

void cx8_crt_apply(uint32_t *pixels, int w, int h)
{
    if (s_mode == CX8_CRT_OFF || !pixels) return;

    if (s_luts_dirty) rebuild_luts();

    /* Always apply scanlines in any CRT mode */
    apply_scanlines(pixels, w, h);

    if (s_mode >= CX8_CRT_FULL) {
        apply_vignette(pixels, w, h);
    }

    if (s_mode >= CX8_CRT_HEAVY) {
        apply_bloom(pixels, w, h);
    }
}

void cx8_crt_set_scanline_intensity(float intensity)
{
    if (intensity < 0.0f) intensity = 0.0f;
    if (intensity > 1.0f) intensity = 1.0f;
    s_scanline_intensity = intensity;
    s_luts_dirty = true;
}

void cx8_crt_set_vignette_strength(float strength)
{
    if (strength < 0.0f) strength = 0.0f;
    if (strength > 1.0f) strength = 1.0f;
    s_vignette_strength = strength;
    s_luts_dirty = true;
}

void cx8_crt_set_curvature(float amount)
{
    if (amount < 0.0f) amount = 0.0f;
    if (amount > 1.0f) amount = 1.0f;
    s_curvature = amount;
}

void cx8_crt_set_bloom(float amount)
{
    if (amount < 0.0f) amount = 0.0f;
    if (amount > 1.0f) amount = 1.0f;
    s_bloom = amount;
}
