/*
 * cx8_ed_map.c — Chromaplex 8 Map Editor
 *
 * Layout (256×144):
 *   Top 9px:   tab bar (drawn by parent)
 *   Main area: scrollable map grid view (tiles shown as colours)
 *   Right:     tile selector strip (from spritesheet)
 *   Bottom:    coordinates / help
 */

#include "cx8_ed_map.h"
#include "cx8_gpu.h"
#include "cx8_font.h"
#include <string.h>
#include <stdio.h>

/* ─── State ────────────────────────────────────────────────── */

static uint8_t s_map[CX8_MAP_W * CX8_MAP_H];
static int  s_cur_x    = 0;     /* cursor tile x (0-255) */
static int  s_cur_y    = 0;     /* cursor tile y (0-255) */
static int  s_scroll_x = 0;    /* top-left visible tile x */
static int  s_scroll_y = 0;    /* top-left visible tile y */
static int  s_tile     = 1;     /* selected tile (sprite index) */
static int  s_blink    = 0;

/* Grid cell size in screen pixels */
#define CELL_SIZE     5
#define GRID_LEFT     4
#define GRID_TOP      12
#define GRID_COLS     ((200 - GRID_LEFT) / CELL_SIZE)   /* ~39 */
#define GRID_ROWS     ((CX8_SCREEN_H - 30 - GRID_TOP) / CELL_SIZE) /* ~20 */

/* Tile selector on the right */
#define TSEL_LEFT     (GRID_LEFT + GRID_COLS * CELL_SIZE + 4)
#define TSEL_TOP      12
#define TSEL_CELL     6
#define TSEL_COLS     4
#define TSEL_VIS_ROWS 16

/* ─── Helpers ──────────────────────────────────────────────── */

static uint8_t map_get(int x, int y)
{
    if (x < 0 || x >= CX8_MAP_W || y < 0 || y >= CX8_MAP_H) return 0;
    return s_map[y * CX8_MAP_W + x];
}

static void map_set(int x, int y, uint8_t tile)
{
    if (x < 0 || x >= CX8_MAP_W || y < 0 || y >= CX8_MAP_H) return;
    s_map[y * CX8_MAP_W + x] = tile;
}

/* Get the "dominant colour" of a sprite for map preview */
static uint8_t sprite_dominant_color(int idx)
{
    const uint8_t *sheet = cx8_gpu_get_spritesheet();
    int bx = (idx % 16) * 8;
    int by = (idx / 16) * 8;
    int counts[64] = {0};
    for (int py = 0; py < 8; py++)
        for (int px = 0; px < 8; px++) {
            uint8_t c = sheet[(by + py) * CX8_SPRITESHEET_W + (bx + px)];
            counts[c & 0x3F]++;
        }
    /* Find most common non-zero colour */
    int best = 0, best_c = 1;
    for (int i = 1; i < 64; i++) {
        if (counts[i] > best) { best = counts[i]; best_c = i; }
    }
    return (uint8_t)(best > 0 ? best_c : 1);
}

/* ─── Init / sync / shutdown ───────────────────────────────── */

void cx8_ed_map_init(void)
{
    const uint8_t *gpu_map = cx8_gpu_get_mapdata();
    memcpy(s_map, gpu_map, sizeof(s_map));
    s_cur_x = 0;
    s_cur_y = 0;
    s_scroll_x = 0;
    s_scroll_y = 0;
    s_tile = 1;
}

void cx8_ed_map_sync(void)
{
    uint8_t *gpu_map = cx8_gpu_get_mapdata();
    memcpy(gpu_map, s_map, sizeof(s_map));
}

void cx8_ed_map_shutdown(void)
{
    /* nothing */
}

/* ─── Update ───────────────────────────────────────────────── */

