/* The pad as the player holds it: keyboard and SDL gamepads, with hot-plug.

   Keyboard (port 0, by key position, whatever the layout):
     S accelerate (Cross)      Space fire (Circle)       Left Alt drop weapon (Square)
     A / D left / right air brake (L2 / R2)    Q / E (L1 / R1)
     arrow keys steer and pitch (directional pad)
     Enter start and confirm (Start + Cross)   P pause (Start)
     Escape back (Triangle)    V camera view (Triangle)   Backspace Select
   Gamepads use the PlayStation layout on the standard gamepad buttons: south/east/west/north
   are Cross/Circle/Square/Triangle, the shoulders L1/R1, the triggers L2/R2, the left stick
   and the d-pad the directional pad. The first pad plugged in takes port 0 (joining the
   keyboard), the second port 1; unplugging frees the port.

   With the NeGcon type selected (WIPEOUT_PAD_TYPE=negcon) the left stick, or the left and
   right arrow keys, twist the controller and the triggers are the analogue buttons. */
#include <SDL3/SDL.h>

#include <string.h>

#include "internal.h"
#include "pad.h"

#define PORTS 2

typedef struct KeyBinding {
    SDL_Scancode key;
    uint16_t buttons;
} KeyBinding;

static const KeyBinding keyBindings[] = {
    {SDL_SCANCODE_S, PAD_CROSS},       {SDL_SCANCODE_SPACE, PAD_CIRCLE},
    {SDL_SCANCODE_LALT, PAD_SQUARE},   {SDL_SCANCODE_A, PAD_L2},
    {SDL_SCANCODE_D, PAD_R2},          {SDL_SCANCODE_Q, PAD_L1},
    {SDL_SCANCODE_E, PAD_R1},          {SDL_SCANCODE_UP, PAD_UP},
    {SDL_SCANCODE_DOWN, PAD_DOWN},     {SDL_SCANCODE_LEFT, PAD_LEFT},
    {SDL_SCANCODE_RIGHT, PAD_RIGHT},   {SDL_SCANCODE_RETURN, PAD_START | PAD_CROSS},
    {SDL_SCANCODE_KP_ENTER, PAD_START | PAD_CROSS},
    {SDL_SCANCODE_P, PAD_START},       {SDL_SCANCODE_ESCAPE, PAD_TRIANGLE},
    {SDL_SCANCODE_V, PAD_TRIANGLE},    {SDL_SCANCODE_BACKSPACE, PAD_SELECT}};

#define KEY_COUNT (sizeof(keyBindings) / sizeof(keyBindings[0]))

static bool keyDown[KEY_COUNT];
static SDL_Gamepad *gamepads[PORTS];

static void update_keyboard(void)
{
    PadState st;
    size_t i;

    memset(&st, 0, sizeof(st));
    st.twist = 0x80;
    for (i = 0; i < KEY_COUNT; i++) {
        if (keyDown[i]) {
            st.buttons |= keyBindings[i].buttons;
        }
    }
    if (PsxPadGetType(0) == PAD_NEGCON) {
        /* NeGcon: S is button I (thrust), Space button II (fire), A the left shoulder (analogue
           L), D the right shoulder (R), the arrows twist, Alt button B, V button A */
        uint16_t held = st.buttons;

        st.buttons &= (uint16_t)~(PAD_CROSS | PAD_CIRCLE | PAD_SQUARE | PAD_L2 | PAD_R2 | PAD_LEFT | PAD_RIGHT);
        st.analogI = (held & PAD_CROSS) ? 0xFF : 0;
        st.analogII = (held & PAD_CIRCLE) ? 0xFF : 0;
        st.analogL = (held & PAD_L2) ? 0xFF : 0;
        if (held & PAD_SQUARE) st.buttons |= PAD_CIRCLE; /* button B */
        if (held & PAD_R2) st.buttons |= PAD_R1;         /* the digital right shoulder */
        st.twist = (held & PAD_LEFT) ? ((held & PAD_RIGHT) ? 0x80 : 0x00) : (held & PAD_RIGHT) ? 0xFF : 0x80;
    }
    PsxPadSetSource(0, PAD_SRC_KEYBOARD, &st, true);
}

