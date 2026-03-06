/*
 * cx8_input.c — Chromaplex 8 Input Subsystem
 *
 * Keyboard + SDL GameController support for up to 4 players.
 * Player 0 can use keyboard OR controller.
 * Players 1-3 require controllers.
 */

#include "cx8_input.h"
#include <stdio.h>

/* ─── Per-player state ─────────────────────────────────────── */
typedef struct {
    bool current[CX8_BTN_COUNT];
    bool previous[CX8_BTN_COUNT];
} cx8_player_input_t;

static cx8_player_input_t s_players[CX8_MAX_CONTROLLERS];

/* For backwards compatibility, player 0 aliases */
#define s_current   s_players[0].current
#define s_previous  s_players[0].previous

/* ─── Controller tracking ─────────────────────────────────── */
typedef struct {
    SDL_GameController *controller;
    SDL_JoystickID      joystick_id;
    bool                connected;
    const char         *name;
} cx8_controller_t;

static cx8_controller_t s_controllers[CX8_MAX_CONTROLLERS];
static int s_controller_count = 0;

/* ─── Init / Shutdown ──────────────────────────────────────── */

void cx8_input_init(void)
{
    memset(s_players, 0, sizeof(s_players));
    memset(s_controllers, 0, sizeof(s_controllers));
    s_controller_count = 0;

    /* Open any controllers already connected at startup */
    int num_joysticks = SDL_NumJoysticks();
    for (int i = 0; i < num_joysticks && s_controller_count < CX8_MAX_CONTROLLERS; i++) {
        if (SDL_IsGameController(i)) {
            SDL_GameController *gc = SDL_GameControllerOpen(i);
            if (gc) {
                int slot = s_controller_count++;
                s_controllers[slot].controller = gc;
                s_controllers[slot].joystick_id = SDL_JoystickInstanceID(
                    SDL_GameControllerGetJoystick(gc));
                s_controllers[slot].connected = true;
                s_controllers[slot].name = SDL_GameControllerName(gc);
                printf("[CX8-INPUT] Controller %d: %s\n",
                       slot, s_controllers[slot].name ? s_controllers[slot].name : "Unknown");
            }
        }
    }

    if (s_controller_count > 0)
        printf("[CX8-INPUT] %d controller(s) ready\n", s_controller_count);
}

void cx8_input_shutdown(void)
{
    for (int i = 0; i < CX8_MAX_CONTROLLERS; i++) {
        if (s_controllers[i].controller) {
            SDL_GameControllerClose(s_controllers[i].controller);
            s_controllers[i].controller = NULL;
        }
    }
    s_controller_count = 0;
}

/* ─── Frame management ─────────────────────────────────────── */

void cx8_input_begin_frame(void)
{
    for (int p = 0; p < CX8_MAX_CONTROLLERS; p++)
        memcpy(s_players[p].previous, s_players[p].current,
               sizeof(s_players[p].current));
}

/* ─── Button state (player 0 / backwards compatible) ──────── */

void cx8_input_set(int btn, bool pressed)
{
    cx8_input_set_player(0, btn, pressed);
}

void cx8_input_set_player(int player, int btn, bool pressed)
{
    if (player >= 0 && player < CX8_MAX_CONTROLLERS &&
        btn >= 0 && btn < CX8_BTN_COUNT)
        s_players[player].current[btn] = pressed;
}

bool cx8_input_btn(int btn)
{
    return cx8_input_btn_player(0, btn);
}

bool cx8_input_btn_player(int player, int btn)
{
    if (player >= 0 && player < CX8_MAX_CONTROLLERS &&
        btn >= 0 && btn < CX8_BTN_COUNT)
        return s_players[player].current[btn];
    return false;
}

bool cx8_input_btnp(int btn)
{
    return cx8_input_btnp_player(0, btn);
}

bool cx8_input_btnp_player(int player, int btn)
{
    if (player >= 0 && player < CX8_MAX_CONTROLLERS &&
        btn >= 0 && btn < CX8_BTN_COUNT)
        return s_players[player].current[btn] && !s_players[player].previous[btn];
    return false;
}

/* ─── Controller slot lookup ───────────────────────────────── */

static int find_controller_slot(SDL_JoystickID id)
{
    for (int i = 0; i < CX8_MAX_CONTROLLERS; i++) {
        if (s_controllers[i].connected && s_controllers[i].joystick_id == id)
            return i;
    }
    return -1;
}

static int find_free_slot(void)
{
    for (int i = 0; i < CX8_MAX_CONTROLLERS; i++) {
        if (!s_controllers[i].connected) return i;
    }
    return -1;
}

/* ─── Handle SDL Controller events ─────────────────────────── */

