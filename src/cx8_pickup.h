/*
 * cx8_pickup.h — Chromaplex 8 PickUp Language Support
 *
 * PickUp (github.com/godofthunder8756/Pickup-Lang) is a lightweight
 * scripting language inspired by Lua with modern conveniences:
 *   - 0-based array indexing
 *   - [...] array literals
 *   - try/catch/throw error handling
 *   - continue in loops
 *   - import "module" syntax
 *
 * This module transpiles PickUp source to Lua so it can run on
 * the Chromaplex 8 Lua VM without requiring the Rust runtime.
 */

#ifndef CX8_PICKUP_H
#define CX8_PICKUP_H

#include "cx8.h"

/*
 * Transpile PickUp source to Lua source.
 * Returns a newly allocated string (caller must free).
 * Returns NULL on error (sets *err_msg if provided).
 */
char *cx8_pickup_to_lua(const char *source, size_t source_len, const char **err_msg);

/* Check if a filename has a PickUp extension (.up, .pickup) */
bool cx8_pickup_is_pickup_file(const char *filename);

#endif /* CX8_PICKUP_H */
