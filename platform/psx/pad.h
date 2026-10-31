/* The two controller ports. Input sources (keyboard, gamepads, a test script) set a source's
   state per port; the port's buttons are the union of the sources, delivered to the game in the
   BIOS pad buffers once per video field. */
#ifndef PSX_PAD_H
#define PSX_PAD_H

#include <stdbool.h>
#include <stdint.h>

/* Button bits, as the console reports them (active high here; the buffers hold the inverse). */
enum {
    PAD_SELECT = 0x0001, PAD_L3 = 0x0002, PAD_R3 = 0x0004, PAD_START = 0x0008,
    PAD_UP = 0x0010, PAD_RIGHT = 0x0020, PAD_DOWN = 0x0040, PAD_LEFT = 0x0080,
    PAD_L2 = 0x0100, PAD_R2 = 0x0200, PAD_L1 = 0x0400, PAD_R1 = 0x0800,
    PAD_TRIANGLE = 0x1000, PAD_CIRCLE = 0x2000, PAD_CROSS = 0x4000, PAD_SQUARE = 0x8000
};

typedef enum { PAD_DIGITAL = 0x41, PAD_NEGCON = 0x23 } PadType;

/* Input sources. Each keeps its own state for a port. */
typedef enum { PAD_SRC_KEYBOARD, PAD_SRC_GAMEPAD, PAD_SRC_SCRIPT, PAD_SRC_COUNT } PadSource;

typedef struct PadState {
    uint16_t buttons;        /* active high */
    uint8_t twist;           /* NeGcon: 0 left .. 0x80 centre .. 0xFF right */
    uint8_t analogI;         /* NeGcon buttons I, II and the left shoulder, 0 .. 0xFF */
    uint8_t analogII;
    uint8_t analogL;
} PadState;

/* Sets what a source holds on a port. `connected` says whether a pad is plugged into the port
   at all (the keyboard is always port 0; a gamepad claims a port while it is attached). */
void PsxPadSetSource(int port, PadSource source, const PadState *state, bool connected);
/* The port's merged state; false if nothing is connected. */
bool PsxPadGet(int port, PadState *state);
/* Selects what the port reports to the game: a digital pad or a NeGcon. */
void PsxPadSetType(int port, PadType type);
PadType PsxPadGetType(int port);
/* Whether the player (or WIPEOUT_PAD_TYPE) said which pad it is, rather than leaving it to whatever
   the adapter's controls look like. */
bool PsxPadTypeChosen(int port);

/* What the keyboard's keys mean. In the menus (and the pause menu, the results and the name entry)
   the keys press the menu buttons; in a race that is being steered they press the race controls.
   The game tells the pad each race frame; a few fields after the last call the menus are back. */
typedef enum { PAD_CONTEXT_MENU, PAD_CONTEXT_RACE } PadContext;

/* The six race actions, in the order of the game's key bindings (keyDef[]): the button of each is
   what the player assigned in the controller configuration. */
enum {
    PAD_ACTION_VIEW, PAD_ACTION_FIRE, PAD_ACTION_DITCH, PAD_ACTION_BRAKE_RIGHT,
    PAD_ACTION_BRAKE_LEFT, PAD_ACTION_THRUST, PAD_ACTIONS
};

/* Called by the game every frame of a race: whether the player is steering the ship (racing and
   not paused) and the buttons the digital pad's actions are bound to. */
void PsxPadRaceFrame(bool steering, const uint16_t bindings[PAD_ACTIONS]);
PadContext PsxPadContext(void);
/* The button each race action presses now (the game's binding, or the default layout's when the
   game has none yet or binds an analogue axis, and on the NeGcon the keyboard's own layout). */
uint16_t PsxPadActionButton(int action);

/* The device the player used last, which decides the button glyphs and names the game shows. */
typedef enum {
    PAD_DEVICE_NONE, PAD_DEVICE_KEYBOARD, PAD_DEVICE_PLAYSTATION, PAD_DEVICE_XBOX, PAD_DEVICE_NINTENDO,
    PAD_DEVICE_GENERIC, PAD_DEVICE_SATURN /* (after the others: the link sends the number) */
} PadDevice;
void PsxPadNoteDevice(PadDevice device);
PadDevice PsxPadLastDevice(void);

/* Touch controls (platform/touch.cpp): the buttons the fingers hold in a context, pressed with the keyboard's
   (pad_sdl.c asks each time the keyboard's buttons are worked out); PsxPadTouchChanged when they changed. */
/* The name being typed (controls.h's PortNameEntry): the keys that are the name's, those typed. */
void PsxPadNameEntry(int keys);
int PsxPadNameKey(void);

void PsxPadSetTouchSource(uint16_t (*buttons)(PadContext context));
void PsxPadTouchChanged(void);

/* Parses a script (see pad.c) from memory; returns false on a syntax error (message in error). */
bool PsxPadLoadScript(const char *text, char *error, int errorSize);

/* Writes a port's BIOS buffer as the pad driver would (exposed for tests). */
void PsxPadFillBuffer(int port, uint8_t *buffer, int length);

/* The pads' motors (pad_sdl.c; nothing without SDL), each player's own: player 0 this game's, 1 the second
   player's in a two-player link (the master's window holds their controller). In the game's units, 2 a knock
   to 20 a rocket, and three kinds:
   - PAD_RUMBLE_JOLT: a hit. It runs at its strength for its time, and a weaker one that comes while it runs
     does not end it.
   - PAD_RUMBLE_LEVEL: a level held. The game says it again every frame it wants the motors at it (0 stops
     them), and they stop by themselves a few fields after it stops saying.
   - PAD_RUMBLE_SCRAPE: a wall's scrape, held as a level is, on the small motor alone.
   For one player only the gamepad used last shakes, and none while the keyboard is. The game's own calls are
   PortPadRumble, PortPadRumbleLevel and PortPadRumbleScrape (platform/main.c), which keep the setting and the
   tests' hidden runs out of this library. */
enum { PAD_RUMBLE_JOLT, PAD_RUMBLE_LEVEL, PAD_RUMBLE_SCRAPE, PAD_RUMBLE_KINDS };
void PsxPadRumble(int player, int kind, int level);
/* (platform/main.c) the second player's request, which their game sent over the link: at this game's setting */
void PortPadSecondRumble(int kind, int level);

#endif
