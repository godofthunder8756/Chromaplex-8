/*
 * cx8_hub.h — Chromaplex 8 Community Cart Hub
 *
 * Simple HTTP-based cartridge sharing:
 *   - Browse public carts from a community server
 *   - Download carts to local carts/ directory
 *   - Upload/share carts (with user consent)
 *
 * The hub uses a simple REST API:
 *   GET  /api/carts         → list carts (JSON)
 *   GET  /api/carts/:id     → download cart
 *   POST /api/carts         → upload cart
 *
 * Default hub URL: https://hub.chromaplex8.io (configurable)
 * Falls back gracefully when offline.
 */

#ifndef CX8_HUB_H
#define CX8_HUB_H

#include "cx8.h"

/* Maximum carts per page listing */
#define CX8_HUB_PAGE_SIZE   20

/* Cart listing entry */
typedef struct {
    char    id[64];
    char    title[64];
    char    author[64];
    char    description[256];
    int     downloads;
    int     likes;
    size_t  size;
} cx8_hub_entry_t;

/* Hub state */
typedef enum {
    CX8_HUB_IDLE = 0,
    CX8_HUB_FETCHING,
    CX8_HUB_READY,
    CX8_HUB_ERROR,
    CX8_HUB_OFFLINE
} cx8_hub_state_t;

/* Initialise the hub (configure URL, check connectivity) */
void cx8_hub_init(const char *carts_dir);
void cx8_hub_shutdown(void);

/* Set custom hub URL (default: https://hub.chromaplex8.io) */
void cx8_hub_set_url(const char *url);

/* Get current hub state */
cx8_hub_state_t cx8_hub_get_state(void);

/* Request cart listing (async-style: call update until READY) */
void cx8_hub_fetch_list(int page);

/* Get number of entries after fetch completes */
int  cx8_hub_entry_count(void);

/* Get entry by index (0-based) */
const cx8_hub_entry_t *cx8_hub_entry(int index);

/* Download a cart by ID to local carts/ directory.
 * Returns path to downloaded file, or NULL on failure. */
const char *cx8_hub_download(const char *cart_id);

/* Upload a local cart to the hub. Returns true on success. */
bool cx8_hub_upload(const char *path);

/* Get last error message */
const char *cx8_hub_error(void);

#endif /* CX8_HUB_H */
