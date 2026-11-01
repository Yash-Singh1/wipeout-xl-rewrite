/* SDL virtual gamepad through the platform mapping and the game's real button decoder. */
#include "../../platform/psx/pad_sdl.c"
#include "../../platform/settings.c"
#include "../../game/include/pad.h"
#include <assert.h>

PortConfig portConfig;
static int focused = 1, updateAlert;
const char *PortUpdateAvailable(void) { return updateAlert ? "v0.0.7" : NULL; }
static LinkInput linkedInput;
static int linkRole = LINK_NONE;
static int linkSplit = 1;
int PortLinkRaceActive(void) { return linkSplit; }
int PortWindowFocused(void) { return focused; }
int PortLinkRole(void) { return linkRole; }
void PortLinkSetInput(const LinkInput *input) { linkedInput = *input; }
int PortLinkGetInput(LinkInput *input) { *input = linkedInput; return 1; }
int PortLinkTakeRumble(int kind, int *level) { return 0; }
void PortPadSecondRumble(int kind, int level) {}
uint64_t PsxFieldCount(void) { return 0; }
void PortLog(const char *format, ...) {}
int PortAnalogSteeringEnabled(void) { return !portSettings.digitalSteering; }
int PortGamepadSteering(void) { return PsxPadSteering(0); }
void PortScriptGamePad(unsigned char *packet) {}

u_short keyDefineThrustPad[2], keyDefineFire[2], keyDefineView[2], keyDefineDitch[2];
u_short keyDefineLBrake[2], keyDefineRBrake[2], keyDef[6];
u_short buttonCross, buttonTriangle, buttonCircle, buttonSquare, buttonL2, buttonR2;
u_short buttonUp, buttonDown, buttonLeft, buttonRight;
void InitPadSettings(void);
void buildGameKeyPad(PadPacket *pad, ControlInput *control);

