/* The pad as the player holds it: keyboard and SDL gamepads, with hot-plug.

   The keyboard (port 0, by key position, whatever the layout) means different buttons in the menus
   and in a race, because the game is steered with the same keys that drive the menus' cursor:

     menus (also the pause menu, the results and the name entry)
       Enter accept (Cross, and Start for the title)   Escape back (Triangle)
       arrow keys the directional pad                   Space Circle      Left Alt Square
       Q / E L1 / R1 and Backspace Select (the cheat chords)            P Start
     a race that is being steered
       S thrust   Space fire   Left Alt drop weapon   F1 or V view   A / D left / right air brake
       arrow keys steer and pitch   Escape, Enter or P Start (pause)   Q / E L1 / R1   F9 Select
     The race keys press the button the player bound to that action in the controller
     configuration (the game's keyDef[]), so S is the thrust whatever the thrust button is.

   Gamepads use the PlayStation layout on the standard gamepad buttons: south/east/west/north
   are Cross/Circle/Square/Triangle, the shoulders L1/R1, the triggers L2/R2, Start and Back
   Start and Select, the left stick and the d-pad the directional pad. Every gamepad plugged in
   drives the console's first port, with the keyboard (the game reads no other); in the two-player
   lobby each player's own controller is assigned (see Route).

   Gamepads count only while the game's window has the focus (SDL reports them in the background
   too). Buttons still held when the focus comes back stay quiet until they are released.

   The last device used (a key, or a button or stick of a gamepad) is told to the pad (see
   PsxPadNoteDevice) for the button glyphs the game shows.

   With the NeGcon type selected (WIPEOUT_PAD_TYPE=negcon) the left stick, or the left and
   right arrow keys, twist the controller and the triggers are the analogue buttons. */
#include <SDL3/SDL.h>

#include <string.h>

#include "../link.h"
#include "../platform.h"
#include "internal.h"
#include "pad.h"

#define GAMEPADS 8 /* gamepads open at once */
#define NO_ACTION (-1)

typedef struct KeyBinding {
    SDL_Scancode key;
    uint16_t menu;   /* the buttons it presses in the menus */
    uint16_t race;   /* ... in a race (unless it performs an action) */
    int action;      /* the race action it performs: the button is the game's binding of it */
} KeyBinding;

static const KeyBinding keyBindings[] = {
    {SDL_SCANCODE_RETURN, PAD_CROSS | PAD_START, PAD_START, NO_ACTION},
    {SDL_SCANCODE_KP_ENTER, PAD_CROSS | PAD_START, PAD_START, NO_ACTION},
    {SDL_SCANCODE_ESCAPE, PAD_TRIANGLE, PAD_START, NO_ACTION},
    {SDL_SCANCODE_P, PAD_START, PAD_START, NO_ACTION},
    {SDL_SCANCODE_BACKSPACE, PAD_SELECT, 0, NO_ACTION},
    {SDL_SCANCODE_F9, PAD_SELECT, PAD_SELECT, NO_ACTION},
    {SDL_SCANCODE_UP, PAD_UP, PAD_UP, NO_ACTION},
    {SDL_SCANCODE_DOWN, PAD_DOWN, PAD_DOWN, NO_ACTION},
    {SDL_SCANCODE_LEFT, PAD_LEFT, PAD_LEFT, NO_ACTION},
    {SDL_SCANCODE_RIGHT, PAD_RIGHT, PAD_RIGHT, NO_ACTION},
    {SDL_SCANCODE_SPACE, PAD_CIRCLE, 0, PAD_ACTION_FIRE},
    {SDL_SCANCODE_LALT, PAD_SQUARE, 0, PAD_ACTION_DITCH},
    {SDL_SCANCODE_S, 0, 0, PAD_ACTION_THRUST},
    {SDL_SCANCODE_A, 0, 0, PAD_ACTION_BRAKE_LEFT},
    {SDL_SCANCODE_D, 0, 0, PAD_ACTION_BRAKE_RIGHT},
    {SDL_SCANCODE_F1, 0, 0, PAD_ACTION_VIEW},
    {SDL_SCANCODE_V, 0, 0, PAD_ACTION_VIEW},
    {SDL_SCANCODE_Q, PAD_L1, PAD_L1, NO_ACTION},
    {SDL_SCANCODE_E, PAD_R1, PAD_R1, NO_ACTION}};

#define KEY_COUNT (sizeof(keyBindings) / sizeof(keyBindings[0]))

static bool keyDown[KEY_COUNT];
/* Keys that were held when the context changed, pressed buttons there and press others now stay
   quiet until they are released (Enter pauses the race: it must not also accept the first menu
   entry). A key that pressed nothing before, like S held while the race starts, takes effect. */
