/*
 * cx8_gpu.c — Chromaplex 8 PRISM-64 Graphics Processing Unit
 *
 * Implements all rendering: primitives, sprites with hardware
 * scaling/rotation, tile-map drawing, camera with zoom, and
 * palette management.
 */

#include "cx8_gpu.h"
#include "cx8_font.h"
#include <stdlib.h>
#include <stdio.h>
#include <math.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

/* ─── Internal state ───────────────────────────────────────── */
static uint8_t      s_vram[CX8_SCREEN_W * CX8_SCREEN_H];
static uint8_t      s_sprites[CX8_SPRITESHEET_W * CX8_SPRITESHEET_H];
static uint8_t      s_map[CX8_MAP_W * CX8_MAP_H];
static cx8_color_t  s_palette[CX8_PALETTE_SIZE];
static uint8_t      s_pal_remap[CX8_PALETTE_SIZE];   /* draw-time remap */
static cx8_camera_t s_camera;
static cx8_clip_t   s_clip;

/* ─── Default 64-colour palette (SNES-inspired) ────────────── */
static const uint32_t s_default_palette[64] = {
    /* Row 0 — Monochrome */
    0x000000, 0x1B1B2F, 0x3D3D56, 0x5E5E7A,
    0x8585A0, 0xABABC4, 0xD5D5E3, 0xFFFFFF,
    /* Row 1 — Reds & warm */
    0x3A0008, 0x6B1020, 0x9B2035, 0xCC3850,
    0xE85070, 0xFF7890, 0xFFA0B0, 0xFFCCD5,
    /* Row 2 — Oranges & earth */
    0x3A1A00, 0x6B3400, 0x9B5500, 0xCC7700,
    0xEE9922, 0xFFBB44, 0xFFDD88, 0xFFF0CC,
    /* Row 3 — Yellows & lime */
    0x2A2A00, 0x556600, 0x77AA00, 0x99CC22,
    0xBBEE44, 0xDDFF77, 0xEEFFAA, 0xF5FFDD,
    /* Row 4 — Greens */
    0x003A10, 0x005520, 0x008838, 0x22AA55,
    0x44CC77, 0x77EE99, 0xAAFFBB, 0xDDFFDD,
    /* Row 5 — Blues */
    0x00152A, 0x003060, 0x004488, 0x2266BB,
    0x4488DD, 0x6699EE, 0x88BBFF, 0xAADDFF,
    /* Row 6 — Purples & magenta */
    0x220040, 0x441177, 0x6622AA, 0x8844CC,
    0xAA66EE, 0xCC88FF, 0xDD99FF, 0xEECCFF,
    /* Row 7 — Skin tones, metallics, neon accents */
    0x4A2A10, 0x7A5030, 0xAA7850, 0xD4A070,
    0xF0C898, 0x00FFCC, 0xFF0088, 0xFFFF00,
};

/* ─── Helpers ──────────────────────────────────────────────── */

static inline bool in_clip(int x, int y)
{
    return x >= s_clip.x && x < s_clip.x + s_clip.w
        && y >= s_clip.y && y < s_clip.y + s_clip.h;
}

static inline void raw_pset(int x, int y, uint8_t c)
{
    if (x >= 0 && x < CX8_SCREEN_W && y >= 0 && y < CX8_SCREEN_H && in_clip(x, y))
        s_vram[y * CX8_SCREEN_W + x] = s_pal_remap[c & 0x3F];
}

static inline uint8_t raw_pget(int x, int y)
{
    if (x >= 0 && x < CX8_SCREEN_W && y >= 0 && y < CX8_SCREEN_H)
        return s_vram[y * CX8_SCREEN_W + x];
    return 0;
}

/* Apply camera transform to a point */
static inline void cam_transform(int *x, int *y)
{
    if (s_camera.zoom != 1.0f) {
        float cx = CX8_SCREEN_W * 0.5f;
        float cy = CX8_SCREEN_H * 0.5f;
        *x = (int)((*x - s_camera.x - cx) * s_camera.zoom + cx);
        *y = (int)((*y - s_camera.y - cy) * s_camera.zoom + cy);
    } else {
        *x -= (int)s_camera.x;
        *y -= (int)s_camera.y;
    }
}

