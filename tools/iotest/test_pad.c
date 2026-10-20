/* The pad: the BIOS buffers the game reads (PadPacket: 0x4100 digital pad, 0x2300 NeGcon, buttons
   active low), scripted input by video field, the keyboard layout in the menus and in a race
   through SDL key events, and the NeGcon mapping. Gamepad hot-plug needs a device and is checked by hand. */
#include <SDL3/SDL.h>

#include <stdio.h>
#include <unistd.h>
#include <string.h>

#include "../../platform/psx/internal.h"
#include "../../platform/psx/pad.h"
#include "testenv.h"
#include "testutil.h"

void InitPAD(void *, int, void *, int);
void StartPAD(void);
int StopPAD(void);

/* The game's view of a buffer (game/include/pad.h). */
typedef struct PadPacket {
    uint16_t header;
    uint16_t buttons;
    uint8_t steering;
    uint8_t analog[3];
} PadPacket;

static void key(SDL_Scancode code, bool down)
{
    SDL_Event e;

    memset(&e, 0, sizeof(e));
    e.type = down ? SDL_EVENT_KEY_DOWN : SDL_EVENT_KEY_UP;
    e.key.scancode = code;
    e.key.down = down;
    PsxInputEvent(&e);
}

static void fields_to(uint64_t n)
{
    while (test_fields() < n) {
        PsxVBlank();
    }
}

static PadPacket packet(const uint8_t *b)
{
    PadPacket p;

    memcpy(&p, b, sizeof(p));
    return p;
}

