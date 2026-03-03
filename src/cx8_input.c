/*
 * cx8_input.c — Chromaplex 8 Input Subsystem
 */

#include "cx8_input.h"

static bool s_current[CX8_BTN_COUNT];
static bool s_previous[CX8_BTN_COUNT];

void cx8_input_init(void)
{
    memset(s_current,  0, sizeof(s_current));
    memset(s_previous, 0, sizeof(s_previous));
}

void cx8_input_begin_frame(void)
{
    memcpy(s_previous, s_current, sizeof(s_current));
}

void cx8_input_set(int btn, bool pressed)
{
    if (btn >= 0 && btn < CX8_BTN_COUNT)
        s_current[btn] = pressed;
}

bool cx8_input_btn(int btn)
{
    if (btn >= 0 && btn < CX8_BTN_COUNT)
        return s_current[btn];
    return false;
}

bool cx8_input_btnp(int btn)
{
    if (btn >= 0 && btn < CX8_BTN_COUNT)
        return s_current[btn] && !s_previous[btn];
    return false;
}
