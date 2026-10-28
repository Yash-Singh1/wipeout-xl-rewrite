/* The pad as the player holds it: keyboard and SDL gamepads, with hot-plug.

   The keyboard (port 0, by key position, whatever the layout) means different buttons in the menus
   and in a race, because the game is steered with the same keys that drive the menus' cursor:

     menus (also the pause menu, the results and the name entry)
       Enter accept (Cross, and Start for the title)   Escape back (Triangle)
       arrow keys the directional pad                   Space Circle      Left Alt Square
       Q / E L1 / R1 and Backspace Select (the cheat chords)            P Start
     a name being entered (the Hall of Fame's, the preferences')
       the letters (and on the PC front end digits, space, hyphen) type it   Backspace deletes   Enter ends it
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
static SDL_Joystick *joysticks[GAMEPADS]; /* a pad the adapter reports only as a joystick (a PSX adapter
                                             with a neGcon, which SDL has no gamepad mapping for) */
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

/* The name being typed (PsxPadNameEntry): its keys (PORT_NAME_*, controls.h), as of the field the game last said so,
   and those typed, waiting for the game. */
#define NAME_LETTERS 1
#define NAME_MORE 2
#define NAME_ENTER 4
#define NAME_KEYS 32
static int nameKeys, nameKeysBefore;
static uint64_t nameField;
static char typed[NAME_KEYS];
static int typedFirst, typedCount;

static void Route(void);
static PadDevice gamepad_device(SDL_Gamepad *g);
static void update_joystick(int port);
static void SendSecondPlayer(void);
static void ReceiveSecondPlayer(void);
static void rumble_tick(void);

static uint16_t key_buttons(size_t i, PadContext context)
{
    const KeyBinding *k = &keyBindings[i];

    if (context == PAD_CONTEXT_MENU) {
        return k->menu;
    }
    return k->action == NO_ACTION ? k->race : PsxPadActionButton(k->action);
}

/* The keys of the name being typed, while the game says it is (it says so each frame). */
static int NameKeys(void)
{
    return PsxFieldCount() - nameField <= 4 ? nameKeys : 0;
}

void PsxPadNameEntry(int keys)
{
    if (!NameKeys()) {
        typedCount = 0; /* (what was typed before the entry is not its) */
    }
    nameKeys = keys;
    nameField = PsxFieldCount();
}

int PsxPadNameKey(void)
{
    char key;

    if (!NameKeys() || !typedCount) {
        return 0;
    }
    key = typed[typedFirst];
    typedFirst = (typedFirst + 1) % NAME_KEYS;
    typedCount--;
    return (unsigned char)key;
}

/* What a key is in the name being typed (0: not the name's), by the character it makes in the player's layout. */
static char NameKey(SDL_Scancode scancode, int keys)
{
    SDL_Keycode code = SDL_GetKeyFromScancode(scancode, SDL_KMOD_NONE, false);

    if (!keys) {
        return 0;
    }
    if (scancode == SDL_SCANCODE_BACKSPACE) {
        return '\b';
    }
    if (scancode == SDL_SCANCODE_RETURN || scancode == SDL_SCANCODE_KP_ENTER) {
        return (keys & NAME_ENTER) ? '\n' : 0;
    }
    if (code >= 'a' && code <= 'z') {
        return (keys & NAME_LETTERS) ? (char)(code - 'a' + 'A') : 0;
    }
    if ((code >= '0' && code <= '9') || code == ' ' || code == '-') {
        return (keys & NAME_MORE) ? (char)code : 0;
    }
    return 0;
}

