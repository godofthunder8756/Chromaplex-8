/*
 * cx8_ed_sfx.c — Chromaplex 8 SFX / Audio Editor
 *
 * Layout (256×144):
 *   Top 9px:    tab bar (drawn by parent)
 *   Left:       waveform visualisation & ADSR envelope graph
 *   Right:      parameter controls (frequency, waveform, ADSR knobs)
 *   Bottom:     channel selector, help text
 *
 * The editor stores up to 64 SFX presets (slots).
 * Each slot has: waveform, frequency, volume, ADSR, duty cycle.
 */

#include "cx8_ed_sfx.h"
#include "cx8_gpu.h"
#include "cx8_apu.h"
#include "cx8_font.h"
#include <string.h>
#include <stdio.h>
#include <math.h>

/* ─── SFX slot ─────────────────────────────────────────────── */

#define SFX_SLOT_COUNT  64

typedef struct {
    uint8_t  waveform;     /* 0-5 */
    float    frequency;    /* Hz (20-4000) */
    float    volume;       /* 0.0-1.0 */
    float    duty;         /* pulse duty (0.1-0.9) */
    float    attack;       /* seconds */
    float    decay;
    float    sustain;      /* 0-1 level */
    float    release;
} sfx_slot_t;

static sfx_slot_t s_slots[SFX_SLOT_COUNT];
static int  s_cur_slot    = 0;
static int  s_cur_param   = 0;   /* 0=wave 1=freq 2=vol 3=duty 4=A 5=D 6=S 7=R */
static int  s_blink       = 0;
static bool s_playing     = false;

#define PARAM_COUNT     8

static const char *s_param_names[PARAM_COUNT] = {
    "WAVE", "FREQ", "VOL", "DUTY", "ATK", "DEC", "SUS", "REL"
};

static const char *s_wave_names[] = {
    "SQUARE", "TRIANGLE", "SAW", "NOISE", "PULSE", "SINE"
};

/* ─── Layout constants ─────────────────────────────────────── */

#define VIS_LEFT    6
#define VIS_TOP     14
#define VIS_W       100
#define VIS_H       40

#define ENV_LEFT    6
#define ENV_TOP     60
#define ENV_W       100
#define ENV_H       30

#define PARAM_LEFT  116
#define PARAM_TOP   14
#define PARAM_H     10

#define SLOT_LEFT   4
#define SLOT_TOP    (CX8_SCREEN_H - 22)

/* ─── Init / shutdown ──────────────────────────────────────── */

void cx8_ed_sfx_init(void)
{
    for (int i = 0; i < SFX_SLOT_COUNT; i++) {
        s_slots[i].waveform  = CX8_WAVE_SQUARE;
        s_slots[i].frequency = 440.0f;
        s_slots[i].volume    = 0.5f;
        s_slots[i].duty      = 0.5f;
        s_slots[i].attack    = 0.05f;
        s_slots[i].decay     = 0.1f;
        s_slots[i].sustain   = 0.6f;
        s_slots[i].release   = 0.2f;
    }
    s_cur_slot  = 0;
    s_cur_param = 0;
    s_playing   = false;
}

void cx8_ed_sfx_shutdown(void)
{
    cx8_apu_stop(0);
}

/* ─── Helpers ──────────────────────────────────────────────── */

static void play_current(void)
{
    sfx_slot_t *s = &s_slots[s_cur_slot];
    cx8_apu_stop(0);
    cx8_apu_envelope(0, s->attack, s->decay, s->sustain, s->release);
    cx8_apu_play(0, s->frequency, s->volume, s->waveform, s->duty);
    s_playing = true;
}

static void adjust_param(int dir)
{
    sfx_slot_t *s = &s_slots[s_cur_slot];
    switch (s_cur_param) {
    case 0: /* waveform */
        s->waveform = (uint8_t)((s->waveform + dir + 6) % 6);
        break;
    case 1: /* frequency */
        s->frequency += dir * 10.0f;
        if (s->frequency < 20.0f) s->frequency = 20.0f;
        if (s->frequency > 4000.0f) s->frequency = 4000.0f;
        break;
    case 2: /* volume */
        s->volume += dir * 0.05f;
        if (s->volume < 0.0f) s->volume = 0.0f;
        if (s->volume > 1.0f) s->volume = 1.0f;
        break;
    case 3: /* duty */
        s->duty += dir * 0.05f;
        if (s->duty < 0.1f) s->duty = 0.1f;
        if (s->duty > 0.9f) s->duty = 0.9f;
        break;
    case 4: /* attack */
        s->attack += dir * 0.01f;
        if (s->attack < 0.0f) s->attack = 0.0f;
        if (s->attack > 2.0f) s->attack = 2.0f;
        break;
    case 5: /* decay */
        s->decay += dir * 0.01f;
        if (s->decay < 0.0f) s->decay = 0.0f;
        if (s->decay > 2.0f) s->decay = 2.0f;
        break;
    case 6: /* sustain */
        s->sustain += dir * 0.05f;
        if (s->sustain < 0.0f) s->sustain = 0.0f;
        if (s->sustain > 1.0f) s->sustain = 1.0f;
        break;
    case 7: /* release */
        s->release += dir * 0.01f;
        if (s->release < 0.0f) s->release = 0.0f;
        if (s->release > 2.0f) s->release = 2.0f;
        break;
    }
}