static SDL_Joystick *stick;
static ControlInput control;
static void Read(void)
{
    PadPacket packet;
    SDL_UpdateJoysticks();
    update_gamepad(0);
    PsxPadFillBuffer(0, (uint8_t *)&packet, sizeof(packet));
    buildGameKeyPad(&packet, &control);
}
static void Axis(int value)
{
    assert(SDL_SetJoystickVirtualAxis(stick, SDL_GAMEPAD_AXIS_LEFTX, (Sint16)value));
    Read();
}
int main(void)
{
    SDL_VirtualJoystickDesc desc;
    SDL_JoystickID id;
    uint16_t bindings[PAD_ACTIONS] = {PAD_TRIANGLE, PAD_CIRCLE, PAD_SQUARE, PAD_R2, PAD_L2, PAD_CROSS};

    assert(SDL_Init(SDL_INIT_GAMEPAD | SDL_INIT_EVENTS));
    SDL_INIT_INTERFACE(&desc);
    desc.type = SDL_JOYSTICK_TYPE_GAMEPAD;
    desc.naxes = SDL_GAMEPAD_AXIS_COUNT;
    desc.nbuttons = SDL_GAMEPAD_BUTTON_COUNT;
    desc.button_mask = desc.axis_mask = ~0u;
    desc.name = "steering regression pad";
    id = SDL_AttachVirtualJoystick(&desc);
    assert(id);
    stick = SDL_OpenJoystick(id);
    gamepads[0] = SDL_OpenGamepad(id);
    assert(stick && gamepads[0]);
    lastSlot = 0;
    lastDevice = id;
    PsxPadSetType(0, PAD_DIGITAL);
    InitPadSettings();
    PsxPadRaceFrame(true, bindings);
    assert(!portSettings.digitalSteering); /* existing settings files also get analog by default */
    Axis(0);
    assert(control.head == 1 && (control.buttons & CONTROL_ANALOG_STEERING));
    assert(control.steering == 128);
    Axis(2000); assert(control.steering == 128);
    Axis(5000); assert(control.steering > 128 && control.steering < 150);
    Axis(10000); assert(control.steering > 150 && control.steering < 180);
    Axis(20000); assert(control.steering > 180 && control.steering < 230);
    Axis(32767); assert(control.steering == 255);
    Axis(-32768); assert(control.steering == 0);
    assert(!(control.buttons & (CONTROL_LEFT | CONTROL_RIGHT | CONTROL_ANALOG_THRUST)));
    assert(SDL_SetJoystickVirtualButton(stick, SDL_GAMEPAD_BUTTON_SOUTH, true));
    assert(SDL_SetJoystickVirtualButton(stick, SDL_GAMEPAD_BUTTON_EAST, true));
    Read();
    assert((control.buttons & (CONTROL_THRUST | CONTROL_FIRE)) == (CONTROL_THRUST | CONTROL_FIRE));
    assert(SDL_SetJoystickVirtualButton(stick, SDL_GAMEPAD_BUTTON_DPAD_LEFT, true));
    Read();
    assert((control.buttons & CONTROL_LEFT) && !(control.buttons & CONTROL_ANALOG_STEERING));
    assert(SDL_SetJoystickVirtualButton(stick, SDL_GAMEPAD_BUTTON_DPAD_LEFT, false));
    Set("digital_steering", "1");
    Axis(10000); assert(!(control.buttons & (CONTROL_RIGHT | CONTROL_LEFT | CONTROL_ANALOG_STEERING)));
    Axis(20000); assert((control.buttons & CONTROL_RIGHT) && !(control.buttons & CONTROL_ANALOG_STEERING));
    Set("digital_steering", "0");
    assert(SDL_SetJoystickVirtualButton(stick, SDL_GAMEPAD_BUTTON_START, true));
    Read(); assert(PsxPadButtons(0) & PAD_START);
    PortInputRaceStart(); Read(); assert(!(PsxPadButtons(0) & PAD_START));
    assert(SDL_SetJoystickVirtualButton(stick, SDL_GAMEPAD_BUTTON_START, false)); Read();
    assert(SDL_SetJoystickVirtualButton(stick, SDL_GAMEPAD_BUTTON_START, true)); Read();
    assert(PsxPadButtons(0) & PAD_START);
    assert(SDL_SetJoystickVirtualButton(stick, SDL_GAMEPAD_BUTTON_START, false)); Read();
    /* A keyboard action must not turn a held stick into digital steering. */
    lastDevice = DEVICE_KEYBOARD;
    Axis(10000); assert(control.buttons & CONTROL_ANALOG_STEERING);
    keyboardState.buttons = PAD_LEFT;
    Route(); Read();
    assert((control.buttons & CONTROL_LEFT) && !(control.buttons & CONTROL_ANALOG_STEERING));
    keyboardState.buttons = 0;
    /* The second player's controller carries its steering angle over the local link. */
    PortInputAssign(DEVICE_KEYBOARD, (int)id);
    SendSecondPlayer();
    assert(linkedInput.pad.analogSteering && linkedInput.pad.twist > 128);
    linkRole = LINK_SLAVE;
    linkSplit = 0;
    linkedInput.pad.buttons |= PAD_START;
    ReceiveSecondPlayer();
    assert(PsxPadButtons(0) == 0 && PsxPadSteering(0) == -1); /* no input in hidden setup menus */
    linkSplit = 1;
    ReceiveSecondPlayer();
    assert(!(PsxPadButtons(0) & PAD_START)); /* held setup Start must not pause the race */
    linkedInput.pad.buttons &= ~PAD_START;
    ReceiveSecondPlayer();
    linkedInput.pad.buttons |= PAD_START;
    ReceiveSecondPlayer();
    assert(PsxPadButtons(0) & PAD_START); /* a fresh in-race press still pauses */
    linkedInput.pad.buttons &= ~PAD_START;
    ReceiveSecondPlayer();
    PadPacket linkedPacket;
    PsxPadFillBuffer(0, (uint8_t *)&linkedPacket, sizeof(linkedPacket));
    buildGameKeyPad(&linkedPacket, &control);
    assert((control.buttons & CONTROL_ANALOG_STEERING) && control.steering == linkedInput.pad.twist);
    linkedInput.keyboard = 1;
    keyContext = PAD_CONTEXT_RACE; /* PsxPadSdlField updates this before receiving input in the game */
    size_t enterKey = 0, thrustKey = 0;
    for (size_t i = 0; i < KEY_COUNT; i++) {
        if (keyBindings[i].key == SDL_SCANCODE_RETURN) enterKey = i;
        if (keyBindings[i].key == SDL_SCANCODE_S) thrustKey = i;
    }
    linkedInput.keys = (1u << enterKey) | (1u << thrustKey);
    linkSplit = 0; ReceiveSecondPlayer(); assert(PsxPadButtons(0) == 0);
    linkSplit = 1; ReceiveSecondPlayer();
    assert(!(PsxPadButtons(0) & PAD_START) && (PsxPadButtons(0) & PAD_CROSS));
    linkedInput.keys &= ~(1u << enterKey); ReceiveSecondPlayer();
    linkedInput.keys |= 1u << enterKey; ReceiveSecondPlayer();
    assert(PsxPadButtons(0) & PAD_START);
    linkedInput.keys = 0; ReceiveSecondPlayer(); linkedInput.keyboard = 0;
    linkRole = LINK_NONE;
    PortInputAssign(DEVICE_NONE, DEVICE_NONE);
    focused = 0;
    Read(); assert(!(control.buttons & (CONTROL_ANALOG_STEERING | CONTROL_THRUST | CONTROL_FIRE)));
    focused = 1;
    SDL_Event focusEvent = {0};
    focusEvent.type = SDL_EVENT_WINDOW_FOCUS_GAINED;
    PsxInputEvent(&focusEvent);
    Axis(0);
    assert(!(control.buttons & (CONTROL_THRUST | CONTROL_FIRE))); /* held buttons don't resume on focus */
    /* Alert navigation must not steer, fire, or accept a game menu underneath it. */
    updateAlert = 1;
    Axis(10000);
    assert(PsxPadButtons(0) == 0 && PsxPadSteering(0) == -1);
    assert(SDL_SetJoystickVirtualButton(stick, SDL_GAMEPAD_BUTTON_SOUTH, true));
    Read(); assert(PsxPadButtons(0) == 0);
    PortInputAssign(DEVICE_KEYBOARD, (int)id);
    SendSecondPlayer();
    assert(linkedInput.pad.buttons == 0 && !linkedInput.pad.analogSteering);
    PortInputAssign(DEVICE_NONE, DEVICE_NONE);
    updateAlert = 0;
    Read(); assert(!(control.buttons & CONTROL_THRUST)); /* dismissal press stays consumed */
    assert(SDL_SetJoystickVirtualButton(stick, SDL_GAMEPAD_BUTTON_SOUTH, false)); Read();
    assert(SDL_SetJoystickVirtualButton(stick, SDL_GAMEPAD_BUTTON_SOUTH, true)); Read();
    assert(control.buttons & CONTROL_THRUST); /* a fresh press works normally */
    assert(SDL_SetJoystickVirtualButton(stick, SDL_GAMEPAD_BUTTON_SOUTH, false)); Read();
    PsxPadRaceFrame(false, bindings);
    Axis(20000);
    assert(PsxPadSteering(0) == -1);
    PadState state;
    assert(PsxPadGet(0, &state) && (state.buttons & PAD_RIGHT)); /* menu navigation */
    assert(StickSteering(2600) == 128 && StickSteering(2601) == 128);
    assert(StickSteering(-2600) == 128 && StickSteering(-2601) == 128);
    /* Switching back to Automatic changes the packet and mapping without a restart. */
    PsxPadSetType(0, PAD_NEGCON); Read();
    assert(control.head == 2 && PsxPadTypeChosen(0));
    PsxPadSetType(0, PAD_AUTO); Read();
    assert(control.head == 1 && PsxPadGetType(0) == PAD_DIGITAL && !PsxPadTypeChosen(0));
    PsxPadSetType(0, PAD_DIGITAL);
    PsxPadSetType(0, PAD_AUTO); Read();
    assert(control.head == 1 && !PsxPadTypeChosen(0));
    /* PR #7's raw raphnet joystick still auto-detects as NeGcon, with independent analogue buttons. */
    SDL_VirtualJoystickDesc adapter;
    SDL_INIT_INTERFACE(&adapter);
    adapter.type = SDL_JOYSTICK_TYPE_FLIGHT_STICK;
    adapter.vendor_id = 0x289b; adapter.product_id = 0x0044;
    adapter.naxes = 5; adapter.nbuttons = 16;
    adapter.axis_mask = adapter.button_mask = ~0u;
    adapter.name = "raphnet NeGcon regression pad";
    SDL_JoystickID adapterId = SDL_AttachVirtualJoystick(&adapter);
    assert(adapterId && !SDL_IsGamepad(adapterId));
    open_gamepad(adapterId);
    assert(joysticks[1] && PsxPadGetType(1) == PAD_NEGCON);
    assert(SDL_SetJoystickVirtualAxis(joysticks[1], 0, 5000));
    assert(SDL_SetJoystickVirtualAxis(joysticks[1], 2, 10000));
    assert(SDL_SetJoystickVirtualAxis(joysticks[1], 3, 20000));
    assert(SDL_SetJoystickVirtualAxis(joysticks[1], 4, 30000));
    assert(SDL_SetJoystickVirtualButton(joysticks[1], 2, true));
    assert(SDL_SetJoystickVirtualButton(joysticks[1], 3, true));
    SDL_UpdateJoysticks();
    Set("digital_steering", "1"); /* the ordinary stick option must not change a real NeGcon */
    update_port(1);
    assert(gamepadState[1].twist == (5000 + 32768) / 256);
    assert(gamepadState[1].analogI == 20000 / 128);
    assert(gamepadState[1].analogII == 30000 / 128);
    assert(gamepadState[1].analogL == 10000 / 128);
    assert((gamepadState[1].buttons & (PAD_CIRCLE | PAD_TRIANGLE)) == (PAD_CIRCLE | PAD_TRIANGLE));
    PortInputAssign((int)adapterId, DEVICE_NONE);
    assert(PsxPadGetType(0) == PAD_NEGCON && !PsxPadTypeChosen(0));
    assert(!PsxPadTypeChosen(1)); /* detection must never become a manual override */
    PsxPadFillBuffer(0, (uint8_t *)&linkedPacket, sizeof(linkedPacket));
    buildGameKeyPad(&linkedPacket, &control);
    assert(control.head == 2 && (control.buttons & CONTROL_ANALOG_STEERING));
    PsxPadSetType(0, PAD_DIGITAL);
    PsxPadSdlField(); /* no new physical input events */
    PsxPadFillBuffer(0, (uint8_t *)&linkedPacket, sizeof(linkedPacket));
    buildGameKeyPad(&linkedPacket, &control);
    assert(control.head == 1 && PsxPadTypeChosen(0));
    PsxPadSetType(0, PAD_AUTO);
    PsxPadSdlField();
    PsxPadFillBuffer(0, (uint8_t *)&linkedPacket, sizeof(linkedPacket));
    buildGameKeyPad(&linkedPacket, &control);
    assert(control.head == 2 && !PsxPadTypeChosen(0));
    assert(gamepadState[1].analogI == 20000 / 128 && gamepadState[1].analogL == 10000 / 128);
    PsxPadSetType(0, PAD_NEGCON);
    PsxPadSetType(0, PAD_AUTO);
    assert(PsxPadGetType(0) == PAD_NEGCON && !PsxPadTypeChosen(0));
    close_gamepad(adapterId);
    assert(SDL_DetachVirtualJoystick(adapterId));
    assert(PsxPadGetType(0) == PAD_DIGITAL && !PsxPadTypeChosen(0));
    adapterId = SDL_AttachVirtualJoystick(&adapter);
    assert(adapterId); open_gamepad(adapterId);
    PortInputAssign((int)adapterId, DEVICE_NONE);
    assert(PsxPadGetType(0) == PAD_NEGCON && !PsxPadTypeChosen(0));
    close_gamepad(adapterId); assert(SDL_DetachVirtualJoystick(adapterId));
    /* Reusing that slot for an unknown adapter must not retain NeGcon detection. */
    adapter.vendor_id = 0x1234; adapter.product_id = 0x5678;
    PsxPadSetType(0, PAD_NEGCON);
    adapterId = SDL_AttachVirtualJoystick(&adapter);
    assert(adapterId); open_gamepad(adapterId);
    PortInputAssign((int)adapterId, DEVICE_NONE);
    assert(PsxPadGetType(0) == PAD_NEGCON && PsxPadTypeChosen(0)); /* preserve an explicit choice */
    PsxPadSetType(0, PAD_AUTO);
    assert(PsxPadGetType(0) == PAD_DIGITAL && !PsxPadTypeChosen(0));
    close_gamepad(adapterId); assert(SDL_DetachVirtualJoystick(adapterId));
    PortInputAssign(DEVICE_NONE, DEVICE_NONE);
    PsxPadSetType(0, PAD_DIGITAL);
    Set("pad_type", "2"); Set("digital_steering", "0");
    assert(portSettings.negconPad == 2 && !portSettings.digitalSteering);
    Set("frame_cap", "15"); assert(portSettings.frameCap == 15);
    Set("frame_cap", "1"); assert(portSettings.frameCap == 15);
    Set("frame_cap", "1001"); assert(portSettings.frameCap == 1000);
    SDL_CloseGamepad(gamepads[0]);
    gamepads[0] = NULL;
    SDL_CloseJoystick(stick);
    SDL_DetachVirtualJoystick(id);
    /* The hidden game never initialized the controller subsystem and must not quit it. */
    linkRole = LINK_SLAVE;
    assert(SDL_WasInit(SDL_INIT_GAMEPAD));
    PsxPadSdlShutdown();
    assert(SDL_WasInit(SDL_INIT_GAMEPAD));
    SDL_Quit();
    puts("analog steering, bindings, menus, focus, live Automatic selection and NeGcon hotplug: OK");
    return 0;
}
