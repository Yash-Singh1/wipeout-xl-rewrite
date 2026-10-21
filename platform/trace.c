/* Test recordings: the drawing commands the game sends, in the console
   recorder's trace format, so a run can be compared with a recording made on
   the console (tools/lockstep.py), and scripted pad input in the recorder's
   own form, so both runs see the same buttons at the same pad reads.

   WIPEOUT_TRACE=FILE      write the trace (gzip when built with zlib and FILE
                           ends in .gz)
   WIPEOUT_PADS=pads.txt   the recording's pad script: lines "COUNT BUTTONS",
                           buttons active high in hex, applied from the
                           COUNT-th read of the fixed pad on */
#include <SDL3/SDL.h>
#include <dlfcn.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef PORT_HAVE_ZLIB
#include <zlib.h>
#endif

#include "platform.h"
#include "trace.h"

enum {
    KIND_BOUNDARY = 1,
    KIND_RETURN = 2,
    KIND_GP0 = 3,
    KIND_GP1 = 4,
    KIND_PAD = 11,
    KIND_STATE = 14,
    KIND_END = 18
};

static FILE *file;
#ifdef PORT_HAVE_ZLIB
static gzFile gz;
#endif
static uint32_t iteration;

static void Write(const void *data, size_t size)
{
#ifdef PORT_HAVE_ZLIB
    if (gz) {
        gzwrite(gz, data, (unsigned)size);
        return;
    }
#endif
    fwrite(data, 1, size, file);
}

static void Event(uint32_t kind, const uint32_t *words, int count)
{
    uint64_t lines = PsxScanlines();
    uint32_t header[7];

    header[0] = kind;
    header[1] = (uint32_t)count * 4;
    header[2] = iteration;
    header[3] = (uint32_t)PsxFieldCount();
    header[4] = (uint32_t)lines;
    header[5] = (uint32_t)(lines >> 32);
    header[6] = 0;
    Write(header, sizeof(header));
    if (count) {
        Write(words, (size_t)count * 4);
    }
}

static void Close(void)
{
    if (!PortTracing()) {
        return;
    }
    Event(KIND_END, NULL, 0);
#ifdef PORT_HAVE_ZLIB
    if (gz) {
        gzclose(gz);
        gz = NULL;
    }
#endif
    if (file) {
        fclose(file);
        file = NULL;
    }
}

int PortTracing(void)
{
#ifdef PORT_HAVE_ZLIB
    if (gz) {
        return 1;
    }
#endif
    return file != NULL;
}

/* ---- the pad script ---- */

typedef struct PadStep {
    uint32_t count;
    uint16_t buttons;
} PadStep;

static PadStep *padSteps;
static int padStepCount;
static uint32_t padReads;

static void LoadPads(const char *path)
{
    FILE *f = fopen(path, "r");
    unsigned long count;
    unsigned buttons;
    int capacity = 0;

    if (!f) {
        fprintf(stderr, "trace: cannot read the pad script %s\n", path);
        exit(1);
    }
    while (fscanf(f, "%lu %x", &count, &buttons) == 2) {
        if (padStepCount == capacity) {
            capacity = capacity ? capacity * 2 : 64;
            padSteps = SDL_realloc(padSteps, (size_t)capacity * sizeof(*padSteps));
        }
        padSteps[padStepCount].count = (uint32_t)count;
        padSteps[padStepCount].buttons = (uint16_t)buttons;
        padStepCount++;
    }
    fclose(f);
}

/* A pad read as the recorder logs it: the reader (0 fixed, 1 game), the read
   count, the packet's two words. */
static void TracePad(uint32_t reader, const unsigned char *packet)
{
    uint32_t words[5];

    if (!PortTracing()) {
        return;
    }
    words[0] = reader;
    words[1] = padReads;
    words[2] = 0;
    words[3] = (uint32_t)packet[0] | (uint32_t)packet[1] << 8 | (uint32_t)packet[2] << 16 | (uint32_t)packet[3] << 24;
    words[4] = (uint32_t)packet[4] | (uint32_t)packet[5] << 8 | (uint32_t)packet[6] << 16 | (uint32_t)packet[7] << 24;
    Event(KIND_PAD, words, 5);
}

int PortPadScripted(void)
{
    return padSteps != NULL;
}

