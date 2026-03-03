/*
 * cx8_modules.h — Chromaplex 8 Expansion Module System
 *
 * "Plug-in" virtual hardware mods that extend the base console.
 * Each module has its own manufacturer, aesthetic, and personality.
 *
 * ┌──────────────────────────────────────────────────────────┐
 * │  TURBO-RAM 8X      NovaByte Industries                  │
 * │  "When 128K just isn't enough."                         │
 * │  +128 KB RAM  ·  Industrial grey, stamped metal casing  │
 * ├──────────────────────────────────────────────────────────┤
 * │  SYNTHWAVE-16      AudioLux Labs                        │
 * │  "Sixteen channels of pure neon thunder."               │
 * │  +12 sound channels  ·  Chrome & purple, 80s CRT glow  │
 * ├──────────────────────────────────────────────────────────┤
 * │  PIXEL-STRETCH PRO  VisualFX Co.                        │
 * │  "Every pixel tells a story."                           │
 * │  Palette cycling, dithering modes, per-pixel shading    │
 * │  Holographic rainbow shell                              │
 * ├──────────────────────────────────────────────────────────┤
 * │  CART-DOUBLER       MegaMedia                           │
 * │  "More game, more glory."                               │
 * │  +64 KB cartridge storage  ·  Chunky red adapter        │
 * ├──────────────────────────────────────────────────────────┤
 * │  NETLINK-1          CyberConnect                        │
 * │  "The world is your lobby."                             │
 * │  Multiplayer networking  ·  Antenna + blinking LEDs     │
 * └──────────────────────────────────────────────────────────┘
 */

#ifndef CX8_MODULES_H
#define CX8_MODULES_H

#include "cx8.h"

/* Initialise the module registry */
void cx8_modules_init(void);

/* Get the descriptor for a module by ID */
const cx8_module_t *cx8_module_get(int id);

/* Load/unload a module — triggers hardware expansion */
bool cx8_module_load(int id);
void cx8_module_unload(int id);

/* Check if a module is loaded */
bool cx8_module_is_loaded(int id);

/* Get number of loaded modules */
int  cx8_module_count_loaded(void);

/* Iterate all available modules (returns count) */
int  cx8_module_list(const cx8_module_t **out, int max);

#endif /* CX8_MODULES_H */
