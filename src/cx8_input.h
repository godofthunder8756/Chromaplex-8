/*
 * cx8_input.h — Chromaplex 8 Input Subsystem
 *
 * 8 buttons: Left/Right/Up/Down/A/B/X/Y
 * Supports held state (btn) and just-pressed detection (btnp).
 *
 * Gamepad/Controller support:
 *   Up to 4 controllers (matching NETLINK-1 player count).
 *   SDL GameController API with auto-mapping.
 *   Player 0 uses keyboard OR controller 0.
 *   Players 1-3 require controllers.
 */

#ifndef CX8_INPUT_H
#define CX8_INPUT_H

#include "cx8.h"
#include <SDL.h>

/* ─── Constants ────────────────────────────────────────────── */
#define CX8_MAX_CONTROLLERS  4
#define CX8_STICK_DEADZONE   8000   /* out of 32767 */

void cx8_input_init(void);
void cx8_input_shutdown(void);

/* Call at the start of each frame to latch previous state */
void cx8_input_begin_frame(void);

/* Set a button state for a player (called when processing SDL events) */
void cx8_input_set(int btn, bool pressed);
void cx8_input_set_player(int player, int btn, bool pressed);

/* Query current held state */
bool cx8_input_btn(int btn);
bool cx8_input_btn_player(int player, int btn);

/* Query just-pressed (rising edge) this frame */
bool cx8_input_btnp(int btn);
bool cx8_input_btnp_player(int player, int btn);

/* ─── Gamepad management ───────────────────────────────────── */

/* Handle SDL controller events (connect/disconnect/button/axis) */
void cx8_input_handle_controller_event(const SDL_Event *e);

/* Get number of connected controllers */
int  cx8_input_controller_count(void);

/* Check if a specific player has a controller */
bool cx8_input_has_controller(int player);

/* Get controller name (or NULL) */
const char *cx8_input_controller_name(int player);

/* Rumble / haptic feedback (if supported) */
void cx8_input_rumble(int player, float strength, int duration_ms);

#endif /* CX8_INPUT_H */
