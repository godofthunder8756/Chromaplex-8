/*
 * cx8_scripting.c — Chromaplex 8 Lua Scripting Bridge
 *
 * Registers every Chromaplex 8 API function into the Lua global
 * namespace so cartridges can call them directly (PICO-8 style).
 */

#include "cx8_scripting.h"
#include "cx8_gpu.h"
#include "cx8_apu.h"
#include "cx8_input.h"
#include "cx8_memory.h"
#include "cx8_modules.h"
#include <lua.h>
#include <lauxlib.h>
#include <lualib.h>
#include <stdio.h>
#include <math.h>
#include <time.h>
#include <stdlib.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

/*
 * Lua 5.4 checkinteger is strict — rejects 3.0 as "not an integer".
 * PICO-8 style: everything is a number.  Truncate to int gracefully.
 */
#define CHECKINT(L, n)   ((int)luaL_checknumber(L, n))
#define OPTINT(L, n, d)  ((int)luaL_optnumber(L, n, (double)(d)))
#define CHECKU8(L, n)    ((uint8_t)(int)luaL_checknumber(L, n))
#define OPTU8(L, n, d)   ((uint8_t)(int)luaL_optnumber(L, n, (double)(d)))
#define CHECKU16(L, n)   ((uint16_t)(int)luaL_checknumber(L, n))
#define CHECKU32(L, n)   ((uint32_t)luaL_checknumber(L, n))
#define CHECKSIZE(L, n)  ((size_t)luaL_checknumber(L, n))

static clock_t s_start_time;

/* ═══════════════════════════════════════════════════════════════
 *  DRAWING API
 * ═══════════════════════════════════════════════════════════════ */

/* cls([col]) */
static int l_cls(lua_State *L)
{
    uint8_t c = OPTU8(L, 1, 0);
    cx8_gpu_cls(c);
    return 0;
}

/* pset(x, y, col) */
static int l_pset(lua_State *L)
{
    int x = CHECKINT(L, 1);
    int y = CHECKINT(L, 2);
    uint8_t c = CHECKU8(L, 3);
    cx8_gpu_pset(x, y, c);
    return 0;
}

/* col = pget(x, y) */
static int l_pget(lua_State *L)
{
    int x = CHECKINT(L, 1);
    int y = CHECKINT(L, 2);
    lua_pushinteger(L, cx8_gpu_pget(x, y));
    return 1;
}

/* line(x0,y0, x1,y1, col) */
static int l_line(lua_State *L)
{
    int x0 = CHECKINT(L, 1);
    int y0 = CHECKINT(L, 2);
    int x1 = CHECKINT(L, 3);
    int y1 = CHECKINT(L, 4);
    uint8_t c = CHECKU8(L, 5);
    cx8_gpu_line(x0, y0, x1, y1, c);
    return 0;
}

/* rect(x0,y0, x1,y1, col) */
static int l_rect(lua_State *L)
{
    int x0 = CHECKINT(L, 1);
    int y0 = CHECKINT(L, 2);
    int x1 = CHECKINT(L, 3);
    int y1 = CHECKINT(L, 4);
    uint8_t c = CHECKU8(L, 5);
    cx8_gpu_rect(x0, y0, x1, y1, c);
    return 0;
}

/* rectfill(x0,y0, x1,y1, col) */
static int l_rectfill(lua_State *L)
{
    int x0 = CHECKINT(L, 1);
    int y0 = CHECKINT(L, 2);
    int x1 = CHECKINT(L, 3);
    int y1 = CHECKINT(L, 4);
    uint8_t c = CHECKU8(L, 5);
    cx8_gpu_rectfill(x0, y0, x1, y1, c);
    return 0;
}

/* circ(cx,cy, r, col) */
static int l_circ(lua_State *L)
{
    int cx = CHECKINT(L, 1);
    int cy = CHECKINT(L, 2);
    int r  = CHECKINT(L, 3);
    uint8_t c = CHECKU8(L, 4);
    cx8_gpu_circ(cx, cy, r, c);
    return 0;
}

