/*
 * cx8_pixstretch.c — Chromaplex 8 PIXEL-STRETCH PRO Visual FX
 *
 * All effects are applied as a post-processing pass on the ARGB
 * pixel buffer, after the GPU has rendered the frame.
 */

#include "cx8_pixstretch.h"
#include "cx8_gpu.h"
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <string.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

/* ─── State ────────────────────────────────────────────────── */
static cx8_fx_state_t s_fx;

/* ─── Dither matrices ──────────────────────────────────────── */

/* 2×2 Bayer matrix (values 0-3, normalised to 0-1) */
static const float BAYER2[2][2] = {
    { 0.0f / 4.0f, 2.0f / 4.0f },
    { 3.0f / 4.0f, 1.0f / 4.0f },
};

/* 4×4 Bayer matrix */
static const float BAYER4[4][4] = {
    {  0.0f/16.0f,  8.0f/16.0f,  2.0f/16.0f, 10.0f/16.0f },
    { 12.0f/16.0f,  4.0f/16.0f, 14.0f/16.0f,  6.0f/16.0f },
    {  3.0f/16.0f, 11.0f/16.0f,  1.0f/16.0f,  9.0f/16.0f },
    { 15.0f/16.0f,  7.0f/16.0f, 13.0f/16.0f,  5.0f/16.0f },
};

/* ─── Lifecycle ────────────────────────────────────────────── */

void cx8_fx_init(void)
{
    memset(&s_fx, 0, sizeof(s_fx));
    s_fx.fade_level  = 1.0f;
    s_fx.fade_target = 1.0f;
    printf("[PIXSTRETCH] Visual FX engine initialised\n");
}

void cx8_fx_shutdown(void)
{
    memset(&s_fx, 0, sizeof(s_fx));
}

void cx8_fx_enable(void)
{
    s_fx.enabled = true;
    printf("[PIXSTRETCH] ╔══════════════════════════════════════╗\n");
    printf("[PIXSTRETCH] ║  PIXEL-STRETCH PRO by VisualFX Co.  ║\n");
    printf("[PIXSTRETCH] ║  \"Every pixel tells a story.\"        ║\n");
    printf("[PIXSTRETCH] ║  Advanced FX pipeline enabled        ║\n");
    printf("[PIXSTRETCH] ╚══════════════════════════════════════╝\n");
}

bool cx8_fx_is_enabled(void)
{
    return s_fx.enabled;
}

/* ─── Palette Cycling ──────────────────────────────────────── */

void cx8_fx_cycle(int start, int end, float speed)
{
    if (!s_fx.enabled) return;
    if (start < 0 || start >= CX8_PALETTE_SIZE) return;
    if (end < 0 || end >= CX8_PALETTE_SIZE) return;
    if (start == end) return;

    /* Check if this range is already cycling */
    for (int i = 0; i < s_fx.cycle_count; i++) {
        if (s_fx.cycles[i].start == start && s_fx.cycles[i].active) {
            s_fx.cycles[i].end   = end;
            s_fx.cycles[i].speed = speed;
            return;
        }
    }

    /* Find a free slot */
    for (int i = 0; i < CX8_FX_MAX_CYCLES; i++) {
        if (!s_fx.cycles[i].active) {
            s_fx.cycles[i].start  = start;
            s_fx.cycles[i].end    = end;
            s_fx.cycles[i].speed  = speed;
            s_fx.cycles[i].phase  = 0.0f;
            s_fx.cycles[i].active = true;
            if (i >= s_fx.cycle_count) s_fx.cycle_count = i + 1;
            return;
        }
    }
}

void cx8_fx_cycle_stop(int start)
{
    for (int i = 0; i < s_fx.cycle_count; i++) {
        if (s_fx.cycles[i].start == start && s_fx.cycles[i].active) {
            s_fx.cycles[i].active = false;

            /* Restore original palette for this range */
            /* (palette is restored via pal_reset or next cycle) */
        }
    }
}

/* ─── Dithering ────────────────────────────────────────────── */

void cx8_fx_dither(int mode)
{
    if (mode >= 0 && mode < CX8_DITHER_MAX)
        s_fx.dither_mode = mode;
}

int cx8_fx_get_dither(void)
{
    return s_fx.dither_mode;
}

bool cx8_fx_dither_test(int x, int y, float threshold)
{
    if (!s_fx.enabled || s_fx.dither_mode == CX8_DITHER_NONE) return true;

    float d = 0.5f;

    switch (s_fx.dither_mode) {
    case CX8_DITHER_BAYER2:
        d = BAYER2[y & 1][x & 1];
        break;
    case CX8_DITHER_BAYER4:
        d = BAYER4[y & 3][x & 3];
        break;
    case CX8_DITHER_CHECKER:
        d = ((x + y) & 1) ? 1.0f : 0.0f;
        break;
    case CX8_DITHER_HLINE:
        d = (y & 1) ? 1.0f : 0.0f;
        break;
    case CX8_DITHER_VLINE:
        d = (x & 1) ? 1.0f : 0.0f;
        break;
    case CX8_DITHER_DIAG:
        d = ((x + y) & 3) < 2 ? 0.0f : 1.0f;
        break;
    }

    return threshold > d;
}

