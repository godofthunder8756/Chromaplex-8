/*
 * cx8_ed_code.h — Chromaplex 8 Built-in Code Editor
 */

#ifndef CX8_ED_CODE_H
#define CX8_ED_CODE_H

#include "cx8.h"
#include <SDL.h>

#define CX8_CODE_MAX_LINES  4096
#define CX8_CODE_MAX_COLS   256

void cx8_ed_code_init(const char *source);
void cx8_ed_code_update(const SDL_Event *events, int count);
void cx8_ed_code_draw(int tab_y);

/* Get the full source text (caller must free) */
char *cx8_ed_code_get_source(void);

/* Get line/col cursor position */
int cx8_ed_code_cursor_line(void);
int cx8_ed_code_cursor_col(void);
int cx8_ed_code_line_count(void);

void cx8_ed_code_shutdown(void);

#endif /* CX8_ED_CODE_H */