/* circfill(cx,cy, r, col) */
static int l_circfill(lua_State *L)
{
    int cx = CHECKINT(L, 1);
    int cy = CHECKINT(L, 2);
    int r  = CHECKINT(L, 3);
    uint8_t c = CHECKU8(L, 4);
    cx8_gpu_circfill(cx, cy, r, c);
    return 0;
}

/* tri(x0,y0, x1,y1, x2,y2, col) */
static int l_tri(lua_State *L)
{
    int x0 = CHECKINT(L, 1);
    int y0 = CHECKINT(L, 2);
    int x1 = CHECKINT(L, 3);
    int y1 = CHECKINT(L, 4);
    int x2 = CHECKINT(L, 5);
    int y2 = CHECKINT(L, 6);
    uint8_t c = CHECKU8(L, 7);
    cx8_gpu_tri(x0, y0, x1, y1, x2, y2, c);
    return 0;
}

/* trifill(x0,y0, x1,y1, x2,y2, col) */
static int l_trifill(lua_State *L)
{
    int x0 = CHECKINT(L, 1);
    int y0 = CHECKINT(L, 2);
    int x1 = CHECKINT(L, 3);
    int y1 = CHECKINT(L, 4);
    int x2 = CHECKINT(L, 5);
    int y2 = CHECKINT(L, 6);
    uint8_t c = CHECKU8(L, 7);
    cx8_gpu_trifill(x0, y0, x1, y1, x2, y2, c);
    return 0;
}

/* print(str, x, y, col) */
static int l_print(lua_State *L)
{
    const char *str = luaL_checkstring(L, 1);
    int x = OPTINT(L, 2, 0);
    int y = OPTINT(L, 3, 0);
    uint8_t c = OPTU8(L, 4, 7);
    int result = cx8_gpu_print(str, x, y, c);
    lua_pushinteger(L, result);
    return 1;
}

/* ═══════════════════════════════════════════════════════════════
 *  SPRITE API
 * ═══════════════════════════════════════════════════════════════ */

/* spr(n, x, y, [w, h, flip_x, flip_y, scale, angle]) */
static int l_spr(lua_State *L)
{
    int n = CHECKINT(L, 1);
    int x = CHECKINT(L, 2);
    int y = CHECKINT(L, 3);
    int w = OPTINT(L, 4, 1);
    int h = OPTINT(L, 5, 1);
    bool fx = lua_toboolean(L, 6);
    bool fy = lua_toboolean(L, 7);
    float scale = (float)luaL_optnumber(L, 8, 1.0);
    float angle = (float)luaL_optnumber(L, 9, 0.0);
    cx8_gpu_spr(n, x, y, w, h, fx, fy, scale, angle);
    return 0;
}

/* sset(x, y, col) */
static int l_sset(lua_State *L)
{
    int x = CHECKINT(L, 1);
    int y = CHECKINT(L, 2);
    uint8_t c = CHECKU8(L, 3);
    cx8_gpu_sset(x, y, c);
    return 0;
}

/* col = sget(x, y) */
static int l_sget(lua_State *L)
{
    int x = CHECKINT(L, 1);
    int y = CHECKINT(L, 2);
    lua_pushinteger(L, cx8_gpu_sget(x, y));
    return 1;
}

/* ═══════════════════════════════════════════════════════════════
 *  MAP API
 * ═══════════════════════════════════════════════════════════════ */

/* map(cel_x, cel_y, sx, sy, cel_w, cel_h) */
static int l_map(lua_State *L)
{
    int cx = CHECKINT(L, 1);
    int cy = CHECKINT(L, 2);
    int sx = CHECKINT(L, 3);
    int sy = CHECKINT(L, 4);
    int cw = CHECKINT(L, 5);
    int ch = CHECKINT(L, 6);
    cx8_gpu_map(cx, cy, sx, sy, cw, ch);
    return 0;
}

/* mset(x, y, tile) */
static int l_mset(lua_State *L)
{
    int x = CHECKINT(L, 1);
    int y = CHECKINT(L, 2);
    uint8_t t = CHECKU8(L, 3);
    cx8_gpu_mset(x, y, t);
    return 0;
}

