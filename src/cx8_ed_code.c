/*
 * cx8_ed_code.c — Chromaplex 8 Built-in Code Editor
 *
 * A minimal text editor rendered on the 256×144 display.
 * Features: scrolling, cursor, basic editing, Lua keyword colouring.
 */

#include "cx8_ed_code.h"
#include "cx8_gpu.h"
#include "cx8_font.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

/* ─── State ────────────────────────────────────────────────── */

/* Each line stored as a simple char buffer */
static char  s_lines[CX8_CODE_MAX_LINES][CX8_CODE_MAX_COLS];
static int   s_line_count = 0;
static int   s_cur_line   = 0;   /* cursor line   */
static int   s_cur_col    = 0;   /* cursor column */
static int   s_scroll_y   = 0;   /* top visible line */
static int   s_scroll_x   = 0;   /* horizontal scroll */
static int   s_blink      = 0;   /* cursor blink timer */

#define CODE_CHAR_W   5       /* font char width including spacing */
#define CODE_CHAR_H   7       /* font char height including spacing */
#define CODE_LEFT     20      /* left margin for line numbers */
#define CODE_RIGHT    (CX8_SCREEN_W - 2)
#define CODE_COLS     ((CODE_RIGHT - CODE_LEFT) / CODE_CHAR_W)

/* ─── Lua keyword colouring ────────────────────────────────── */

static const char *s_keywords[] = {
    "and","break","do","else","elseif","end","false","for","function",
    "goto","if","in","local","nil","not","or","repeat","return",
    "then","true","until","while", NULL
};

static const char *s_builtins[] = {
    "print","cls","pset","pget","line","rect","rectfill","circ","circfill",
    "tri","trifill","spr","sset","sget","map","mset","mget","camera",
    "clip","pal","btn","btnp","sfx","envelope","quiet","peek","poke",
    "peek16","poke16","memcpy","memset","mod_load","mod_check","mod_info",
    "rnd","flr","ceil","sin","cos","atan2","sgn","mid","time", NULL
};

static bool is_keyword(const char *word, int len)
{
    for (int i = 0; s_keywords[i]; i++) {
        if ((int)strlen(s_keywords[i]) == len && strncmp(word, s_keywords[i], (size_t)len) == 0)
            return true;
    }
    return false;
}

static bool is_builtin(const char *word, int len)
{
    for (int i = 0; s_builtins[i]; i++) {
        if ((int)strlen(s_builtins[i]) == len && strncmp(word, s_builtins[i], (size_t)len) == 0)
            return true;
    }
    return false;
}

/* ─── Init / shutdown ──────────────────────────────────────── */

void cx8_ed_code_init(const char *source)
{
    memset(s_lines, 0, sizeof(s_lines));
    s_line_count = 1;
    s_cur_line = 0;
    s_cur_col  = 0;
    s_scroll_y = 0;
    s_scroll_x = 0;

    if (!source || !*source) return;

    /* Split source into lines */
    s_line_count = 0;
    const char *p = source;
    while (*p && s_line_count < CX8_CODE_MAX_LINES) {
        const char *eol = strchr(p, '\n');
        int len;
        if (eol) {
            len = (int)(eol - p);
        } else {
            len = (int)strlen(p);
        }
        if (len >= CX8_CODE_MAX_COLS) len = CX8_CODE_MAX_COLS - 1;
        memcpy(s_lines[s_line_count], p, (size_t)len);
        s_lines[s_line_count][len] = '\0';
        /* strip trailing \r */
        if (len > 0 && s_lines[s_line_count][len - 1] == '\r')
            s_lines[s_line_count][--len] = '\0';
        s_line_count++;
        if (!eol) break;
        p = eol + 1;
    }
    if (s_line_count == 0) s_line_count = 1;
}

void cx8_ed_code_shutdown(void)
{
    /* nothing dynamic */
}

/* ─── Source extraction ────────────────────────────────────── */