/* ─── Update ───────────────────────────────────────────────── */

void cx8_ed_sfx_update(const SDL_Event *events, int count)
{
    s_blink++;

    for (int i = 0; i < count; i++) {
        const SDL_Event *e = &events[i];
        if (e->type != SDL_KEYDOWN) continue;

        SDL_Keycode key = e->key.keysym.sym;
        SDL_Keymod mod  = (SDL_Keymod)e->key.keysym.mod;
        bool shift = (mod & KMOD_SHIFT) != 0;

        if (shift) {
            /* Shift+Left/Right: change slot */
            switch (key) {
            case SDLK_LEFT:  s_cur_slot = (s_cur_slot - 1 + SFX_SLOT_COUNT) % SFX_SLOT_COUNT; break;
            case SDLK_RIGHT: s_cur_slot = (s_cur_slot + 1) % SFX_SLOT_COUNT; break;
            default: break;
            }
        } else {
            switch (key) {
            /* Navigate parameters */
            case SDLK_UP:
                s_cur_param = (s_cur_param - 1 + PARAM_COUNT) % PARAM_COUNT;
                break;
            case SDLK_DOWN:
                s_cur_param = (s_cur_param + 1) % PARAM_COUNT;
                break;

            /* Adjust value */
            case SDLK_LEFT:
                adjust_param(-1);
                break;
            case SDLK_RIGHT:
                adjust_param(1);
                break;

            /* Play / preview (Z) */
            case SDLK_z:
            case SDLK_RETURN:
                play_current();
                break;

            /* Stop (X) */
            case SDLK_x:
                cx8_apu_stop(0);
                s_playing = false;
                break;

            /* Copy slot (C) — copy to next slot */
            case SDLK_c:
                if (s_cur_slot < SFX_SLOT_COUNT - 1)
                    s_slots[s_cur_slot + 1] = s_slots[s_cur_slot];
                break;

            default: break;
            }
        }
    }
}

/* ─── Draw ─────────────────────────────────────────────────── */

static void draw_waveform_preview(int left, int top, int w, int h, int tab_y)
{
    sfx_slot_t *s = &s_slots[s_cur_slot];
    int y0 = top + tab_y;

    /* Background */
    cx8_gpu_rectfill(left, y0, left + w - 1, y0 + h - 1, 1);
    /* Centre line */
    cx8_gpu_line(left, y0 + h / 2, left + w - 1, y0 + h / 2, 2);

    /* Draw waveform preview */
    for (int x = 0; x < w; x++) {
        float t = (float)x / (float)w * 2.0f; /* 2 cycles */
        float phase = fmodf(t, 1.0f);
        float val = 0.0f;

        switch (s->waveform) {
        case CX8_WAVE_SQUARE:
            val = phase < 0.5f ? 1.0f : -1.0f;
            break;
        case CX8_WAVE_TRIANGLE:
            val = phase < 0.5f ? (phase * 4.0f - 1.0f) : (3.0f - phase * 4.0f);
            break;
        case CX8_WAVE_SAW:
            val = 2.0f * phase - 1.0f;
            break;
        case CX8_WAVE_NOISE:
            val = ((float)(rand() % 200) / 100.0f) - 1.0f;
            break;
        case CX8_WAVE_PULSE:
            val = phase < s->duty ? 1.0f : -1.0f;
            break;
        case CX8_WAVE_SINE:
            val = sinf(phase * 2.0f * 3.14159f);
            break;
        }

        int py = y0 + h / 2 - (int)(val * (float)(h / 2 - 2));
        if (py >= y0 && py < y0 + h)
            cx8_gpu_pset(left + x, py, 47);
    }

    /* Label */
    cx8_gpu_print("WAVEFORM", left, y0 - 8, 5);
}

