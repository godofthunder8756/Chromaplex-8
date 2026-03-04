/*
 * cx8_editor.c — Chromaplex 8 Editor Framework
 *
 * Manages the tab bar (F1=Code, F2=Sprite, F3=Map, F4=SFX),
 * delegates events and drawing to the active sub-editor,
 * and handles global editor actions (Ctrl+S, Ctrl+R, ESC).
 */

#include "cx8_editor.h"
#include "cx8_gpu.h"
#include "cx8_font.h"
#include "cx8_cart.h"
#include "cx8_ed_code.h"
#include "cx8_ed_sprite.h"
#include "cx8_ed_map.h"
#include "cx8_ed_sfx.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

/* ─── State ────────────────────────────────────────────────── */

static cx8_ed_tab_t  s_tab = CX8_ED_CODE;
static cx8_cart_t   *s_cart = NULL;       /* cart being edited */
static char          s_save_path[512];
static bool          s_dirty = false;

#define TAB_BAR_H  10  /* height of the tab bar in pixels */

static const char *s_tab_labels[CX8_ED_TAB_COUNT] = {
    "CODE", "SPRITE", "MAP", "SFX"
};

/* ─── Init / shutdown ──────────────────────────────────────── */

void cx8_editor_init(cx8_cart_t *cart, const char *save_path)
{
    s_cart = cart;
    s_tab  = CX8_ED_CODE;
    s_dirty = false;

    if (save_path)
        strncpy(s_save_path, save_path, sizeof(s_save_path) - 1);
    else
        s_save_path[0] = '\0';

    /* Init sub-editors */
    cx8_ed_code_init(cart ? cart->source : NULL);
    cx8_ed_sprite_init();
    cx8_ed_map_init();
    cx8_ed_sfx_init();

    printf("[CX8-EDITOR] Opened: %s\n", save_path ? save_path : "(new cart)");
}

void cx8_editor_shutdown(void)
{
    cx8_ed_code_shutdown();
    cx8_ed_sprite_shutdown();
    cx8_ed_map_shutdown();
    cx8_ed_sfx_shutdown();
}

/* ─── Cart sync ────────────────────────────────────────────── */

void cx8_editor_sync_to_cart(void)
{
    if (!s_cart) return;

    /* Sync source from code editor */
    if (s_cart->source) {
        free(s_cart->source);
        s_cart->source = NULL;
        s_cart->source_len = 0;
    }
    char *src = cx8_ed_code_get_source();
    if (src) {
        s_cart->source = src;
        s_cart->source_len = strlen(src);
    }

    /* Sync sprite data */
    cx8_ed_sprite_sync();
    if (!s_cart->sprite_data) {
        s_cart->sprite_data = (uint8_t *)malloc(CX8_SPRITESHEET_W * CX8_SPRITESHEET_H);
        if (s_cart->sprite_data)
            s_cart->sprite_len = CX8_SPRITESHEET_W * CX8_SPRITESHEET_H;
    }
    if (s_cart->sprite_data) {
        memcpy(s_cart->sprite_data, cx8_gpu_get_spritesheet(),
               CX8_SPRITESHEET_W * CX8_SPRITESHEET_H);
    }

    /* Sync map data */
    cx8_ed_map_sync();
    if (!s_cart->map_data) {
        s_cart->map_data = (uint8_t *)malloc(CX8_MAP_W * CX8_MAP_H);
        if (s_cart->map_data)
            s_cart->map_len = CX8_MAP_W * CX8_MAP_H;
    }
    if (s_cart->map_data) {
        memcpy(s_cart->map_data, cx8_gpu_get_mapdata(),
               CX8_MAP_W * CX8_MAP_H);
    }
}

cx8_cart_t *cx8_editor_get_cart(void)
{
    return s_cart;
}

const char *cx8_editor_get_path(void)
{
    return s_save_path[0] ? s_save_path : NULL;
}

/* ─── Update ───────────────────────────────────────────────── */