static void update_keyboard(void)
{
    PadState st;
    size_t i;

    memset(&st, 0, sizeof(st));
    st.twist = 0x80;
    for (i = 0; i < KEY_COUNT; i++) {
        if (keyDown[i] && !keyHeldOver[i] && !NameKey(keyBindings[i].key, NameKeys())) {
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

    rumble_tick(); /* the pads' motors, whose held level stops by itself */
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
    if (NameKeys() != nameKeysBefore) {
        size_t i;

        /* a key held as the name's entry begins or ends (the Enter that ended it) presses nothing until it is
           released */
        for (i = 0; i < KEY_COUNT; i++) {
            if (keyDown[i] && !NameKey(keyBindings[i].key, nameKeysBefore) != !NameKey(keyBindings[i].key, NameKeys())) {
                keyHeldOver[i] = true;
            }
        }
        nameKeysBefore = NameKeys();
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

/* The device in a port: a gamepad, or a pad the adapter reports only as a joystick. */
static SDL_JoystickID device_id(int port)
{
    if (gamepads[port]) {
        return SDL_GetGamepadID(gamepads[port]);
    }
    if (joysticks[port]) {
        return SDL_GetJoystickID(joysticks[port]);
    }
    return 0;
}

/* The family of the device in a port, for the button glyphs. */
static PadDevice device_kind(int port)
{
    return gamepads[port] ? gamepad_device(gamepads[port]) : PAD_DEVICE_PLAYSTATION;
}

/* An analogue button's press, from its axis: 0 let go to 0xFF pressed. */
static uint8_t analogue_button(Sint16 value)
{
    if (value < 0) {
        value = 0;
    }
    return (uint8_t)(value >> 7);
}

/* The twist's turn, from its axis: 0x00 left to 0xFF right, 0x80 straight ahead. */
static uint8_t analogue_twist(Sint16 value)
{
    return (uint8_t)((value + 32768) >> 8);
}

/* The adapters whose pads SDL has no gamepad mapping for, and how their axes and buttons are numbered.
   The raphnet PSX-to-USB adapter's numbers were measured with a script that asks for one control at a
   time; it is the one that presents a neGcon. Any other adapter (a generic PSX/PS2 one, say) is read as
   the console's own packet: the buttons' bit order and the sticks steering, so a digital pad or a
   DualShock on one is a digital pad with the right glyphs. */
static const struct {
    uint16_t vendor, product;
    int neGcon; /* it presents a neGcon, numbered as below */
    int axisTwist, axisI, axisII, axisL;
    const struct { int button; uint16_t bit; } buttons[8];
} adapters[] = {
    {0x289B, 0x0044, 1, 0, 3, 4, 2, {{2, PAD_CIRCLE},   {3, PAD_TRIANGLE}, {4, PAD_START}, {7, PAD_R1},
                                     {12, PAD_UP},     {13, PAD_DOWN},    {14, PAD_LEFT}, {15, PAD_RIGHT}}},
};

/* The adapter a joystick is, or -1 for one nobody has written down. */
static int adapter_index(SDL_Joystick *j)
{
    uint16_t vendor = SDL_GetJoystickVendor(j), product = SDL_GetJoystickProduct(j);
    int i;

    for (i = 0; i < (int)SDL_arraysize(adapters); i++) {
        if (adapters[i].vendor == vendor && adapters[i].product == product) {
            return i;
        }
    }
    return -1;
}

/* Whether the pad this adapter presents can be a neGcon (its twist and its analogue I, II and L). */
static bool adapter_negcon(SDL_Joystick *j)
{
    int a = adapter_index(j);

    return a >= 0 && adapters[a].neGcon;
}

/* A pad the adapter reports only as a joystick, by the console's own packet: the twist and the three
   analogue buttons become the packet's four analogue bytes and the buttons its bits. So a neGcon twists
   and presses its analogue I, II and L, and a digital pad presses its buttons, whatever the adapter's
   own numbering. */
static void update_joystick(int port)
{
    SDL_Joystick *j = joysticks[port];
    PadState st;
    Sint16 turned = 0, buttonI = 0, buttonII = 0, buttonL = 0;
    int i, axes, buttons, a;

    memset(&st, 0, sizeof(st));
    st.twist = 0x80;
    if (!j) {
        gamepadState[port] = st;
        PsxPadSetSource(port, PAD_SRC_GAMEPAD, &st, false);
        return;
    }
    if (!PortWindowFocused()) {
        gamepadState[port] = st;
        Route();
        return;
    }
    a = adapter_index(j);
    axes = SDL_GetNumJoystickAxes(j);
    if (a >= 0) {
        /* the adapter's own numbering (measured for the raphnet: twist 0, I 3, II 4, L 2) */
        if (axes > adapters[a].axisTwist) turned = SDL_GetJoystickAxis(j, adapters[a].axisTwist);
        if (axes > adapters[a].axisI) buttonI = SDL_GetJoystickAxis(j, adapters[a].axisI);
        if (axes > adapters[a].axisII) buttonII = SDL_GetJoystickAxis(j, adapters[a].axisII);
        if (axes > adapters[a].axisL) buttonL = SDL_GetJoystickAxis(j, adapters[a].axisL);
    } else {
        /* an adapter nobody has written down: the console's own, where the left stick is the twist */
        if (axes > 0) turned = SDL_GetJoystickAxis(j, 0);
    }
    if (PsxPadGetType(port) == PAD_NEGCON) {
        st.twist = analogue_twist(turned);
        if (st.twist > 0x70 && st.twist < 0x90) st.twist = 0x80; /* dead zone */
        st.analogI = analogue_button(buttonI);
        st.analogII = analogue_button(buttonII);
        st.analogL = analogue_button(buttonL);
    } else {
        /* a digital pad: the twist steers and the analogue buttons press its buttons, so a neGcon is
           playable before anyone says it is one */
        if (turned < -16384) st.buttons |= PAD_LEFT;
        if (turned > 16384) st.buttons |= PAD_RIGHT;
        if (a < 0 && axes > 1) {
            Sint16 updown = SDL_GetJoystickAxis(j, 1);

            if (updown < -16384) st.buttons |= PAD_UP;
            if (updown > 16384) st.buttons |= PAD_DOWN;
        }
        if (buttonI > 8192) st.buttons |= PAD_CROSS;
        if (buttonII > 8192) st.buttons |= PAD_CIRCLE;
        if (buttonL > 8192) st.buttons |= PAD_L2;
    }
    buttons = SDL_GetNumJoystickButtons(j);
    if (a >= 0) {
        /* the adapter's own numbering (the neGcon's eight on the raphnet) */
        for (i = 0; i < (int)SDL_arraysize(adapters[a].buttons); i++) {
            if (adapters[a].buttons[i].button < buttons) {
                if (SDL_GetJoystickButton(j, adapters[a].buttons[i].button)) {
                    st.buttons |= adapters[a].buttons[i].bit;
                }
            }
        }
    } else {
        /* a digital pad or a DualShock: the buttons are the console's packet's own bit order */
        for (i = 0; i < 16 && i < buttons; i++) {
            if (SDL_GetJoystickButton(j, i)) {
                st.buttons |= (uint16_t)(1u << i);
            }
        }
    }
    if (padRegained[port]) {
        padRegained[port] = false;
        padHeldOver[port] = st.buttons;
    }
    padHeldOver[port] &= st.buttons;
    st.buttons &= (uint16_t)~padHeldOver[port];
    gamepadState[port] = st;
    Route();
}

/* A port's device changed state: whichever kind of device is in it. */
static void update_port(int port)
{
    if (joysticks[port] && !gamepads[port]) {
        update_joystick(port);
    } else {
        update_gamepad(port);
    }
}

static int GamepadPort(int id)
{
    int port;

    for (port = 0; port < GAMEPADS; port++) {
        if (device_id(port) == (SDL_JoystickID)id) {
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
            if (device_id(port)) {
                merged.buttons |= gamepadState[port].buttons;
                any = 1;
            }
        }
        if (lastSlot >= 0 && lastSlot < GAMEPADS && device_id(lastSlot)) {
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
        input.device = device_kind(port);
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
        if (device_id(port)) {
            out[n].id = (int)device_id(port);
            out[n].kind = device_kind(port);
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
        if (device_id(port) == id) {
            return;
        }
    }
    for (port = 0; port < GAMEPADS; port++) {
        if (device_id(port) != 0) {
            continue;
        }
        if (SDL_IsGamepad(id)) {
            gamepads[port] = SDL_OpenGamepad(id);
            if (gamepads[port]) {
                PortLog("pad: %s (gamepad %d)\n", SDL_GetGamepadName(gamepads[port]), port + 1);
                update_gamepad(port);
            }
        } else {
            /* a pad the adapter reports only as a joystick, which SDL has no gamepad mapping for */
            joysticks[port] = SDL_OpenJoystick(id);
            if (joysticks[port]) {
                PortLog("pad: %s (joystick %d)\n", SDL_GetJoystickName(joysticks[port]), port + 1);
                if (adapter_negcon(joysticks[port]) && !PsxPadTypeChosen(port)) {
                    /* the adapter is one that presents a neGcon (its twist and its analogue I, II and
                       L); the debug console's "Controller" says otherwise */
                    PsxPadSetType(port, PAD_NEGCON);
                    PortLog("pad: it is a neGcon\n");
                }
                update_joystick(port);
            }
        }
        return;
    }
}

static void close_gamepad(SDL_JoystickID id)
{
    int port;

    for (port = 0; port < GAMEPADS; port++) {
        if (device_id(port) != id) {
            continue;
        }
        if (gamepads[port]) {
            SDL_CloseGamepad(gamepads[port]);
            gamepads[port] = NULL;
        }
        if (joysticks[port]) {
            SDL_CloseJoystick(joysticks[port]);
            joysticks[port] = NULL;
        }
        update_port(port);
        PortLog("pad: port %d unplugged\n", port + 1);
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
    ids = SDL_GetJoysticks(&count); /* every pad, gamepads included; open_gamepad tells them apart */
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
        if (joysticks[port]) {
            SDL_CloseJoystick(joysticks[port]);
            joysticks[port] = NULL;
        }
    }
    opened = !SDL_getenv("WIPEOUT_NO_GAMEPAD");
    if (opened) {
        SDL_QuitSubSystem(SDL_INIT_GAMEPAD);
    }
}

/* Sega Saturn pads and adapters for them (SDL has no type for them), and 8BitDo's M30 with the same six buttons
   (A B C below X Y Z): by name, or by the vendor and product the gamepad database lists them under. */
static bool saturn_pad(SDL_Gamepad *g)
{
    static const struct { uint16_t vendor, product; } known[] = {
        {0x04B4, 0x010A}, /* Sega Saturn USB Gamepad */
        {0x1781, 0x057E}, /* Sega Saturn */
        {0x0E8F, 0x3010}, /* Mayflash adapter for the original pad */
        {0x0500, 0x9B28}, {0x289B, 0x0005}, /* Saturn_Adapter_2.0 */
        {0x2DC8, 0x0650}, {0x2DC8, 0x5006}, {0x2DC8, 0x5106}, /* 8BitDo M30 */
    };
    const char *name = SDL_GetGamepadName(g);
    uint16_t vendor = SDL_GetGamepadVendor(g), product = SDL_GetGamepadProduct(g);
    size_t i;

    if (name && SDL_strcasestr(name, "saturn")) {
        return true;
    }
    for (i = 0; i < SDL_arraysize(known); i++) {
        if (known[i].vendor == vendor && known[i].product == product) {
            return true;
        }
    }
    return false;
}

/* The family a gamepad belongs to, for its button glyphs. */
static PadDevice gamepad_device(SDL_Gamepad *g)
{
    if (saturn_pad(g)) {
        return PAD_DEVICE_SATURN;
    }
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
        if (device_id(port) == id) {
            PsxPadNoteDevice(device_kind(port));
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
        if (event->key.down && typedCount < NAME_KEYS) {
            char key = NameKey(event->key.scancode, NameKeys());

            if (key && (!event->key.repeat || key == '\b')) {
                typed[(typedFirst + typedCount++) % NAME_KEYS] = key;
                PsxPadNoteDevice(PAD_DEVICE_KEYBOARD);
            }
        }
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
        port = GamepadPort((int)event->gbutton.which);
        if (port >= 0) {
            update_port(port);
        }
        break;
    case SDL_EVENT_GAMEPAD_AXIS_MOTION:
        if (event->gaxis.value > 16384 || event->gaxis.value < -16384) {
            note_gamepad(event->gaxis.which);
        }
        port = GamepadPort((int)event->gaxis.which);
        if (port >= 0) {
            update_port(port);
        }
        break;
    case SDL_EVENT_JOYSTICK_ADDED:
        if (!SDL_IsGamepad(event->jdevice.which)) {
            open_gamepad(event->jdevice.which); /* a pad the adapter reports only as a joystick */
        }
        break;
    case SDL_EVENT_JOYSTICK_REMOVED:
        if (!SDL_IsGamepad(event->jdevice.which)) {
            close_gamepad(event->jdevice.which);
        }
        break;
    case SDL_EVENT_JOYSTICK_BUTTON_DOWN:
        if (!SDL_IsGamepad(event->jbutton.which)) {
            note_gamepad(event->jbutton.which);
        }
        /* fall through */
    case SDL_EVENT_JOYSTICK_BUTTON_UP:
        if (!SDL_IsGamepad(event->jbutton.which)) {
            port = GamepadPort((int)event->jbutton.which);
            if (port >= 0) {
                update_port(port);
            }
        }
        break;
    case SDL_EVENT_JOYSTICK_AXIS_MOTION:
        if (SDL_IsGamepad(event->jaxis.which)) {
            break; /* a gamepad reports its own events above */
        }
        if (event->jaxis.value > 16384 || event->jaxis.value < -16384) {
            note_gamepad(event->jaxis.which);
        }
        port = GamepadPort((int)event->jaxis.which);
        if (port >= 0) {
            update_port(port);
        }
        break;
    default:
        break;
    }
}

/* The pads' motors. The pads that plugged into the console had none; the port's gamepads shake as the
   picture does, and growl with the launch thrust. Each player's motors are their own (player 0 this game's,
   1 the second player's, whose game asks over the link), and three things can want them at once:
   - a jolt, a hit of `level` that runs for its time and is not ended by a weaker one;
   - a level held while the game keeps saying it and stopped a few fields after it stops;
   - a scrape, held as a level is, on the small motor alone.
   All are in the game's units: 2 a knock, 20 the full strength. */
typedef struct Rumble {
    int level[PAD_RUMBLE_KINDS];      /* what runs of each kind ... */
    uint64_t until[PAD_RUMBLE_KINDS]; /* ... and when it ends */
    int port;                         /* the gamepad that shakes (-1: none), to stop it when the player moves on */
} Rumble;

static Rumble rumbles[2] = {{{0}, {0}, -1}, {{0}, {0}, -1}};

#define HOLD_MS 100 /* a held level's life without a call: a few fields */

static uint16_t rumble_strength(int level)
{
    /* the square root of the level's share of the scale: the light knocks are felt and the rocket keeps
       its lead over them (a pad's motors are weakest exactly where the levels are lowest) */
    return (uint16_t)(SDL_sqrtf((float)level / 20.0f) * 32767.0f);
}

/* The gamepad a player holds: with the devices assigned (the two players' link), each player's own; for one
   player, the gamepad used last, and none while it is the keyboard - a gamepad lying by is not theirs. */
static int rumble_port(int player)
{
    if (routed) {
        return GamepadPort(player ? secondDevice : firstDevice);
    }
    if (player || lastDevice == DEVICE_KEYBOARD || lastSlot < 0 || lastSlot >= GAMEPADS || !device_id(lastSlot)) {
        return -1;
    }
    return lastSlot;
}

/* The motors of the device in a port: a gamepad's, or a pad the adapter reports only as a joystick's (a
   DualShock on a PSX adapter has them too; a neGcon has none). */
static void rumble_device(int port, uint16_t low, uint16_t high, uint32_t duration)
{
    if (port < 0) {
        return;
    }
    if (gamepads[port]) {
        SDL_RumbleGamepad(gamepads[port], low, high, duration);
    } else if (joysticks[port]) {
        SDL_RumbleJoystick(joysticks[port], low, high, duration);
    }
}

static void rumble_apply(int player)
{
    Rumble *r = &rumbles[player];
    uint64_t now = SDL_GetTicks();
    uint16_t low = 0, high = 0;
    uint32_t duration = 0;
    int kind, level = 0, port;

    for (kind = 0; kind < PAD_RUMBLE_KINDS; kind++) {
        if (now >= r->until[kind]) {
            r->level[kind] = 0;
            r->until[kind] = 0;
        }
    }
    if (r->level[PAD_RUMBLE_JOLT] > level) {
        level = r->level[PAD_RUMBLE_JOLT];
        duration = (uint32_t)(r->until[PAD_RUMBLE_JOLT] - now);
    }
    if (r->level[PAD_RUMBLE_LEVEL] > level) {
        level = r->level[PAD_RUMBLE_LEVEL];
        duration = HOLD_MS;
    }
    if (level > 0) {
        low = rumble_strength(level);
        high = low; /* both motors: the small one is the buzz a growl is felt through */
    }
    if (rumble_strength(r->level[PAD_RUMBLE_SCRAPE]) > high) {
        high = rumble_strength(r->level[PAD_RUMBLE_SCRAPE]); /* a scrape is a rattle, not a thud: the small motor's alone */
    }
    if (!PortWindowFocused()) {
        low = high = 0; /* nothing runs while the game is in the background */
    }
    if (low || high) {
        if (duration < HOLD_MS) {
            duration = HOLD_MS;
        }
    } else {
        duration = 0;
    }
    port = rumble_port(player);
    if (r->port != port && r->port >= 0) {
        rumble_device(r->port, 0, 0, 0); /* the player has put that one down */
    }
    r->port = port;
    rumble_device(port, low, high, duration);
}

/* Once a field: a held level stops by itself when the game stops holding it, and the second player's game
   says over the link what it asks of their motors. */
static void rumble_tick(void)
{
    int player, kind, level;

    if (routed && secondDevice != DEVICE_NONE && PortLinkRole() == LINK_MASTER) {
        for (kind = 0; kind < PAD_RUMBLE_KINDS; kind++) {
            if (PortLinkTakeRumble(kind, &level)) {
                PortPadSecondRumble(kind, level);
            }
        }
    }
    for (player = 0; player < 2; player++) {
        for (kind = 0; kind < PAD_RUMBLE_KINDS; kind++) {
            if (rumbles[player].until[kind]) {
                rumble_apply(player);
                break;
            }
        }
    }
}

void PsxPadRumble(int player, int kind, int level)
{
    Rumble *r = &rumbles[player ? 1 : 0];
    uint64_t now = SDL_GetTicks();

    if (kind < 0 || kind >= PAD_RUMBLE_KINDS) {
        return;
    }
    if (level > 20) {
        level = 20;
    }
    if (level <= 0) {
        r->level[kind] = 0;
        r->until[kind] = 0;
    } else if (kind == PAD_RUMBLE_JOLT) {
        if (level < r->level[kind] && now < r->until[kind]) {
            return; /* the jolt that is running is the stronger of the two */
        }
        r->level[kind] = level;
        r->until[kind] = now + (uint64_t)(100 + level * 20);
    } else {
        r->level[kind] = level;
        r->until[kind] = now + HOLD_MS;
    }
    rumble_apply(player ? 1 : 0);
}
