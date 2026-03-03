/*
 * cx8_cart.h — Chromaplex 8 Cartridge System
 *
 * Cartridge format: .cx8 files containing Lua source with optional
 * embedded binary sections for sprites and map data.
 * Max cartridge size: 64 KB (128 KB with CART-DOUBLER module).
 */

#ifndef CX8_CART_H
#define CX8_CART_H

#include "cx8.h"

/* Cartridge info */
typedef struct {
    char     title[64];
    char     author[64];
    char     description[256];
    char     filename[512];
    char    *source;          /* Lua source code              */
    size_t   source_len;
    uint8_t *sprite_data;     /* embedded sprite sheet (opt)  */
    size_t   sprite_len;
    uint8_t *map_data;        /* embedded map data (opt)      */
    size_t   map_len;
    size_t   max_size;        /* 64KB default, 128KB expanded */
} cx8_cart_t;

/* Initialise the cartridge system */
void cx8_cart_init(void);
void cx8_cart_shutdown(void);

/* Expand max cart size (CART-DOUBLER module) */
void cx8_cart_expand(size_t extra_bytes);

/* Load a .cx8 or .lua cartridge from disk */
bool cx8_cart_load(const char *path, cx8_cart_t *cart);

/* Save a cartridge back to disk */
bool cx8_cart_save(const char *path, const cx8_cart_t *cart);

/* Free cartridge data */
void cx8_cart_free(cx8_cart_t *cart);

#endif /* CX8_CART_H */
