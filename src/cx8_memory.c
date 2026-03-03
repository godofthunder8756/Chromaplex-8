/*
 * cx8_memory.c — Chromaplex 8 Memory Subsystem
 */

#include "cx8_memory.h"
#include <stdlib.h>
#include <stdio.h>

/* ─── Internal state ───────────────────────────────────────── */
static uint8_t *s_ram      = NULL;
static size_t   s_ram_size = 0;

/* ─── Public API ───────────────────────────────────────────── */

void cx8_mem_init(void)
{
    s_ram_size = CX8_BASE_RAM;
    s_ram = (uint8_t *)calloc(1, s_ram_size);
    if (!s_ram) {
        fprintf(stderr, "[CX8-MEM] FATAL: failed to allocate %zu bytes\n", s_ram_size);
        exit(1);
    }
    printf("[CX8-MEM] Initialised %zu KB RAM\n", s_ram_size / 1024);
}

void cx8_mem_expand(size_t extra_bytes)
{
    size_t new_size = s_ram_size + extra_bytes;
    uint8_t *new_ram = (uint8_t *)realloc(s_ram, new_size);
    if (!new_ram) {
        fprintf(stderr, "[CX8-MEM] WARNING: expansion to %zu KB failed\n", new_size / 1024);
        return;
    }
    /* Zero the newly added region */
    memset(new_ram + s_ram_size, 0, extra_bytes);
    s_ram      = new_ram;
    s_ram_size = new_size;
    printf("[CX8-MEM] Expanded to %zu KB RAM\n", s_ram_size / 1024);
}

uint8_t cx8_mem_peek(uint32_t addr)
{
    if (addr >= s_ram_size) return 0;
    return s_ram[addr];
}

void cx8_mem_poke(uint32_t addr, uint8_t val)
{
    if (addr >= s_ram_size) return;
    s_ram[addr] = val;
}

uint16_t cx8_mem_peek16(uint32_t addr)
{
    if (addr + 1 >= s_ram_size) return 0;
    return (uint16_t)s_ram[addr] | ((uint16_t)s_ram[addr + 1] << 8);
}

void cx8_mem_poke16(uint32_t addr, uint16_t val)
{
    if (addr + 1 >= s_ram_size) return;
    s_ram[addr]     = (uint8_t)(val & 0xFF);
    s_ram[addr + 1] = (uint8_t)(val >> 8);
}

uint32_t cx8_mem_peek32(uint32_t addr)
{
    if (addr + 3 >= s_ram_size) return 0;
    return (uint32_t)s_ram[addr]
         | ((uint32_t)s_ram[addr + 1] << 8)
         | ((uint32_t)s_ram[addr + 2] << 16)
         | ((uint32_t)s_ram[addr + 3] << 24);
}

void cx8_mem_poke32(uint32_t addr, uint32_t val)
{
    if (addr + 3 >= s_ram_size) return;
    s_ram[addr]     = (uint8_t)(val & 0xFF);
    s_ram[addr + 1] = (uint8_t)((val >> 8)  & 0xFF);
    s_ram[addr + 2] = (uint8_t)((val >> 16) & 0xFF);
    s_ram[addr + 3] = (uint8_t)((val >> 24) & 0xFF);
}

void cx8_mem_copy(uint32_t dst, uint32_t src, size_t len)
{
    if (dst + len > s_ram_size || src + len > s_ram_size) return;
    memmove(s_ram + dst, s_ram + src, len);
}

void cx8_mem_set(uint32_t dst, uint8_t val, size_t len)
{
    if (dst + len > s_ram_size) return;
    memset(s_ram + dst, val, len);
}

uint8_t *cx8_mem_raw(void)
{
    return s_ram;
}

size_t cx8_mem_size(void)
{
    return s_ram_size;
}

void cx8_mem_free(void)
{
    free(s_ram);
    s_ram      = NULL;
    s_ram_size = 0;
}
