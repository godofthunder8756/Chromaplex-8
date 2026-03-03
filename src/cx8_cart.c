/*
 * cx8_cart.c — Chromaplex 8 Cartridge System
 *
 * Loads .cx8 cartridge files (Lua source + optional binary data)
 * or plain .lua scripts.
 *
 * .cx8 format:
 *   --[[cx8 cart]]
 *   -- title:  My Game
 *   -- author: Me
 *   -- desc:   A cool game
 *   <lua source>
 *   __sprites__
 *   <hex-encoded sprite data>
 *   __map__
 *   <hex-encoded map data>
 *   __end__
 */

#include "cx8_cart.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>

static size_t s_max_cart = CX8_CART_SIZE;

/* ─── Helpers ──────────────────────────────────────────────── */

static void trim(char *s)
{
    char *end = s + strlen(s) - 1;
    while (end > s && isspace((unsigned char)*end)) *end-- = '\0';
    char *start = s;
    while (*start && isspace((unsigned char)*start)) start++;
    if (start != s) memmove(s, start, strlen(start) + 1);
}

static bool starts_with(const char *line, const char *prefix)
{
    return strncmp(line, prefix, strlen(prefix)) == 0;
}

/* Decode hex string into binary buffer. Returns bytes written. */
static size_t hex_decode(const char *hex, uint8_t *buf, size_t max)
{
    size_t n = 0;
    while (*hex && n < max) {
        while (*hex && isspace((unsigned char)*hex)) hex++;
        if (!*hex) break;
        char hi = *hex++;
        if (!*hex) break;
        char lo = *hex++;
        int hv = (hi >= 'a') ? hi - 'a' + 10 : (hi >= 'A') ? hi - 'A' + 10 : hi - '0';
        int lv = (lo >= 'a') ? lo - 'a' + 10 : (lo >= 'A') ? lo - 'A' + 10 : lo - '0';
        buf[n++] = (uint8_t)((hv << 4) | lv);
    }
    return n;
}

/* ─── Public API ───────────────────────────────────────────── */

void cx8_cart_init(void)
{
    s_max_cart = CX8_CART_SIZE;
    printf("[CX8-CART] Cartridge system ready (%zu KB max)\n", s_max_cart / 1024);
}

void cx8_cart_shutdown(void)
{
    /* Nothing to clean up */
}

void cx8_cart_expand(size_t extra_bytes)
{
    s_max_cart += extra_bytes;
    printf("[CX8-CART] Expanded to %zu KB max\n", s_max_cart / 1024);
}

