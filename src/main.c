/*
 * ╔══════════════════════════════════════════════════════════════╗
 * ║                      CHROMAPLEX  8                           ║
 * ║              Fantasy Game Console  —  v1.0                   ║
 * ║                                                              ║
 * ║  CPU CX8-A  ·  GPU PRISM-64  ·  APU WAVE-4                  ║
 * ║  256×144 widescreen  ·  64-colour palette                    ║
 * ║  128 KB RAM  ·  64 KB cart  ·  Lua 5.4 scripting             ║
 * ║                                                              ║
 * ║  "Plug in. Power up. Play."                                  ║
 * ╚══════════════════════════════════════════════════════════════╝
 *
 * main.c — SDL2 entry point, state machine, boot/home/game/editor.
 *
 * States:  BOOT → HOME ⇄ RUNNING
 *                      ⇄ EDITOR  (Ctrl+R from editor → RUNNING)
 */

#include <SDL.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#include "cx8.h"
#include "cx8_gpu.h"
#include "cx8_apu.h"
#include "cx8_input.h"
#include "cx8_memory.h"
#include "cx8_cart.h"
#include "cx8_modules.h"
#include "cx8_scripting.h"
#include "cx8_font.h"
#include "cx8_home.h"
#include "cx8_editor.h"

/* ─── Application states ───────────────────────────────────── */

typedef enum {
    STATE_BOOT,
    STATE_HOME,
    STATE_RUNNING,
    STATE_EDITOR,
    STATE_QUIT
} app_state_t;

/* ─── Globals ──────────────────────────────────────────────── */
static SDL_Window   *g_window   = NULL;
static SDL_Renderer *g_renderer = NULL;
static SDL_Texture  *g_texture  = NULL;
static SDL_AudioDeviceID g_audio_dev = 0;
static uint32_t      g_pixels[CX8_SCREEN_W * CX8_SCREEN_H];
static app_state_t   g_state    = STATE_BOOT;
static int           g_scale    = CX8_WINDOW_SCALE;

/* Cart directory — defaults to ./carts or sibling carts\ */
static char g_carts_dir[512] = "carts";

/* Currently loaded cart + Lua VM for running state */
static cx8_cart_t   g_cart;
static bool         g_cart_loaded = false;
static lua_State   *g_lua = NULL;

/* Boot screen frame counter */
static int g_boot_frame = 0;

/* ─── SDL Initialization ──────────────────────────────────── */

static bool init_sdl(void)
{
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_GAMECONTROLLER) != 0) {
        fprintf(stderr, "SDL_Init failed: %s\n", SDL_GetError());
        return false;
    }

    g_window = SDL_CreateWindow(
        "CHROMAPLEX 8",
        SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
        CX8_SCREEN_W * g_scale, CX8_SCREEN_H * g_scale,
        SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE
    );
    if (!g_window) {
        fprintf(stderr, "SDL_CreateWindow failed: %s\n", SDL_GetError());
        return false;
    }

    g_renderer = SDL_CreateRenderer(g_window, -1,
                                    SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    if (!g_renderer) {
        fprintf(stderr, "SDL_CreateRenderer failed: %s\n", SDL_GetError());
        return false;
    }

    /* Keep pixel-perfect scaling */
    SDL_RenderSetLogicalSize(g_renderer, CX8_SCREEN_W, CX8_SCREEN_H);
    SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "0"); /* nearest-neighbour */

    g_texture = SDL_CreateTexture(g_renderer,
                                 SDL_PIXELFORMAT_ARGB8888,
                                 SDL_TEXTUREACCESS_STREAMING,
                                 CX8_SCREEN_W, CX8_SCREEN_H);
    if (!g_texture) {
        fprintf(stderr, "SDL_CreateTexture failed: %s\n", SDL_GetError());
        return false;
    }

    /* Audio setup */
    SDL_AudioSpec want, have;
    SDL_memset(&want, 0, sizeof(want));
    want.freq     = CX8_SAMPLE_RATE;
    want.format   = AUDIO_F32SYS;
    want.channels = 1;
    want.samples  = CX8_AUDIO_BUFSIZE;
    want.callback = cx8_apu_callback;

    g_audio_dev = SDL_OpenAudioDevice(NULL, 0, &want, &have, 0);
    if (g_audio_dev == 0) {
        fprintf(stderr, "SDL_OpenAudioDevice failed: %s\n", SDL_GetError());
        /* Audio failure is non-fatal — continue without sound */
    } else {
        SDL_PauseAudioDevice(g_audio_dev, 0); /* start playback */
    }

    return true;
}