/* ─── Fade ─────────────────────────────────────────────────── */

void cx8_fx_fade(float target, float speed)
{
    if (target < 0.0f) target = 0.0f;
    if (target > 1.0f) target = 1.0f;
    if (speed <= 0.0f) speed = 1.0f;
    s_fx.fade_target = target;
    s_fx.fade_speed  = speed;
}

float cx8_fx_get_fade(void)
{
    return s_fx.fade_level;
}

/* ─── Screen Shake ─────────────────────────────────────────── */

void cx8_fx_shake(float amount, float duration)
{
    if (!s_fx.enabled) return;
    s_fx.shake_amount = amount;
    s_fx.shake_time   = duration;
}

/* ─── Colour Tint ──────────────────────────────────────────── */

void cx8_fx_tint(float r, float g, float b, float amount)
{
    s_fx.tint_r = r;
    s_fx.tint_g = g;
    s_fx.tint_b = b;
    s_fx.tint_amount = amount;
}

/* ─── Flash ────────────────────────────────────────────────── */

void cx8_fx_flash(uint8_t col, int frames)
{
    s_fx.flash_col    = col;
    s_fx.flash_frames = frames;
}

/* ─── Scanline Wave ────────────────────────────────────────── */

void cx8_fx_wave(float amplitude, float frequency)
{
    s_fx.wave_amp  = amplitude;
    s_fx.wave_freq = frequency;
}

/* ─── Reset ────────────────────────────────────────────────── */

void cx8_fx_reset(void)
{
    bool was_enabled = s_fx.enabled;
    memset(&s_fx, 0, sizeof(s_fx));
    s_fx.enabled     = was_enabled;
    s_fx.fade_level  = 1.0f;
    s_fx.fade_target = 1.0f;
}

/* ─── Per-frame update ─────────────────────────────────────── */

void cx8_fx_update(float dt)
{
    if (!s_fx.enabled) return;

    /* Update palette cycling */
    for (int i = 0; i < s_fx.cycle_count; i++) {
        cx8_fx_cycle_t *c = &s_fx.cycles[i];
        if (!c->active) continue;

        c->phase += c->speed * dt;
        while (c->phase >= 1.0f) c->phase -= 1.0f;
        while (c->phase < 0.0f)  c->phase += 1.0f;

        /* Rotate the palette entries in this range */
        int range = abs(c->end - c->start) + 1;
        int shift = (int)(c->phase * range) % range;

        if (shift > 0) {
            int lo = (c->start < c->end) ? c->start : c->end;
            cx8_color_t temp[CX8_PALETTE_SIZE];
            for (int j = 0; j < range; j++) {
                int src_idx = lo + (j + shift) % range;
                temp[j] = cx8_gpu_get_color((uint8_t)src_idx);
            }
            for (int j = 0; j < range; j++) {
                cx8_gpu_set_color((uint8_t)(lo + j), temp[j]);
            }
        }
    }

    /* Update fade */
    if (s_fx.fade_level != s_fx.fade_target) {
        float delta = s_fx.fade_speed * dt;
        if (s_fx.fade_level < s_fx.fade_target) {
            s_fx.fade_level += delta;
            if (s_fx.fade_level > s_fx.fade_target)
                s_fx.fade_level = s_fx.fade_target;
        } else {
            s_fx.fade_level -= delta;
            if (s_fx.fade_level < s_fx.fade_target)
                s_fx.fade_level = s_fx.fade_target;
        }
    }

    /* Update shake */
    if (s_fx.shake_time > 0.0f) {
        s_fx.shake_time -= dt;
        float amt = s_fx.shake_amount;
        s_fx.shake_x = (int)((((float)rand() / RAND_MAX) * 2.0f - 1.0f) * amt);
        s_fx.shake_y = (int)((((float)rand() / RAND_MAX) * 2.0f - 1.0f) * amt);
    } else {
        s_fx.shake_x = 0;
        s_fx.shake_y = 0;
        s_fx.shake_amount = 0;
    }

    /* Update flash */
    if (s_fx.flash_frames > 0) {
        s_fx.flash_frames--;
    }

    /* Update wave phase */
    if (s_fx.wave_amp > 0.0f) {
        s_fx.wave_phase += dt * 2.0f;
        if (s_fx.wave_phase > 1000.0f) s_fx.wave_phase -= 1000.0f;
    }
}

/* ─── Apply to pixel buffer ───────────────────────────────── */

