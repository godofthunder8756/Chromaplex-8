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
#include "cx8_pickup.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>

static size_t s_max_cart = CX8_CART_SIZE;

/* Forward declarations for binary format */
#define CX8_BINARY_MAGIC    "CX8B"
#define CX8_BINARY_VERSION  1
#define CX8_FLAG_COMPRESSED 0x01
static bool load_binary_cart(const char *path, cx8_cart_t *cart,
                             const uint8_t *data, size_t data_len);

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

    /* Check for binary .cx8 format (magic: "CX8B") */
    if (fsize >= 6 && memcmp(raw, CX8_BINARY_MAGIC, 4) == 0) {
        bool ok = load_binary_cart(path, cart, (const uint8_t *)raw, (size_t)fsize);
        free(raw);
        return ok;
    }

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

    /* If this is a PickUp file, transpile to Lua */
    if (cx8_pickup_is_pickup_file(path)) {
        const char *pickup_err = NULL;
        char *lua_source = cx8_pickup_to_lua(cart->source, cart->source_len, &pickup_err);
        if (lua_source) {
            free(cart->source);
            cart->source     = lua_source;
            cart->source_len = strlen(lua_source);
        } else {
            fprintf(stderr, "[CX8-CART] PickUp transpile error: %s\n",
                    pickup_err ? pickup_err : "unknown");
            free(raw);
            free(cart->source);
            cart->source = NULL;
            return false;
        }
    }

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

/* ═══════════════════════════════════════════════════════════════
 *  BINARY .CX8 FORMAT (with compression)
 * ═══════════════════════════════════════════════════════════════
 *
 *  Magic:   "CX8B" (4 bytes)
 *  Version: uint8_t (1)
 *  Flags:   uint8_t (bit 0 = compressed)
 *  Header:
 *    title_len:  uint8_t + title (UTF-8)
 *    author_len: uint8_t + author (UTF-8)
 *    desc_len:   uint8_t + desc (UTF-8)
 *  Sections:
 *    source_len:  uint32_t LE + data
 *    sprite_len:  uint32_t LE + data
 *    map_len:     uint32_t LE + data
 *
 *  Compression: Simple LZ77-style (run-length + backreferences)
 * ═══════════════════════════════════════════════════════════════ */

/* Simple RLE compression for cart data */
static size_t rle_compress(const uint8_t *src, size_t src_len,
                           uint8_t *dst, size_t dst_max)
{
    size_t si = 0, di = 0;

    while (si < src_len && di < dst_max - 2) {
        uint8_t byte = src[si];
        size_t run = 1;

        while (si + run < src_len && src[si + run] == byte && run < 255)
            run++;

        if (run >= 3) {
            /* Encode run: 0xFF <byte> <count> */
            if (di + 3 > dst_max) break;
            dst[di++] = 0xFF;
            dst[di++] = byte;
            dst[di++] = (uint8_t)run;
            si += run;
        } else {
            /* Literal byte (escape 0xFF itself) */
            if (byte == 0xFF) {
                if (di + 3 > dst_max) break;
                dst[di++] = 0xFF;
                dst[di++] = 0xFF;
                dst[di++] = 1;
                si++;
            } else {
                dst[di++] = byte;
                si++;
            }
        }
    }

    return di;
}

static size_t rle_decompress(const uint8_t *src, size_t src_len,
                             uint8_t *dst, size_t dst_max)
{
    size_t si = 0, di = 0;

    while (si < src_len && di < dst_max) {
        if (src[si] == 0xFF) {
            if (si + 2 >= src_len) break;
            uint8_t byte = src[si + 1];
            uint8_t count = src[si + 2];
            si += 3;
            for (int i = 0; i < count && di < dst_max; i++)
                dst[di++] = byte;
        } else {
            dst[di++] = src[si++];
        }
    }

    return di;
}

/* Write a little-endian uint32 */
static void write_u32(FILE *f, uint32_t v)
{
    uint8_t buf[4] = {
        (uint8_t)(v & 0xFF),
        (uint8_t)((v >> 8) & 0xFF),
        (uint8_t)((v >> 16) & 0xFF),
        (uint8_t)((v >> 24) & 0xFF)
    };
    fwrite(buf, 1, 4, f);
}

/* Read a little-endian uint32 */
static uint32_t read_u32(const uint8_t *p)
{
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8)
         | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