static void shutdown_sdl(void)
{
    if (g_audio_dev) SDL_CloseAudioDevice(g_audio_dev);
    if (g_texture)   SDL_DestroyTexture(g_texture);
    if (g_renderer)  SDL_DestroyRenderer(g_renderer);
    if (g_window)    SDL_DestroyWindow(g_window);
    SDL_Quit();
}

/* ─── Render VRAM to SDL ──────────────────────────────────── */

static void present_frame(void)
{
    const uint8_t     *vram    = cx8_gpu_get_vram();
    const cx8_color_t *palette = cx8_gpu_get_palette();

    /* Convert indexed VRAM → ARGB32 */
    for (int i = 0; i < CX8_SCREEN_W * CX8_SCREEN_H; i++) {
        const cx8_color_t *c = &palette[vram[i] & 0x3F];
        g_pixels[i] = (0xFFu << 24) | ((uint32_t)c->r << 16)
                     | ((uint32_t)c->g << 8) | (uint32_t)c->b;
    }

    SDL_UpdateTexture(g_texture, NULL, g_pixels, CX8_SCREEN_W * sizeof(uint32_t));

    SDL_SetRenderDrawColor(g_renderer, 0, 0, 0, 255);
    SDL_RenderClear(g_renderer);
    SDL_RenderCopy(g_renderer, g_texture, NULL, NULL);
    SDL_RenderPresent(g_renderer);
}

/* ─── Input mapping (game mode) ────────────────────────────── */

static void handle_game_key(SDL_Keycode key, bool pressed)
{
    switch (key) {
    case SDLK_LEFT:   cx8_input_set(CX8_BTN_LEFT,  pressed); break;
    case SDLK_RIGHT:  cx8_input_set(CX8_BTN_RIGHT, pressed); break;
    case SDLK_UP:     cx8_input_set(CX8_BTN_UP,    pressed); break;
    case SDLK_DOWN:   cx8_input_set(CX8_BTN_DOWN,  pressed); break;
    case SDLK_z:
    case SDLK_c:      cx8_input_set(CX8_BTN_A,     pressed); break;
    case SDLK_x:
    case SDLK_v:      cx8_input_set(CX8_BTN_B,     pressed); break;
    case SDLK_a:
    case SDLK_s:      cx8_input_set(CX8_BTN_X,     pressed); break;
    case SDLK_d:
    case SDLK_f:      cx8_input_set(CX8_BTN_Y,     pressed); break;
    default: break;
    }
}

/* ─── Cart loading helper ──────────────────────────────────── */