static bool keyHeldOver[KEY_COUNT];
static PadContext keyContext = PAD_CONTEXT_MENU;
static SDL_Gamepad *gamepads[GAMEPADS];
static uint16_t padHeldOver[GAMEPADS]; /* buttons held when the window regained the focus */
static bool padRegained[GAMEPADS];     /* the focus came back: note the buttons held now */

/* Two players on one machine (link.c): the devices of the first player (the only one this game reads)
   and of the second (sent over the link), once the lobby has assigned them. */
#define DEVICE_NONE (-1)
#define DEVICE_KEYBOARD 0
static int routed;
static int lastDevice = DEVICE_KEYBOARD; /* the device the player used last */
static int lastSlot = -1;                /* ... its slot, when it is a gamepad */
static int firstDevice = DEVICE_NONE, secondDevice = DEVICE_NONE;
static PadState keyboardState, gamepadState[GAMEPADS];
static uint16_t (*touchSource)(PadContext context); /* the touch controls (PsxPadSetTouchSource) */

static void Route(void);
static PadDevice gamepad_device(SDL_Gamepad *g);
static void SendSecondPlayer(void);
static void ReceiveSecondPlayer(void);

static uint16_t key_buttons(size_t i, PadContext context)
{
    const KeyBinding *k = &keyBindings[i];

    if (context == PAD_CONTEXT_MENU) {
        return k->menu;
    }
    return k->action == NO_ACTION ? k->race : PsxPadActionButton(k->action);
}