/* tile = mget(x, y) */
static int l_mget(lua_State *L)
{
    int x = CHECKINT(L, 1);
    int y = CHECKINT(L, 2);
    lua_pushinteger(L, cx8_gpu_mget(x, y));
    return 1;
}

/* ═══════════════════════════════════════════════════════════════
 *  CAMERA & CLIPPING
 * ═══════════════════════════════════════════════════════════════ */

/* camera([x, y, zoom]) */
static int l_camera(lua_State *L)
{
    float x = (float)luaL_optnumber(L, 1, 0.0);
    float y = (float)luaL_optnumber(L, 2, 0.0);
    float z = (float)luaL_optnumber(L, 3, 1.0);
    cx8_gpu_camera(x, y, z);
    return 0;
}

/* clip([x, y, w, h]) — no args resets */
static int l_clip(lua_State *L)
{
    if (lua_gettop(L) == 0) {
        cx8_gpu_clip_reset();
    } else {
        int x = CHECKINT(L, 1);
        int y = CHECKINT(L, 2);
        int w = CHECKINT(L, 3);
        int h = CHECKINT(L, 4);
        cx8_gpu_clip(x, y, w, h);
    }
    return 0;
}

/* pal(c0, c1) or pal() to reset */
static int l_pal(lua_State *L)
{
    if (lua_gettop(L) == 0) {
        cx8_gpu_pal_reset();
    } else {
        uint8_t c0 = CHECKU8(L, 1);
        uint8_t c1 = CHECKU8(L, 2);
        cx8_gpu_pal(c0, c1);
    }
    return 0;
}

/* ═══════════════════════════════════════════════════════════════
 *  INPUT API
 * ═══════════════════════════════════════════════════════════════ */

/* pressed = btn(b) */
static int l_btn(lua_State *L)
{
    int b = CHECKINT(L, 1);
    lua_pushboolean(L, cx8_input_btn(b));
    return 1;
}

/* just_pressed = btnp(b) */
static int l_btnp(lua_State *L)
{
    int b = CHECKINT(L, 1);
    lua_pushboolean(L, cx8_input_btnp(b));
    return 1;
}

/* ═══════════════════════════════════════════════════════════════
 *  AUDIO API
 * ═══════════════════════════════════════════════════════════════ */

/* sfx(channel, freq, [vol, waveform, duty]) */
static int l_sfx(lua_State *L)
{
    int ch   = CHECKINT(L, 1);
    float f  = (float)luaL_checknumber(L, 2);
    float v  = (float)luaL_optnumber(L, 3, 0.5);
    int wave = OPTINT(L, 4, CX8_WAVE_SQUARE);
    float d  = (float)luaL_optnumber(L, 5, 0.5);
    cx8_apu_play(ch, f, v, (uint8_t)wave, d);
    return 0;
}

/* envelope(channel, attack, decay, sustain, release) */
static int l_envelope(lua_State *L)
{
    int ch = CHECKINT(L, 1);
    float a = (float)luaL_checknumber(L, 2);
    float d = (float)luaL_checknumber(L, 3);
    float s = (float)luaL_checknumber(L, 4);
    float r = (float)luaL_checknumber(L, 5);
    cx8_apu_envelope(ch, a, d, s, r);
    return 0;
}

/* quiet([channel]) — stop one or all channels */
static int l_quiet(lua_State *L)
{
    if (lua_gettop(L) == 0) {
        cx8_apu_stop_all();
    } else {
        cx8_apu_stop(CHECKINT(L, 1));
    }
    return 0;
}

/* ═══════════════════════════════════════════════════════════════
 *  MEMORY API
 * ═══════════════════════════════════════════════════════════════ */

/* val = peek(addr) */
static int l_peek(lua_State *L)
{
    uint32_t a = CHECKU32(L, 1);
    lua_pushinteger(L, cx8_mem_peek(a));
    return 1;
}

/* poke(addr, val) */
static int l_poke(lua_State *L)
{
    uint32_t a = CHECKU32(L, 1);
    uint8_t v  = CHECKU8(L, 2);
    cx8_mem_poke(a, v);
    return 0;
}