static bool load_and_run_cart(const char *path)
{
    /* Clean up any previous cart */
    if (g_lua) { cx8_script_shutdown(g_lua); g_lua = NULL; }
    if (g_cart_loaded) { cx8_cart_free(&g_cart); g_cart_loaded = false; }

    /* Reset GPU state */
    cx8_gpu_cls(0);
    cx8_gpu_camera(0, 0, 1.0f);
    cx8_gpu_clip_reset();
    cx8_gpu_pal_reset();
    cx8_apu_stop_all();

    /* Load cart */
    if (!cx8_cart_load(path, &g_cart)) {
        fprintf(stderr, "[CX8] Failed to load cartridge: %s\n", path);
        return false;
    }
    g_cart_loaded = true;

    /* Load sprite data into GPU */
    if (g_cart.sprite_data && g_cart.sprite_len > 0) {
        uint8_t *sheet = cx8_gpu_get_spritesheet();
        size_t copy_len = g_cart.sprite_len;
        if (copy_len > CX8_SPRITESHEET_W * CX8_SPRITESHEET_H)
            copy_len = CX8_SPRITESHEET_W * CX8_SPRITESHEET_H;
        memcpy(sheet, g_cart.sprite_data, copy_len);
    }

    /* Load map data into GPU */
    if (g_cart.map_data && g_cart.map_len > 0) {
        uint8_t *mapdata = cx8_gpu_get_mapdata();
        size_t copy_len = g_cart.map_len;
        if (copy_len > CX8_MAP_W * CX8_MAP_H)
            copy_len = CX8_MAP_W * CX8_MAP_H;
        memcpy(mapdata, g_cart.map_data, copy_len);
    }

    /* Init Lua */
    g_lua = cx8_script_init();
    if (!g_lua) {
        fprintf(stderr, "[CX8] Failed to initialise scripting engine.\n");
        return false;
    }

    if (!cx8_script_load(g_lua, g_cart.source,
                         g_cart.title[0] ? g_cart.title : g_cart.filename)) {
        fprintf(stderr, "[CX8] Failed to load cart script.\n");
        cx8_script_shutdown(g_lua);
        g_lua = NULL;
        return false;
    }

    cx8_script_call_init(g_lua);
    printf("[CX8] Running cartridge: %s\n", path);
    return true;
}

/* ─── Boot screen ──────────────────────────────────────────── */

static void boot_frame(int frame)
{
    cx8_gpu_cls(0);

    int phase = frame / 10;

    if (phase >= 0) {
        for (int x = 0; x < CX8_SCREEN_W; x++) {
            uint8_t c = (uint8_t)(40 + (x % 8));
            cx8_gpu_pset(x, 52, c);
            cx8_gpu_pset(x, 88, c);
        }
    }

    if (phase >= 1) {
        const char *title = "CHROMAPLEX";
        int tw = (int)strlen(title) * 10;
        int tx = (CX8_SCREEN_W - tw) / 2;
        for (int i = 0; title[i]; i++) {
            const uint8_t *glyph = cx8_font_glyph(title[i]);
            for (int row = 0; row < 6; row++) {
                for (int col = 0; col < 4; col++) {
                    if (glyph[row] & (0x8 >> col)) {
                        int px = tx + i * 10 + col * 2;
                        int py = 56 + row * 2;
                        uint8_t c = (uint8_t)(42 + (i % 6));
                        cx8_gpu_pset(px,     py,     c);
                        cx8_gpu_pset(px + 1, py,     c);
                        cx8_gpu_pset(px,     py + 1, c);
                        cx8_gpu_pset(px + 1, py + 1, c);
                    }
                }
            }
        }
    }

    if (phase >= 2) {
        int ex = CX8_SCREEN_W / 2 + 52;
        const uint8_t *glyph = cx8_font_glyph('8');
        for (int row = 0; row < 6; row++) {
            for (int col = 0; col < 4; col++) {
                if (glyph[row] & (0x8 >> col)) {
                    int px = ex + col * 3;
                    int py = 54 + row * 3;
                    for (int dy = 0; dy < 3; dy++)
                        for (int dx = 0; dx < 3; dx++)
                            cx8_gpu_pset(px + dx, py + dy, 61);
                }
            }
        }
    }

    if (phase >= 3)
        cx8_gpu_print("CPU: CX8-A /// GPU: PRISM-64 /// APU: WAVE-4", 16, 96, 5);

    if (phase >= 4)
        cx8_gpu_print("256x144 WIDESCREEN  64 COLOURS  128KB RAM", 20, 106, 4);

    if (phase >= 5) {
        int loaded = cx8_module_count_loaded();
        char buf[128];
        snprintf(buf, sizeof(buf), "MODULES: %d LOADED", loaded);
        cx8_gpu_print(buf, 16, 120, 3);

        if (loaded > 0) {
            const cx8_module_t *mods[CX8_MOD_MAX];
            int n = cx8_module_list(mods, CX8_MOD_MAX);
            int mx = 16 + (int)strlen(buf) * 5 + 8;
            for (int i = 0; i < n; i++) {
                if (mods[i]->loaded) {
                    cx8_gpu_print(mods[i]->name, mx, 120, 61);
                    mx += (int)strlen(mods[i]->name) * 5 + 8;
                }
            }
        }
    }

    if (phase >= 7) {
        if ((frame / 15) % 2 == 0)
            cx8_gpu_print("READY.", 16, 134, 7);
    }

    for (int x = 0; x < CX8_SCREEN_W; x++) {
        cx8_gpu_pset(x, 0, 1);
        cx8_gpu_pset(x, CX8_SCREEN_H - 1, 1);
    }
    for (int y = 0; y < CX8_SCREEN_H; y++) {
        cx8_gpu_pset(0, y, 1);
        cx8_gpu_pset(CX8_SCREEN_W - 1, y, 1);
    }
}

