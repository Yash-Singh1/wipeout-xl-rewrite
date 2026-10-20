/* libpad as the game uses it: the BIOS pad driver. InitPAD hands the driver two 8-byte
   buffers (one per port); once per vertical blank the driver writes into them

       byte 0   0x00 if a pad answered, 0xFF if none
       byte 1   pad type: 0x41 digital pad, 0x23 NeGcon (high nibble the type, low the data size)
       byte 2,3 buttons, 0 = pressed (bit layout in pad.h)
       byte 4-7 NeGcon only: twist, button I, button II, left shoulder

   which the game reads as PadPacket (game/include/pad.h). The game supports exactly these
   two controllers. Test scripts (see PsxPadLoadScript) replace live input:

       # field port buttons...
       300 0 CROSS+START
       360 0 none
       400 0 0x4008
       500 0 twist=64 i=255

   Each line sets what the port holds from that video field on. */
#include <ctype.h>
#include <stdlib.h>
#include <string.h>

#include "internal.h"
#include "pad.h"

#ifdef _WIN32
#define strtok_r strtok_s
#endif

#define PORTS 2

typedef struct Port {
    PadState state[PAD_SRC_COUNT];
    bool connected[PAD_SRC_COUNT];
    PadType type;
} Port;

typedef struct ScriptEntry {
    uint64_t field;
    int port;
    PadState state;
} ScriptEntry;

static Port ports[PORTS];
static uint8_t *buffers[PORTS];
static int bufferLengths[PORTS];
static bool driverRunning;
static ScriptEntry *script;
static int scriptCount, scriptNext;
static bool scriptActive;

/* The race frame the game reported last (its field), whether the ship was steerable, and the
   action bindings. */
#define RACE_FRAME_LIMIT 4 /* fields: a race frame takes two, a hitch a few more */
static uint64_t currentField, raceField;
static bool raceSteering;
static uint16_t raceBindings[PAD_ACTIONS];
static PadDevice lastDevice;

/* The default layout (InitPadSettings): the actions on a digital pad. */
static const uint16_t defaultBindings[PAD_ACTIONS] = {
    PAD_TRIANGLE, PAD_CIRCLE, PAD_SQUARE, PAD_R2, PAD_L2, PAD_CROSS};

#ifndef PSX_NO_SDL
void PsxPadSdlInit(void);
void PsxPadSdlShutdown(void);
void PsxPadSdlField(void);
#endif

void PsxPadRaceFrame(bool steering, const uint16_t bindings[PAD_ACTIONS])
{
    raceField = currentField;
    raceSteering = steering;
    memcpy(raceBindings, bindings, sizeof(raceBindings));
}

PadContext PsxPadContext(void)
{
    return raceSteering && currentField - raceField <= RACE_FRAME_LIMIT ? PAD_CONTEXT_RACE : PAD_CONTEXT_MENU;
}

uint16_t PsxPadActionButton(int action)
{
    uint16_t button;

    if (action < 0 || action >= PAD_ACTIONS) {
        return 0;
    }
    if (ports[0].type == PAD_NEGCON) {
        return defaultBindings[action]; /* the keyboard's NeGcon mapping (pad_sdl.c) has its own layout */
    }
    /* the game binds a digital pad's actions to buttons; 0x8FF0 and up are analogue axes */
    button = raceBindings[action];
    return button && button < 0x8FF0 ? button : defaultBindings[action];
}

void PsxPadNoteDevice(PadDevice device)
{
    lastDevice = device;
}

PadDevice PsxPadLastDevice(void)
{
    return lastDevice;
}

void PsxPadSetSource(int port, PadSource source, const PadState *state, bool connected)
{
    if (port < 0 || port >= PORTS) {
        return;
    }
    ports[port].state[source] = *state;
    ports[port].connected[source] = connected;
}

void PsxPadSetType(int port, PadType type)
{
    if (port >= 0 && port < PORTS) {
        ports[port].type = type;
    }
}

PadType PsxPadGetType(int port)
{
    return port >= 0 && port < PORTS ? ports[port].type : PAD_DIGITAL;
}

static uint8_t deviation(uint8_t twist)
{
    return twist >= 0x80 ? (uint8_t)(twist - 0x80) : (uint8_t)(0x80 - twist);
}

bool PsxPadGet(int port, PadState *out)
{
    const Port *p;
    int first, last, s;
    bool any = false;

    memset(out, 0, sizeof(*out));
    out->twist = 0x80;
    if (port < 0 || port >= PORTS) {
        return false;
    }
    p = &ports[port];
    /* a script, when loaded, is the only source */
    first = scriptActive ? PAD_SRC_SCRIPT : 0;
    last = scriptActive ? PAD_SRC_SCRIPT : PAD_SRC_SCRIPT - 1;
    for (s = first; s <= last; s++) {
        const PadState *st = &p->state[s];

        if (!p->connected[s]) {
            continue;
        }
        any = true;
        out->buttons |= st->buttons;
        if (deviation(st->twist) > deviation(out->twist)) {
            out->twist = st->twist;
        }
        if (st->analogI > out->analogI) out->analogI = st->analogI;
        if (st->analogII > out->analogII) out->analogII = st->analogII;
        if (st->analogL > out->analogL) out->analogL = st->analogL;
    }
    return any;
}

