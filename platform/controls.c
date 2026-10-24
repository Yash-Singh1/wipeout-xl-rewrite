/* The player's controls as the game sees them: the context the keyboard is in, the names of the
   buttons of the device in the game's text, and (for tests) scripted key and gamepad events.

   WIPEOUT_KEYS=file replays input through SDL's own event queue, so the keys and gamepad buttons
   take the path real ones do. Lines, from the video field given on, keys by SDL's names:
       120 down Return          125 up Return           (a key; "Left Alt", "Escape", "F1", ...)
       200 pad attach xbox      (a virtual gamepad: xbox, ps5, switch, saturn or generic)
       210 pad down a           215 pad up a            (an SDL gamepad button: a is the south one, then
                                                          b east, x west, y north, start, back, dpup, ...)
       220 pad axis leftx 30000                          (an axis; triggers 0..32767)
       300 pad detach
       ("pad2 ..." is a second virtual gamepad, the same way)
       400 focus lost           450 focus gained        (the window loses or regains the input focus) */
#include <SDL3/SDL.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "controls.h"
#include "platform.h"
#include "psx/pad.h"

/* ---- the game's calls ---- */

void PortRaceControls(int steering, const unsigned short *bindings)
{
    uint16_t actions[PAD_ACTIONS];
    int i;

    for (i = 0; i < PAD_ACTIONS; i++) {
        actions[i] = bindings[i];
    }
    PsxPadRaceFrame(steering != 0, actions);
}

/* ---- names of buttons ---- */

typedef struct ButtonNames {
    const char *start, *l1, *l2, *r1, *r2, *accept;
} ButtonNames;

/* The font has capitals, digits and a few marks: the Switch's plus is spelled out. */
static const ButtonNames keyboardNames = {"ENTER", "Q", "A", "E", "D", "ENTER"};
static const ButtonNames xboxNames = {"MENU", "LB", "LT", "RB", "RT", "A"};
static const ButtonNames nintendoNames = {"PLUS", "L", "ZL", "R", "ZR", "B"};
/* (a Saturn pad's C and Z go to different places on each adapter: those keep the console's names) */
static const ButtonNames saturnNames = {"START", "L", NULL, "R", NULL, "A"};

char *PortButtonLabel(PortButton button, char *console)
{
    const ButtonNames *names;
    const char *name;

    switch (PsxPadLastDevice()) {
    case PAD_DEVICE_KEYBOARD: names = &keyboardNames; break;
    case PAD_DEVICE_XBOX: names = &xboxNames; break;
    case PAD_DEVICE_NINTENDO: names = &nintendoNames; break;
    case PAD_DEVICE_SATURN: names = &saturnNames; break;
    default: return console;
    }
    switch (button) {
    case PORT_BUTTON_START: name = names->start; break;
    case PORT_BUTTON_L1: name = names->l1; break;
    case PORT_BUTTON_L2: name = names->l2; break;
    case PORT_BUTTON_R1: name = names->r1; break;
    case PORT_BUTTON_R2: name = names->r2; break;
    case PORT_BUTTON_ACCEPT: name = names->accept; break;
    default: return console;
    }
    return name ? (char *)name : console;
}

/* ---- scripted input ---- */

typedef enum { STEP_KEY, STEP_ATTACH, STEP_DETACH, STEP_BUTTON, STEP_AXIS, STEP_FOCUS, STEP_TOUCH } StepKind;

typedef struct Step {
    uint64_t field;
    StepKind kind;
    int pad;      /* which virtual gamepad (0 or 1) */
    bool down;
    int code;     /* scancode, button, axis or pad family; a finger */
    int value;    /* a finger: SDL_EVENT_FINGER_DOWN, _MOTION or _UP */
    float x, y;   /* ... where, in fractions of the window */
} Step;

static Step *steps;
static int stepCount, stepNext;
static bool stepsLoaded;
static SDL_JoystickID virtualIds[2];
static SDL_Joystick *virtualJoysticks[2];

static const struct { const char *name; uint16_t vendor, product; } families[] = {
    {"xbox", 0x045E, 0x02EA}, {"ps5", 0x054C, 0x0CE6}, {"switch", 0x057E, 0x2009}, {"generic", 0x1234, 0x5678},
    {"saturn", 0x04B4, 0x010A}};