/* The script's buttons for the current read count, written into the packet. */
static void ApplyScript(unsigned char *packet)
{
    uint16_t buttons = 0;
    int i;

    for (i = 0; i < padStepCount && padSteps[i].count <= padReads; i++) {
        buttons = padSteps[i].buttons;
    }
    /* a digital pad's packet: status, type 0x41, buttons active low */
    packet[0] = 0x00;
    packet[1] = 0x41;
    packet[2] = (unsigned char)~buttons;
    packet[3] = (unsigned char)(~buttons >> 8);
}

/* ---- replay recording (--record): what makes a session repeat exactly ----

   pads.txt (the buttons at each fixed pad read, when they change), vcounts.txt (every scanline counter
   read: the frame timing and the random seeds), loads.txt (the loading screens' lengths), and copies of
   the memory card and the settings at the start. tools/replay.sh plays the folder back. */
static FILE *recordPads, *recordVcounts, *recordLoads;
static int recordedButtons = -1;
static char recordDir[1024];

static void RecordClose(void)
{
    if (recordPads) fclose(recordPads);
    if (recordVcounts) fclose(recordVcounts);
    if (recordLoads) fclose(recordLoads);
    recordPads = recordVcounts = recordLoads = NULL;
}

const char *PortRecordDir(void)
{
    return recordDir[0] ? recordDir : NULL;
}

int PortRecordStart(const char *dir, const char *saveDir, const char *settingsPath)
{
    char path[1100];
    size_t size;
    void *data;

    SDL_CreateDirectory(dir);
    SDL_strlcpy(recordDir, dir, sizeof(recordDir));
    SDL_snprintf(path, sizeof(path), "%s/pads.txt", dir);
    recordPads = fopen(path, "w");
    SDL_snprintf(path, sizeof(path), "%s/vcounts.txt", dir);
    recordVcounts = fopen(path, "w");
    SDL_snprintf(path, sizeof(path), "%s/loads.txt", dir);
    recordLoads = fopen(path, "w");
    if (!recordPads || !recordVcounts || !recordLoads) {
        RecordClose();
        recordDir[0] = 0;
        return 0;
    }
    /* the starting state: the memory card and the settings */
    SDL_snprintf(path, sizeof(path), "%smemcard0.mcr", saveDir ? saveDir : "");
    if ((data = SDL_LoadFile(path, &size))) {
        SDL_snprintf(path, sizeof(path), "%s/memcard0.mcr", dir);
        SDL_SaveFile(path, data, size);
        SDL_free(data);
    }
    if (settingsPath && (data = SDL_LoadFile(settingsPath, &size))) {
        SDL_snprintf(path, sizeof(path), "%s/wipeoutxl.ini", dir);
        SDL_SaveFile(path, data, size);
        SDL_free(data);
    }
    atexit(RecordClose);
    return 1;
}

void PortRecordVcount(int value)
{
    if (recordVcounts) {
        fprintf(recordVcounts, "%d\n", value);
    }
}

static void RecordPad(const unsigned char *packet)
{
    int buttons;

    if (!recordPads) {
        return;
    }
    buttons = (uint16_t)~(packet[2] | packet[3] << 8);
    if (packet[1] != 0x41 && packet[1] != 0x23) {
        buttons = 0; /* nothing connected */
    }
    if (buttons != recordedButtons) {
        fprintf(recordPads, "%u %04x\n", padReads, buttons);
        recordedButtons = buttons;
    }
}

/* buildFixedPad: the read the script counts. */
void PortScriptFixedPad(unsigned char *packet)
{
    if (padSteps) {
        ApplyScript(packet);
    }
    RecordPad(packet);
    TracePad(0, packet);
    padReads++;
}

/* buildGameKeyPad reads the same packet; the console recorder injects the
   script there too, at the current count, without advancing it. */
void PortScriptGamePad(unsigned char *packet)
{
    if (padSteps) {
        ApplyScript(packet);
    }
    TracePad(1, packet);
}

/* ---- scripted writes ---- */

/* game/src/porttest.c */
void PortTestWall(void *ship, int value);
int PortTestPit(void *ship);
int PortTestPlayerState(int *x, int *y, int *z);
const void *PortTestPlayerShip(int *size);

typedef struct ScriptWrite {
    uint32_t count;      /* applied at the first UpdateShips from this pad read on */
    char target[64];     /* ship, config, owner:GLOBAL (indexed by the player), GLOBAL, wall, pit */
    uint32_t offset, width;
    char value[64];      /* a number, or @NAME: the address of a function or global */
    int done;
} ScriptWrite;