char *cx8_ed_code_get_source(void)
{
    /* Calculate total size */
    size_t total = 0;
    for (int i = 0; i < s_line_count; i++)
        total += strlen(s_lines[i]) + 1; /* +1 for \n */

    char *buf = (char *)malloc(total + 1);
    if (!buf) return NULL;

    char *p = buf;
    for (int i = 0; i < s_line_count; i++) {
        size_t len = strlen(s_lines[i]);
        memcpy(p, s_lines[i], len);
        p += len;
        *p++ = '\n';
    }
    *p = '\0';
    return buf;
}

int cx8_ed_code_cursor_line(void) { return s_cur_line; }
int cx8_ed_code_cursor_col(void)  { return s_cur_col; }
int cx8_ed_code_line_count(void)  { return s_line_count; }

/* ─── Editing helpers ──────────────────────────────────────── */

static void insert_char(char ch)
{
    char *line = s_lines[s_cur_line];
    int len = (int)strlen(line);
    if (len >= CX8_CODE_MAX_COLS - 1) return;

    /* Shift right */
    memmove(line + s_cur_col + 1, line + s_cur_col, (size_t)(len - s_cur_col + 1));
    line[s_cur_col] = ch;
    s_cur_col++;
}

static void delete_char(void)
{
    char *line = s_lines[s_cur_line];
    int len = (int)strlen(line);
    if (s_cur_col < len) {
        memmove(line + s_cur_col, line + s_cur_col + 1, (size_t)(len - s_cur_col));
    }
}

static void backspace(void)
{
    if (s_cur_col > 0) {
        s_cur_col--;
        delete_char();
    } else if (s_cur_line > 0) {
        /* Merge with previous line */
        int prev_len = (int)strlen(s_lines[s_cur_line - 1]);
        char *prev = s_lines[s_cur_line - 1];
        char *cur  = s_lines[s_cur_line];
        int cur_len = (int)strlen(cur);
        if (prev_len + cur_len < CX8_CODE_MAX_COLS - 1) {
            strcat(prev, cur);
            /* Shift lines up */
            for (int i = s_cur_line; i < s_line_count - 1; i++)
                memcpy(s_lines[i], s_lines[i + 1], CX8_CODE_MAX_COLS);
            s_line_count--;
            s_cur_line--;
            s_cur_col = prev_len;
        }
    }
}

static void insert_newline(void)
{
    if (s_line_count >= CX8_CODE_MAX_LINES) return;

    char *line = s_lines[s_cur_line];
    int len = (int)strlen(line);

    /* Shift lines down */
    for (int i = s_line_count; i > s_cur_line + 1; i--)
        memcpy(s_lines[i], s_lines[i - 1], CX8_CODE_MAX_COLS);
    s_line_count++;

    /* Split current line at cursor */
    int tail_len = len - s_cur_col;
    if (tail_len < 0) tail_len = 0;
    memcpy(s_lines[s_cur_line + 1], line + s_cur_col, (size_t)tail_len);
    s_lines[s_cur_line + 1][tail_len] = '\0';
    line[s_cur_col] = '\0';

    /* Auto-indent: copy leading whitespace */
    int indent = 0;
    while (indent < s_cur_col && (line[indent] == ' ' || line[indent] == '\t'))
        indent++;

    if (indent > 0) {
        char *new_line = s_lines[s_cur_line + 1];
        int new_len = (int)strlen(new_line);
        memmove(new_line + indent, new_line, (size_t)(new_len + 1));
        memcpy(new_line, line, (size_t)indent);
    }

    s_cur_line++;
    s_cur_col = indent;
}

/* ─── Event handling ───────────────────────────────────────── */