bool cx8_cart_load(const char *path, cx8_cart_t *cart)
{
    if (!path || !cart) return false;
    memset(cart, 0, sizeof(*cart));
    cart->max_size = s_max_cart;

    FILE *f = fopen(path, "rb");
    if (!f) {
        fprintf(stderr, "[CX8-CART] Cannot open: %s\n", path);
        return false;
    }

    /* Read entire file */
    fseek(f, 0, SEEK_END);
    long fsize = ftell(f);
    fseek(f, 0, SEEK_SET);

    if (fsize <= 0 || (size_t)fsize > s_max_cart) {
        fprintf(stderr, "[CX8-CART] File too large (%ld bytes, max %zu)\n",
                fsize, s_max_cart);
        fclose(f);
        return false;
    }

    char *raw = (char *)malloc((size_t)fsize + 1);
    if (!raw) { fclose(f); return false; }
    fread(raw, 1, (size_t)fsize, f);
    raw[fsize] = '\0';
    fclose(f);

    strncpy(cart->filename, path, sizeof(cart->filename) - 1);

    /* Parse header comments for metadata */
    char *line_start = raw;

    while (*line_start == '-' || *line_start == '\n' || *line_start == '\r') {
        char *line_end = strchr(line_start, '\n');
        if (!line_end) break;
        *line_end = '\0';

        if (starts_with(line_start, "-- title:")) {
            strncpy(cart->title, line_start + 9, sizeof(cart->title) - 1);
            trim(cart->title);
        } else if (starts_with(line_start, "-- author:")) {
            strncpy(cart->author, line_start + 10, sizeof(cart->author) - 1);
            trim(cart->author);
        } else if (starts_with(line_start, "-- desc:")) {
            strncpy(cart->description, line_start + 8, sizeof(cart->description) - 1);
            trim(cart->description);
        }

        *line_end = '\n';
        line_start = line_end + 1;
    }

    /* Find section markers */
    char *sprite_section = strstr(raw, "__sprites__");
    char *map_section    = strstr(raw, "__map__");
    char *end_section    = strstr(raw, "__end__");

    /* Extract Lua source (everything before __sprites__ or end) */
    size_t src_len;
    if (sprite_section) {
        src_len = (size_t)(sprite_section - raw);
    } else if (end_section) {
        src_len = (size_t)(end_section - raw);
    } else {
        src_len = (size_t)fsize;
    }

    cart->source     = (char *)malloc(src_len + 1);
    cart->source_len = src_len;
    memcpy(cart->source, raw, src_len);
    cart->source[src_len] = '\0';

    /* Extract sprite data */
    if (sprite_section) {
        char *data_start = sprite_section + strlen("__sprites__");
        while (*data_start == '\n' || *data_start == '\r') data_start++;
        char *data_end = map_section ? map_section : (end_section ? end_section : raw + fsize);
        size_t hex_len = (size_t)(data_end - data_start);
        cart->sprite_data = (uint8_t *)malloc(hex_len / 2 + 1);
        cart->sprite_len  = hex_decode(data_start, cart->sprite_data, hex_len / 2 + 1);
    }

    /* Extract map data */
    if (map_section) {
        char *data_start = map_section + strlen("__map__");
        while (*data_start == '\n' || *data_start == '\r') data_start++;
        char *data_end = end_section ? end_section : raw + fsize;
        size_t hex_len = (size_t)(data_end - data_start);
        cart->map_data = (uint8_t *)malloc(hex_len / 2 + 1);
        cart->map_len  = hex_decode(data_start, cart->map_data, hex_len / 2 + 1);
    }

    free(raw);

    if (cart->title[0])
        printf("[CX8-CART] Loaded: \"%s\" by %s\n", cart->title,
               cart->author[0] ? cart->author : "unknown");
    else
        printf("[CX8-CART] Loaded: %s (%zu bytes)\n", path, cart->source_len);

    return true;
}

bool cx8_cart_save(const char *path, const cx8_cart_t *cart)
{
    if (!path || !cart || !cart->source) return false;

    FILE *f = fopen(path, "wb");
    if (!f) return false;

    /* Header */
    fprintf(f, "--[[cx8 cart]]\n");
    if (cart->title[0])       fprintf(f, "-- title:  %s\n", cart->title);
    if (cart->author[0])      fprintf(f, "-- author: %s\n", cart->author);
    if (cart->description[0]) fprintf(f, "-- desc:   %s\n", cart->description);
    fprintf(f, "\n");

    /* Source */
    fwrite(cart->source, 1, cart->source_len, f);
    fprintf(f, "\n");

    /* Sprites */
    if (cart->sprite_data && cart->sprite_len > 0) {
        fprintf(f, "__sprites__\n");
        for (size_t i = 0; i < cart->sprite_len; i++) {
            fprintf(f, "%02x", cart->sprite_data[i]);
            if ((i + 1) % 64 == 0) fprintf(f, "\n");
        }
        fprintf(f, "\n");
    }

    /* Map */
    if (cart->map_data && cart->map_len > 0) {
        fprintf(f, "__map__\n");
        for (size_t i = 0; i < cart->map_len; i++) {
            fprintf(f, "%02x", cart->map_data[i]);
            if ((i + 1) % 64 == 0) fprintf(f, "\n");
        }
        fprintf(f, "\n");
    }

    fprintf(f, "__end__\n");
    fclose(f);
    return true;
}

void cx8_cart_free(cx8_cart_t *cart)
{
    if (!cart) return;
    free(cart->source);
    free(cart->sprite_data);
    free(cart->map_data);
    memset(cart, 0, sizeof(*cart));
}
