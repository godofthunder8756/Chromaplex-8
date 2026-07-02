/*
 * cx8_persist.h — Chromaplex 8 Persistent Storage
 *
 * Provides dset/dget style save data for cartridges.
 * Each cart gets 64 numeric save slots (like PICO-8).
 * Data is stored in a .cx8save file next to the cart.
 */

#ifndef CX8_PERSIST_H
#define CX8_PERSIST_H

#include "cx8.h"

#define CX8_PERSIST_SLOTS  64

/* Initialise persistent storage for the given cart path */
void   cx8_persist_init(const char *cart_path);

/* Shutdown and flush to disk */
void   cx8_persist_shutdown(void);

/* Store a number in a slot (0-63) */
void   cx8_persist_set(int slot, double value);

/* Retrieve a number from a slot (returns 0 if unset) */
double cx8_persist_get(int slot);

/* Flush all data to disk immediately */
bool   cx8_persist_flush(void);

#endif /* CX8_PERSIST_H */
