/*
 * cx8_ed_sprite.c — Chromaplex 8 Sprite Editor
 *
 * Layout (256×144):
 *   Top 9px: tab bar (drawn by parent)
 *   Left:    zoomed sprite canvas (8×8 pixels, 10px each = 80×80)
 *   Right:   spritesheet overview (128×128 scaled to 64×64)
 *   Bottom:  palette row (64 colours)
 */

#include "cx8_ed_sprite.h"
#include "cx8_gpu.h"
#include "cx8_font.h"
#include <string.h>
#include <stdio.h>

/* ─── State ────────────────────────────────────────────────── */

/* Local copy of the spritesheet for editing */
static uint8_t s_sheet[CX8_SPRITESHEET_W * CX8_SPRITESHEET_H];
static int  s_sel_sprite = 0;     /* selected sprite index 0-255 */
static int  s_cur_x      = 0;     /* cursor x within sprite (0-7) */
static int  s_cur_y      = 0;     /* cursor y within sprite (0-7) */
static int  s_color      = 7;     /* current paint colour */
static int  s_blink       = 0;    /* cursor blink */
static bool s_drawing     = false; /* held-draw mode */

/* ─── Layout constants ─────────────────────────────────────── */

#define CANVAS_LEFT   4
#define CANVAS_TOP    12
#define CANVAS_ZOOM   9     /* each sprite pixel = 9 screen pixels */
#define CANVAS_SIZE   (8 * CANVAS_ZOOM)  /* 72 px */

#define SHEET_LEFT    88
#define SHEET_TOP     12
#define SHEET_SCALE   1     /* 1:1 → 128px, need to halve: 0.5 */

/* We'll draw the sheet at half size = 64×64 */
#define SHEET_DRAW_W  64
#define SHEET_DRAW_H  64

#define PAL_LEFT      4
#define PAL_TOP       (CX8_SCREEN_H - 20)
#define PAL_CELL      3

/* ─── Helpers ──────────────────────────────────────────────── */

static int sprite_base_x(int idx) { return (idx % 16) * 8; }
static int sprite_base_y(int idx) { return (idx / 16) * 8; }

static uint8_t sheet_get(int x, int y)
{
    if (x < 0 || x >= CX8_SPRITESHEET_W || y < 0 || y >= CX8_SPRITESHEET_H) return 0;
    return s_sheet[y * CX8_SPRITESHEET_W + x];
}

static void sheet_set(int x, int y, uint8_t c)
{
    if (x < 0 || x >= CX8_SPRITESHEET_W || y < 0 || y >= CX8_SPRITESHEET_H) return;
    s_sheet[y * CX8_SPRITESHEET_W + x] = c;
}

/* ─── Init / sync / shutdown ───────────────────────────────── */

void cx8_ed_sprite_init(void)
{
    /* Copy current GPU spritesheet */
    const uint8_t *gpu_sheet = cx8_gpu_get_spritesheet();
    memcpy(s_sheet, gpu_sheet, sizeof(s_sheet));
    s_sel_sprite = 0;
    s_cur_x = 0;
    s_cur_y = 0;
    s_color = 7;
    s_drawing = false;
}

void cx8_ed_sprite_sync(void)
{
    /* Write back to GPU */
    uint8_t *gpu_sheet = cx8_gpu_get_spritesheet();
    memcpy(gpu_sheet, s_sheet, sizeof(s_sheet));
}

void cx8_ed_sprite_shutdown(void)
{
    /* nothing */
}

/* ─── Update ───────────────────────────────────────────────── */