/* ─── Lifecycle ────────────────────────────────────────────── */

void cx8_gpu_init(void)
{
    /* Load default palette */
    for (int i = 0; i < CX8_PALETTE_SIZE; i++) {
        uint32_t c = s_default_palette[i];
        s_palette[i].r = (c >> 16) & 0xFF;
        s_palette[i].g = (c >> 8)  & 0xFF;
        s_palette[i].b = c & 0xFF;
        s_palette[i].a = 255;
        s_pal_remap[i] = (uint8_t)i;
    }
    /* Reset state */
    memset(s_vram,    0, sizeof(s_vram));
    memset(s_sprites, 0, sizeof(s_sprites));
    memset(s_map,     0, sizeof(s_map));
    s_camera = (cx8_camera_t){ 0, 0, 1.0f };
    s_clip   = (cx8_clip_t){ 0, 0, CX8_SCREEN_W, CX8_SCREEN_H };
    printf("[CX8-GPU] PRISM-64 initialised (%dx%d, %d colours)\n",
           CX8_SCREEN_W, CX8_SCREEN_H, CX8_PALETTE_SIZE);
}

void cx8_gpu_shutdown(void)
{
    /* Nothing to free — all static */
}

/* ─── Screen ───────────────────────────────────────────────── */

void cx8_gpu_cls(uint8_t color)
{
    memset(s_vram, color & 0x3F, sizeof(s_vram));
}

/* ─── Pixel ────────────────────────────────────────────────── */

void cx8_gpu_pset(int x, int y, uint8_t color)
{
    cam_transform(&x, &y);
    raw_pset(x, y, color);
}

uint8_t cx8_gpu_pget(int x, int y)
{
    cam_transform(&x, &y);
    return raw_pget(x, y);
}

/* ─── Line (Bresenham) ─────────────────────────────────────── */

void cx8_gpu_line(int x0, int y0, int x1, int y1, uint8_t color)
{
    cam_transform(&x0, &y0);
    cam_transform(&x1, &y1);

    int dx = abs(x1 - x0);
    int dy = -abs(y1 - y0);
    int sx = x0 < x1 ? 1 : -1;
    int sy = y0 < y1 ? 1 : -1;
    int err = dx + dy;

    for (;;) {
        raw_pset(x0, y0, color);
        if (x0 == x1 && y0 == y1) break;
        int e2 = 2 * err;
        if (e2 >= dy) { err += dy; x0 += sx; }
        if (e2 <= dx) { err += dx; y0 += sy; }
    }
}

/* ─── Rectangle ────────────────────────────────────────────── */

void cx8_gpu_rect(int x0, int y0, int x1, int y1, uint8_t color)
{
    cx8_gpu_line(x0, y0, x1, y0, color);
    cx8_gpu_line(x1, y0, x1, y1, color);
    cx8_gpu_line(x1, y1, x0, y1, color);
    cx8_gpu_line(x0, y1, x0, y0, color);
}

void cx8_gpu_rectfill(int x0, int y0, int x1, int y1, uint8_t color)
{
    int tx0 = x0, ty0 = y0, tx1 = x1, ty1 = y1;
    cam_transform(&tx0, &ty0);
    cam_transform(&tx1, &ty1);

    if (tx0 > tx1) { int t = tx0; tx0 = tx1; tx1 = t; }
    if (ty0 > ty1) { int t = ty0; ty0 = ty1; ty1 = t; }

    for (int y = ty0; y <= ty1; y++)
        for (int x = tx0; x <= tx1; x++)
            raw_pset(x, y, color);
}

/* ─── Circle (Midpoint algorithm) ──────────────────────────── */