void cx8_input_handle_controller_event(const SDL_Event *e)
{
    if (!e) return;

    switch (e->type) {

    case SDL_CONTROLLERDEVICEADDED: {
        int joy_idx = e->cdevice.which;
        if (!SDL_IsGameController(joy_idx)) break;

        int slot = find_free_slot();
        if (slot < 0) {
            printf("[CX8-INPUT] No free controller slot\n");
            break;
        }

        SDL_GameController *gc = SDL_GameControllerOpen(joy_idx);
        if (!gc) break;

        s_controllers[slot].controller  = gc;
        s_controllers[slot].joystick_id = SDL_JoystickInstanceID(
            SDL_GameControllerGetJoystick(gc));
        s_controllers[slot].connected   = true;
        s_controllers[slot].name        = SDL_GameControllerName(gc);
        s_controller_count++;

        printf("[CX8-INPUT] Controller %d connected: %s\n",
               slot, s_controllers[slot].name ? s_controllers[slot].name : "Unknown");
        break;
    }

    case SDL_CONTROLLERDEVICEREMOVED: {
        SDL_JoystickID id = e->cdevice.which;
        int slot = find_controller_slot(id);
        if (slot < 0) break;

        printf("[CX8-INPUT] Controller %d disconnected: %s\n",
               slot, s_controllers[slot].name ? s_controllers[slot].name : "Unknown");

        SDL_GameControllerClose(s_controllers[slot].controller);
        s_controllers[slot].controller = NULL;
        s_controllers[slot].connected  = false;
        s_controllers[slot].name       = NULL;
        s_controller_count--;

        /* Clear that player's inputs */
        memset(s_players[slot].current, 0, sizeof(s_players[slot].current));
        break;
    }

    case SDL_CONTROLLERBUTTONDOWN:
    case SDL_CONTROLLERBUTTONUP: {
        SDL_JoystickID id = e->cbutton.which;
        int slot = find_controller_slot(id);
        if (slot < 0) break;

        bool pressed = (e->type == SDL_CONTROLLERBUTTONDOWN);

        switch (e->cbutton.button) {
        case SDL_CONTROLLER_BUTTON_DPAD_LEFT:
            cx8_input_set_player(slot, CX8_BTN_LEFT, pressed);
            break;
        case SDL_CONTROLLER_BUTTON_DPAD_RIGHT:
            cx8_input_set_player(slot, CX8_BTN_RIGHT, pressed);
            break;
        case SDL_CONTROLLER_BUTTON_DPAD_UP:
            cx8_input_set_player(slot, CX8_BTN_UP, pressed);
            break;
        case SDL_CONTROLLER_BUTTON_DPAD_DOWN:
            cx8_input_set_player(slot, CX8_BTN_DOWN, pressed);
            break;
        case SDL_CONTROLLER_BUTTON_A:
            cx8_input_set_player(slot, CX8_BTN_A, pressed);
            break;
        case SDL_CONTROLLER_BUTTON_B:
            cx8_input_set_player(slot, CX8_BTN_B, pressed);
            break;
        case SDL_CONTROLLER_BUTTON_X:
            cx8_input_set_player(slot, CX8_BTN_X, pressed);
            break;
        case SDL_CONTROLLER_BUTTON_Y:
            cx8_input_set_player(slot, CX8_BTN_Y, pressed);
            break;
        default:
            break;
        }
        break;
    }

    case SDL_CONTROLLERAXISMOTION: {
        SDL_JoystickID id = e->caxis.which;
        int slot = find_controller_slot(id);
        if (slot < 0) break;

        int value = e->caxis.value;

        switch (e->caxis.axis) {
        case SDL_CONTROLLER_AXIS_LEFTX:
            cx8_input_set_player(slot, CX8_BTN_LEFT,  value < -CX8_STICK_DEADZONE);
            cx8_input_set_player(slot, CX8_BTN_RIGHT, value >  CX8_STICK_DEADZONE);
            break;
        case SDL_CONTROLLER_AXIS_LEFTY:
            cx8_input_set_player(slot, CX8_BTN_UP,   value < -CX8_STICK_DEADZONE);
            cx8_input_set_player(slot, CX8_BTN_DOWN, value >  CX8_STICK_DEADZONE);
            break;
        default:
            break;
        }
        break;
    }

    default:
        break;
    }
}

/* ─── Queries ──────────────────────────────────────────────── */

int cx8_input_controller_count(void)
{
    return s_controller_count;
}

bool cx8_input_has_controller(int player)
{
    if (player >= 0 && player < CX8_MAX_CONTROLLERS)
        return s_controllers[player].connected;
    return false;
}

const char *cx8_input_controller_name(int player)
{
    if (player >= 0 && player < CX8_MAX_CONTROLLERS && s_controllers[player].connected)
        return s_controllers[player].name;
    return NULL;
}

void cx8_input_rumble(int player, float strength, int duration_ms)
{
    if (player < 0 || player >= CX8_MAX_CONTROLLERS) return;
    if (!s_controllers[player].connected || !s_controllers[player].controller) return;

    uint16_t low  = (uint16_t)(strength * 65535.0f);
    uint16_t high = (uint16_t)(strength * 65535.0f);
    SDL_GameControllerRumble(s_controllers[player].controller,
                            low, high, (uint32_t)duration_ms);
}