cx8_edit_result_t cx8_editor_update(const SDL_Event *events, int event_count)
{
    /* Pre-filter: check for global keys before passing to sub-editor */
    for (int i = 0; i < event_count; i++) {
        const SDL_Event *e = &events[i];
        if (e->type != SDL_KEYDOWN) continue;

        SDL_Keycode key = e->key.keysym.sym;
        SDL_Keymod mod  = (SDL_Keymod)e->key.keysym.mod;
        bool ctrl = (mod & KMOD_CTRL) != 0;

        /* Tab switching: F1-F4 */
        if (key == SDLK_F1) { s_tab = CX8_ED_CODE;   return CX8_EDIT_NONE; }
        if (key == SDLK_F2) { s_tab = CX8_ED_SPRITE; return CX8_EDIT_NONE; }
        if (key == SDLK_F3) { s_tab = CX8_ED_MAP;    return CX8_EDIT_NONE; }
        if (key == SDLK_F4) { s_tab = CX8_ED_SFX;    return CX8_EDIT_NONE; }

        /* ESC: exit editor */
        if (key == SDLK_ESCAPE) {
            cx8_editor_sync_to_cart();
            return CX8_EDIT_EXIT;
        }

        /* Ctrl+S: save */
        if (ctrl && key == SDLK_s) {
            cx8_editor_sync_to_cart();
            if (s_save_path[0] && s_cart) {
                if (cx8_cart_save(s_save_path, s_cart)) {
                    printf("[CX8-EDITOR] Saved: %s\n", s_save_path);
                    s_dirty = false;
                } else {
                    printf("[CX8-EDITOR] Save FAILED: %s\n", s_save_path);
                }
            }
            return CX8_EDIT_SAVE;
        }

        /* Ctrl+R: run */
        if (ctrl && key == SDLK_r) {
            cx8_editor_sync_to_cart();
            return CX8_EDIT_RUN;
        }
    }

    /* Delegate to active sub-editor */
    switch (s_tab) {
    case CX8_ED_CODE:   cx8_ed_code_update(events, event_count); break;
    case CX8_ED_SPRITE: cx8_ed_sprite_update(events, event_count); break;
    case CX8_ED_MAP:    cx8_ed_map_update(events, event_count); break;
    case CX8_ED_SFX:    cx8_ed_sfx_update(events, event_count); break;
    default: break;
    }

    return CX8_EDIT_NONE;
}

/* ─── Draw ─────────────────────────────────────────────────── */

void cx8_editor_draw(void)
{
    cx8_gpu_cls(0);

    /* ── Tab bar ─────────────────────────────────────────── */
    cx8_gpu_rectfill(0, 0, CX8_SCREEN_W - 1, TAB_BAR_H - 1, 1);

    int tx = 2;
    for (int i = 0; i < CX8_ED_TAB_COUNT; i++) {
        bool active = (i == (int)s_tab);
        uint8_t bg = active ? 0 : 1;
        uint8_t fg = active ? 7 : 4;

        int tw = (int)strlen(s_tab_labels[i]) * 5 + 4;
        cx8_gpu_rectfill(tx, 1, tx + tw, TAB_BAR_H - 1, bg);
        cx8_gpu_print(s_tab_labels[i], tx + 2, 2, fg);

        /* F-key label */
        char fk[4];
        snprintf(fk, sizeof(fk), "F%d", i + 1);
        cx8_gpu_print(fk, tx + tw + 2, 2, 3);

        tx += tw + 14;
    }

    /* Dirty indicator */
    if (s_dirty) {
        cx8_gpu_print("*", CX8_SCREEN_W - 8, 2, 42);
    }

    /* ── Active editor content ───────────────────────────── */
    switch (s_tab) {
    case CX8_ED_CODE:   cx8_ed_code_draw(TAB_BAR_H); break;
    case CX8_ED_SPRITE: cx8_ed_sprite_draw(TAB_BAR_H); break;
    case CX8_ED_MAP:    cx8_ed_map_draw(TAB_BAR_H); break;
    case CX8_ED_SFX:    cx8_ed_sfx_draw(TAB_BAR_H); break;
    default: break;
    }
}