static void AddStep(Step step)
{
    steps = SDL_realloc(steps, (size_t)(stepCount + 1) * sizeof(*steps));
    steps[stepCount++] = step;
}

static void ParseLine(char *line, int number)
{
    char verb[16], word[32], rest[64];
    unsigned long long field;
    Step step;
    int n = (int)strlen(line);

    memset(&step, 0, sizeof(step));
    if (line[0] == '#' || sscanf(line, "%llu %15s %n", &field, verb, &n) < 2) {
        return;
    }
    step.field = field;
    SDL_strlcpy(rest, line + n, sizeof(rest));
    rest[strcspn(rest, "\r\n")] = 0;
    if (!strcmp(verb, "down") || !strcmp(verb, "up")) {
        step.kind = STEP_KEY;
        step.down = verb[0] == 'd';
        step.code = (int)SDL_GetScancodeFromName(rest);
        if (step.code == SDL_SCANCODE_UNKNOWN) {
            fprintf(stderr, "keys: line %d: no key called \"%s\"\n", number, rest);
            return;
        }
    } else if (!strcmp(verb, "touch")) {
        /* touch down|move ID X Y, touch up ID: a finger on the window (X and Y fractions of its size) */
        step.kind = STEP_TOUCH;
        if (sscanf(rest, "%31s %d %f %f", word, &step.code, &step.x, &step.y) < 2 ||
            (strcmp(word, "down") && strcmp(word, "move") && strcmp(word, "up"))) {
            fprintf(stderr, "keys: line %d: touch down|move|up ID X Y\n", number);
            return;
        }
        step.value = word[0] == 'd' ? SDL_EVENT_FINGER_DOWN : word[0] == 'm' ? SDL_EVENT_FINGER_MOTION : SDL_EVENT_FINGER_UP;
    } else if (!strcmp(verb, "focus") && (!strcmp(rest, "lost") || !strcmp(rest, "gained"))) {
        step.kind = STEP_FOCUS;
        step.down = rest[0] == 'g';
    } else if ((!strcmp(verb, "pad") || !strcmp(verb, "pad2")) && (step.pad = verb[3] == '2', 1) && (n = (int)strlen(rest), sscanf(rest, "%31s %n", word, &n) >= 1)) {
        char *tail = rest + n;

        if (!strcmp(word, "attach")) {
            size_t i;

            step.kind = STEP_ATTACH;
            for (i = 0; i < sizeof(families) / sizeof(families[0]); i++) {
                if (!strcmp(tail, families[i].name)) {
                    step.code = (int)i;
                }
            }
        } else if (!strcmp(word, "detach")) {
            step.kind = STEP_DETACH;
        } else if (!strcmp(word, "down") || !strcmp(word, "up")) {
            step.kind = STEP_BUTTON;
            step.down = word[0] == 'd';
            step.code = (int)SDL_GetGamepadButtonFromString(tail);
            if (step.code == SDL_GAMEPAD_BUTTON_INVALID) {
                fprintf(stderr, "keys: line %d: no gamepad button \"%s\"\n", number, tail);
                return;
            }
        } else if (!strcmp(word, "axis")) {
            char axis[24];

            step.kind = STEP_AXIS;
            if (sscanf(tail, "%23s %d", axis, &step.value) != 2 ||
                (step.code = (int)SDL_GetGamepadAxisFromString(axis)) == SDL_GAMEPAD_AXIS_INVALID) {
                fprintf(stderr, "keys: line %d: bad axis\n", number);
                return;
            }
        } else {
            fprintf(stderr, "keys: line %d: unknown pad command\n", number);
            return;
        }
    } else {
        fprintf(stderr, "keys: line %d: unknown command %s\n", number, verb);
        return;
    }
    AddStep(step);
}

static void LoadSteps(void)
{
    const char *path = SDL_getenv("WIPEOUT_KEYS");
    char line[256];
    int number = 0;
    FILE *f;

    stepsLoaded = true;
    if (!path) {
        return;
    }
    f = fopen(path, "r");
    if (!f) {
        fprintf(stderr, "keys: cannot read %s\n", path);
        return;
    }
    while (fgets(line, sizeof(line), f)) {
        ParseLine(line, ++number);
    }
    fclose(f);
}

