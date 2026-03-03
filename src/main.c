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
 * main.c — SDL2 entry point, boot screen, main game loop.
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

/* ─── Globals ──────────────────────────────────────────────── */
static SDL_Window   *g_window   = NULL;
static SDL_Renderer *g_renderer = NULL;
static SDL_Texture  *g_texture  = NULL;
static SDL_AudioDeviceID g_audio_dev = 0;
static uint32_t      g_pixels[CX8_SCREEN_W * CX8_SCREEN_H];
static bool          g_running  = true;
static int           g_scale    = CX8_WINDOW_SCALE;

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

    /* Optional CRT scanline effect */
    SDL_SetRenderDrawColor(g_renderer, 0, 0, 0, 255);
    SDL_RenderClear(g_renderer);
    SDL_RenderCopy(g_renderer, g_texture, NULL, NULL);
    SDL_RenderPresent(g_renderer);
}

/* ─── Input mapping ────────────────────────────────────────── */

static void handle_key(SDL_Keycode key, bool pressed)
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
    case SDLK_ESCAPE: g_running = false; break;
    default: break;
    }
}

/* ─── Boot screen ──────────────────────────────────────────── */

static void boot_frame(int frame)
{
    cx8_gpu_cls(0);

    int phase = frame / 10;  /* each animation phase is ~166ms */

    if (phase >= 0) {
        /* Background gradient strip */
        for (int x = 0; x < CX8_SCREEN_W; x++) {
            uint8_t c = (uint8_t)(40 + (x % 8));  /* subtle blue gradient */
            cx8_gpu_pset(x, 52, c);
            cx8_gpu_pset(x, 88, c);
        }
    }

    if (phase >= 1) {
        /* "CHROMAPLEX" title — big and centered */
        const char *title = "CHROMAPLEX";
        int tw = (int)strlen(title) * 10; /* large text: 2x scale */
        int tx = (CX8_SCREEN_W - tw) / 2;
        /* Draw each character 2x scaled */
        for (int i = 0; title[i]; i++) {
            /* Use the font system but draw 2x manually */
            const uint8_t *glyph = cx8_font_glyph(title[i]);
            for (int row = 0; row < 6; row++) {
                for (int col = 0; col < 4; col++) {
                    if (glyph[row] & (0x8 >> col)) {
                        int px = tx + i * 10 + col * 2;
                        int py = 56 + row * 2;
                        /* Use a colour cycle based on position */
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
        /* The "8" — large, in a distinct accent colour */
        int ex = CX8_SCREEN_W / 2 + 52;
        const uint8_t *glyph = cx8_font_glyph('8');
        for (int row = 0; row < 6; row++) {
            for (int col = 0; col < 4; col++) {
                if (glyph[row] & (0x8 >> col)) {
                    int px = ex + col * 3;
                    int py = 54 + row * 3;
                    /* Bright neon accent */
                    for (int dy = 0; dy < 3; dy++)
                        for (int dx = 0; dx < 3; dx++)
                            cx8_gpu_pset(px + dx, py + dy, 61); /* neon cyan */
                }
            }
        }
    }

    if (phase >= 3) {
        /* System specs line */
        cx8_gpu_print("CPU: CX8-A /// GPU: PRISM-64 /// APU: WAVE-4",
                      16, 96, 5);
    }

    if (phase >= 4) {
        /* Hardware info */
        cx8_gpu_print("256x144 WIDESCREEN  64 COLOURS  128KB RAM",
                      20, 106, 4);
    }

    if (phase >= 5) {
        /* Module bay status */
        int loaded = cx8_module_count_loaded();
        char buf[128];
        snprintf(buf, sizeof(buf), "MODULES: %d LOADED", loaded);
        cx8_gpu_print(buf, 16, 120, 3);

        /* List loaded modules */
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
        /* Ready prompt (blinking) */
        if ((frame / 15) % 2 == 0)
            cx8_gpu_print("READY.", 16, 134, 7);
    }

    /* Decorative border lines */
    for (int x = 0; x < CX8_SCREEN_W; x++) {
        cx8_gpu_pset(x, 0, 1);
        cx8_gpu_pset(x, CX8_SCREEN_H - 1, 1);
    }
    for (int y = 0; y < CX8_SCREEN_H; y++) {
        cx8_gpu_pset(0, y, 1);
        cx8_gpu_pset(CX8_SCREEN_W - 1, y, 1);
    }
}

static void run_boot_screen(void)
{
    int boot_frames = 120;  /* 2 seconds at 60fps */

    for (int frame = 0; frame < boot_frames && g_running; frame++) {
        SDL_Event e;
        while (SDL_PollEvent(&e)) {
            if (e.type == SDL_QUIT) { g_running = false; return; }
            if (e.type == SDL_KEYDOWN) {
                if (e.key.keysym.sym == SDLK_ESCAPE) { g_running = false; return; }
                if (e.key.keysym.sym == SDLK_RETURN || e.key.keysym.sym == SDLK_SPACE)
                    return; /* skip boot */
            }
        }

        boot_frame(frame);
        present_frame();
        SDL_Delay(1000 / CX8_FPS);
    }
}

/* ─── Main game loop ──────────────────────────────────────── */

static void run_game_loop(lua_State *L)
{
    Uint32 frame_time = 1000 / CX8_FPS;
    Uint32 last_time  = SDL_GetTicks();

    cx8_script_call_init(L);

    while (g_running) {
        Uint32 now = SDL_GetTicks();

        /* ── Events ─────────────────────────────────────── */
        SDL_Event e;
        cx8_input_begin_frame();
        while (SDL_PollEvent(&e)) {
            switch (e.type) {
            case SDL_QUIT:
                g_running = false;
                break;
            case SDL_KEYDOWN:
                handle_key(e.key.keysym.sym, true);
                break;
            case SDL_KEYUP:
                handle_key(e.key.keysym.sym, false);
                break;
            }
        }

        if (!g_running) break;

        /* ── Update ─────────────────────────────────────── */
        cx8_script_call_update(L);

        /* ── Draw ───────────────────────────────────────── */
        cx8_script_call_draw(L);
        present_frame();

        /* ── Frame pacing ───────────────────────────────── */
        Uint32 elapsed = SDL_GetTicks() - now;
        if (elapsed < frame_time)
            SDL_Delay(frame_time - elapsed);

        last_time = now;
    }
    (void)last_time;
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
    printf("Usage: %s [options] <cartridge.lua>\n\n", prog);
    printf("Options:\n");
    printf("  --scale N      Window scale (default: %d)\n", CX8_WINDOW_SCALE);
    printf("  --mod ID       Load expansion module (0-4)\n");
    printf("  --help         Show this message\n\n");
    printf("Controls:\n");
    printf("  Arrow keys     D-Pad (Left/Right/Up/Down)\n");
    printf("  Z / C          Button A\n");
    printf("  X / V          Button B\n");
    printf("  A / S          Button X\n");
    printf("  D / F          Button Y\n");
    printf("  Escape         Quit\n");
    printf("  Enter/Space    Skip boot screen\n\n");
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
        else if (strcmp(argv[i], "--mod") == 0 && i + 1 < argc) {
            int mod_id = atoi(argv[++i]);
            if (mod_id >= 0 && mod_id < CX8_MOD_MAX && mod_count < CX8_MOD_MAX)
                mods_to_load[mod_count++] = mod_id;
        }
        else if (argv[i][0] != '-') {
            cart_path = argv[i];
        }
    }

    if (!cart_path) {
        fprintf(stderr, "Error: no cartridge specified.\n\n");
        print_usage(argv[0]);
        return 1;
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

    /* ── Boot screen ────────────────────────────────────── */
    run_boot_screen();

    if (!g_running) {
        shutdown_sdl();
        cx8_mem_free();
        return 0;
    }

    /* ── Load cartridge ─────────────────────────────────── */
    cx8_cart_t cart;
    if (!cx8_cart_load(cart_path, &cart)) {
        fprintf(stderr, "Failed to load cartridge: %s\n", cart_path);
        shutdown_sdl();
        cx8_mem_free();
        return 1;
    }

    /* Load sprite data from cart into GPU */
    if (cart.sprite_data && cart.sprite_len > 0) {
        uint8_t *sheet = cx8_gpu_get_spritesheet();
        size_t copy_len = cart.sprite_len;
        if (copy_len > CX8_SPRITESHEET_W * CX8_SPRITESHEET_H)
            copy_len = CX8_SPRITESHEET_W * CX8_SPRITESHEET_H;
        memcpy(sheet, cart.sprite_data, copy_len);
    }

    /* Load map data from cart into GPU */
    if (cart.map_data && cart.map_len > 0) {
        uint8_t *mapdata = cx8_gpu_get_mapdata();
        size_t copy_len = cart.map_len;
        if (copy_len > CX8_MAP_W * CX8_MAP_H)
            copy_len = CX8_MAP_W * CX8_MAP_H;
        memcpy(mapdata, cart.map_data, copy_len);
    }

    /* ── Initialise Lua scripting ───────────────────────── */
    lua_State *L = cx8_script_init();
    if (!L) {
        fprintf(stderr, "Failed to initialise scripting engine.\n");
        cx8_cart_free(&cart);
        shutdown_sdl();
        cx8_mem_free();
        return 1;
    }

    if (!cx8_script_load(L, cart.source, cart.title[0] ? cart.title : cart.filename)) {
        fprintf(stderr, "Failed to load cartridge script.\n");
        cx8_script_shutdown(L);
        cx8_cart_free(&cart);
        shutdown_sdl();
        cx8_mem_free();
        return 1;
    }

    /* ── Main game loop ─────────────────────────────────── */
    printf("[CX8] Running cartridge...\n");
    run_game_loop(L);

    /* ── Cleanup ────────────────────────────────────────── */
    printf("[CX8] Shutting down...\n");
    cx8_script_shutdown(L);
    cx8_cart_free(&cart);
    cx8_apu_shutdown();
    cx8_gpu_shutdown();
    shutdown_sdl();
    cx8_mem_free();

    printf("[CX8] Goodbye.\n");
    return 0;
}
