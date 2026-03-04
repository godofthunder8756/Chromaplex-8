/*
 * cx8_home.c — Chromaplex 8 Home Screen & Cart Browser
 *
 * Scans a directory for .lua/.cx8 files, displays them in a
 * scrollable list with animated selection, and lets the user
 * launch or edit cartridges.
 */

#include "cx8_home.h"
#include "cx8_gpu.h"
#include "cx8_input.h"
#include "cx8_font.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#include <windows.h>
#else
#include <dirent.h>
#endif

/* ─── State ────────────────────────────────────────────────── */

static cx8_home_entry_t s_entries[CX8_HOME_MAX_CARTS];
static int              s_count    = 0;
static int              s_cursor   = 0;
static int              s_scroll   = 0;
static int              s_anim     = 0;   /* animation frame counter */
static char             s_carts_dir[CX8_HOME_PATH_LEN] = "";

#define VISIBLE_ROWS  12    /* how many carts fit on screen */
#define LIST_TOP      36    /* y-position where list starts */
#define LIST_LEFT     12
#define ROW_HEIGHT    8

/* ─── Helpers ──────────────────────────────────────────────── */

static bool ends_with(const char *s, const char *suffix)
{
    size_t slen = strlen(s);
    size_t plen = strlen(suffix);
    if (plen > slen) return false;
    return strcmp(s + slen - plen, suffix) == 0;
}

/* Try to extract title from a cart file's header comments */
static void extract_title(const char *path, char *name_out, char *author_out)
{
    FILE *f = fopen(path, "r");
    if (!f) return;

    char line[256];
    while (fgets(line, sizeof(line), f)) {
        if (line[0] != '-') break;
        if (strncmp(line, "-- title:", 9) == 0) {
            char *t = line + 9;
            while (*t == ' ') t++;
            char *end = t + strlen(t) - 1;
            while (end > t && (*end == '\n' || *end == '\r' || *end == ' ')) *end-- = '\0';
            name_out[CX8_HOME_NAME_LEN - 1] = '\0';
            snprintf(name_out, CX8_HOME_NAME_LEN, "%s", t);
        } else if (strncmp(line, "-- author:", 10) == 0) {
            char *a = line + 10;
            while (*a == ' ') a++;
            char *end = a + strlen(a) - 1;
            while (end > a && (*end == '\n' || *end == '\r' || *end == ' ')) *end-- = '\0';
            author_out[CX8_HOME_NAME_LEN - 1] = '\0';
            snprintf(author_out, CX8_HOME_NAME_LEN, "%s", a);
        }
    }
    fclose(f);
}

/* Get just the filename (without path) */
static const char *basename_of(const char *path)
{
    const char *last = path;
    for (const char *p = path; *p; p++) {
        if (*p == '/' || *p == '\\') last = p + 1;
    }
    return last;
}

/* ─── Scan directory ───────────────────────────────────────── */

static void scan_directory(const char *dir)
{
    s_count = 0;

#ifdef _WIN32
    char pattern[CX8_HOME_PATH_LEN];
    snprintf(pattern, sizeof(pattern), "%s\\*", dir);

    WIN32_FIND_DATAA fd;
    HANDLE hFind = FindFirstFileA(pattern, &fd);
    if (hFind == INVALID_HANDLE_VALUE) return;

    do {
        if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) continue;
        const char *fname = fd.cFileName;
        if (!ends_with(fname, ".lua") && !ends_with(fname, ".cx8")) continue;
        if (s_count >= CX8_HOME_MAX_CARTS) break;

        cx8_home_entry_t *e = &s_entries[s_count];
        memset(e, 0, sizeof(*e));
        snprintf(e->path, sizeof(e->path), "%s\\%s", dir, fname);

        /* Default name = filename without extension */
        e->name[CX8_HOME_NAME_LEN - 1] = '\0';
        snprintf(e->name, CX8_HOME_NAME_LEN, "%s", fname);
        char *dot = strrchr(e->name, '.');
        if (dot) *dot = '\0';

        /* Try to read title from file header */
        extract_title(e->path, e->name, e->author);
        s_count++;
    } while (FindNextFileA(hFind, &fd));
    FindClose(hFind);