/* val = peek16(addr) */
static int l_peek16(lua_State *L)
{
    uint32_t a = CHECKU32(L, 1);
    lua_pushinteger(L, cx8_mem_peek16(a));
    return 1;
}

/* poke16(addr, val) */
static int l_poke16(lua_State *L)
{
    uint32_t a = CHECKU32(L, 1);
    uint16_t v = CHECKU16(L, 2);
    cx8_mem_poke16(a, v);
    return 0;
}

/* memcpy(dst, src, len) */
static int l_memcpy(lua_State *L)
{
    uint32_t d = CHECKU32(L, 1);
    uint32_t s = CHECKU32(L, 2);
    size_t n   = CHECKSIZE(L, 3);
    cx8_mem_copy(d, s, n);
    return 0;
}

/* memset(dst, val, len) */
static int l_memset(lua_State *L)
{
    uint32_t d = CHECKU32(L, 1);
    uint8_t v  = CHECKU8(L, 2);
    size_t n   = CHECKSIZE(L, 3);
    cx8_mem_set(d, v, n);
    return 0;
}

/* ═══════════════════════════════════════════════════════════════
 *  MODULE API
 * ═══════════════════════════════════════════════════════════════ */

/* mod_load(id) */
static int l_mod_load(lua_State *L)
{
    int id = CHECKINT(L, 1);
    lua_pushboolean(L, cx8_module_load(id));
    return 1;
}

/* loaded = mod_check(id) */
static int l_mod_check(lua_State *L)
{
    int id = CHECKINT(L, 1);
    lua_pushboolean(L, cx8_module_is_loaded(id));
    return 1;
}

/* info = mod_info(id) — returns table with name, manufacturer, etc. */
static int l_mod_info(lua_State *L)
{
    int id = CHECKINT(L, 1);
    const cx8_module_t *mod = cx8_module_get(id);
    if (!mod) {
        lua_pushnil(L);
        return 1;
    }
    lua_newtable(L);
    lua_pushstring(L, mod->name);         lua_setfield(L, -2, "name");
    lua_pushstring(L, mod->manufacturer); lua_setfield(L, -2, "manufacturer");
    lua_pushstring(L, mod->description);  lua_setfield(L, -2, "description");
    lua_pushstring(L, mod->flavor);       lua_setfield(L, -2, "flavor");
    lua_pushboolean(L, mod->loaded);      lua_setfield(L, -2, "loaded");
    return 1;
}

/* ═══════════════════════════════════════════════════════════════
 *  MATH & SYSTEM API
 * ═══════════════════════════════════════════════════════════════ */

/* n = rnd([max]) — random float [0, max) */
static int l_rnd(lua_State *L)
{
    float mx = (float)luaL_optnumber(L, 1, 1.0);
    float r = ((float)rand() / (float)RAND_MAX) * mx;
    lua_pushnumber(L, r);
    return 1;
}

/* n = flr(x) */
static int l_flr(lua_State *L)
{
    lua_pushnumber(L, floor(luaL_checknumber(L, 1)));
    return 1;
}

/* n = ceil(x) */
static int l_ceil(lua_State *L)
{
    lua_pushnumber(L, ceil(luaL_checknumber(L, 1)));
    return 1;
}

/* n = sin(x) — PICO-8 style: input is 0-1, not radians */
static int l_sin(lua_State *L)
{
    double x = luaL_checknumber(L, 1);
    lua_pushnumber(L, -sin(x * 2.0 * M_PI));
    return 1;
}

/* n = cos(x) — PICO-8 style */
static int l_cos(lua_State *L)
{
    double x = luaL_checknumber(L, 1);
    lua_pushnumber(L, cos(x * 2.0 * M_PI));
    return 1;
}

/* n = atan2(dx, dy) — returns 0-1 */
static int l_atan2(lua_State *L)
{
    double dx = luaL_checknumber(L, 1);
    double dy = luaL_checknumber(L, 2);
    double a = atan2(-dy, dx) / (2.0 * M_PI);
    if (a < 0) a += 1.0;
    lua_pushnumber(L, a);
    return 1;
}