static void update_gamepad(int port)
{
    SDL_Gamepad *g = gamepads[port];
    PadState st;
    int x, y, lt, rt;

    memset(&st, 0, sizeof(st));
    st.twist = 0x80;
    if (!g) {
        PsxPadSetSource(port, PAD_SRC_GAMEPAD, &st, false);
        return;
    }
#define BUTTON(sdl, bit) if (SDL_GetGamepadButton(g, sdl)) st.buttons |= (bit)
    BUTTON(SDL_GAMEPAD_BUTTON_SOUTH, PAD_CROSS);
    BUTTON(SDL_GAMEPAD_BUTTON_EAST, PAD_CIRCLE);
    BUTTON(SDL_GAMEPAD_BUTTON_WEST, PAD_SQUARE);
    BUTTON(SDL_GAMEPAD_BUTTON_NORTH, PAD_TRIANGLE);
    BUTTON(SDL_GAMEPAD_BUTTON_LEFT_SHOULDER, PAD_L1);
    BUTTON(SDL_GAMEPAD_BUTTON_RIGHT_SHOULDER, PAD_R1);
    BUTTON(SDL_GAMEPAD_BUTTON_START, PAD_START);
    BUTTON(SDL_GAMEPAD_BUTTON_BACK, PAD_SELECT);
    BUTTON(SDL_GAMEPAD_BUTTON_LEFT_STICK, PAD_L3);
    BUTTON(SDL_GAMEPAD_BUTTON_RIGHT_STICK, PAD_R3);
    BUTTON(SDL_GAMEPAD_BUTTON_DPAD_UP, PAD_UP);
    BUTTON(SDL_GAMEPAD_BUTTON_DPAD_DOWN, PAD_DOWN);
    BUTTON(SDL_GAMEPAD_BUTTON_DPAD_LEFT, PAD_LEFT);
    BUTTON(SDL_GAMEPAD_BUTTON_DPAD_RIGHT, PAD_RIGHT);
#undef BUTTON
    x = SDL_GetGamepadAxis(g, SDL_GAMEPAD_AXIS_LEFTX);
    y = SDL_GetGamepadAxis(g, SDL_GAMEPAD_AXIS_LEFTY);
    lt = SDL_GetGamepadAxis(g, SDL_GAMEPAD_AXIS_LEFT_TRIGGER);
    rt = SDL_GetGamepadAxis(g, SDL_GAMEPAD_AXIS_RIGHT_TRIGGER);
    if (lt > 8192) st.buttons |= PAD_L2;
    if (rt > 8192) st.buttons |= PAD_R2;
    if (PsxPadGetType(port) == PAD_NEGCON) {
        /* the stick twists; the right trigger and south button are button I, west button II,
           the left trigger the analogue left shoulder, east button B, north button A */
        uint16_t held = st.buttons;

        st.buttons &= (uint16_t)~(PAD_CROSS | PAD_SQUARE | PAD_L2 | PAD_R2 | PAD_LEFT | PAD_RIGHT);
        st.twist = (uint8_t)((x + 32768) >> 8);
        if (st.twist > 0x70 && st.twist < 0x90) st.twist = 0x80; /* dead zone */
        st.analogI = (held & PAD_CROSS) ? 0xFF : (uint8_t)(rt >> 7);
        st.analogII = (held & PAD_SQUARE) ? 0xFF : 0;
        st.analogL = (uint8_t)(lt >> 7);
        if (held & PAD_R2) st.buttons |= PAD_R1;
        if (held & PAD_LEFT) st.twist = 0x00;
        if (held & PAD_RIGHT) st.twist = 0xFF;
    } else {
        if (x < -16384) st.buttons |= PAD_LEFT;
        if (x > 16384) st.buttons |= PAD_RIGHT;
        if (y < -16384) st.buttons |= PAD_UP;
        if (y > 16384) st.buttons |= PAD_DOWN;
    }
    PsxPadSetSource(port, PAD_SRC_GAMEPAD, &st, true);
}