/* ─── Usage ────────────────────────────────────────────────── */

static void print_banner(void)
{
    printf("\n");
    printf("  ╔═══════════════════════════════════════════╗\n");
    printf("  ║           C H R O M A P L E X   8        ║\n");
    printf("  ║         Fantasy Game Console v1.0         ║\n");
    printf("  ╟───────────────────────────────────────────╢\n");
    printf("  ║  CPU: CX8-A      GPU: PRISM-64           ║\n");
    printf("  ║  APU: WAVE-4     RAM: 128 KB              ║\n");
    printf("  ║  Display: 256x144  Palette: 64 colours   ║\n");
    printf("  ║  Cart: 64 KB  (no token limit)            ║\n");
    printf("  ╚═══════════════════════════════════════════╝\n");
    printf("\n");
}

static void print_usage(const char *prog)
{
    printf("Usage: %s [options] [cartridge.lua]\n\n", prog);
    printf("If no cartridge is given, the home screen is shown.\n\n");
    printf("Options:\n");
    printf("  --scale N      Window scale (default: %d)\n", CX8_WINDOW_SCALE);
    printf("  --carts DIR    Cartridge folder (default: carts)\n");
    printf("  --mod ID       Load expansion module (0-4)\n");
    printf("  --help         Show this message\n\n");
    printf("Controls (Game):\n");
    printf("  Arrow keys     D-Pad (Left/Right/Up/Down)\n");
    printf("  Z / C          Button A\n");
    printf("  X / V          Button B\n");
    printf("  A / S          Button X\n");
    printf("  D / F          Button Y\n");
    printf("  Escape         Return to home screen\n\n");
    printf("Controls (Home):\n");
    printf("  Up/Down        Browse cartridges\n");
    printf("  Z              Run selected cart\n");
    printf("  X              Edit selected cart\n");
    printf("  A              Create new cart\n");
    printf("  Escape         Quit\n\n");
    printf("Controls (Editor):\n");
    printf("  F1-F4          Switch tab (Code/Sprite/Map/SFX)\n");
    printf("  Ctrl+S         Save cartridge\n");
    printf("  Ctrl+R         Run cartridge\n");
    printf("  Escape         Return to home screen\n\n");
    printf("Expansion Modules:\n");
    printf("  0  TURBO-RAM 8X      (NovaByte Industries)   +128KB RAM\n");
    printf("  1  SYNTHWAVE-16      (AudioLux Labs)         +12 channels\n");
    printf("  2  PIXEL-STRETCH PRO (VisualFX Co.)          Advanced FX\n");
    printf("  3  CART-DOUBLER      (MegaMedia)             +64KB cart\n");
    printf("  4  NETLINK-1         (CyberConnect)          Multiplayer\n");
}

/* ─── Entry point ──────────────────────────────────────────── */

