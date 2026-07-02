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
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

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

/* ═══════════════════════════════════════════════════════════════
 *  AUTOMATIC MODULE SCANNING
 * ═══════════════════════════════════════════════════════════════ */

/* Name aliases for resolving "-- modules: ..." declarations */
static const struct { const char *alias; int id; } s_name_map[] = {
    { "turbo_ram",    CX8_MOD_TURBO_RAM   },
    { "turbo-ram",    CX8_MOD_TURBO_RAM   },
    { "turboram",     CX8_MOD_TURBO_RAM   },
    { "ram",          CX8_MOD_TURBO_RAM   },
    { "synthwave",    CX8_MOD_SYNTHWAVE16  },
    { "synthwave16",  CX8_MOD_SYNTHWAVE16  },
    { "synthwave-16", CX8_MOD_SYNTHWAVE16  },
    { "audio",        CX8_MOD_SYNTHWAVE16  },
    { "pixstretch",   CX8_MOD_PIXSTRETCH   },
    { "pixel-stretch",CX8_MOD_PIXSTRETCH   },
    { "fx",           CX8_MOD_PIXSTRETCH   },
    { "cart_doubler",  CX8_MOD_CART_DOUBLER },
    { "cart-doubler",  CX8_MOD_CART_DOUBLER },
    { "cartdoubler",   CX8_MOD_CART_DOUBLER },
    { "netlink",      CX8_MOD_NETLINK      },
    { "netlink-1",    CX8_MOD_NETLINK      },
    { "network",      CX8_MOD_NETLINK      },
    { "net",          CX8_MOD_NETLINK      },
    { NULL, -1 }
};

int cx8_module_id_from_name(const char *name)
{
    if (!name) return -1;

    /* Skip whitespace */
    while (*name && isspace((unsigned char)*name)) name++;
    if (!*name) return -1;

    /* Try numeric ID first */
    if (isdigit((unsigned char)*name)) {
        int id = atoi(name);
        if (id >= 0 && id < CX8_MOD_MAX) return id;
    }

    /* Try name aliases (case-insensitive) */
    for (int i = 0; s_name_map[i].alias; i++) {
        const char *a = s_name_map[i].alias;
        const char *n = name;
        bool match = true;
        while (*a && *n && !isspace((unsigned char)*n) && *n != ',') {
            if (tolower((unsigned char)*a) != tolower((unsigned char)*n)) {
                match = false;
                break;
            }
            a++;
            n++;
        }
        if (match && *a == '\0' && (*n == '\0' || isspace((unsigned char)*n) || *n == ','))
            return s_name_map[i].id;
    }

    return -1;
}

/* Parse "-- modules: netlink, pixstretch" from cart header */
static int scan_header_modules(const char *source)
{
    int loaded = 0;

    /* Look for "-- modules:" in the first few lines */
    const char *p = source;
    int lines_checked = 0;

    while (*p && lines_checked < 20) {
        /* Skip whitespace at line start */
        while (*p == ' ' || *p == '\t') p++;

        if (*p == '-' && *(p + 1) == '-') {
            /* Check for "-- modules:" */
            const char *line = p + 2;
            while (*line == ' ') line++;

            if (strncmp(line, "modules:", 8) == 0) {
                const char *list = line + 8;
                while (*list == ' ') list++;

                /* Parse comma-separated module names */
                while (*list && *list != '\n' && *list != '\r') {
                    /* Skip whitespace and commas */
                    while (*list == ' ' || *list == ',' || *list == '\t') list++;
                    if (!*list || *list == '\n' || *list == '\r') break;

                    /* Extract name */
                    const char *start = list;
                    while (*list && *list != ',' && *list != '\n' && *list != '\r'
                           && *list != ' ' && *list != '\t')
                        list++;

                    char name[64];
                    int len = (int)(list - start);
                    if (len > 0 && len < (int)sizeof(name)) {
                        memcpy(name, start, (size_t)len);
                        name[len] = '\0';

                        int id = cx8_module_id_from_name(name);
                        if (id >= 0 && !cx8_module_is_loaded(id)) {
                            printf("[CX8-MOD] Auto-detected module from header: %s -> %d\n",
                                   name, id);
                            cx8_module_load(id);
                            loaded++;
                        }
                    }
                }
                break;  /* Only one modules: line */
            }
        }

        /* Advance to next line */
        while (*p && *p != '\n') p++;
        if (*p == '\n') p++;
        lines_checked++;

        /* Stop scanning at first non-comment, non-blank line */
        if (*p && *p != '-' && *p != '\n' && *p != '\r' && *p != ' ' && *p != '\t')
            break;
    }

    return loaded;
}

/* Scan source for mod_load(MOD_*) calls */
static int scan_code_mod_load(const char *source)
{
    int loaded = 0;
    const char *p = source;

    /* Map of MOD_* constant names to IDs */
    static const struct { const char *constant; int id; } mod_constants[] = {
        { "MOD_TURBO_RAM",    CX8_MOD_TURBO_RAM   },
        { "MOD_SYNTHWAVE16",  CX8_MOD_SYNTHWAVE16  },
        { "MOD_PIXSTRETCH",   CX8_MOD_PIXSTRETCH   },
        { "MOD_CART_DOUBLER", CX8_MOD_CART_DOUBLER },
        { "MOD_NETLINK",      CX8_MOD_NETLINK      },
        { NULL, -1 }
    };

    while ((p = strstr(p, "mod_load(")) != NULL) {
        /* Make sure it's not inside a comment */
        const char *line_start = p;
        while (line_start > source && *(line_start - 1) != '\n') line_start--;
        bool in_comment = false;
        const char *c = line_start;
        while (c < p) {
            if (*c == '-' && *(c + 1) == '-') { in_comment = true; break; }
            c++;
        }
        if (in_comment) { p += 9; continue; }

        /* Extract the argument */
        const char *arg = p + 9;  /* skip "mod_load(" */
        while (*arg == ' ') arg++;

        /* Check for MOD_* constants */
        for (int i = 0; mod_constants[i].constant; i++) {
            int clen = (int)strlen(mod_constants[i].constant);
            if (strncmp(arg, mod_constants[i].constant, (size_t)clen) == 0) {
                int id = mod_constants[i].id;
                if (!cx8_module_is_loaded(id)) {
                    printf("[CX8-MOD] Auto-detected module from code: %s\n",
                           mod_constants[i].constant);
                    cx8_module_load(id);
                    loaded++;
                }
                break;
            }
        }

        /* Also try numeric: mod_load(0), mod_load(1), etc. */
        if (isdigit((unsigned char)*arg)) {
            int id = atoi(arg);
            if (id >= 0 && id < CX8_MOD_MAX && !cx8_module_is_loaded(id)) {
                printf("[CX8-MOD] Auto-detected module from code: mod_load(%d)\n", id);
                cx8_module_load(id);
                loaded++;
            }
        }

        p += 9;
    }

    return loaded;
}

int cx8_modules_auto_scan(const char *source)
{
    if (!source) return 0;

    int total = 0;
    printf("[CX8-MOD] Scanning cart for module requirements...\n");

    total += scan_header_modules(source);
    total += scan_code_mod_load(source);

    if (total > 0)
        printf("[CX8-MOD] Auto-loaded %d module(s)\n", total);
    else
        printf("[CX8-MOD] No module requirements detected\n");

    return total;
}