void cx8_gpu_circ(int cx, int cy, int r, uint8_t color)
{
    cam_transform(&cx, &cy);
    int x = r, y = 0, d = 1 - r;

    while (x >= y) {
        raw_pset(cx + x, cy + y, color);
        raw_pset(cx - x, cy + y, color);
        raw_pset(cx + x, cy - y, color);
        raw_pset(cx - x, cy - y, color);
        raw_pset(cx + y, cy + x, color);
        raw_pset(cx - y, cy + x, color);
        raw_pset(cx + y, cy - x, color);
        raw_pset(cx - y, cy - x, color);
        y++;
        if (d <= 0) {
            d += 2 * y + 1;
        } else {
            x--;
            d += 2 * (y - x) + 1;
        }
    }
}

void cx8_gpu_circfill(int cx, int cy, int r, uint8_t color)
{
    cam_transform(&cx, &cy);
    int x = r, y = 0, d = 1 - r;

    while (x >= y) {
        for (int i = cx - x; i <= cx + x; i++) {
            raw_pset(i, cy + y, color);
            raw_pset(i, cy - y, color);
        }
        for (int i = cx - y; i <= cx + y; i++) {
            raw_pset(i, cy + x, color);
            raw_pset(i, cy - x, color);
        }
        y++;
        if (d <= 0) {
            d += 2 * y + 1;
        } else {
            x--;
            d += 2 * (y - x) + 1;
        }
    }
}

/* ─── Triangle ─────────────────────────────────────────────── */

void cx8_gpu_tri(int x0, int y0, int x1, int y1, int x2, int y2, uint8_t color)
{
    cx8_gpu_line(x0, y0, x1, y1, color);
    cx8_gpu_line(x1, y1, x2, y2, color);
    cx8_gpu_line(x2, y2, x0, y0, color);
}

static void fill_flat_bottom(int x0, int y0, int x1, int y1, int x2, int y2, uint8_t c)
{
    float slope1 = (float)(x1 - x0) / (float)(y1 - y0);
    float slope2 = (float)(x2 - x0) / (float)(y2 - y0);
    float cx1 = (float)x0, cx2 = (float)x0;
    for (int y = y0; y <= y1; y++) {
        int lx = (int)cx1, rx = (int)cx2;
        if (lx > rx) { int t = lx; lx = rx; rx = t; }
        for (int x = lx; x <= rx; x++) raw_pset(x, y, c);
        cx1 += slope1;
        cx2 += slope2;
    }
}

static void fill_flat_top(int x0, int y0, int x1, int y1, int x2, int y2, uint8_t c)
{
    float slope1 = (float)(x2 - x0) / (float)(y2 - y0);
    float slope2 = (float)(x2 - x1) / (float)(y2 - y1);
    float cx1 = (float)x2, cx2 = (float)x2;
    for (int y = y2; y >= y0; y--) {
        int lx = (int)cx1, rx = (int)cx2;
        if (lx > rx) { int t = lx; lx = rx; rx = t; }
        for (int x = lx; x <= rx; x++) raw_pset(x, y, c);
        cx1 -= slope1;
        cx2 -= slope2;
    }
}

void cx8_gpu_trifill(int x0, int y0, int x1, int y1, int x2, int y2, uint8_t color)
{
    cam_transform(&x0, &y0);
    cam_transform(&x1, &y1);
    cam_transform(&x2, &y2);

    /* Sort by y */
    if (y0 > y1) { int t; t=x0;x0=x1;x1=t; t=y0;y0=y1;y1=t; }
    if (y0 > y2) { int t; t=x0;x0=x2;x2=t; t=y0;y0=y2;y2=t; }
    if (y1 > y2) { int t; t=x1;x1=x2;x2=t; t=y1;y1=y2;y2=t; }

    if (y1 == y2) {
        fill_flat_bottom(x0,y0,x1,y1,x2,y2,color);
    } else if (y0 == y1) {
        fill_flat_top(x0,y0,x1,y1,x2,y2,color);
    } else {
        int x3 = (int)(x0 + ((float)(y1-y0)/(float)(y2-y0)) * (x2-x0));
        fill_flat_bottom(x0,y0,x1,y1,x3,y1,color);
        fill_flat_top(x1,y1,x3,y1,x2,y2,color);
    }
}

/* ─── Sprites with hardware scaling & rotation ─────────────── */