static void draw_envelope_graph(int left, int top, int w, int h, int tab_y)
{
    sfx_slot_t *s = &s_slots[s_cur_slot];
    int y0 = top + tab_y;

    /* Background */
    cx8_gpu_rectfill(left, y0, left + w - 1, y0 + h - 1, 1);

    /* Calculate ADSR pixel widths */
    float total = s->attack + s->decay + 0.3f + s->release; /* sustain is 0.3s display */
    if (total < 0.01f) total = 0.01f;

    int a_w = (int)(s->attack / total * (float)w);
    int d_w = (int)(s->decay / total * (float)w);
    int s_w = (int)(0.3f / total * (float)w);
    int r_w = w - a_w - d_w - s_w;
    if (r_w < 1) r_w = 1;

    /* Draw ADSR segments */
    int x0 = left;
    /* Attack: 0 → 1 */
    if (a_w > 0)
        cx8_gpu_line(x0, y0 + h - 1, x0 + a_w, y0, 42);
    x0 += a_w;
    /* Decay: 1 → sustain */
    int sus_y = y0 + h - 1 - (int)(s->sustain * (float)(h - 1));
    if (d_w > 0)
        cx8_gpu_line(x0, y0, x0 + d_w, sus_y, 61);
    x0 += d_w;
    /* Sustain: flat */
    if (s_w > 0)
        cx8_gpu_line(x0, sus_y, x0 + s_w, sus_y, 47);
    x0 += s_w;
    /* Release: sustain → 0 */
    if (r_w > 0)
        cx8_gpu_line(x0, sus_y, x0 + r_w, y0 + h - 1, 42);

    /* Label */
    cx8_gpu_print("ENVELOPE", left, y0 - 8, 5);
}

void cx8_ed_sfx_draw(int tab_y)
{
    /* Waveform preview */
    draw_waveform_preview(VIS_LEFT, VIS_TOP, VIS_W, VIS_H, tab_y);

    /* ADSR envelope graph */
    draw_envelope_graph(ENV_LEFT, ENV_TOP, ENV_W, ENV_H, tab_y);

    /* ── Parameter list ──────────────────────────────────── */
    sfx_slot_t *s = &s_slots[s_cur_slot];
    int px = PARAM_LEFT;
    int py = PARAM_TOP + tab_y;

    for (int p = 0; p < PARAM_COUNT; p++) {
        bool sel = (p == s_cur_param);

        if (sel && (s_blink / 10) % 2 == 0) {
            cx8_gpu_rectfill(px - 2, py - 1, CX8_SCREEN_W - 4, py + 7, 1);
        }

        /* Name */
        uint8_t nc = sel ? 7 : 4;
        cx8_gpu_print(s_param_names[p], px, py, nc);

        /* Value */
        char vbuf[32];
        switch (p) {
        case 0: snprintf(vbuf, sizeof(vbuf), "%s", s_wave_names[s->waveform]); break;
        case 1: snprintf(vbuf, sizeof(vbuf), "%.0f HZ", s->frequency); break;
        case 2: snprintf(vbuf, sizeof(vbuf), "%.0f%%", s->volume * 100.0f); break;
        case 3: snprintf(vbuf, sizeof(vbuf), "%.0f%%", s->duty * 100.0f); break;
        case 4: snprintf(vbuf, sizeof(vbuf), "%.2fs", s->attack); break;
        case 5: snprintf(vbuf, sizeof(vbuf), "%.2fs", s->decay); break;
        case 6: snprintf(vbuf, sizeof(vbuf), "%.0f%%", s->sustain * 100.0f); break;
        case 7: snprintf(vbuf, sizeof(vbuf), "%.2fs", s->release); break;
        }

        uint8_t vc = sel ? 61 : 5;
        cx8_gpu_print(vbuf, px + 30, py, vc);

        /* Arrows for selected */
        if (sel) {
            cx8_gpu_print("<", px + 28 - 6, py, 42);
            int vw = (int)strlen(vbuf) * 5;
            cx8_gpu_print(">", px + 30 + vw + 2, py, 42);
        }

        py += PARAM_H;
    }

    /* ── Slot selector ───────────────────────────────────── */
    int sy = SLOT_TOP + tab_y;
    cx8_gpu_rectfill(0, sy, CX8_SCREEN_W - 1, sy + 10, 1);

    char sbuf[32];
    snprintf(sbuf, sizeof(sbuf), "SFX #%02d", s_cur_slot);
    cx8_gpu_print(sbuf, SLOT_LEFT, sy + 2, 7);

    /* Slot indicator dots */
    for (int i = 0; i < 16; i++) {
        int dx = SLOT_LEFT + 48 + i * 5;
        uint8_t dc = (i == s_cur_slot % 16) ? 61 : 2;
        cx8_gpu_rectfill(dx, sy + 3, dx + 2, sy + 5, dc);
    }

    /* Playing indicator */
    if (s_playing) {
        cx8_gpu_print(">>", CX8_SCREEN_W - 60, sy + 2, 42);
    }

    /* ── Status / help ───────────────────────────────────── */
    int bar_y = CX8_SCREEN_H - 9;
    cx8_gpu_rectfill(0, bar_y, CX8_SCREEN_W - 1, CX8_SCREEN_H - 1, 1);
    cx8_gpu_print("Z:PLAY X:STOP", 4, bar_y + 2, 61);
    cx8_gpu_print("ARROWS:EDIT SHIFT:SLOT", CX8_SCREEN_W - 118, bar_y + 2, 4);
}