void cx8_ed_map_update(const SDL_Event *events, int count)
{
    s_blink++;

    for (int i = 0; i < count; i++) {
        const SDL_Event *e = &events[i];
        if (e->type != SDL_KEYDOWN) continue;

        SDL_Keycode key = e->key.keysym.sym;
        SDL_Keymod mod  = (SDL_Keymod)e->key.keysym.mod;
        bool shift = (mod & KMOD_SHIFT) != 0;

        if (shift) {
            /* Shift+arrows: change selected tile */
            switch (key) {
            case SDLK_LEFT:  s_tile = (s_tile - 1 + 256) % 256; break;
            case SDLK_RIGHT: s_tile = (s_tile + 1) % 256; break;
            case SDLK_UP:    s_tile = (s_tile - 16 + 256) % 256; break;
            case SDLK_DOWN:  s_tile = (s_tile + 16) % 256; break;
            default: break;
            }
        } else {
            switch (key) {
            /* Cursor movement */
            case SDLK_LEFT:
                if (s_cur_x > 0) s_cur_x--;
                break;
            case SDLK_RIGHT:
                if (s_cur_x < CX8_MAP_W - 1) s_cur_x++;
                break;
            case SDLK_UP:
                if (s_cur_y > 0) s_cur_y--;
                break;
            case SDLK_DOWN:
                if (s_cur_y < CX8_MAP_H - 1) s_cur_y++;
                break;

            /* Place tile (Z / Enter) */
            case SDLK_z:
            case SDLK_RETURN:
                map_set(s_cur_x, s_cur_y, (uint8_t)s_tile);
                break;

            /* Erase tile (X) */
            case SDLK_x:
                map_set(s_cur_x, s_cur_y, 0);
                break;

            /* Pick tile under cursor (C) */
            case SDLK_c:
                s_tile = map_get(s_cur_x, s_cur_y);
                break;

            /* Page movement */
            case SDLK_PAGEUP:
                s_cur_y -= GRID_ROWS;
                if (s_cur_y < 0) s_cur_y = 0;
                break;
            case SDLK_PAGEDOWN:
                s_cur_y += GRID_ROWS;
                if (s_cur_y >= CX8_MAP_H) s_cur_y = CX8_MAP_H - 1;
                break;

            default: break;
            }
        }
    }

    /* Keep cursor visible in scroll region */
    if (s_cur_x < s_scroll_x) s_scroll_x = s_cur_x;
    if (s_cur_x >= s_scroll_x + GRID_COLS) s_scroll_x = s_cur_x - GRID_COLS + 1;
    if (s_cur_y < s_scroll_y) s_scroll_y = s_cur_y;
    if (s_cur_y >= s_scroll_y + GRID_ROWS) s_scroll_y = s_cur_y - GRID_ROWS + 1;
}

/* ─── Draw ─────────────────────────────────────────────────── */

void cx8_ed_map_draw(int tab_y)
{
    /* ── Map grid ────────────────────────────────────────── */
    for (int gy = 0; gy < GRID_ROWS; gy++) {
        for (int gx = 0; gx < GRID_COLS; gx++) {
            int mx = s_scroll_x + gx;
            int my = s_scroll_y + gy;
            if (mx >= CX8_MAP_W || my >= CX8_MAP_H) continue;

            uint8_t tile = map_get(mx, my);
            int sx = GRID_LEFT + gx * CELL_SIZE;
            int sy = GRID_TOP + gy * CELL_SIZE + tab_y;

            if (tile == 0) {
                /* Empty tile: dark dot */
                cx8_gpu_pset(sx + 2, sy + 2, 1);
            } else {
                /* Show tile as its dominant colour */
                uint8_t c = sprite_dominant_color(tile);
                cx8_gpu_rectfill(sx, sy, sx + CELL_SIZE - 2, sy + CELL_SIZE - 2, c);
            }

            /* Cursor highlight */
            if (mx == s_cur_x && my == s_cur_y && (s_blink / 10) % 2 == 0) {
                cx8_gpu_rect(sx - 1, sy - 1, sx + CELL_SIZE - 1, sy + CELL_SIZE - 1, 7);
            }
        }
    }

    /* ── Tile selector (right side) ──────────────────────── */
    int ts_x = TSEL_LEFT;
    int ts_y = TSEL_TOP + tab_y;

    cx8_gpu_print("TILE", ts_x, ts_y - 8, 5);

    /* Show a 4×16 grid of sprite indices around current tile */
    int base_tile = (s_tile / (TSEL_COLS * TSEL_VIS_ROWS)) * (TSEL_COLS * TSEL_VIS_ROWS);
    for (int r = 0; r < TSEL_VIS_ROWS; r++) {
        for (int c = 0; c < TSEL_COLS; c++) {
            int tidx = base_tile + r * TSEL_COLS + c;
            if (tidx >= 256) continue;
            int tx = ts_x + c * TSEL_CELL;
            int ty = ts_y + r * TSEL_CELL;

            uint8_t dc = sprite_dominant_color(tidx);
            cx8_gpu_rectfill(tx, ty, tx + TSEL_CELL - 2, ty + TSEL_CELL - 2, dc);

            if (tidx == s_tile) {
                cx8_gpu_rect(tx - 1, ty - 1, tx + TSEL_CELL - 1, ty + TSEL_CELL - 1, 7);
            }
        }
    }

    /* ── Status bar ──────────────────────────────────────── */
    int bar_y = CX8_SCREEN_H - 9;
    cx8_gpu_rectfill(0, bar_y, CX8_SCREEN_W - 1, CX8_SCREEN_H - 1, 1);

    char status[80];
    snprintf(status, sizeof(status), "POS:%d,%d TILE:#%d", s_cur_x, s_cur_y, s_tile);
    cx8_gpu_print(status, 4, bar_y + 2, 5);
    cx8_gpu_print("Z:PLACE X:ERASE C:PICK", CX8_SCREEN_W - 120, bar_y + 2, 4);
}