void cx8_gpu_spr(int n, int x, int y, int w, int h,
                 bool flip_x, bool flip_y, float scale, float angle)
{
    if (n < 0 || n >= CX8_SPRITE_COUNT) return;
    if (w < 1) w = 1;
    if (h < 1) h = 1;
    if (scale <= 0.0f) scale = 1.0f;

    /* Source rectangle in sprite sheet */
    int sprites_per_row = CX8_SPRITESHEET_W / CX8_SPRITE_W;
    int src_x0 = (n % sprites_per_row) * CX8_SPRITE_W;
    int src_y0 = (n / sprites_per_row) * CX8_SPRITE_H;
    int src_w  = w * CX8_SPRITE_W;
    int src_h  = h * CX8_SPRITE_H;

    /* Destination size */
    int dst_w = (int)(src_w * scale);
    int dst_h = (int)(src_h * scale);

    /* Camera */
    int dx = x, dy = y;
    cam_transform(&dx, &dy);

    if (angle == 0.0f) {
        /* Fast path: no rotation */
        for (int py = 0; py < dst_h; py++) {
            for (int px = 0; px < dst_w; px++) {
                /* Map destination pixel to source */
                int sx = (int)(px / scale);
                int sy = (int)(py / scale);
                if (flip_x) sx = src_w - 1 - sx;
                if (flip_y) sy = src_h - 1 - sy;
                sx = CX8_CLAMP(sx, 0, src_w - 1);
                sy = CX8_CLAMP(sy, 0, src_h - 1);

                uint8_t c = s_sprites[(src_y0 + sy) * CX8_SPRITESHEET_W + (src_x0 + sx)];
                if (c != 0) /* colour 0 = transparent */
                    raw_pset(dx + px, dy + py, c);
            }
        }
    } else {
        /* Rotated rendering (affine inverse mapping) */
        float rad   = angle * (float)(M_PI / 180.0);
        float cos_a = cosf(rad);
        float sin_a = sinf(rad);
        float half_dw = dst_w * 0.5f;
        float half_dh = dst_h * 0.5f;

        /* Bounding box for rotated sprite */
        int bound = (int)(sqrtf((float)(dst_w * dst_w + dst_h * dst_h)) * 0.5f) + 1;

        for (int py = -bound; py <= bound; py++) {
            for (int px = -bound; px <= bound; px++) {
                /* Inverse rotate around centre */
                float fx =  cos_a * px + sin_a * py;
                float fy = -sin_a * px + cos_a * py;
                fx += half_dw;
                fy += half_dh;

                if (fx < 0 || fx >= dst_w || fy < 0 || fy >= dst_h)
                    continue;

                int sx = (int)(fx / scale);
                int sy = (int)(fy / scale);
                if (flip_x) sx = src_w - 1 - sx;
                if (flip_y) sy = src_h - 1 - sy;
                if (sx < 0 || sx >= src_w || sy < 0 || sy >= src_h)
                    continue;

                uint8_t c = s_sprites[(src_y0 + sy) * CX8_SPRITESHEET_W + (src_x0 + sx)];
                if (c != 0)
                    raw_pset(dx + px + (int)half_dw, dy + py + (int)half_dh, c);
            }
        }
    }
}

void cx8_gpu_sset(int x, int y, uint8_t color)
{
    if (x >= 0 && x < CX8_SPRITESHEET_W && y >= 0 && y < CX8_SPRITESHEET_H)
        s_sprites[y * CX8_SPRITESHEET_W + x] = color & 0x3F;
}

uint8_t cx8_gpu_sget(int x, int y)
{
    if (x >= 0 && x < CX8_SPRITESHEET_W && y >= 0 && y < CX8_SPRITESHEET_H)
        return s_sprites[y * CX8_SPRITESHEET_W + x];
    return 0;
}

/* ─── Map ──────────────────────────────────────────────────── */

