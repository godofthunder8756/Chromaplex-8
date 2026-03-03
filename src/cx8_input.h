/*
 * cx8_input.h — Chromaplex 8 Input Subsystem
 *
 * 8 buttons: Left/Right/Up/Down/A/B/X/Y
 * Supports held state (btn) and just-pressed detection (btnp).
 */

#ifndef CX8_INPUT_H
#define CX8_INPUT_H

#include "cx8.h"

void cx8_input_init(void);

/* Call at the start of each frame to latch previous state */
void cx8_input_begin_frame(void);

/* Set a button state (called when processing SDL events) */
void cx8_input_set(int btn, bool pressed);

/* Query current held state */
bool cx8_input_btn(int btn);

/* Query just-pressed (rising edge) this frame */
bool cx8_input_btnp(int btn);

#endif /* CX8_INPUT_H */
