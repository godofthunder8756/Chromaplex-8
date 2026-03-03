/*
 * cx8_memory.h — Chromaplex 8 Memory Subsystem
 *
 * Provides a flat, addressable memory space with peek/poke access.
 * Base: 128 KB  ·  Expandable to 256 KB via TURBO-RAM 8X module.
 */

#ifndef CX8_MEMORY_H
#define CX8_MEMORY_H

#include "cx8.h"

/* Initialise memory (zeroes all RAM) */
void     cx8_mem_init(void);

/* Expand RAM (called when TURBO-RAM 8X module is loaded) */
void     cx8_mem_expand(size_t extra_bytes);

/* Read / write a single byte */
uint8_t  cx8_mem_peek(uint32_t addr);
void     cx8_mem_poke(uint32_t addr, uint8_t val);

/* Read / write a 16-bit value (little-endian) */
uint16_t cx8_mem_peek16(uint32_t addr);
void     cx8_mem_poke16(uint32_t addr, uint16_t val);

/* Read / write a 32-bit value (little-endian) */
uint32_t cx8_mem_peek32(uint32_t addr);
void     cx8_mem_poke32(uint32_t addr, uint32_t val);

/* Bulk operations */
void     cx8_mem_copy(uint32_t dst, uint32_t src, size_t len);
void     cx8_mem_set(uint32_t dst, uint8_t val, size_t len);

/* Direct pointer to the raw buffer (for subsystem internals) */
uint8_t *cx8_mem_raw(void);
size_t   cx8_mem_size(void);

/* Cleanup */
void     cx8_mem_free(void);

#endif /* CX8_MEMORY_H */
