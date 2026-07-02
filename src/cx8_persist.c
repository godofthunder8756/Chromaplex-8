/*
 * cx8_persist.c — Chromaplex 8 Persistent Storage
 *
 * Stores 64 numeric slots per cartridge as a simple binary file.
 */

#include "cx8_persist.h"
#include <stdio.h>
#include <string.h>

static double s_slots[CX8_PERSIST_SLOTS];
static char   s_path[512];
static bool   s_dirty = false;

void cx8_persist_init(const char *cart_path)
{
    memset(s_slots, 0, sizeof(s_slots));
    s_path[0] = '\0';
    s_dirty = false;

    if (!cart_path) return;

    /* Build save path: replace extension with .cx8save */
    strncpy(s_path, cart_path, sizeof(s_path) - 10);
    char *dot = strrchr(s_path, '.');
    if (dot)
        strcpy(dot, ".cx8save");
    else
        strcat(s_path, ".cx8save");

    /* Try to load existing save data */
    FILE *f = fopen(s_path, "rb");
    if (f) {
        size_t read = fread(s_slots, sizeof(double), CX8_PERSIST_SLOTS, f);
        fclose(f);
        printf("[CX8-PERSIST] Loaded %zu slots from %s\n", read, s_path);
    }
}

void cx8_persist_shutdown(void)
{
    if (s_dirty)
        cx8_persist_flush();
    s_path[0] = '\0';
}

void cx8_persist_set(int slot, double value)
{
    if (slot < 0 || slot >= CX8_PERSIST_SLOTS) return;
    s_slots[slot] = value;
    s_dirty = true;
}

double cx8_persist_get(int slot)
{
    if (slot < 0 || slot >= CX8_PERSIST_SLOTS) return 0.0;
    return s_slots[slot];
}

bool cx8_persist_flush(void)
{
    if (!s_path[0]) return false;

    FILE *f = fopen(s_path, "wb");
    if (!f) {
        fprintf(stderr, "[CX8-PERSIST] Failed to save: %s\n", s_path);
        return false;
    }
    fwrite(s_slots, sizeof(double), CX8_PERSIST_SLOTS, f);
    fclose(f);
    s_dirty = false;
    return true;
}