void cx8_ed_code_update(const SDL_Event *events, int count)
{
    s_blink++;

    for (int i = 0; i < count; i++) {
        const SDL_Event *e = &events[i];

        if (e->type == SDL_TEXTINPUT) {
            /* Regular character input */
            for (int j = 0; e->text.text[j]; j++) {
                char ch = e->text.text[j];
                if (ch >= 32 && ch < 127)
                    insert_char(ch);
            }
            s_blink = 0;
            continue;
        }

        if (e->type != SDL_KEYDOWN) continue;

        SDL_Keycode key = e->key.keysym.sym;
        SDL_Keymod mod  = (SDL_Keymod)e->key.keysym.mod;
        bool ctrl = (mod & KMOD_CTRL) != 0;
        (void)ctrl;

        switch (key) {
        case SDLK_LEFT:
            if (s_cur_col > 0) s_cur_col--;
            else if (s_cur_line > 0) { s_cur_line--; s_cur_col = (int)strlen(s_lines[s_cur_line]); }
            break;
        case SDLK_RIGHT:
            if (s_cur_col < (int)strlen(s_lines[s_cur_line])) s_cur_col++;
            else if (s_cur_line < s_line_count - 1) { s_cur_line++; s_cur_col = 0; }
            break;
        case SDLK_UP:
            if (s_cur_line > 0) {
                s_cur_line--;
                int len = (int)strlen(s_lines[s_cur_line]);
                if (s_cur_col > len) s_cur_col = len;
            }
            break;
        case SDLK_DOWN:
            if (s_cur_line < s_line_count - 1) {
                s_cur_line++;
                int len = (int)strlen(s_lines[s_cur_line]);
                if (s_cur_col > len) s_cur_col = len;
            }
            break;
        case SDLK_HOME:
            s_cur_col = 0;
            break;
        case SDLK_END:
            s_cur_col = (int)strlen(s_lines[s_cur_line]);
            break;
        case SDLK_PAGEUP:
            s_cur_line -= 16;
            if (s_cur_line < 0) s_cur_line = 0;
            { int len = (int)strlen(s_lines[s_cur_line]); if (s_cur_col > len) s_cur_col = len; }
            break;
        case SDLK_PAGEDOWN:
            s_cur_line += 16;
            if (s_cur_line >= s_line_count) s_cur_line = s_line_count - 1;
            { int len = (int)strlen(s_lines[s_cur_line]); if (s_cur_col > len) s_cur_col = len; }
            break;
        case SDLK_RETURN:
        case SDLK_KP_ENTER:
            insert_newline();
            break;
        case SDLK_BACKSPACE:
            backspace();
            break;
        case SDLK_DELETE:
            delete_char();
            break;
        case SDLK_TAB:
            /* Insert 2 spaces */
            insert_char(' ');
            insert_char(' ');
            break;
        default:
            break;
        }

        s_blink = 0;
    }

    /* Scroll to keep cursor visible */
    int vis_lines = (CX8_SCREEN_H - 20) / CODE_CHAR_H;
    if (s_cur_line < s_scroll_y) s_scroll_y = s_cur_line;
    if (s_cur_line >= s_scroll_y + vis_lines) s_scroll_y = s_cur_line - vis_lines + 1;
    if (s_cur_col < s_scroll_x) s_scroll_x = s_cur_col;
    if (s_cur_col >= s_scroll_x + CODE_COLS) s_scroll_x = s_cur_col - CODE_COLS + 1;
}

/* ─── Syntax colouring for a single line ───────────────────── */