#else
    DIR *d = opendir(dir);
    if (!d) return;
    struct dirent *ent;
    while ((ent = readdir(d)) != NULL && s_count < CX8_HOME_MAX_CARTS) {
        if (ent->d_type == DT_DIR) continue;
        if (!ends_with(ent->d_name, ".lua") && !ends_with(ent->d_name, ".cx8")) continue;

        cx8_home_entry_t *e = &s_entries[s_count];
        memset(e, 0, sizeof(*e));
        snprintf(e->path, sizeof(e->path), "%s/%s", dir, ent->d_name);
        strncpy(e->name, ent->d_name, CX8_HOME_NAME_LEN - 1);
        char *dot = strrchr(e->name, '.');
        if (dot) *dot = '\0';
        extract_title(e->path, e->name, e->author);
        s_count++;
    }
    closedir(d);
#endif
}

/* ─── Public API ───────────────────────────────────────────── */

void cx8_home_init(const char *carts_dir)
{
    strncpy(s_carts_dir, carts_dir, sizeof(s_carts_dir) - 1);
    s_cursor = 0;
    s_scroll = 0;
    s_anim   = 0;
    scan_directory(carts_dir);
    printf("[CX8-HOME] Found %d cartridge(s) in %s\n", s_count, carts_dir);
}

cx8_home_result_t cx8_home_update(void)
{
    s_anim++;

    /* Navigation */
    if (cx8_input_btnp(CX8_BTN_UP)) {
        s_cursor--;
        if (s_cursor < 0) s_cursor = s_count > 0 ? s_count - 1 : 0;
    }
    if (cx8_input_btnp(CX8_BTN_DOWN)) {
        s_cursor++;
        if (s_cursor >= s_count) s_cursor = 0;
    }

    /* Keep cursor visible */
    if (s_cursor < s_scroll) s_scroll = s_cursor;
    if (s_cursor >= s_scroll + VISIBLE_ROWS) s_scroll = s_cursor - VISIBLE_ROWS + 1;

    /* A / Z = run cart */
    if (cx8_input_btnp(CX8_BTN_A) && s_count > 0) {
        return CX8_HOME_RUN;
    }

    /* B / X = edit cart */
    if (cx8_input_btnp(CX8_BTN_B) && s_count > 0) {
        return CX8_HOME_EDIT;
    }

    /* X button = new blank cart */
    if (cx8_input_btnp(CX8_BTN_X)) {
        return CX8_HOME_NEW;
    }

    return CX8_HOME_NONE;
}

