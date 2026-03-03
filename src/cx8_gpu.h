/*
 * cx8_gpu.h — Chromaplex 8 PRISM-64 Graphics Processing Unit
 *
 * 256×144 widescreen  ·  64-colour palette  ·  256 sprites
 * Hardware sprite scaling & rotation  ·  Camera with zoom
 */

#ifndef CX8_GPU_H
#define CX8_GPU_H

#include "cx8.h"

/* ─── Lifecycle ────────────────────────────────────────────── */
void     cx8_gpu_init(void);
void     cx8_gpu_shutdown(void);

/* ─── Screen ───────────────────────────────────────────────── */
void     cx8_gpu_cls(uint8_t color);

/* ─── Pixel ────────────────────────────────────────────────── */
void     cx8_gpu_pset(int x, int y, uint8_t color);
uint8_t  cx8_gpu_pget(int x, int y);

/* ─── Primitives ───────────────────────────────────────────── */
void     cx8_gpu_line(int x0, int y0, int x1, int y1, uint8_t color);
void     cx8_gpu_rect(int x0, int y0, int x1, int y1, uint8_t color);
void     cx8_gpu_rectfill(int x0, int y0, int x1, int y1, uint8_t color);
void     cx8_gpu_circ(int cx, int cy, int r, uint8_t color);
void     cx8_gpu_circfill(int cx, int cy, int r, uint8_t color);
void     cx8_gpu_tri(int x0, int y0, int x1, int y1, int x2, int y2, uint8_t color);
void     cx8_gpu_trifill(int x0, int y0, int x1, int y1, int x2, int y2, uint8_t color);

/* ─── Sprites  (hardware scaling + rotation) ───────────────── */
/*  n       = sprite index (0-255)
 *  x, y    = screen position
 *  w, h    = width/height in sprite units (default 1×1 = 8×8 px)
 *  flip_x/y = mirror
 *  scale   = 1.0 = normal
 *  angle   = rotation in degrees
 */
void     cx8_gpu_spr(int n, int x, int y, int w, int h,
                     bool flip_x, bool flip_y, float scale, float angle);
void     cx8_gpu_sset(int x, int y, uint8_t color);
uint8_t  cx8_gpu_sget(int x, int y);

/* ─── Map ──────────────────────────────────────────────────── */
void     cx8_gpu_map(int cel_x, int cel_y, int sx, int sy,
                     int cel_w, int cel_h);
void     cx8_gpu_mset(int x, int y, uint8_t tile);
uint8_t  cx8_gpu_mget(int x, int y);

/* ─── Camera ───────────────────────────────────────────────── */
void     cx8_gpu_camera(float x, float y, float zoom);
cx8_camera_t cx8_gpu_get_camera(void);

/* ─── Clipping ─────────────────────────────────────────────── */
void     cx8_gpu_clip(int x, int y, int w, int h);
void     cx8_gpu_clip_reset(void);

/* ─── Palette ──────────────────────────────────────────────── */
void     cx8_gpu_pal(uint8_t c0, uint8_t c1);  /* swap c0→c1 */
void     cx8_gpu_pal_reset(void);
cx8_color_t cx8_gpu_get_color(uint8_t idx);
void     cx8_gpu_set_color(uint8_t idx, cx8_color_t col);

/* ─── Text ─────────────────────────────────────────────────── */
int      cx8_gpu_print(const char *str, int x, int y, uint8_t color);

/* ─── Direct access ────────────────────────────────────────── */
uint8_t     *cx8_gpu_get_vram(void);
cx8_color_t *cx8_gpu_get_palette(void);
uint8_t     *cx8_gpu_get_spritesheet(void);
uint8_t     *cx8_gpu_get_mapdata(void);

#endif /* CX8_GPU_H */