int main(int argc, char *argv[])
{
    print_banner();

    const char *cart_path = NULL;
    int mods_to_load[CX8_MOD_MAX];
    int mod_count = 0;

    /* Parse command-line arguments */
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--help") == 0 || strcmp(argv[i], "-h") == 0) {
            print_usage(argv[0]);
            return 0;
        }
        else if (strcmp(argv[i], "--scale") == 0 && i + 1 < argc) {
            g_scale = atoi(argv[++i]);
            if (g_scale < 1) g_scale = 1;
            if (g_scale > 8) g_scale = 8;
        }
        else if (strcmp(argv[i], "--carts") == 0 && i + 1 < argc) {
            strncpy(g_carts_dir, argv[++i], sizeof(g_carts_dir) - 1);
        }
        else if (strcmp(argv[i], "--mod") == 0 && i + 1 < argc) {
            int mod_id = atoi(argv[++i]);
            if (mod_id >= 0 && mod_id < CX8_MOD_MAX && mod_count < CX8_MOD_MAX)
                mods_to_load[mod_count++] = mod_id;
        }
        else if (argv[i][0] != '-') {
            cart_path = argv[i];
        }
    }

    /* ── Initialise subsystems ──────────────────────────── */
    cx8_mem_init();
    cx8_gpu_init();
    cx8_apu_init();
    cx8_input_init();
    cx8_cart_init();
    cx8_modules_init();

    /* Load requested modules */
    for (int i = 0; i < mod_count; i++)
        cx8_module_load(mods_to_load[i]);

    /* ── Initialise SDL ─────────────────────────────────── */
    if (!init_sdl()) {
        fprintf(stderr, "Failed to initialise SDL.\n");
        return 1;
    }

    /* ── If a cart was given on the command line, go straight to running ── */
    if (cart_path) {
        g_state = STATE_BOOT;
    } else {
        g_state = STATE_BOOT;
    }
    g_boot_frame = 0;

    /* ── Event buffer for editor mode ───────────────────── */
    #define MAX_FRAME_EVENTS 64
    SDL_Event frame_events[MAX_FRAME_EVENTS];
    int frame_event_count = 0;

    /* ── Main loop (state machine) ──────────────────────── */
    Uint32 frame_time = 1000 / CX8_FPS;

    while (g_state != STATE_QUIT) {
        Uint32 now = SDL_GetTicks();

        /* ── Collect events ─────────────────────────────── */
        SDL_Event e;
        cx8_input_begin_frame();
        frame_event_count = 0;

        while (SDL_PollEvent(&e)) {
            if (e.type == SDL_QUIT) {
                g_state = STATE_QUIT;
                break;
            }

            /* Store event for editor */
            if (frame_event_count < MAX_FRAME_EVENTS)
                frame_events[frame_event_count++] = e;

            /* Map keys to input system (for HOME and RUNNING) */
            if (e.type == SDL_KEYDOWN)
                handle_game_key(e.key.keysym.sym, true);
            else if (e.type == SDL_KEYUP)
                handle_game_key(e.key.keysym.sym, false);
        }

        if (g_state == STATE_QUIT) break;

        /* ── State tick ─────────────────────────────────── */
        switch (g_state) {

        /* ═══ BOOT ═══════════════════════════════════════ */
        case STATE_BOOT:
            /* Check for skip */
            for (int i = 0; i < frame_event_count; i++) {
                if (frame_events[i].type == SDL_KEYDOWN) {
                    SDL_Keycode k = frame_events[i].key.keysym.sym;
                    if (k == SDLK_RETURN || k == SDLK_SPACE) {
                        g_boot_frame = 999; /* skip */
                    }
                    if (k == SDLK_ESCAPE) {
                        g_state = STATE_QUIT;
                    }
                }
            }

            boot_frame(g_boot_frame);
            present_frame();
            g_boot_frame++;

            if (g_boot_frame >= 120) {
                /* Boot done — either run CLI cart or go home */
                if (cart_path) {
                    if (load_and_run_cart(cart_path)) {
                        g_state = STATE_RUNNING;
                    } else {
                        /* Fall back to home screen on load failure */
                        cx8_home_init(g_carts_dir);
                        g_state = STATE_HOME;
                    }
                } else {
                    cx8_home_init(g_carts_dir);
                    g_state = STATE_HOME;
                }
            }
            break;

        /* ═══ HOME ═══════════════════════════════════════ */
        case STATE_HOME: {
            /* Check for ESC = quit */
            for (int i = 0; i < frame_event_count; i++) {
                if (frame_events[i].type == SDL_KEYDOWN &&
                    frame_events[i].key.keysym.sym == SDLK_ESCAPE) {
                    g_state = STATE_QUIT;
                }
            }
            if (g_state == STATE_QUIT) break;

            cx8_home_result_t hr = cx8_home_update();

            switch (hr) {
            case CX8_HOME_RUN: {
                const char *path = cx8_home_selected_path();
                if (path && load_and_run_cart(path)) {
                    g_state = STATE_RUNNING;
                    SDL_StopTextInput();
                }
                break;
            }
            case CX8_HOME_EDIT: {
                const char *path = cx8_home_selected_path();
                if (path) {
                    /* Load cart for editing */
                    if (g_cart_loaded) { cx8_cart_free(&g_cart); g_cart_loaded = false; }
                    if (cx8_cart_load(path, &g_cart)) {
                        g_cart_loaded = true;
                        /* Load sprite/map data into GPU for editor */
                        if (g_cart.sprite_data && g_cart.sprite_len > 0) {
                            uint8_t *sheet = cx8_gpu_get_spritesheet();
                            size_t copy_len = g_cart.sprite_len;
                            if (copy_len > CX8_SPRITESHEET_W * CX8_SPRITESHEET_H)
                                copy_len = CX8_SPRITESHEET_W * CX8_SPRITESHEET_H;
                            memcpy(sheet, g_cart.sprite_data, copy_len);
                        }
                        if (g_cart.map_data && g_cart.map_len > 0) {
                            uint8_t *mapdata = cx8_gpu_get_mapdata();
                            size_t copy_len = g_cart.map_len;
                            if (copy_len > CX8_MAP_W * CX8_MAP_H)
                                copy_len = CX8_MAP_W * CX8_MAP_H;
                            memcpy(mapdata, g_cart.map_data, copy_len);
                        }
                        cx8_editor_init(&g_cart, path);
                        SDL_StartTextInput();
                        g_state = STATE_EDITOR;
                    }
                }
                break;
            }
            case CX8_HOME_NEW: {
                /* Create a blank cart */
                if (g_cart_loaded) { cx8_cart_free(&g_cart); g_cart_loaded = false; }
                memset(&g_cart, 0, sizeof(g_cart));
                strncpy(g_cart.title, "UNTITLED", sizeof(g_cart.title) - 1);
                strncpy(g_cart.author, "UNKNOWN", sizeof(g_cart.author) - 1);
                g_cart.source = (char *)calloc(1, 256);
                if (g_cart.source) {
                    strcpy(g_cart.source,
                        "-- title: untitled\n"
                        "-- author: you\n"
                        "\n"
                        "function _init()\n"
                        "end\n"
                        "\n"
                        "function _update()\n"
                        "end\n"
                        "\n"
                        "function _draw()\n"
                        "  cls(0)\n"
                        "  print(\"hello world!\", 80, 60, 7)\n"
                        "end\n");
                    g_cart.source_len = strlen(g_cart.source);
                }
                g_cart.max_size = CX8_CART_SIZE;
                g_cart_loaded = true;

                /* Build a save path */
                char new_path[512];
                snprintf(new_path, sizeof(new_path), "%s/untitled.lua", g_carts_dir);
                cx8_editor_init(&g_cart, new_path);
                SDL_StartTextInput();
                g_state = STATE_EDITOR;
                break;
            }
            case CX8_HOME_QUIT:
                g_state = STATE_QUIT;
                break;
            default:
                break;
            }

            if (g_state == STATE_HOME) {
                cx8_home_draw();
                present_frame();
            }
            break;
        }

        /* ═══ RUNNING ════════════════════════════════════ */
        case STATE_RUNNING: {
            /* Check for ESC → back to home */
            for (int i = 0; i < frame_event_count; i++) {
                if (frame_events[i].type == SDL_KEYDOWN &&
                    frame_events[i].key.keysym.sym == SDLK_ESCAPE) {
                    /* Stop the game and go home */
                    printf("[CX8] Returning to home screen.\n");
                    if (g_lua) { cx8_script_shutdown(g_lua); g_lua = NULL; }
                    cx8_apu_stop_all();
                    cx8_home_init(g_carts_dir);
                    g_state = STATE_HOME;
                    break;
                }
            }
            if (g_state != STATE_RUNNING) break;

            /* Update & draw */
            if (g_lua) {
                cx8_script_call_update(g_lua);
                cx8_script_call_draw(g_lua);
            }
            present_frame();
            break;
        }

        /* ═══ EDITOR ═════════════════════════════════════ */
        case STATE_EDITOR: {
            cx8_edit_result_t er = cx8_editor_update(frame_events, frame_event_count);

            switch (er) {
            case CX8_EDIT_EXIT:
                SDL_StopTextInput();
                cx8_editor_shutdown();
                cx8_home_init(g_carts_dir);
                g_state = STATE_HOME;
                break;
            case CX8_EDIT_RUN: {
                /* Sync editor → cart, then run */
                cx8_editor_sync_to_cart();
                SDL_StopTextInput();
                cx8_editor_shutdown();

                /* Reset GPU state */
                cx8_gpu_cls(0);
                cx8_gpu_camera(0, 0, 1.0f);
                cx8_gpu_clip_reset();
                cx8_gpu_pal_reset();
                cx8_apu_stop_all();

                /* Load sprite/map data from cart into GPU */
                if (g_cart.sprite_data && g_cart.sprite_len > 0) {
                    uint8_t *sheet = cx8_gpu_get_spritesheet();
                    size_t copy_len = g_cart.sprite_len;
                    if (copy_len > CX8_SPRITESHEET_W * CX8_SPRITESHEET_H)
                        copy_len = CX8_SPRITESHEET_W * CX8_SPRITESHEET_H;
                    memcpy(sheet, g_cart.sprite_data, copy_len);
                }
                if (g_cart.map_data && g_cart.map_len > 0) {
                    uint8_t *mapdata = cx8_gpu_get_mapdata();
                    size_t copy_len = g_cart.map_len;
                    if (copy_len > CX8_MAP_W * CX8_MAP_H)
                        copy_len = CX8_MAP_W * CX8_MAP_H;
                    memcpy(mapdata, g_cart.map_data, copy_len);
                }

                /* Init Lua and run */
                g_lua = cx8_script_init();
                if (g_lua && cx8_script_load(g_lua, g_cart.source,
                        g_cart.title[0] ? g_cart.title : "editor-cart")) {
                    cx8_script_call_init(g_lua);
                    printf("[CX8] Running from editor.\n");
                    g_state = STATE_RUNNING;
                } else {
                    /* Script error — go back home */
                    fprintf(stderr, "[CX8] Script error, returning to home.\n");
                    if (g_lua) { cx8_script_shutdown(g_lua); g_lua = NULL; }
                    cx8_home_init(g_carts_dir);
                    g_state = STATE_HOME;
                }
                break;
            }
            case CX8_EDIT_SAVE:
            case CX8_EDIT_NONE:
            default:
                break;
            }

            if (g_state == STATE_EDITOR) {
                cx8_editor_draw();
                present_frame();
            }
            break;
        }

        default:
            break;
        }

        /* ── Frame pacing ───────────────────────────────── */
        Uint32 elapsed = SDL_GetTicks() - now;
        if (elapsed < frame_time)
            SDL_Delay(frame_time - elapsed);
    }

    /* ── Cleanup ────────────────────────────────────────── */
    printf("[CX8] Shutting down...\n");
    SDL_StopTextInput();
    if (g_lua) { cx8_script_shutdown(g_lua); g_lua = NULL; }
    if (g_cart_loaded) { cx8_cart_free(&g_cart); g_cart_loaded = false; }
    cx8_apu_shutdown();
    cx8_gpu_shutdown();
    shutdown_sdl();
    cx8_mem_free();

    printf("[CX8] Goodbye.\n");
    return 0;
}