int main(void)
{
    static uint8_t buf1[8], buf2[8];
    const char *script = "/tmp/psxio-pad-script.txt";
    FILE *f;
    PadPacket p;

    /* the bit layout the game's buildFixedPad and InitPadSettings use */
    CHECK_EQ_INT(PAD_CROSS, 0x4000);
    CHECK_EQ_INT(PAD_TRIANGLE, 0x1000);
    CHECK_EQ_INT(PAD_CIRCLE, 0x2000);
    CHECK_EQ_INT(PAD_SQUARE, 0x8000);
    CHECK_EQ_INT(PAD_R2, 0x200);
    CHECK_EQ_INT(PAD_L2, 0x100);
    CHECK_EQ_INT(PAD_UP, 0x10);
    CHECK_EQ_INT(PAD_DOWN, 0x40);
    CHECK_EQ_INT(PAD_LEFT, 0x80);
    CHECK_EQ_INT(PAD_RIGHT, 0x20);
    CHECK_EQ_INT(PAD_START, 0x8);
    CHECK_EQ_INT(PAD_SELECT, 0x1);
    CHECK_EQ_INT(PAD_L1, 0x400);
    CHECK_EQ_INT(PAD_R1, 0x800);

    f = fopen(script, "w");
    fputs("# field port buttons\n"
          "10 0 CROSS+START\n"
          "20 0 none\n"
          "30 0 0x8000\n"
          "40 1 L1+UP\n"
          "50 0 twist=0 i=255\n", f);
    fclose(f);
    if (!test_io_start(1.0, script)) {
        printf("test_pad: disc not found, skipped\n");
        return 0;
    }
    InitPAD(buf1, 8, buf2, 8);
    StartPAD();

    /* scripted input, by video field */
    fields_to(9);
    p = packet(buf1);
    CHECK_EQ_INT(p.header, 0x4100);
    CHECK_EQ_INT(p.buttons, 0xFFFF); /* nothing pressed */
    fields_to(10);
    p = packet(buf1);
    CHECK_EQ_INT(p.header, 0x4100);
    CHECK_EQ_INT(p.buttons, (uint16_t)~(PAD_CROSS | PAD_START));
    CHECK_EQ_INT(buf1[2], 0xF7);
    CHECK_EQ_INT(buf1[3], 0xBF);
    CHECK_EQ_INT(PsxPadButtons(0), PAD_CROSS | PAD_START);
    fields_to(20);
    CHECK_EQ_INT(packet(buf1).buttons, 0xFFFF);
    fields_to(30);
    CHECK_EQ_INT(packet(buf1).buttons, (uint16_t)~PAD_SQUARE);
    /* port 2 */
    CHECK_EQ_INT(packet(buf2).header, 0x4100);
    CHECK_EQ_INT(packet(buf2).buttons, 0xFFFF);
    fields_to(40);
    CHECK_EQ_INT(packet(buf2).buttons, (uint16_t)~(PAD_L1 | PAD_UP));
    CHECK_EQ_INT(packet(buf1).buttons, (uint16_t)~PAD_SQUARE); /* port 1 is unchanged */

    /* a stopped driver leaves the buffers alone */
    StopPAD();
    fields_to(45);
    PsxPadSetType(0, PAD_NEGCON);
    fields_to(50);
    CHECK_EQ_INT(packet(buf1).buttons, (uint16_t)~PAD_SQUARE);
    StartPAD();
    fields_to(51);
    /* NeGcon: header 0x2300; only start, the d-pad, R, A and B exist; twist and analogue buttons */
    p = packet(buf1);
    CHECK_EQ_INT(p.header, 0x2300);
    CHECK_EQ_INT(p.steering, 0);
    CHECK_EQ_INT(p.analog[0], 255);
    CHECK_EQ_INT(p.analog[1], 0);
    CHECK_EQ_INT(p.buttons, 0xFFFF); /* SQUARE is not a NeGcon button */
    test_io_stop();

    /* live input: keyboard through SDL key events (no script) */
    if (!test_io_start(1.0, NULL)) {
        return 1;
    }
    InitPAD(buf1, 8, buf2, 8);
    StartPAD();
    PsxVBlank();
    CHECK_EQ_INT(packet(buf1).header, 0x4100);  /* the keyboard is always plugged into port 1 */
    CHECK_EQ_INT(packet(buf2).header & 0xFF, 0xFF); /* nothing in port 2 */
    /* the keys of the menus: the cursor, accept and back (Enter is Start too, for the title) */
    {
        static const struct { SDL_Scancode key; uint16_t buttons; } menus[] = {
            {SDL_SCANCODE_RETURN, PAD_CROSS | PAD_START},
            {SDL_SCANCODE_ESCAPE, PAD_TRIANGLE},
            {SDL_SCANCODE_SPACE, PAD_CIRCLE},
            {SDL_SCANCODE_LALT, PAD_SQUARE},
            {SDL_SCANCODE_LEFT, PAD_LEFT},
            {SDL_SCANCODE_RIGHT, PAD_RIGHT},
            {SDL_SCANCODE_UP, PAD_UP},
            {SDL_SCANCODE_DOWN, PAD_DOWN},
            {SDL_SCANCODE_P, PAD_START},
            {SDL_SCANCODE_BACKSPACE, PAD_SELECT},
            {SDL_SCANCODE_Q, PAD_L1},
            {SDL_SCANCODE_E, PAD_R1},
            {SDL_SCANCODE_S, 0}, /* the race keys do nothing in the menus */
            {SDL_SCANCODE_A, 0},
            {SDL_SCANCODE_F1, 0},
        };
        size_t i;

        CHECK_EQ_INT(PsxPadContext(), PAD_CONTEXT_MENU);
        CHECK_EQ_INT(PsxPadLastDevice(), PAD_DEVICE_NONE);
        for (i = 0; i < sizeof(menus) / sizeof(menus[0]); i++) {
            key(menus[i].key, true);
            PsxVBlank();
            CHECK_EQ_INT(PsxPadButtons(0), menus[i].buttons);
            CHECK_EQ_INT(packet(buf1).buttons, (uint16_t)~menus[i].buttons);
            key(menus[i].key, false);
            PsxVBlank();
            CHECK_EQ_INT(packet(buf1).buttons, 0xFFFF);
        }
        CHECK_EQ_INT(PsxPadLastDevice(), PAD_DEVICE_KEYBOARD);
    }
    /* the keys of a race that is being steered press the buttons the player bound to the actions */
    {
        static const struct { SDL_Scancode key; int action; uint16_t buttons; } race[] = {
            {SDL_SCANCODE_S, PAD_ACTION_THRUST, PAD_CROSS},
            {SDL_SCANCODE_SPACE, PAD_ACTION_FIRE, PAD_CIRCLE},
            {SDL_SCANCODE_LALT, PAD_ACTION_DITCH, PAD_SQUARE},
            {SDL_SCANCODE_A, PAD_ACTION_BRAKE_LEFT, PAD_L2},
            {SDL_SCANCODE_D, PAD_ACTION_BRAKE_RIGHT, PAD_R2},
            {SDL_SCANCODE_F1, PAD_ACTION_VIEW, PAD_TRIANGLE},
            {SDL_SCANCODE_V, PAD_ACTION_VIEW, PAD_TRIANGLE},
        };
        static const uint16_t defaults[PAD_ACTIONS] = {PAD_TRIANGLE, PAD_CIRCLE, PAD_SQUARE, PAD_R2, PAD_L2, PAD_CROSS};
        static const uint16_t rebound[PAD_ACTIONS] = {PAD_L1, PAD_R1, PAD_L2, PAD_CROSS, PAD_CIRCLE, 0x8FF0};
        size_t i;

        PsxPadRaceFrame(true, defaults);
        PsxVBlank();
        CHECK_EQ_INT(PsxPadContext(), PAD_CONTEXT_RACE);
        for (i = 0; i < sizeof(race) / sizeof(race[0]); i++) {
            key(race[i].key, true);
            PsxVBlank();
            PsxPadRaceFrame(true, defaults);
            CHECK_EQ_INT(PsxPadButtons(0), race[i].buttons);
            key(race[i].key, false);
            PsxVBlank();
            PsxPadRaceFrame(true, defaults);
            CHECK_EQ_INT(packet(buf1).buttons, 0xFFFF);
        }
        /* Enter, Escape and P pause; the arrows steer and pitch whatever the bindings */
        key(SDL_SCANCODE_RETURN, true);
        PsxVBlank();
        CHECK_EQ_INT(PsxPadButtons(0), PAD_START);
        key(SDL_SCANCODE_RETURN, false);
        key(SDL_SCANCODE_ESCAPE, true);
        key(SDL_SCANCODE_LEFT, true);
        PsxVBlank();
        CHECK_EQ_INT(PsxPadButtons(0), PAD_START | PAD_LEFT);
        key(SDL_SCANCODE_ESCAPE, false);
        key(SDL_SCANCODE_LEFT, false);
        /* rebound actions: the keys keep their meaning, an analogue binding falls back to the default */
        PsxPadRaceFrame(true, rebound);
        key(SDL_SCANCODE_S, true);
        key(SDL_SCANCODE_SPACE, true);
        key(SDL_SCANCODE_LALT, true);
        key(SDL_SCANCODE_A, true);
        key(SDL_SCANCODE_D, true);
        key(SDL_SCANCODE_F1, true);
        PsxVBlank();
        PsxPadRaceFrame(true, rebound);
        CHECK_EQ_INT(PsxPadButtons(0), PAD_CROSS | PAD_L1 | PAD_R1 | PAD_L2 | PAD_CIRCLE);
        key(SDL_SCANCODE_S, false);
        key(SDL_SCANCODE_SPACE, false);
        key(SDL_SCANCODE_LALT, false);
        key(SDL_SCANCODE_A, false);
        key(SDL_SCANCODE_D, false);
        key(SDL_SCANCODE_F1, false);
        PsxVBlank();
        /* paused: the game says the ship is not steered, and the menus are back */
        PsxPadRaceFrame(false, defaults);
        PsxVBlank();
        CHECK_EQ_INT(PsxPadContext(), PAD_CONTEXT_MENU);
        key(SDL_SCANCODE_S, true);
        PsxVBlank();
        CHECK_EQ_INT(PsxPadButtons(0), 0);
        key(SDL_SCANCODE_S, false);
        /* the game stops calling (the race is over): the menus are back a few fields later */
        PsxPadRaceFrame(true, defaults);
        PsxVBlank();
        CHECK_EQ_INT(PsxPadContext(), PAD_CONTEXT_RACE);
        for (i = 0; i < 6; i++) {
            PsxVBlank();
        }
        CHECK_EQ_INT(PsxPadContext(), PAD_CONTEXT_MENU);
    }
    /* Enter pauses the race and must not also accept the first entry of the pause menu: a key held
       through the change of context stays quiet until it is released */
    PsxPadRaceFrame(true, (const uint16_t[PAD_ACTIONS]){PAD_TRIANGLE, PAD_CIRCLE, PAD_SQUARE, PAD_R2, PAD_L2, PAD_CROSS});
    PsxVBlank();
    key(SDL_SCANCODE_RETURN, true);
    key(SDL_SCANCODE_RIGHT, true);
    PsxVBlank();
    CHECK_EQ_INT(PsxPadButtons(0), PAD_START | PAD_RIGHT);
    PsxPadRaceFrame(false, (const uint16_t[PAD_ACTIONS]){PAD_TRIANGLE, PAD_CIRCLE, PAD_SQUARE, PAD_R2, PAD_L2, PAD_CROSS});
    PsxVBlank();
    CHECK_EQ_INT(PsxPadButtons(0), PAD_RIGHT); /* Enter is held over; the arrow means the same in both */
    key(SDL_SCANCODE_RETURN, false);
    key(SDL_SCANCODE_RIGHT, false);
    key(SDL_SCANCODE_RETURN, true);
    PsxVBlank();
    CHECK_EQ_INT(PsxPadButtons(0), PAD_CROSS | PAD_START);
    key(SDL_SCANCODE_RETURN, false);
    /* combinations, and a key released while another is held */
    key(SDL_SCANCODE_Q, true);
    key(SDL_SCANCODE_LEFT, true);
    key(SDL_SCANCODE_SPACE, true);
    PsxVBlank();
    CHECK_EQ_INT(PsxPadButtons(0), PAD_L1 | PAD_LEFT | PAD_CIRCLE);
    key(SDL_SCANCODE_LEFT, false);
    PsxVBlank();
    CHECK_EQ_INT(PsxPadButtons(0), PAD_L1 | PAD_CIRCLE);
    key(SDL_SCANCODE_Q, false);
    key(SDL_SCANCODE_SPACE, false);
    /* losing window focus releases every key */
    key(SDL_SCANCODE_SPACE, true);
    {
        SDL_Event e;

        memset(&e, 0, sizeof(e));
        e.type = SDL_EVENT_WINDOW_FOCUS_LOST;
        PsxInputEvent(&e);
    }
    CHECK_EQ_INT(PsxPadButtons(0), 0);
    /* NeGcon on the keyboard: twist from the arrow keys, S is button I, Space button II (in a race) */
    PsxPadSetType(0, PAD_NEGCON);
    PsxPadRaceFrame(true, (const uint16_t[PAD_ACTIONS]){PAD_TRIANGLE, PAD_CIRCLE, PAD_SQUARE, PAD_R2, PAD_L2, PAD_CROSS});
    key(SDL_SCANCODE_LEFT, true);
    key(SDL_SCANCODE_S, true);
    PsxVBlank();
    p = packet(buf1);
    CHECK_EQ_INT(p.header, 0x2300);
    CHECK_EQ_INT(p.steering, 0x00);
    CHECK_EQ_INT(p.analog[0], 0xFF);
    key(SDL_SCANCODE_LEFT, false);
    key(SDL_SCANCODE_RIGHT, true);
    PsxVBlank();
    CHECK_EQ_INT(packet(buf1).steering, 0xFF);
    test_io_stop();
    unlink(script);
    return test_report("pad");
}