void cx8_fx_apply(uint32_t *pixels, int width, int height)
{
    if (!s_fx.enabled) return;

    /* ── Flash overlay ─────────────────────────────────────── */
    if (s_fx.flash_frames > 0) {
        cx8_color_t fc = cx8_gpu_get_color(s_fx.flash_col);
        uint32_t flash_argb = (0xFFu << 24) | ((uint32_t)fc.r << 16)
                             | ((uint32_t)fc.g << 8) | (uint32_t)fc.b;
        /* Blend flash at 50% */
        for (int i = 0; i < width * height; i++) {
            uint32_t p = pixels[i];
            uint8_t pr = (p >> 16) & 0xFF;
            uint8_t pg = (p >> 8)  & 0xFF;
            uint8_t pb = p & 0xFF;
            pr = (uint8_t)((pr + fc.r) / 2);
            pg = (uint8_t)((pg + fc.g) / 2);
            pb = (uint8_t)((pb + fc.b) / 2);
            pixels[i] = (0xFFu << 24) | ((uint32_t)pr << 16)
                       | ((uint32_t)pg << 8) | (uint32_t)pb;
        }
        (void)flash_argb;
    }

    /* ── Scanline wave distortion ──────────────────────────── */
    if (s_fx.wave_amp > 0.0f) {
        /* Temporary buffer for wave effect */
        static uint32_t wave_buf[256 * 144];
        memcpy(wave_buf, pixels, width * height * sizeof(uint32_t));

        for (int y = 0; y < height; y++) {
            float offset = s_fx.wave_amp * sinf(
                (float)(y * s_fx.wave_freq / height + s_fx.wave_phase)
                * 2.0f * (float)M_PI);
            int shift = (int)offset;

            for (int x = 0; x < width; x++) {
                int sx = x - shift;
                if (sx < 0) sx = 0;
                if (sx >= width) sx = width - 1;
                pixels[y * width + x] = wave_buf[y * width + sx];
            }
        }
    }

    /* ── Screen shake (shift pixels) ───────────────────────── */
    if (s_fx.shake_x != 0 || s_fx.shake_y != 0) {
        static uint32_t shake_buf[256 * 144];
        memcpy(shake_buf, pixels, width * height * sizeof(uint32_t));

        for (int y = 0; y < height; y++) {
            for (int x = 0; x < width; x++) {
                int sx = x - s_fx.shake_x;
                int sy = y - s_fx.shake_y;
                if (sx >= 0 && sx < width && sy >= 0 && sy < height)
                    pixels[y * width + x] = shake_buf[sy * width + sx];
                else
                    pixels[y * width + x] = 0xFF000000; /* black border */
            }
        }
    }

    /* ── Fade ──────────────────────────────────────────────── */
    if (s_fx.fade_level < 1.0f) {
        int fade = (int)(s_fx.fade_level * 255.0f);
        if (fade < 0) fade = 0;
        if (fade > 255) fade = 255;

        for (int i = 0; i < width * height; i++) {
            uint32_t p = pixels[i];
            uint8_t r = (uint8_t)(((p >> 16) & 0xFF) * fade / 255);
            uint8_t g = (uint8_t)(((p >> 8)  & 0xFF) * fade / 255);
            uint8_t b = (uint8_t)((p & 0xFF)         * fade / 255);
            pixels[i] = (0xFFu << 24) | ((uint32_t)r << 16)
                       | ((uint32_t)g << 8) | (uint32_t)b;
        }
    }

    /* ── Colour tint ───────────────────────────────────────── */
    if (s_fx.tint_amount > 0.0f) {
        float a = s_fx.tint_amount;
        if (a > 1.0f) a = 1.0f;
        float inv_a = 1.0f - a;

        uint8_t tr = (uint8_t)(s_fx.tint_r * 255.0f);
        uint8_t tg = (uint8_t)(s_fx.tint_g * 255.0f);
        uint8_t tb = (uint8_t)(s_fx.tint_b * 255.0f);

        for (int i = 0; i < width * height; i++) {
            uint32_t p = pixels[i];
            uint8_t r = (uint8_t)(((p >> 16) & 0xFF) * inv_a + tr * a);
            uint8_t g = (uint8_t)(((p >> 8)  & 0xFF) * inv_a + tg * a);
            uint8_t b = (uint8_t)((p & 0xFF)         * inv_a + tb * a);
            pixels[i] = (0xFFu << 24) | ((uint32_t)r << 16)
                       | ((uint32_t)g << 8) | (uint32_t)b;
        }
    }

    /* ── Dithering post-process ────────────────────────────── */
    if (s_fx.dither_mode != CX8_DITHER_NONE) {
        /* Apply subtle dithering by reducing colour precision
         * through the dither pattern */
        for (int y = 0; y < height; y++) {
            for (int x = 0; x < width; x++) {
                if (!cx8_fx_dither_test(x, y, 0.5f)) {
                    /* Darken every other pixel slightly */
                    int i = y * width + x;
                    uint32_t p = pixels[i];
                    uint8_t r = (uint8_t)(((p >> 16) & 0xFF) * 220 / 255);
                    uint8_t g = (uint8_t)(((p >> 8)  & 0xFF) * 220 / 255);
                    uint8_t b = (uint8_t)((p & 0xFF)         * 220 / 255);
                    pixels[i] = (0xFFu << 24) | ((uint32_t)r << 16)
                               | ((uint32_t)g << 8) | (uint32_t)b;
                }
            }
        }
    }
}