/* n = sgn(x) */
static int l_sgn(lua_State *L)
{
    double x = luaL_checknumber(L, 1);
    lua_pushnumber(L, x > 0 ? 1 : (x < 0 ? -1 : 0));
    return 1;
}

/* n = mid(a, b, c) — middle value */
static int l_mid(lua_State *L)
{
    double a = luaL_checknumber(L, 1);
    double b = luaL_checknumber(L, 2);
    double c = luaL_checknumber(L, 3);
    double result;
    if (a > b) { double t = a; a = b; b = t; }
    result = (c < a) ? a : (c > b) ? b : c;
    lua_pushnumber(L, result);
    return 1;
}

/* t = time() — seconds since boot */
static int l_time(lua_State *L)
{
    double elapsed = (double)(clock() - s_start_time) / (double)CLOCKS_PER_SEC;
    lua_pushnumber(L, elapsed);
    return 1;
}

/* ═══════════════════════════════════════════════════════════════
 *  SYSTEM CONSTANTS
 * ═══════════════════════════════════════════════════════════════ */

static void register_constants(lua_State *L)
{
    /* Button IDs */
    lua_pushinteger(L, CX8_BTN_LEFT);   lua_setglobal(L, "BTN_LEFT");
    lua_pushinteger(L, CX8_BTN_RIGHT);  lua_setglobal(L, "BTN_RIGHT");
    lua_pushinteger(L, CX8_BTN_UP);     lua_setglobal(L, "BTN_UP");
    lua_pushinteger(L, CX8_BTN_DOWN);   lua_setglobal(L, "BTN_DOWN");
    lua_pushinteger(L, CX8_BTN_A);      lua_setglobal(L, "BTN_A");
    lua_pushinteger(L, CX8_BTN_B);      lua_setglobal(L, "BTN_B");
    lua_pushinteger(L, CX8_BTN_X);      lua_setglobal(L, "BTN_X");
    lua_pushinteger(L, CX8_BTN_Y);      lua_setglobal(L, "BTN_Y");

    /* Waveforms */
    lua_pushinteger(L, CX8_WAVE_SQUARE);   lua_setglobal(L, "WAVE_SQUARE");
    lua_pushinteger(L, CX8_WAVE_TRIANGLE); lua_setglobal(L, "WAVE_TRIANGLE");
    lua_pushinteger(L, CX8_WAVE_SAW);      lua_setglobal(L, "WAVE_SAW");
    lua_pushinteger(L, CX8_WAVE_NOISE);    lua_setglobal(L, "WAVE_NOISE");
    lua_pushinteger(L, CX8_WAVE_PULSE);    lua_setglobal(L, "WAVE_PULSE");
    lua_pushinteger(L, CX8_WAVE_SINE);     lua_setglobal(L, "WAVE_SINE");

    /* Module IDs */
    lua_pushinteger(L, CX8_MOD_TURBO_RAM);    lua_setglobal(L, "MOD_TURBO_RAM");
    lua_pushinteger(L, CX8_MOD_SYNTHWAVE16);   lua_setglobal(L, "MOD_SYNTHWAVE16");
    lua_pushinteger(L, CX8_MOD_PIXSTRETCH);    lua_setglobal(L, "MOD_PIXSTRETCH");
    lua_pushinteger(L, CX8_MOD_CART_DOUBLER);  lua_setglobal(L, "MOD_CART_DOUBLER");
    lua_pushinteger(L, CX8_MOD_NETLINK);       lua_setglobal(L, "MOD_NETLINK");

    /* Screen dimensions */
    lua_pushinteger(L, CX8_SCREEN_W); lua_setglobal(L, "SCREEN_W");
    lua_pushinteger(L, CX8_SCREEN_H); lua_setglobal(L, "SCREEN_H");
}

/* ═══════════════════════════════════════════════════════════════
 *  REGISTRATION TABLE
 * ═══════════════════════════════════════════════════════════════ */