void cx8_ed_sprite_update(const SDL_Event *events, int count)
{
    s_blink++;

    for (int i = 0; i < count; i++) {
        const SDL_Event *e = &events[i];
        if (e->type != SDL_KEYDOWN) continue;

        SDL_Keycode key = e->key.keysym.sym;
        SDL_Keymod mod  = (SDL_Keymod)e->key.keysym.mod;
        bool shift = (mod & KMOD_SHIFT) != 0;

        if (shift) {
            /* Shift+arrows: navigate sprite selection */
            switch (key) {
            case SDLK_LEFT:  s_sel_sprite = (s_sel_sprite - 1 + 256) % 256; break;
            case SDLK_RIGHT: s_sel_sprite = (s_sel_sprite + 1) % 256; break;
            case SDLK_UP:    s_sel_sprite = (s_sel_sprite - 16 + 256) % 256; break;
            case SDLK_DOWN:  s_sel_sprite = (s_sel_sprite + 16) % 256; break;
            default: break;
            }
        } else {
            switch (key) {
            /* Cursor movement within sprite */
            case SDLK_LEFT:  s_cur_x = (s_cur_x - 1 + 8) % 8; break;
            case SDLK_RIGHT: s_cur_x = (s_cur_x + 1) % 8; break;
            case SDLK_UP:    s_cur_y = (s_cur_y - 1 + 8) % 8; break;
            case SDLK_DOWN:  s_cur_y = (s_cur_y + 1) % 8; break;

            /* Draw (Z / Enter) */
            case SDLK_z:
            case SDLK_RETURN: {
                int bx = sprite_base_x(s_sel_sprite);
                int by = sprite_base_y(s_sel_sprite);
                sheet_set(bx + s_cur_x, by + s_cur_y, (uint8_t)s_color);
                break;
            }

            /* Erase (X) - set to colour 0 */
            case SDLK_x: {
                int bx = sprite_base_x(s_sel_sprite);
                int by = sprite_base_y(s_sel_sprite);
                sheet_set(bx + s_cur_x, by + s_cur_y, 0);
                break;
            }

            /* Colour pick from cursor (C) */
            case SDLK_c: {
                int bx = sprite_base_x(s_sel_sprite);
                int by = sprite_base_y(s_sel_sprite);
                s_color = sheet_get(bx + s_cur_x, by + s_cur_y);
                break;
            }

            /* Cycle colour up/down with < > */
            case SDLK_COMMA:
            case SDLK_MINUS:
                s_color = (s_color - 1 + 64) % 64;
                break;
            case SDLK_PERIOD:
            case SDLK_EQUALS:
                s_color = (s_color + 1) % 64;
                break;

            /* Fill entire sprite with current colour (F) */
            case SDLK_f: {
                int bx = sprite_base_x(s_sel_sprite);
                int by = sprite_base_y(s_sel_sprite);
                for (int py = 0; py < 8; py++)
                    for (int px = 0; px < 8; px++)
                        sheet_set(bx + px, by + py, (uint8_t)s_color);
                break;
            }

            /* Clear sprite (D) */
            case SDLK_d: {
                int bx = sprite_base_x(s_sel_sprite);
                int by = sprite_base_y(s_sel_sprite);
                for (int py = 0; py < 8; py++)
                    for (int px = 0; px < 8; px++)
                        sheet_set(bx + px, by + py, 0);
                break;
            }

            default: break;
            }
        }
    }
}

/* ─── Draw ─────────────────────────────────────────────────── */

