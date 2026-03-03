/*
 * cx8_font.h — Chromaplex 8 Built-in Pixel Font
 *
 * Embedded 4×6 bitmap font covering ASCII 32-126.
 * Each glyph is 4 pixels wide, 5 pixels tall + 1 row spacing.
 */

#ifndef CX8_FONT_H
#define CX8_FONT_H

#include "cx8.h"

/* Callback type used to plot a pixel (allows GPU to pass its own pset) */
typedef void (*cx8_plot_fn)(int x, int y, uint8_t color);

/* Draw a single character at (x, y) using the given plot function */
void cx8_font_draw_char(char ch, int x, int y, uint8_t color, cx8_plot_fn plot);

/* Get raw glyph data for a character (6 bytes, 4 bits per row) */
const uint8_t *cx8_font_glyph(char ch);

#endif /* CX8_FONT_H */
