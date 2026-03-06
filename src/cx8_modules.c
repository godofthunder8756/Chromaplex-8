/*
 * cx8_modules.c — Chromaplex 8 Expansion Module System
 *
 * When a module is loaded, it modifies the virtual hardware:
 *   TURBO-RAM 8X    → expands RAM by 128 KB
 *   SYNTHWAVE-16    → sets audio channels to 16
 *   PIXEL-STRETCH   → (enables extended GPU features — future)
 *   CART-DOUBLER    → expands cartridge limit by 64 KB
 *   NETLINK-1       → (enables networking — future)
 */

#include "cx8_modules.h"
#include "cx8_memory.h"
#include "cx8_apu.h"
#include "cx8_cart.h"
#include "cx8_netlink.h"
#include "cx8_pixstretch.h"
#include <stdio.h>

/* ─── Module registry ──────────────────────────────────────── */

static cx8_module_t s_modules[CX8_MOD_MAX];

void cx8_modules_init(void)
{
    memset(s_modules, 0, sizeof(s_modules));

    /* ─── TURBO-RAM 8X ─────────────────────────────────────── */
    s_modules[CX8_MOD_TURBO_RAM] = (cx8_module_t){
        .id           = CX8_MOD_TURBO_RAM,
        .name         = "TURBO-RAM 8X",
        .manufacturer = "NovaByte Industries",
        .description  = "+128 KB RAM expansion module",
        .flavor       = "When 128K just isn't enough.",
        .loaded       = false,
    };

    /* ─── SYNTHWAVE-16 ─────────────────────────────────────── */
    s_modules[CX8_MOD_SYNTHWAVE16] = (cx8_module_t){
        .id           = CX8_MOD_SYNTHWAVE16,
        .name         = "SYNTHWAVE-16",
        .manufacturer = "AudioLux Labs",
        .description  = "+12 sound channels (4 base + 12 = 16 total)",
        .flavor       = "Sixteen channels of pure neon thunder.",
        .loaded       = false,
    };

    /* ─── PIXEL-STRETCH PRO ────────────────────────────────── */
    s_modules[CX8_MOD_PIXSTRETCH] = (cx8_module_t){
        .id           = CX8_MOD_PIXSTRETCH,
        .name         = "PIXEL-STRETCH PRO",
        .manufacturer = "VisualFX Co.",
        .description  = "Palette cycling, dithering modes, advanced FX",
        .flavor       = "Every pixel tells a story.",
        .loaded       = false,
    };

    /* ─── CART-DOUBLER ─────────────────────────────────────── */
    s_modules[CX8_MOD_CART_DOUBLER] = (cx8_module_t){
        .id           = CX8_MOD_CART_DOUBLER,
        .name         = "CART-DOUBLER",
        .manufacturer = "MegaMedia",
        .description  = "+64 KB cartridge storage",
        .flavor       = "More game, more glory.",
        .loaded       = false,
    };

    /* ─── NETLINK-1 ────────────────────────────────────────── */
    s_modules[CX8_MOD_NETLINK] = (cx8_module_t){
        .id           = CX8_MOD_NETLINK,
        .name         = "NETLINK-1",
        .manufacturer = "CyberConnect",
        .description  = "Multiplayer networking module",
        .flavor       = "The world is your lobby.",
        .loaded       = false,
    };

    printf("[CX8-MOD] Module bay initialised (%d slots)\n", CX8_MOD_MAX);
}

const cx8_module_t *cx8_module_get(int id)
{
    if (id >= 0 && id < CX8_MOD_MAX && s_modules[id].name)
        return &s_modules[id];
    return NULL;
}

bool cx8_module_load(int id)
{
    if (id < 0 || id >= CX8_MOD_MAX || !s_modules[id].name) return false;
    if (s_modules[id].loaded) return true; /* already loaded */

    cx8_module_t *mod = &s_modules[id];

    printf("[CX8-MOD] ╔══════════════════════════════════════╗\n");
    printf("[CX8-MOD] ║  INSERTING: %-24s ║\n", mod->name);
    printf("[CX8-MOD] ║  By: %-31s ║\n", mod->manufacturer);
    printf("[CX8-MOD] ║  \"%s\"%*s║\n",
           mod->flavor,
           (int)(36 - strlen(mod->flavor)), " ");
    printf("[CX8-MOD] ╚══════════════════════════════════════╝\n");

    /* Apply hardware modifications */
    switch (id) {
    case CX8_MOD_TURBO_RAM:
        cx8_mem_expand(128 * 1024);
        break;

    case CX8_MOD_SYNTHWAVE16:
        cx8_apu_set_channels(16);
        break;

    case CX8_MOD_PIXSTRETCH:
        cx8_fx_enable();
        printf("[CX8-MOD] PIXEL-STRETCH PRO: Advanced FX enabled\n");
        break;

    case CX8_MOD_CART_DOUBLER:
        cx8_cart_expand(64 * 1024);
        break;

    case CX8_MOD_NETLINK:
        printf("[CX8-MOD] NETLINK-1: Networking active\n");
        break;
    }

    mod->loaded = true;
    return true;
}

void cx8_module_unload(int id)
{
    if (id >= 0 && id < CX8_MOD_MAX)
        s_modules[id].loaded = false;
}

bool cx8_module_is_loaded(int id)
{
    if (id >= 0 && id < CX8_MOD_MAX)
        return s_modules[id].loaded;
    return false;
}

int cx8_module_count_loaded(void)
{
    int count = 0;
    for (int i = 0; i < CX8_MOD_MAX; i++)
        if (s_modules[i].loaded) count++;
    return count;
}

int cx8_module_list(const cx8_module_t **out, int max)
{
    int count = 0;
    for (int i = 0; i < CX8_MOD_MAX && count < max; i++) {
        if (s_modules[i].name) {
            out[count++] = &s_modules[i];
        }
    }
    return count;
}