static void Run(const Step *step)
{
    SDL_Event event;

    memset(&event, 0, sizeof(event));
    switch (step->kind) {
    case STEP_KEY:
        event.type = step->down ? SDL_EVENT_KEY_DOWN : SDL_EVENT_KEY_UP;
        event.key.scancode = (SDL_Scancode)step->code;
        event.key.key = SDL_GetKeyFromScancode((SDL_Scancode)step->code, SDL_KMOD_NONE, false); /* as a keyboard's */
        {
            /* ... and to the game's window (the debug console takes only its window's keys) */
            int windows = 0;
            SDL_Window **list = SDL_GetWindows(&windows);

            event.key.windowID = list && windows ? SDL_GetWindowID(list[0]) : 0;
            SDL_free(list);
        }
        event.key.down = step->down;
        SDL_PushEvent(&event);
        break;
    case STEP_TOUCH: {
        static float lastX[16], lastY[16];
        int windows = 0, finger = step->code & 15;
        SDL_Window **list = SDL_GetWindows(&windows);

        event.type = (Uint32)step->value;
        event.tfinger.touchID = 1;
        event.tfinger.fingerID = (SDL_FingerID)(step->code + 1);
        if (step->value == SDL_EVENT_FINGER_UP) {
            event.tfinger.x = lastX[finger];
            event.tfinger.y = lastY[finger];
        } else {
            event.tfinger.x = lastX[finger] = step->x;
            event.tfinger.y = lastY[finger] = step->y;
        }
        event.tfinger.pressure = step->value == SDL_EVENT_FINGER_UP ? 0.0f : 1.0f;
        event.tfinger.windowID = list && windows ? SDL_GetWindowID(list[0]) : 0;
        SDL_free(list);
        SDL_PushEvent(&event);
        break;
    }
    case STEP_ATTACH:
        if (!virtualIds[step->pad]) {
            SDL_VirtualJoystickDesc desc;

            SDL_INIT_INTERFACE(&desc);
            desc.type = SDL_JOYSTICK_TYPE_GAMEPAD;
            desc.vendor_id = families[step->code].vendor;
            desc.product_id = families[step->code].product;
            desc.naxes = SDL_GAMEPAD_AXIS_COUNT;
            desc.nbuttons = SDL_GAMEPAD_BUTTON_COUNT;
            desc.name = families[step->code].name;
            desc.button_mask = ~0u;
            desc.axis_mask = ~0u;
            virtualIds[step->pad] = SDL_AttachVirtualJoystick(&desc);
            virtualJoysticks[step->pad] = virtualIds[step->pad] ? SDL_OpenJoystick(virtualIds[step->pad]) : NULL;
        }
        break;
    case STEP_DETACH:
        if (virtualIds[step->pad]) {
            SDL_CloseJoystick(virtualJoysticks[step->pad]);
            SDL_DetachVirtualJoystick(virtualIds[step->pad]);
            virtualIds[step->pad] = 0;
            virtualJoysticks[step->pad] = NULL;
        }
        break;
    case STEP_BUTTON:
        if (virtualJoysticks[step->pad]) {
            SDL_SetJoystickVirtualButton(virtualJoysticks[step->pad], step->code, step->down);
        }
        break;
    case STEP_AXIS:
        if (virtualJoysticks[step->pad]) {
            SDL_SetJoystickVirtualAxis(virtualJoysticks[step->pad], step->code, (Sint16)step->value);
        }
        break;
    case STEP_FOCUS:
        event.type = step->down ? SDL_EVENT_WINDOW_FOCUS_GAINED : SDL_EVENT_WINDOW_FOCUS_LOST;
        SDL_PushEvent(&event);
        break;
    }
}

/* Called before the event pump drains SDL's queue, once per video field. */
void PortInputScript(uint64_t field)
{
    if (!stepsLoaded) {
        LoadSteps();
    }
    while (stepNext < stepCount && steps[stepNext].field <= field) {
        Run(&steps[stepNext++]);
    }
}