uint16_t PsxPadButtons(int port)
{
    PadState st;

    return PsxPadGet(port, &st) ? st.buttons : 0;
}

void PsxPadFillBuffer(int port, uint8_t *buffer, int length)
{
    PadState st;
    uint16_t held;
    int i;

    for (i = 0; i < length; i++) {
        buffer[i] = 0;
    }
    if (length < 4) {
        return;
    }
    if (!PsxPadGet(port, &st)) {
        buffer[0] = 0xFF;
        buffer[1] = 0xFF;
        return;
    }
    held = st.buttons;
    buffer[0] = 0x00;
    if (ports[port].type == PAD_NEGCON && length >= 8) {
        /* NeGcon: start, the d-pad, R, A and B; the twist and three analogue buttons */
        held &= PAD_START | PAD_UP | PAD_RIGHT | PAD_DOWN | PAD_LEFT | PAD_R1 | PAD_TRIANGLE | PAD_CIRCLE;
        buffer[1] = 0x23;
        buffer[4] = st.twist;
        buffer[5] = st.analogI;
        buffer[6] = st.analogII;
        buffer[7] = st.analogL;
    } else {
        buffer[1] = 0x41;
    }
    buffer[2] = (uint8_t)~held;
    buffer[3] = (uint8_t)~(held >> 8);
}

/* ---- the BIOS driver --------------------------------------------------------------------------- */

void InitPAD(void *buffer1, int length1, void *buffer2, int length2)
{
    buffers[0] = buffer1;
    bufferLengths[0] = length1;
    buffers[1] = buffer2;
    bufferLengths[1] = length2;
}

int StartPAD(void)
{
    driverRunning = true;
    return 1;
}

void StopPAD(void)
{
    driverRunning = false;
}

void ChangeClearPAD(int mode)
{
    (void)mode;
}

void PsxPadField(uint64_t field)
{
    int port;

    currentField = field;
#ifndef PSX_NO_SDL
    PsxPadSdlField();
#endif
    while (scriptNext < scriptCount && script[scriptNext].field <= field) {
        const ScriptEntry *e = &script[scriptNext++];

        PsxPadSetSource(e->port, PAD_SRC_SCRIPT, &e->state, true);
    }
    if (!driverRunning) {
        return;
    }
    for (port = 0; port < PORTS; port++) {
        if (buffers[port]) {
            PsxPadFillBuffer(port, buffers[port], bufferLengths[port]);
        }
    }
}

/* ---- scripts ----------------------------------------------------------------------------------- */

static const struct { const char *name; uint16_t bit; } buttonNames[] = {
    {"SELECT", PAD_SELECT}, {"L3", PAD_L3}, {"R3", PAD_R3}, {"START", PAD_START},
    {"UP", PAD_UP}, {"RIGHT", PAD_RIGHT}, {"DOWN", PAD_DOWN}, {"LEFT", PAD_LEFT},
    {"L2", PAD_L2}, {"R2", PAD_R2}, {"L1", PAD_L1}, {"R1", PAD_R1},
    {"TRIANGLE", PAD_TRIANGLE}, {"CIRCLE", PAD_CIRCLE}, {"CROSS", PAD_CROSS}, {"SQUARE", PAD_SQUARE}};

static bool parse_buttons(char *spec, uint16_t *buttons)
{
    char *token;

    *buttons = 0;
    if (!strcmp(spec, "none")) {
        return true;
    }
    if (spec[0] == '0' && (spec[1] == 'x' || spec[1] == 'X')) {
        *buttons = (uint16_t)strtoul(spec, NULL, 16);
        return true;
    }
    for (token = strtok(spec, "+"); token; token = strtok(NULL, "+")) {
        size_t i;
        bool found = false;

        for (i = 0; i < sizeof(buttonNames) / sizeof(buttonNames[0]); i++) {
            if (!strcmp(token, buttonNames[i].name)) {
                *buttons |= buttonNames[i].bit;
                found = true;
            }
        }
        if (!found) {
            return false;
        }
    }
    return true;
}