static uint8_t char_color(const char *line, int col)
{
    /* Comment: -- to end of line → dark green */
    for (int i = 0; i < col; i++) {
        if (line[i] == '-' && line[i + 1] == '-') {
            return 3;  /* dark green for comments */
        }
    }
    if (col >= 1 && line[col - 1] == '-' && line[col] == '-') return 3;

    /* Check if inside a string */
    bool in_str = false;
    char quote = 0;
    for (int i = 0; i <= col; i++) {
        if (!in_str && (line[i] == '"' || line[i] == '\'')) {
            in_str = true;
            quote = line[i];
        } else if (in_str && line[i] == quote && (i == 0 || line[i-1] != '\\')) {
            if (i < col) in_str = false;
        }
    }
    if (in_str) return 47;  /* teal for strings */

    /* Check for comment */
    for (int i = 0; i <= col; i++) {
        if (line[i] == '-' && i + 1 < (int)strlen(line) && line[i + 1] == '-')
            return 3;
    }

    char ch = line[col];

    /* Number */
    if (isdigit((unsigned char)ch) ||
        (ch == '.' && col + 1 < (int)strlen(line) && isdigit((unsigned char)line[col + 1]))) {
        return 61;  /* cyan for numbers */
    }

    /* Identifier: check if part of keyword/builtin */
    if (isalpha((unsigned char)ch) || ch == '_') {
        /* Find word boundaries */
        int start = col;
        while (start > 0 && (isalnum((unsigned char)line[start - 1]) || line[start - 1] == '_'))
            start--;
        int end = col;
        while (line[end] && (isalnum((unsigned char)line[end]) || line[end] == '_'))
            end++;
        int wlen = end - start;

        if (col == start) { /* only colour at word start for efficiency */
            if (is_keyword(line + start, wlen)) return 42;  /* orange for keywords */
            if (is_builtin(line + start, wlen)) return 47;  /* teal for builtins */
        } else {
            /* Continue the colour */
            if (is_keyword(line + start, wlen)) return 42;
            if (is_builtin(line + start, wlen)) return 47;
        }
        return 7;  /* white for identifiers */
    }

    /* Operators and symbols */
    if (strchr("+-*/%^#=<>~(){}[];:,.", ch))
        return 5;  /* grey for operators */

    return 7;  /* default white */
}

/* ─── Draw ─────────────────────────────────────────────────── */

void cx8_ed_code_draw(int tab_y)
{
    int vis_lines = (CX8_SCREEN_H - tab_y - 10) / CODE_CHAR_H;

    /* Line number gutter background */
    cx8_gpu_rectfill(0, tab_y, CODE_LEFT - 2, CX8_SCREEN_H - 10, 1);

    for (int i = 0; i < vis_lines && (s_scroll_y + i) < s_line_count; i++) {
        int ln  = s_scroll_y + i;
        int y   = tab_y + i * CODE_CHAR_H;
        char *line = s_lines[ln];
        int len = (int)strlen(line);

        /* Line number */
        char num[8];
        snprintf(num, sizeof(num), "%3d", ln + 1);
        cx8_gpu_print(num, 1, y, 4);

        /* Current line highlight */
        if (ln == s_cur_line) {
            cx8_gpu_rectfill(CODE_LEFT - 1, y - 1,
                             CX8_SCREEN_W - 1, y + CODE_CHAR_H - 2, 1);
        }

        /* Source text with syntax colouring */
        for (int c = s_scroll_x; c < len && c < s_scroll_x + CODE_COLS; c++) {
            int x = CODE_LEFT + (c - s_scroll_x) * CODE_CHAR_W;
            uint8_t col = char_color(line, c);
            char ch[2] = { line[c], 0 };
            cx8_gpu_print(ch, x, y, col);
        }

        /* Cursor */
        if (ln == s_cur_line && (s_blink / 15) % 2 == 0) {
            int cx = CODE_LEFT + (s_cur_col - s_scroll_x) * CODE_CHAR_W;
            cx8_gpu_rectfill(cx, y - 1, cx + CODE_CHAR_W - 2, y + CODE_CHAR_H - 2, 7);
            /* Draw char under cursor in inverted colour */
            if (s_cur_col < len) {
                char ch[2] = { line[s_cur_col], 0 };
                cx8_gpu_print(ch, cx, y, 0);
            }
        }
    }

    /* Status bar */
    cx8_gpu_rectfill(0, CX8_SCREEN_H - 9, CX8_SCREEN_W - 1, CX8_SCREEN_H - 1, 1);
    char status[80];
    snprintf(status, sizeof(status), "LN %d/%d  COL %d", s_cur_line + 1, s_line_count, s_cur_col + 1);
    cx8_gpu_print(status, 4, CX8_SCREEN_H - 7, 5);
    cx8_gpu_print("CTRL+S:SAVE CTRL+R:RUN", CX8_SCREEN_W - 118, CX8_SCREEN_H - 7, 4);
}