bool cx8_cart_save_binary(const char *path, const cx8_cart_t *cart)
{
    if (!path || !cart || !cart->source) return false;

    FILE *f = fopen(path, "wb");
    if (!f) return false;

    /* Magic + version + flags */
    fwrite(CX8_BINARY_MAGIC, 1, 4, f);
    uint8_t version = CX8_BINARY_VERSION;
    uint8_t flags = CX8_FLAG_COMPRESSED;
    fwrite(&version, 1, 1, f);
    fwrite(&flags, 1, 1, f);

    /* Header strings */
    uint8_t tlen = (uint8_t)(strlen(cart->title) > 63 ? 63 : strlen(cart->title));
    uint8_t alen = (uint8_t)(strlen(cart->author) > 63 ? 63 : strlen(cart->author));
    uint8_t dlen = (uint8_t)(strlen(cart->description) > 255 ? 255 : strlen(cart->description));
    fwrite(&tlen, 1, 1, f); fwrite(cart->title, 1, tlen, f);
    fwrite(&alen, 1, 1, f); fwrite(cart->author, 1, alen, f);
    fwrite(&dlen, 1, 1, f); fwrite(cart->description, 1, dlen, f);

    /* Compress and write source */
    size_t src_max = cart->source_len * 2 + 256;
    uint8_t *compressed = (uint8_t *)malloc(src_max);
    if (!compressed) { fclose(f); return false; }

    size_t clen = rle_compress((const uint8_t *)cart->source, cart->source_len,
                               compressed, src_max);
    write_u32(f, (uint32_t)cart->source_len);  /* uncompressed size */
    write_u32(f, (uint32_t)clen);              /* compressed size   */
    fwrite(compressed, 1, clen, f);

    /* Compress and write sprites */
    if (cart->sprite_data && cart->sprite_len > 0) {
        size_t spr_max = cart->sprite_len * 2 + 256;
        uint8_t *spr_comp = (uint8_t *)malloc(spr_max);
        size_t spr_clen = rle_compress(cart->sprite_data, cart->sprite_len,
                                       spr_comp, spr_max);
        write_u32(f, (uint32_t)cart->sprite_len);
        write_u32(f, (uint32_t)spr_clen);
        fwrite(spr_comp, 1, spr_clen, f);
        free(spr_comp);
    } else {
        write_u32(f, 0);
        write_u32(f, 0);
    }

    /* Compress and write map */
    if (cart->map_data && cart->map_len > 0) {
        size_t map_max = cart->map_len * 2 + 256;
        uint8_t *map_comp = (uint8_t *)malloc(map_max);
        size_t map_clen = rle_compress(cart->map_data, cart->map_len,
                                       map_comp, map_max);
        write_u32(f, (uint32_t)cart->map_len);
        write_u32(f, (uint32_t)map_clen);
        fwrite(map_comp, 1, map_clen, f);
        free(map_comp);
    } else {
        write_u32(f, 0);
        write_u32(f, 0);
    }

    free(compressed);
    fclose(f);

    printf("[CX8-CART] Saved binary cart: %s\n", path);
    return true;
}

/* Load a binary .cx8 cart */
static bool load_binary_cart(const char *path, cx8_cart_t *cart,
                             const uint8_t *data, size_t data_len)
{
    /* Already verified magic. Skip it. */
    size_t pos = 4;

    if (pos + 2 > data_len) return false;
    uint8_t version = data[pos++];
    uint8_t flags   = data[pos++];
    (void)version;

    bool compressed = (flags & CX8_FLAG_COMPRESSED) != 0;

    /* Read header strings */
    if (pos >= data_len) return false;
    uint8_t tlen = data[pos++];
    if (pos + tlen > data_len) return false;
    memcpy(cart->title, data + pos, tlen);
    cart->title[tlen] = '\0';
    pos += tlen;

    if (pos >= data_len) return false;
    uint8_t alen = data[pos++];
    if (pos + alen > data_len) return false;
    memcpy(cart->author, data + pos, alen);
    cart->author[alen] = '\0';
    pos += alen;

    if (pos >= data_len) return false;
    uint8_t dlen = data[pos++];
    if (pos + dlen > data_len) return false;
    memcpy(cart->description, data + pos, dlen);
    cart->description[dlen] = '\0';
    pos += dlen;

    /* Read source */
    if (pos + 8 > data_len) return false;
    uint32_t src_uncompressed = read_u32(data + pos); pos += 4;
    uint32_t src_compressed   = read_u32(data + pos); pos += 4;

    if (src_uncompressed > 0 && src_compressed > 0) {
        if (pos + src_compressed > data_len) return false;
        cart->source = (char *)malloc(src_uncompressed + 1);
        if (!cart->source) return false;

        if (compressed) {
            cart->source_len = rle_decompress(data + pos, src_compressed,
                                             (uint8_t *)cart->source, src_uncompressed);
        } else {
            memcpy(cart->source, data + pos, src_compressed);
            cart->source_len = src_compressed;
        }
        cart->source[cart->source_len] = '\0';
        pos += src_compressed;
    }

    /* Read sprites */
    if (pos + 8 > data_len) return false;
    uint32_t spr_uncompressed = read_u32(data + pos); pos += 4;
    uint32_t spr_compressed   = read_u32(data + pos); pos += 4;

    if (spr_uncompressed > 0 && spr_compressed > 0) {
        if (pos + spr_compressed > data_len) return false;
        cart->sprite_data = (uint8_t *)malloc(spr_uncompressed);
        if (compressed) {
            cart->sprite_len = rle_decompress(data + pos, spr_compressed,
                                             cart->sprite_data, spr_uncompressed);
        } else {
            memcpy(cart->sprite_data, data + pos, spr_compressed);
            cart->sprite_len = spr_compressed;
        }
        pos += spr_compressed;
    }

    /* Read map */
    if (pos + 8 > data_len) return false;
    uint32_t map_uncompressed = read_u32(data + pos); pos += 4;
    uint32_t map_compressed   = read_u32(data + pos); pos += 4;

    if (map_uncompressed > 0 && map_compressed > 0) {
        if (pos + map_compressed > data_len) return false;
        cart->map_data = (uint8_t *)malloc(map_uncompressed);
        if (compressed) {
            cart->map_len = rle_decompress(data + pos, map_compressed,
                                          cart->map_data, map_uncompressed);
        } else {
            memcpy(cart->map_data, data + pos, map_compressed);
            cart->map_len = map_compressed;
        }
    }

    strncpy(cart->filename, path, sizeof(cart->filename) - 1);

    if (cart->title[0])
        printf("[CX8-CART] Loaded binary: \"%s\" by %s\n", cart->title,
               cart->author[0] ? cart->author : "unknown");

    return true;
}
