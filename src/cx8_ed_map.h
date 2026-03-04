/*
 * cx8_ed_map.h — Chromaplex 8 Map Editor
 *
 * Tile-based editor for the 256×256 map.
 * Paint tiles from the spritesheet onto the map grid.
 */

#ifndef CX8_ED_MAP_H
#define CX8_ED_MAP_H

#include "cx8.h"
#include <SDL.h>

/* Initialise (loads current map from GPU) */
void cx8_ed_map_init(void);

/* Process events */
void cx8_ed_map_update(const SDL_Event *events, int count);

/* Draw the map editor to VRAM */
void cx8_ed_map_draw(int tab_y);

/* Write edited data back to GPU map */
void cx8_ed_map_sync(void);

/* Shutdown */
void cx8_ed_map_shutdown(void);

#endif /* CX8_ED_MAP_H */