static const struct { const char *name; lua_CFunction func; } s_api[] = {
    /* Drawing */
    { "cls",       l_cls       },
    { "pset",      l_pset      },
    { "pget",      l_pget      },
    { "line",      l_line      },
    { "rect",      l_rect      },
    { "rectfill",  l_rectfill  },
    { "circ",      l_circ      },
    { "circfill",  l_circfill  },
    { "tri",       l_tri       },
    { "trifill",   l_trifill   },
    { "print",     l_print     },

    /* Sprites */
    { "spr",       l_spr       },
    { "sset",      l_sset      },
    { "sget",      l_sget      },

    /* Map */
    { "map",       l_map       },
    { "mset",      l_mset      },
    { "mget",      l_mget      },

    /* Camera & Clip */
    { "camera",    l_camera    },
    { "clip",      l_clip      },
    { "pal",       l_pal       },

    /* Input */
    { "btn",       l_btn       },
    { "btnp",      l_btnp      },

    /* Audio */
    { "sfx",       l_sfx       },
    { "envelope",  l_envelope  },
    { "quiet",     l_quiet     },

    /* Memory */
    { "peek",      l_peek      },
    { "poke",      l_poke      },
    { "peek16",    l_peek16    },
    { "poke16",    l_poke16    },
    { "memcpy",    l_memcpy    },
    { "memset",    l_memset    },

    /* Modules */
    { "mod_load",  l_mod_load  },
    { "mod_check", l_mod_check },
    { "mod_info",  l_mod_info  },

    /* Math & System */
    { "rnd",       l_rnd       },
    { "flr",       l_flr       },
    { "ceil",      l_ceil      },
    { "sin",       l_sin       },
    { "cos",       l_cos       },
    { "atan2",     l_atan2     },
    { "sgn",       l_sgn       },
    { "mid",       l_mid       },
    { "time",      l_time      },

    { NULL, NULL }
};

/* ═══════════════════════════════════════════════════════════════
 *  PUBLIC API
 * ═══════════════════════════════════════════════════════════════ */

lua_State *cx8_script_init(void)
{
    s_start_time = clock();
    srand((unsigned)time(NULL));

    lua_State *L = luaL_newstate();
    if (!L) {
        fprintf(stderr, "[CX8-LUA] Failed to create Lua state\n");
        return NULL;
    }
    luaL_openlibs(L);

    /* Register all API functions as globals */
    for (int i = 0; s_api[i].name; i++) {
        lua_pushcfunction(L, s_api[i].func);
        lua_setglobal(L, s_api[i].name);
    }

    /* Register constants */
    register_constants(L);

    printf("[CX8-LUA] Lua %s.%s scripting engine ready\n",
           LUA_VERSION_MAJOR, LUA_VERSION_MINOR);
    return L;
}

bool cx8_script_load(lua_State *L, const char *source, const char *name)
{
    if (!L || !source) return false;

    int err = luaL_loadbuffer(L, source, strlen(source), name ? name : "cart");
    if (err != LUA_OK) {
        fprintf(stderr, "[CX8-LUA] Load error: %s\n", lua_tostring(L, -1));
        lua_pop(L, 1);
        return false;
    }

    err = lua_pcall(L, 0, 0, 0);
    if (err != LUA_OK) {
        fprintf(stderr, "[CX8-LUA] Run error: %s\n", lua_tostring(L, -1));
        lua_pop(L, 1);
        return false;
    }

    return true;
}

static void safe_call(lua_State *L, const char *func_name)
{
    lua_getglobal(L, func_name);
    if (lua_isfunction(L, -1)) {
        if (lua_pcall(L, 0, 0, 0) != LUA_OK) {
            fprintf(stderr, "[CX8-LUA] %s() error: %s\n",
                    func_name, lua_tostring(L, -1));
            lua_pop(L, 1);
        }
    } else {
        lua_pop(L, 1);
    }
}

void cx8_script_call_init(lua_State *L)   { safe_call(L, "_init");   }
void cx8_script_call_update(lua_State *L) { safe_call(L, "_update"); }
void cx8_script_call_draw(lua_State *L)   { safe_call(L, "_draw");   }

void cx8_script_shutdown(lua_State *L)
{
    if (L) lua_close(L);
}