static void open_gamepad(SDL_JoystickID id)
{
    int port;

    for (port = 0; port < PORTS; port++) {
        if (gamepads[port] && SDL_GetGamepadID(gamepads[port]) == id) {
            return;
        }
    }
    for (port = 0; port < PORTS; port++) {
        if (!gamepads[port]) {
            gamepads[port] = SDL_OpenGamepad(id);
            if (gamepads[port]) {
                PortLog("pad: %s on port %d\n", SDL_GetGamepadName(gamepads[port]), port + 1);
                update_gamepad(port);
            }
            return;
        }
    }
}

static void close_gamepad(SDL_JoystickID id)
{
    int port;

    for (port = 0; port < PORTS; port++) {
        if (gamepads[port] && SDL_GetGamepadID(gamepads[port]) == id) {
            SDL_CloseGamepad(gamepads[port]);
            gamepads[port] = NULL;
            update_gamepad(port);
            PortLog("pad: port %d unplugged\n", port + 1);
        }
    }
}

void PsxPadSdlInit(void)
{
    SDL_JoystickID *ids;
    int count = 0, i;

    if (SDL_getenv("WIPEOUT_NO_GAMEPAD")) {
        return; /* tests: input comes from scripts only */
    }
    SDL_SetHint(SDL_HINT_JOYSTICK_ALLOW_BACKGROUND_EVENTS, "1");
    if (!SDL_InitSubSystem(SDL_INIT_GAMEPAD)) {
        PortLog("pad: gamepads unavailable (%s)\n", SDL_GetError());
        return;
    }
    ids = SDL_GetGamepads(&count);
    for (i = 0; ids && i < count; i++) {
        open_gamepad(ids[i]);
    }
    SDL_free(ids);
}

void PsxPadSdlShutdown(void)
{
    int port;
    bool opened = false;

    for (port = 0; port < PORTS; port++) {
        if (gamepads[port]) {
            SDL_CloseGamepad(gamepads[port]);
            gamepads[port] = NULL;
        }
    }
    opened = !SDL_getenv("WIPEOUT_NO_GAMEPAD");
    if (opened) {
        SDL_QuitSubSystem(SDL_INIT_GAMEPAD);
    }
}

/* The platform's event pump calls this for every SDL event. */
void PsxInputEvent(const SDL_Event *event)
{
    size_t i;
    int port;

    switch (event->type) {
    case SDL_EVENT_KEY_DOWN:
    case SDL_EVENT_KEY_UP:
        for (i = 0; i < KEY_COUNT; i++) {
            if (keyBindings[i].key == event->key.scancode) {
                keyDown[i] = event->key.down;
            }
        }
        update_keyboard();
        break;
    case SDL_EVENT_WINDOW_FOCUS_LOST:
        memset(keyDown, 0, sizeof(keyDown));
        update_keyboard();
        break;
    case SDL_EVENT_GAMEPAD_ADDED:
        open_gamepad(event->gdevice.which);
        break;
    case SDL_EVENT_GAMEPAD_REMOVED:
        close_gamepad(event->gdevice.which);
        break;
    case SDL_EVENT_GAMEPAD_BUTTON_DOWN:
    case SDL_EVENT_GAMEPAD_BUTTON_UP:
        for (port = 0; port < PORTS; port++) {
            if (gamepads[port] && SDL_GetGamepadID(gamepads[port]) == event->gbutton.which) {
                update_gamepad(port);
            }
        }
        break;
    case SDL_EVENT_GAMEPAD_AXIS_MOTION:
        for (port = 0; port < PORTS; port++) {
            if (gamepads[port] && SDL_GetGamepadID(gamepads[port]) == event->gaxis.which) {
                update_gamepad(port);
            }
        }
        break;
    default:
        break;
    }
}