void cx8_ed_sprite_draw(int tab_y)
{
    int bx = sprite_base_x(s_sel_sprite);
    int by = sprite_base_y(s_sel_sprite);

    /* ── Zoomed canvas ──────────────────────────────────── */
    /* Background grid */
    cx8_gpu_rectfill(CANVAS_LEFT - 1, CANVAS_TOP - 1 + tab_y,
                     CANVAS_LEFT + CANVAS_SIZE, CANVAS_TOP + CANVAS_SIZE + tab_y, 1);

    for (int py = 0; py < 8; py++) {
        for (int px = 0; px < 8; px++) {
            uint8_t c = sheet_get(bx + px, by + py);
            int sx = CANVAS_LEFT + px * CANVAS_ZOOM;
            int sy = CANVAS_TOP + py * CANVAS_ZOOM + tab_y;
            cx8_gpu_rectfill(sx, sy, sx + CANVAS_ZOOM - 2, sy + CANVAS_ZOOM - 2, c);
        }
    }

    /* Cursor on canvas */
    if ((s_blink / 10) % 2 == 0) {
        int cx = CANVAS_LEFT + s_cur_x * CANVAS_ZOOM;
        int cy = CANVAS_TOP + s_cur_y * CANVAS_ZOOM + tab_y;
        cx8_gpu_rect(cx - 1, cy - 1, cx + CANVAS_ZOOM - 1, cy + CANVAS_ZOOM - 1, 7);
    }

    /* Sprite index label */
    char label[32];
    snprintf(label, sizeof(label), "SPR #%d", s_sel_sprite);
    cx8_gpu_print(label, CANVAS_LEFT, CANVAS_TOP + CANVAS_SIZE + 3 + tab_y, 7);

    snprintf(label, sizeof(label), "(%d,%d)", s_cur_x, s_cur_y);
    cx8_gpu_print(label, CANVAS_LEFT, CANVAS_TOP + CANVAS_SIZE + 11 + tab_y, 4);

    /* ── Spritesheet overview (half size) ────────────────── */
    int so_left = SHEET_LEFT;
    int so_top  = SHEET_TOP + tab_y;

    /* Outline */
    cx8_gpu_rect(so_left - 1, so_top - 1,
                 so_left + SHEET_DRAW_W, so_top + SHEET_DRAW_H, 1);

    /* Draw sheet at half resolution */
    for (int y = 0; y < CX8_SPRITESHEET_H; y += 2) {
        for (int x = 0; x < CX8_SPRITESHEET_W; x += 2) {
            uint8_t c = sheet_get(x, y);
            cx8_gpu_pset(so_left + x / 2, so_top + y / 2, c);
        }
    }

    /* Highlight selected sprite on the overview */
    int sel_ox = so_left + (bx / 2);
    int sel_oy = so_top + (by / 2);
    cx8_gpu_rect(sel_ox - 1, sel_oy - 1, sel_ox + 4, sel_oy + 4, 7);

    /* ── Preview: actual-size sprite ─────────────────────── */
    int prev_left = so_left + SHEET_DRAW_W + 8;
    int prev_top  = so_top;
    cx8_gpu_rectfill(prev_left - 1, prev_top - 1,
                     prev_left + 16, prev_top + 16, 1);
    /* Draw 2x preview */
    for (int py = 0; py < 8; py++) {
        for (int px = 0; px < 8; px++) {
            uint8_t c = sheet_get(bx + px, by + py);
            if (c > 0) {
                cx8_gpu_pset(prev_left + px * 2, prev_top + py * 2, c);
                cx8_gpu_pset(prev_left + px * 2 + 1, prev_top + py * 2, c);
                cx8_gpu_pset(prev_left + px * 2, prev_top + py * 2 + 1, c);
                cx8_gpu_pset(prev_left + px * 2 + 1, prev_top + py * 2 + 1, c);
            }
        }
    }

    /* ── Palette row ─────────────────────────────────────── */
    int pal_y = PAL_TOP + tab_y;
    for (int i = 0; i < 64; i++) {
        int px = PAL_LEFT + (i % 32) * PAL_CELL;
        int py = pal_y + (i / 32) * (PAL_CELL + 1);
        cx8_gpu_rectfill(px, py, px + PAL_CELL - 1, py + PAL_CELL - 1, (uint8_t)i);
        if (i == s_color) {
            cx8_gpu_rect(px - 1, py - 1, px + PAL_CELL, py + PAL_CELL, 7);
        }
    }

    /* Current colour swatch */
    cx8_gpu_rectfill(PAL_LEFT + 32 * PAL_CELL + 6, pal_y,
                     PAL_LEFT + 32 * PAL_CELL + 14, pal_y + PAL_CELL * 2, (uint8_t)s_color);
    snprintf(label, sizeof(label), "%02d", s_color);
    cx8_gpu_print(label, PAL_LEFT + 32 * PAL_CELL + 18, pal_y + 1, 7);

    /* ── Help text ───────────────────────────────────────── */
    cx8_gpu_print("Z:DRAW X:ERASE C:PICK", so_left, so_top + SHEET_DRAW_H + 4, 4);
    cx8_gpu_print("-/+:COLOR  F:FILL D:CLR", so_left, so_top + SHEET_DRAW_H + 12, 4);
    cx8_gpu_print("SHIFT+DIR:SEL SPR", so_left, so_top + SHEET_DRAW_H + 20, 3);
}