static void update_keyboard(void)
{
    PadState st;
    size_t i;

    memset(&st, 0, sizeof(st));
    st.twist = 0x80;
    for (i = 0; i < KEY_COUNT; i++) {
        if (keyDown[i] && !keyHeldOver[i]) {
            st.buttons |= key_buttons(i, keyContext);
        }
    }
    if (touchSource) {
        st.buttons |= touchSource(keyContext);
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
    keyboardState = st;
    Route();
}

/* Once a field, before the buffers are filled: the game may have switched between the menus and
   the race, or the player's bindings changed. */
void PsxPadSdlField(void)
{
    PadContext context = PsxPadContext();

    if (context != keyContext) {
        size_t i;

        for (i = 0; i < KEY_COUNT; i++) {
            uint16_t before = key_buttons(i, keyContext);

            keyHeldOver[i] = keyDown[i] && before && before != key_buttons(i, context);
        }
        keyContext = context;
        PortLog("pad: keyboard in %s (thrust is button %04X)\n", context == PAD_CONTEXT_RACE ? "the race" : "the menus",
                PsxPadActionButton(PAD_ACTION_THRUST));
    }
    if (PortLinkRole() == LINK_SLAVE) {
        ReceiveSecondPlayer();
        return;
    }
    update_keyboard();
    if (routed && secondDevice != DEVICE_NONE) {
        SendSecondPlayer();
    }
}

static void update_gamepad(int port)
{
    SDL_Gamepad *g = gamepads[port];
    PadState st;
    int x, y, lt, rt;

    memset(&st, 0, sizeof(st));
    st.twist = 0x80;
    if (!g) {
        gamepadState[port] = st;
        PsxPadSetSource(port, PAD_SRC_GAMEPAD, &st, false);
        return;
    }
    if (!PortWindowFocused()) {
        gamepadState[port] = st;
        Route();
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
    if (PsxPadGetType(port) != PAD_NEGCON) {
        if (x < -16384) st.buttons |= PAD_LEFT;
        if (x > 16384) st.buttons |= PAD_RIGHT;
        if (y < -16384) st.buttons |= PAD_UP;
        if (y > 16384) st.buttons |= PAD_DOWN;
    }
    if (padRegained[port]) {
        padRegained[port] = false;
        padHeldOver[port] = st.buttons;
    }
    padHeldOver[port] &= st.buttons;
    st.buttons &= (uint16_t)~padHeldOver[port];
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
    }
    gamepadState[port] = st;
    Route();
}

static int GamepadPort(int id)
{
    int port;

    for (port = 0; port < GAMEPADS; port++) {
        if (gamepads[port] && (int)SDL_GetGamepadID(gamepads[port]) == id) {
            return port;
        }
    }
    return -1;
}

/* The ports' sources: the keyboard on port 0 and each gamepad on its own port; with the devices
   assigned, the first player's device alone on port 0 and the second player's sent over the link. */
static void Route(void)
{
    static const PadState idle = {0, 0x80, 0, 0, 0};
    int port;

    if (!routed) {
        /* one player: every gamepad is theirs (the buttons of all; the sticks and triggers of the one used
           last), as the keyboard is; the console's second port is not read by a one-console game */
        PadState merged = idle;
        int any = 0;

        for (port = 0; port < GAMEPADS; port++) {
            if (gamepads[port]) {
                merged.buttons |= gamepadState[port].buttons;
                any = 1;
            }
        }
        if (lastSlot >= 0 && lastSlot < GAMEPADS && gamepads[lastSlot]) {
            merged.twist = gamepadState[lastSlot].twist;
            merged.analogI = gamepadState[lastSlot].analogI;
            merged.analogII = gamepadState[lastSlot].analogII;
            merged.analogL = gamepadState[lastSlot].analogL;
        }
        PsxPadSetSource(0, PAD_SRC_KEYBOARD, &keyboardState, true);
        PsxPadSetSource(0, PAD_SRC_GAMEPAD, &merged, any);
        PsxPadSetSource(1, PAD_SRC_GAMEPAD, &idle, false);
        return;
    }
    port = GamepadPort(firstDevice);
    PsxPadSetSource(0, PAD_SRC_KEYBOARD, firstDevice == DEVICE_KEYBOARD ? &keyboardState : &idle, true);
    PsxPadSetSource(0, PAD_SRC_GAMEPAD, port >= 0 ? &gamepadState[port] : &idle, port >= 0);
    PsxPadSetSource(1, PAD_SRC_GAMEPAD, &idle, false);
}

/* The second player's controller, for the link (once a field). */
static void SendSecondPlayer(void)
{
    LinkInput input;
    size_t i;
    int port;

    memset(&input, 0, sizeof(input));
    input.pad.twist = 0x80;
    if (secondDevice == DEVICE_KEYBOARD) {
        input.keyboard = 1;
        input.device = PAD_DEVICE_KEYBOARD;
        for (i = 0; i < KEY_COUNT && i < 32; i++) {
            if (keyDown[i]) {
                input.keys |= 1u << i;
            }
        }
    } else if ((port = GamepadPort(secondDevice)) >= 0) {
        input.device = gamepad_device(gamepads[port]);
        input.pad = gamepadState[port];
    }
    PortLinkSetInput(&input);
}

/* The second player's game: its controller is the one the first player's game sends. */
static void ReceiveSecondPlayer(void)
{
    static PadDevice shown = PAD_DEVICE_NONE;
    static const PadState idle = {0, 0x80, 0, 0, 0};
    LinkInput input;
    size_t i;

    if (!PortLinkGetInput(&input)) {
        return;
    }
    if (input.device != PAD_DEVICE_NONE && input.device != (int)shown) {
        shown = (PadDevice)input.device;
        PsxPadNoteDevice(shown);
    }
    for (i = 0; i < KEY_COUNT && i < 32; i++) {
        keyDown[i] = input.keyboard && ((input.keys >> i) & 1);
        if (!keyDown[i]) {
            keyHeldOver[i] = false;
        }
    }
    update_keyboard();
    PsxPadSetSource(0, PAD_SRC_GAMEPAD, input.keyboard ? &idle : &input.pad, !input.keyboard);
}

void PsxPadSetTouchSource(uint16_t (*buttons)(PadContext context))
{
    touchSource = buttons;
}

void PsxPadTouchChanged(void)
{
    if (PortLinkRole() != LINK_SLAVE) {
        update_keyboard();
    }
}

int PortInputLastDevice(void)
{
    return lastDevice;
}

void PortInputAssign(int first, int second)
{
    firstDevice = first;
    secondDevice = second;
    routed = first != DEVICE_NONE || second != DEVICE_NONE;
    Route();
}

int PortInputDevices(PortInputDevice *out, int max)
{
    int n = 0, port;
    size_t i;

    if (n < max) {
        out[n].id = DEVICE_KEYBOARD;
        out[n].kind = PAD_DEVICE_KEYBOARD;
        out[n].buttons = 0;
        for (i = 0; i < KEY_COUNT; i++) {
            if (keyDown[i] && PortWindowFocused()) {
                out[n].buttons |= key_buttons(i, PAD_CONTEXT_MENU);
            }
        }
        n++;
    }
    for (port = 0; port < GAMEPADS && n < max; port++) {
        if (gamepads[port]) {
            out[n].id = (int)SDL_GetGamepadID(gamepads[port]);
            out[n].kind = gamepad_device(gamepads[port]);
            out[n].buttons = gamepadState[port].buttons;
            n++;
        }
    }
    return n;
}

static void open_gamepad(SDL_JoystickID id)
{
    int port;

    if (SDL_getenv("WIPEOUT_NO_GAMEPAD") && !SDL_IsJoystickVirtual(id)) {
        return; /* tests: only the script's virtual gamepad, never a real one */
    }

    for (port = 0; port < GAMEPADS; port++) {
        if (gamepads[port] && SDL_GetGamepadID(gamepads[port]) == id) {
            return;
        }
    }
    for (port = 0; port < GAMEPADS; port++) {
        if (!gamepads[port]) {
            gamepads[port] = SDL_OpenGamepad(id);
            if (gamepads[port]) {
                PortLog("pad: %s (gamepad %d)\n", SDL_GetGamepadName(gamepads[port]), port + 1);
                update_gamepad(port);
            }
            return;
        }
    }
}

static void close_gamepad(SDL_JoystickID id)
{
    int port;

    for (port = 0; port < GAMEPADS; port++) {
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

    SDL_SetHint(SDL_HINT_JOYSTICK_ALLOW_BACKGROUND_EVENTS, "1");
    if (PortLinkRole() == LINK_SLAVE) {
        return; /* the second player's game: its controller comes over the link */
    }
    if (SDL_getenv("WIPEOUT_NO_GAMEPAD")) {
        return; /* tests: no gamepad is opened unless a script attaches one (WIPEOUT_KEYS) */
    }
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

    for (port = 0; port < GAMEPADS; port++) {
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

/* The family a gamepad belongs to, for its button glyphs. */
static PadDevice gamepad_device(SDL_Gamepad *g)
{
    switch (SDL_GetGamepadType(g)) {
    case SDL_GAMEPAD_TYPE_PS3:
    case SDL_GAMEPAD_TYPE_PS4:
    case SDL_GAMEPAD_TYPE_PS5:
        return PAD_DEVICE_PLAYSTATION;
    case SDL_GAMEPAD_TYPE_XBOX360:
    case SDL_GAMEPAD_TYPE_XBOXONE:
        return PAD_DEVICE_XBOX;
    case SDL_GAMEPAD_TYPE_NINTENDO_SWITCH_PRO:
    case SDL_GAMEPAD_TYPE_NINTENDO_SWITCH_JOYCON_LEFT:
    case SDL_GAMEPAD_TYPE_NINTENDO_SWITCH_JOYCON_RIGHT:
    case SDL_GAMEPAD_TYPE_NINTENDO_SWITCH_JOYCON_PAIR:
#if SDL_VERSION_ATLEAST(3, 4, 0)
    case SDL_GAMEPAD_TYPE_GAMECUBE:
#endif
        return PAD_DEVICE_NINTENDO;
    default:
        return PAD_DEVICE_GENERIC;
    }
}

/* A gamepad was used: its buttons and its sticks past a dead zone count, not the noise of a worn stick. */
static void note_gamepad(SDL_JoystickID id)
{
    int port;

    if (!PortWindowFocused()) {
        return;
    }
    if (routed && (int)id != firstDevice) {
        return; /* the second player's: not the glyphs of this game */
    }
    for (port = 0; port < GAMEPADS; port++) {
        if (gamepads[port] && SDL_GetGamepadID(gamepads[port]) == id) {
            PsxPadNoteDevice(gamepad_device(gamepads[port]));
            lastDevice = (int)id;
            lastSlot = port;
            return;
        }
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
                if (!event->key.down) {
                    keyHeldOver[i] = false;
                } else if (!event->key.repeat && (!routed || firstDevice == DEVICE_KEYBOARD)) {
                    PsxPadNoteDevice(PAD_DEVICE_KEYBOARD);
                    lastDevice = DEVICE_KEYBOARD;
                }
            }
        }
        update_keyboard();
        break;
    case SDL_EVENT_WINDOW_FOCUS_LOST:
        memset(keyDown, 0, sizeof(keyDown));
        memset(keyHeldOver, 0, sizeof(keyHeldOver));
        update_keyboard();
        for (port = 0; port < GAMEPADS; port++) {
            update_gamepad(port); /* released while away */
        }
        break;
    case SDL_EVENT_WINDOW_FOCUS_GAINED:
        for (port = 0; port < GAMEPADS; port++) {
            padRegained[port] = gamepads[port] != NULL;
            update_gamepad(port);
        }
        break;
    case SDL_EVENT_GAMEPAD_ADDED:
        open_gamepad(event->gdevice.which);
        break;
    case SDL_EVENT_GAMEPAD_REMOVED:
        close_gamepad(event->gdevice.which);
        break;
    case SDL_EVENT_GAMEPAD_BUTTON_DOWN:
        note_gamepad(event->gbutton.which);
        /* fall through */
    case SDL_EVENT_GAMEPAD_BUTTON_UP:
        for (port = 0; port < GAMEPADS; port++) {
            if (gamepads[port] && SDL_GetGamepadID(gamepads[port]) == event->gbutton.which) {
                update_gamepad(port);
            }
        }
        break;
    case SDL_EVENT_GAMEPAD_AXIS_MOTION:
        if (event->gaxis.value > 16384 || event->gaxis.value < -16384) {
            note_gamepad(event->gaxis.which);
        }
        for (port = 0; port < GAMEPADS; port++) {
            if (gamepads[port] && SDL_GetGamepadID(gamepads[port]) == event->gaxis.which) {
                update_gamepad(port);
            }
        }
        break;
    default:
        break;
    }
}