static ScriptWrite *scriptWrites;
static int scriptWriteCount;

static void LoadWrites(const char *path)
{
    FILE *f = fopen(path, "r");
    ScriptWrite w;
    int capacity = 0;

    if (!f) {
        fprintf(stderr, "trace: cannot read the write script %s\n", path);
        exit(1);
    }
    memset(&w, 0, sizeof(w));
    while (fscanf(f, "%u %63s %u %u %63s", &w.count, w.target, &w.offset, &w.width, w.value) == 5) {
        if (scriptWriteCount == capacity) {
            capacity = capacity ? capacity * 2 : 64;
            scriptWrites = SDL_realloc(scriptWrites, (size_t)capacity * sizeof(*scriptWrites));
        }
        scriptWrites[scriptWriteCount++] = w;
    }
    fclose(f);
}

static void *Symbol(const char *name)
{
    void *address = dlsym(RTLD_DEFAULT, name);

    if (!address) {
        fprintf(stderr, "trace: write target %s not found\n", name);
        exit(1);
    }
    return address;
}

void PortScriptWrites(void *ship, void *config, int owner)
{
    int i;

    for (i = 0; i < scriptWriteCount; i++) {
        ScriptWrite *w = &scriptWrites[i];
        unsigned char *base;

        if (w->done || padReads < w->count) {
            continue;
        }
        w->done = 1;
        if (!strcmp(w->target, "wall")) {
            PortTestWall(ship, atoi(w->value));
            continue;
        }
        if (!strcmp(w->target, "pit")) {
            if (!PortTestPit(ship)) {
                fprintf(stderr, "trace: no red pit section on this track\n");
                exit(1);
            }
            continue;
        }
        if (!strcmp(w->target, "ship")) {
            base = ship;
        } else if (!strcmp(w->target, "config")) {
            base = config;
        } else if (!strncmp(w->target, "owner:", 6)) {
            base = (unsigned char *)Symbol(w->target + 6) + (size_t)owner * w->width;
        } else {
            base = Symbol(w->target);
        }
        if (w->value[0] == '@') {
            void *pointer = Symbol(w->value + 1);
            memcpy(base + w->offset, &pointer, sizeof(pointer));
        } else {
            uint32_t value = (uint32_t)strtoul(w->value, NULL, 0);

            if (w->width == 1) {
                base[w->offset] = (unsigned char)value;
            } else if (w->width == 2) {
                uint16_t v = (uint16_t)value;
                memcpy(base + w->offset, &v, 2);
            } else {
                memcpy(base + w->offset, &value, 4);
            }
        }
    }
}

/* ---- autopilot ---- */

/* WIPEOUT_AUTOPILOT=all or a pad-read count: autopilot is taken up again
   until that read. */
static int autopilotMode = -1;
static uint32_t autopilotUntil;

static void ReadAutopilot(void)
{
    const char *value = SDL_getenv("WIPEOUT_AUTOPILOT");

    autopilotMode = value && *value && strcmp(value, "0");
    autopilotUntil = autopilotMode && strcmp(value, "all") ? (uint32_t)strtoul(value, NULL, 0) : 0;
}

int PortAutopilot(void)
{
    if (autopilotMode < 0) {
        ReadAutopilot();
    }
    return autopilotMode && (!autopilotUntil || padReads < autopilotUntil);
}

int PortAutopilotPatched(void)
{
    if (autopilotMode < 0) {
        ReadAutopilot();
    }
    return autopilotMode;
}

/* ---- loading screens ---- */

static uint32_t *loadFrames;
static int loadCount, loadNext;
static uint32_t callbackSwaps, loadStart;

static void LoadLoads(const char *path)
{
    FILE *f = fopen(path, "r");
    unsigned value;
    int capacity = 0;

    if (!f) {
        fprintf(stderr, "trace: cannot read the loading script %s\n", path);
        exit(1);
    }
    while (fscanf(f, "%u", &value) == 1) {
        if (loadCount == capacity) {
            capacity = capacity ? capacity * 2 : 16;
            loadFrames = SDL_realloc(loadFrames, (size_t)capacity * sizeof(*loadFrames));
        }
        loadFrames[loadCount++] = value;
    }
    fclose(f);
}

