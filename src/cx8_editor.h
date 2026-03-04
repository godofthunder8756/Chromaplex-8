/*
 * cx8_editor.h — Chromaplex 8 Built-in Editor System
 *
 * Provides sprite, code, SFX, and map editors all rendered
 * within the fantasy console's own 256×144 display.
 *
 * Tab switching: F1=Code, F2=Sprite, F3=Map, F4=SFX
 * ESC returns to home / running game.
 */

#ifndef CX8_EDITOR_H
#define CX8_EDITOR_H

#include "cx8.h"
#include "cx8_cart.h"
#include <SDL.h>

/* Editor tabs */
typedef enum {
    CX8_ED_CODE   = 0,
    CX8_ED_SPRITE = 1,
    CX8_ED_MAP    = 2,
    CX8_ED_SFX    = 3,
    CX8_ED_TAB_COUNT
} cx8_ed_tab_t;

/* Result from the editor */
typedef enum {
    CX8_EDIT_NONE,      /* still editing */
    CX8_EDIT_RUN,       /* user wants to run the cart (Ctrl+R) */
    CX8_EDIT_EXIT,      /* user pressed ESC to leave editor */
    CX8_EDIT_SAVE,      /* user saved (Ctrl+S) — auto-handled */
} cx8_edit_result_t;

/* Initialise the editor with a cart (or blank if NULL) */
void cx8_editor_init(cx8_cart_t *cart, const char *save_path);

/* Process one frame of the editor (handles SDL events internally) */
cx8_edit_result_t cx8_editor_update(const SDL_Event *events, int event_count);

/* Draw the editor to VRAM */
void cx8_editor_draw(void);

/* Get the current cart being edited (for running) */
cx8_cart_t *cx8_editor_get_cart(void);

/* Get the current save path */
const char *cx8_editor_get_path(void);

/* Sync editor state back into cart struct before save/run */
void cx8_editor_sync_to_cart(void);

/* Shutdown */
void cx8_editor_shutdown(void);

#endif /* CX8_EDITOR_H */
