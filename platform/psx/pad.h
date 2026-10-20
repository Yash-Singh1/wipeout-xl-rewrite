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

/* Parses a script (see pad.c) from memory; returns false on a syntax error (message in error). */
bool PsxPadLoadScript(const char *text, char *error, int errorSize);

/* Writes a port's BIOS buffer as the pad driver would (exposed for tests). */
void PsxPadFillBuffer(int port, uint8_t *buffer, int length);

#endif
