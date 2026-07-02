/*
 * cx8_scripting.h — Chromaplex 8 Lua Scripting Bridge
 *
 * Exposes the full CX8 API to Lua cartridge code:
 *   Drawing, sprites, map, camera, input, audio, memory, modules.
 */

#ifndef CX8_SCRIPTING_H
#define CX8_SCRIPTING_H

#include "cx8.h"
#include <lua.h>

/* Create and initialise the Lua VM, register all CX8 API functions */
lua_State *cx8_script_init(void);

/* Load and execute a Lua source string */
bool cx8_script_load(lua_State *L, const char *source, const char *name);

/* Call the cartridge callbacks (safe — does nothing if not defined) */
void cx8_script_call_init(lua_State *L);
void cx8_script_call_update(lua_State *L);
void cx8_script_call_draw(lua_State *L);

/* Check if the VM is in an error state; get the error message */
bool        cx8_script_has_error(void);
const char *cx8_script_get_error(void);
void        cx8_script_clear_error(void);

/* Shutdown */
void cx8_script_shutdown(lua_State *L);

#endif /* CX8_SCRIPTING_H */
