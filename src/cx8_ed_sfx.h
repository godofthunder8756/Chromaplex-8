/*
 * cx8_ed_sfx.h — Chromaplex 8 SFX / Audio Editor
 *
 * Create and preview sound effects using the WAVE-4 APU.
 * Waveform selector, frequency control, ADSR envelope editor.
 */

#ifndef CX8_ED_SFX_H
#define CX8_ED_SFX_H

#include "cx8.h"
#include <SDL.h>

/* Initialise the SFX editor */
void cx8_ed_sfx_init(void);

/* Process events */
void cx8_ed_sfx_update(const SDL_Event *events, int count);

/* Draw the SFX editor to VRAM */
void cx8_ed_sfx_draw(int tab_y);

/* Shutdown */
void cx8_ed_sfx_shutdown(void);

#endif /* CX8_ED_SFX_H */
