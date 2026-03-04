/*
 * cx8_ed_sprite.h — Chromaplex 8 Sprite Editor
 *
 * Pixel-art editor for the 128×128 spritesheet (256 8×8 sprites).
 * Tools: draw, erase, colour pick.  64-colour palette.
 */

#ifndef CX8_ED_SPRITE_H
#define CX8_ED_SPRITE_H

#include "cx8.h"
#include <SDL.h>

/* Initialise (loads current spritesheet from GPU) */
void cx8_ed_sprite_init(void);

/* Process events (returns nothing; parent handles tab/exit) */
void cx8_ed_sprite_update(const SDL_Event *events, int count);

/* Draw the sprite editor to VRAM (tab_y = top of content area) */
void cx8_ed_sprite_draw(int tab_y);

/* Write edited data back to GPU spritesheet */
void cx8_ed_sprite_sync(void);

/* Shutdown */
void cx8_ed_sprite_shutdown(void);

#endif /* CX8_ED_SPRITE_H */