void PortLoadingStarts(void)
{
    loadStart = callbackSwaps;
    PortLog("loading: starts at field %llu\n", (unsigned long long)PsxFieldCount());
}

void PortLoadingEnds(void)
{
    uint32_t target;
    int guard = 0;

    PortLog("loading: %u swaps, ends at field %llu\n", callbackSwaps - loadStart, (unsigned long long)PsxFieldCount());
    if (recordLoads) {
        fprintf(recordLoads, "%u\n", callbackSwaps - loadStart);
        fflush(recordLoads);
    }
    if (loadNext >= loadCount) {
        return;
    }
    target = loadFrames[loadNext++];
    if (callbackSwaps - loadStart > target) {
        PortLog("trace: load %d took %u frames, the recording %u\n", loadNext, callbackSwaps - loadStart, target);
    }
    while (callbackSwaps - loadStart < target && guard++ < 100000) {
        PsxVBlank();
    }
}

/* ---- recording ---- */

void PortTraceInit(void)
{
    const char *path = SDL_getenv("WIPEOUT_TRACE");
    const char *pads = SDL_getenv("WIPEOUT_PADS");
    const char *writes = SDL_getenv("WIPEOUT_WRITES");
    const char *loads = SDL_getenv("WIPEOUT_LOADS");
    static const unsigned char magic[12] = {'W', 'X', 'L', 'T', 'R', 'C', '0', '1', 1, 0, 0, 0};
    size_t length;

    if (pads) {
        LoadPads(pads);
    }
    if (writes) {
        LoadWrites(writes);
    }
    if (loads) {
        LoadLoads(loads);
    }
    if (!path) {
        return;
    }
    length = strlen(path);
#ifdef PORT_HAVE_ZLIB
    if (length > 3 && !strcmp(path + length - 3, ".gz")) {
        gz = gzopen(path, "wb1");
    } else
#endif
    {
        (void)length;
        file = fopen(path, "wb");
    }
    if (!PortTracing()) {
        fprintf(stderr, "trace: cannot write %s\n", path);
        exit(1);
    }
    Write(magic, sizeof(magic));
    atexit(Close);
}

void PortTraceGp0(const uint32_t *words, int count)
{
    if (PortTracing() && count > 0) {
        Event(KIND_GP0, words, count);
    }
}

void PortTraceEvent(uint32_t kind, const uint32_t *words, int count)
{
    if (PortTracing()) {
        Event(kind, words, count);
    }
}

void PortTraceGp1(uint32_t word)
{
    if (PortTracing()) {
        Event(KIND_GP1, &word, 1);
    }
}

void PortTraceReturn(uint32_t function, uint32_t value)
{
    if (PortTracing()) {
        uint32_t words[7] = {function, 0, 0, 0, 0, 0, value};
        Event(KIND_RETURN, words, 7);
    }
}

/* The recorder's state event, with the fields the port fills: the pad-read
   count and the game's random seed. */
static void TraceState(void)
{
    static int32_t *seed;
    static int looked;
    uint32_t words[32];

    if (!looked) {
        looked = 1;
        seed = dlsym(RTLD_DEFAULT, "attractShip");
    }
    memset(words, 0, sizeof(words));
    words[0] = padReads;
    words[19] = seed ? (uint32_t)*seed : 0;
    {
        int x, y, z;

        if (PortTestPlayerState(&x, &y, &z)) {
            words[29] = (uint32_t)x;
            words[30] = (uint32_t)y;
            words[31] = (uint32_t)z;
        }
    }
    if (SDL_getenv("WIPEOUT_TRACE_SHIP")) {
        /* diagnostics: the player's ship record, native layout (kind 45) */
        int size;
        const void *ship = PortTestPlayerShip(&size);

        if (ship) {
            Event(45, ship, size / 4);
        }
    }
    Event(KIND_STATE, words, 32);
}

static uint64_t gameFrames;

uint64_t PortGameFrames(void)
{
    return gameFrames;
}

void PortTraceBoundary(uint32_t marker)
{
    gameFrames++;
    if (marker == TRACE_CALLBACK_SWAP) {
        callbackSwaps++;
    }
    if (PortTracing()) {
        uint32_t words[2] = {marker, 0};
        TraceState();
        Event(KIND_BOUNDARY, words, 2);
    }
    iteration++;
}