bool PsxPadLoadScript(const char *text, char *error, int errorSize)
{
    char *copy = malloc(strlen(text) + 1), *line, *save = NULL;
    int lineNumber = 0;
    PadState held[PORTS];

    free(script);
    script = NULL;
    scriptCount = scriptNext = 0;
    memset(held, 0, sizeof(held));
    held[0].twist = held[1].twist = 0x80;
    if (!copy) {
        return false;
    }
    strcpy(copy, text);
    for (line = strtok_r(copy, "\n", &save); line; line = strtok_r(NULL, "\n", &save)) {
        char *hash = strchr(line, '#'), *word, *tail = NULL;
        unsigned long long field;
        int port;
        ScriptEntry entry;

        lineNumber++;
        if (hash) {
            *hash = 0;
        }
        word = strtok_r(line, " \t\r", &tail);
        if (!word) {
            continue;
        }
        field = strtoull(word, NULL, 10);
        word = strtok_r(NULL, " \t\r", &tail);
        if (!word || (port = atoi(word)) < 0 || port >= PORTS) {
            goto bad;
        }
        while ((word = strtok_r(NULL, " \t\r", &tail)) != NULL) {
            if (!strncmp(word, "twist=", 6)) {
                held[port].twist = (uint8_t)strtoul(word + 6, NULL, 0);
            } else if (!strncmp(word, "i=", 2)) {
                held[port].analogI = (uint8_t)strtoul(word + 2, NULL, 0);
            } else if (!strncmp(word, "ii=", 3)) {
                held[port].analogII = (uint8_t)strtoul(word + 3, NULL, 0);
            } else if (!strncmp(word, "l=", 2)) {
                held[port].analogL = (uint8_t)strtoul(word + 2, NULL, 0);
            } else if (!parse_buttons(word, &held[port].buttons)) {
                goto bad;
            }
        }
        entry.field = field;
        entry.port = port;
        entry.state = held[port];
        script = realloc(script, (size_t)(scriptCount + 1) * sizeof(*script));
        if (!script) {
            free(copy);
            return false;
        }
        script[scriptCount++] = entry;
        continue;
    bad:
        if (error && errorSize > 0) {
            snprintf(error, (size_t)errorSize, "pad script line %d: syntax error", lineNumber);
        }
        free(copy);
        free(script);
        script = NULL;
        scriptCount = 0;
        return false;
    }
    free(copy);
    scriptActive = true;
    /* entries must be in field order for the sweep in PsxPadField */
    {
        int i, j;

        for (i = 1; i < scriptCount; i++) {
            ScriptEntry e = script[i];

            for (j = i - 1; j >= 0 && script[j].field > e.field; j--) {
                script[j + 1] = script[j];
            }
            script[j + 1] = e;
        }
    }
    {
        /* both ports of a script are plugged in only if it mentions them */
        int p;
        bool used[PORTS] = {false, false};

        for (p = 0; p < scriptCount; p++) {
            used[script[p].port] = true;
        }
        used[0] = true;
        for (p = 0; p < PORTS; p++) {
            PadState idle;

            memset(&idle, 0, sizeof(idle));
            idle.twist = 0x80;
            PsxPadSetSource(p, PAD_SRC_SCRIPT, &idle, used[p]);
        }
    }
    return true;
}

void PsxPadInit(const char *scriptPath)
{
    PadState idle;
    int port;

    memset(ports, 0, sizeof(ports));
    currentField = raceField = 0;
    raceSteering = false;
    lastDevice = PAD_DEVICE_NONE;
    memcpy(raceBindings, defaultBindings, sizeof(raceBindings));
    memset(&idle, 0, sizeof(idle));
    idle.twist = 0x80;
    for (port = 0; port < PORTS; port++) {
        int s;

        ports[port].type = PAD_DIGITAL;
        for (s = 0; s < PAD_SRC_COUNT; s++) {
            ports[port].state[s] = idle;
        }
    }
    /* the keyboard is always plugged into port 0 */
    PsxPadSetSource(0, PAD_SRC_KEYBOARD, &idle, true);
    {
        const char *type = getenv("WIPEOUT_PAD_TYPE");

        if (type && !strcmp(type, "negcon")) {
            ports[0].type = PAD_NEGCON;
        }
    }
    if (scriptPath) {
        FILE *f = fopen(scriptPath, "rb");

        if (f) {
            long size;
            char *text;

            fseek(f, 0, SEEK_END);
            size = ftell(f);
            fseek(f, 0, SEEK_SET);
            text = malloc((size_t)size + 1);
            if (text && fread(text, 1, (size_t)size, f) == (size_t)size) {
                char error[128] = "";

                text[size] = 0;
                if (!PsxPadLoadScript(text, error, sizeof(error))) {
                    PortLog("%s\n", error);
                }
            }
            free(text);
            fclose(f);
        } else {
            PortLog("pad script %s: cannot open\n", scriptPath);
        }
    }
#ifndef PSX_NO_SDL
    PsxPadSdlInit();
#endif
}

void PsxPadShutdown(void)
{
#ifndef PSX_NO_SDL
    PsxPadSdlShutdown();
#endif
    free(script);
    script = NULL;
    scriptCount = scriptNext = 0;
    scriptActive = false;
    driverRunning = false;
    buffers[0] = buffers[1] = NULL;
}