void cx8_gpu_map(int cel_x, int cel_y, int sx, int sy, int cel_w, int cel_h)
{
    for (int cy = 0; cy < cel_h; cy++) {
        for (int cx = 0; cx < cel_w; cx++) {
            int mx = cel_x + cx;
            int my = cel_y + cy;
            if (mx < 0 || mx >= CX8_MAP_W || my < 0 || my >= CX8_MAP_H) continue;
            uint8_t tile = s_map[my * CX8_MAP_W + mx];
            if (tile != 0) {
                cx8_gpu_spr(tile,
                            sx + cx * CX8_SPRITE_W,
                            sy + cy * CX8_SPRITE_H,
                            1, 1, false, false, 1.0f, 0.0f);
            }
        }
    }
}

void cx8_gpu_mset(int x, int y, uint8_t tile)
{
    if (x >= 0 && x < CX8_MAP_W && y >= 0 && y < CX8_MAP_H)
        s_map[y * CX8_MAP_W + x] = tile;
}

uint8_t cx8_gpu_mget(int x, int y)
{
    if (x >= 0 && x < CX8_MAP_W && y >= 0 && y < CX8_MAP_H)
        return s_map[y * CX8_MAP_W + x];
    return 0;
}

/* ─── Camera ───────────────────────────────────────────────── */

void cx8_gpu_camera(float x, float y, float zoom)
{
    s_camera.x    = x;
    s_camera.y    = y;
    s_camera.zoom = (zoom > 0.0f) ? zoom : 1.0f;
}

cx8_camera_t cx8_gpu_get_camera(void)
{
    return s_camera;
}

/* ─── Clipping ─────────────────────────────────────────────── */

void cx8_gpu_clip(int x, int y, int w, int h)
{
    s_clip.x = CX8_CLAMP(x, 0, CX8_SCREEN_W);
    s_clip.y = CX8_CLAMP(y, 0, CX8_SCREEN_H);
    s_clip.w = CX8_CLAMP(w, 0, CX8_SCREEN_W - s_clip.x);
    s_clip.h = CX8_CLAMP(h, 0, CX8_SCREEN_H - s_clip.y);
}

void cx8_gpu_clip_reset(void)
{
    s_clip = (cx8_clip_t){ 0, 0, CX8_SCREEN_W, CX8_SCREEN_H };
}

/* ─── Palette ──────────────────────────────────────────────── */

void cx8_gpu_pal(uint8_t c0, uint8_t c1)
{
    if (c0 < CX8_PALETTE_SIZE && c1 < CX8_PALETTE_SIZE)
        s_pal_remap[c0] = c1;
}

void cx8_gpu_pal_reset(void)
{
    for (int i = 0; i < CX8_PALETTE_SIZE; i++)
        s_pal_remap[i] = (uint8_t)i;
}

cx8_color_t cx8_gpu_get_color(uint8_t idx)
{
    if (idx < CX8_PALETTE_SIZE) return s_palette[idx];
    return (cx8_color_t){0, 0, 0, 255};
}

void cx8_gpu_set_color(uint8_t idx, cx8_color_t col)
{
    if (idx < CX8_PALETTE_SIZE) s_palette[idx] = col;
}

/* ─── Text ─────────────────────────────────────────────────── */

int cx8_gpu_print(const char *str, int x, int y, uint8_t color)
{
    if (!str) return x;
    int ox = x;
    cam_transform(&x, &y);
    int cx_pos = x;

    while (*str) {
        if (*str == '\n') {
            cx_pos = x;
            y += CX8_GLYPH_H;
            str++;
            continue;
        }
        cx8_font_draw_char(*str, cx_pos, y, color, raw_pset);
        cx_pos += CX8_GLYPH_W + 1;  /* 1px letter spacing */
        str++;
    }
    return cx_pos - x + ox;  /* return cursor x advance */
}

/* ─── Direct access ────────────────────────────────────────── */

uint8_t *cx8_gpu_get_vram(void)
{
    return s_vram;
}

cx8_color_t *cx8_gpu_get_palette(void)
{
    return s_palette;
}

uint8_t *cx8_gpu_get_spritesheet(void)
{
    return s_sprites;
}

uint8_t *cx8_gpu_get_mapdata(void)
{
    return s_map;
}