void cx8_home_draw(void)
{
    cx8_gpu_cls(0);

    /* ── Header bar ──────────────────────────────────────── */
    cx8_gpu_rectfill(0, 0, CX8_SCREEN_W - 1, 8, 1);

    /* Animated rainbow title */
    const char *title = "CHROMAPLEX 8";
    int tx = 4;
    for (int i = 0; title[i]; i++) {
        uint8_t c = (uint8_t)(42 + ((i + s_anim / 8) % 6));
        char buf[2] = { title[i], 0 };
        cx8_gpu_print(buf, tx, 2, c);
        tx += 5;
    }

    cx8_gpu_print("HOME", CX8_SCREEN_W - 24, 2, 7);

    /* ── Decorative line ─────────────────────────────────── */
    for (int x = 0; x < CX8_SCREEN_W; x++) {
        uint8_t c = (uint8_t)(40 + (x / 32) % 4);
        cx8_gpu_pset(x, 10, c);
    }

    /* ── Section header ──────────────────────────────────── */
    cx8_gpu_print("CARTRIDGES", LIST_LEFT, 14, 7);

    /* Cart count */
    char cbuf[32];
    snprintf(cbuf, sizeof(cbuf), "[%d]", s_count);
    cx8_gpu_print(cbuf, LIST_LEFT + 55, 14, 5);

    /* Separator */
    for (int x = LIST_LEFT; x < CX8_SCREEN_W - LIST_LEFT; x++)
        cx8_gpu_pset(x, 22, 1);

    /* ── Folder path ─────────────────────────────────────── */
    cx8_gpu_print(basename_of(s_carts_dir), LIST_LEFT, 26, 4);

    /* ── Cart list ───────────────────────────────────────── */
    if (s_count == 0) {
        cx8_gpu_print("NO CARTS FOUND", LIST_LEFT + 20, 70, 3);
        cx8_gpu_print("PRESS [X] TO CREATE NEW", LIST_LEFT + 4, 82, 4);
    } else {
        int end = s_scroll + VISIBLE_ROWS;
        if (end > s_count) end = s_count;

        for (int i = s_scroll; i < end; i++) {
            int row = i - s_scroll;
            int y   = LIST_TOP + row * ROW_HEIGHT;
            bool selected = (i == s_cursor);

            if (selected) {
                /* Selection highlight */
                uint8_t hc = (s_anim / 4) % 2 == 0 ? 1 : 2;
                cx8_gpu_rectfill(LIST_LEFT - 2, y - 1,
                                 CX8_SCREEN_W - LIST_LEFT + 1, y + 6, hc);

                /* Selection arrow */
                cx8_gpu_print(">", LIST_LEFT, y, 61);
            }

            /* Cart name */
            uint8_t nc = selected ? 7 : 5;
            cx8_gpu_print(s_entries[i].name, LIST_LEFT + 8, y, nc);

            /* Author (if known) */
            if (s_entries[i].author[0]) {
                uint8_t ac = selected ? 4 : 3;
                int ax = LIST_LEFT + 8 + (int)strlen(s_entries[i].name) * 5 + 6;
                if (ax < CX8_SCREEN_W - 40) {
                    cx8_gpu_print(s_entries[i].author, ax, y, ac);
                }
            }
        }

        /* Scroll indicators */
        if (s_scroll > 0)
            cx8_gpu_print("^", CX8_SCREEN_W - 10, LIST_TOP, 4);
        if (s_scroll + VISIBLE_ROWS < s_count)
            cx8_gpu_print("v", CX8_SCREEN_W - 10, LIST_TOP + (VISIBLE_ROWS - 1) * ROW_HEIGHT, 4);
    }

    /* ── Bottom status bar ───────────────────────────────── */
    cx8_gpu_rectfill(0, CX8_SCREEN_H - 10, CX8_SCREEN_W - 1, CX8_SCREEN_H - 1, 1);
    cx8_gpu_print("[Z]RUN", 4, CX8_SCREEN_H - 8, 61);
    cx8_gpu_print("[X]EDIT", 44, CX8_SCREEN_H - 8, 47);
    cx8_gpu_print("[A]NEW", 88, CX8_SCREEN_H - 8, 42);
    cx8_gpu_print("ESC:QUIT", CX8_SCREEN_W - 46, CX8_SCREEN_H - 8, 4);

    /* ── Module bay indicator ────────────────────────────── */
    cx8_gpu_print("MODULES:", CX8_SCREEN_W - 80, 14, 3);
}

const char *cx8_home_selected_path(void)
{
    if (s_cursor >= 0 && s_cursor < s_count)
        return s_entries[s_cursor].path;
    return NULL;
}

int cx8_home_selected_index(void)
{
    return s_cursor;
}

int cx8_home_entry_count(void)
{
    return s_count;
}

const cx8_home_entry_t *cx8_home_get_entry(int idx)
{
    if (idx >= 0 && idx < s_count)
        return &s_entries[idx];
    return NULL;
}
