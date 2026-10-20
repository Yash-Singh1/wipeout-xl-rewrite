/* Video timing: VSync, the vertical-blank callback and the scanline counter
   (root counter 1) the game times its frames with.

   The console shows 59.94 fields a second (NTSC), 262 scanlines each. The
   game waits for fields with VSync and measures a frame by the scanlines
   counted since it last cleared root counter 1 (GsClearVcount), stepping
   its menus and races by that count / 262. The clock here is that scanline
   count: VSync moves it to the start of the next field. The game's own work
   between fields takes no scanlines, so every frame measures as on a console
   that keeps up (the retail game's 30 frames a second); in real time the
   fields are also paced to the wall clock. */
#include <SDL3/SDL.h>
#include <stdio.h>
#include <stdlib.h>

#include "../platform.h"
#include "psx_io.h"
#include "../trace.h"
#include "psyq.h"

#define LINES_PER_FIELD 262
#define FIELD_SECONDS (1.0 / 59.94)

static uint64_t lines;        /* scanlines since power-on */
static uint64_t counterBase;  /* lines when root counter 1 was cleared */
static uint64_t fields;
static void (*vblankCallback)(void);
static uint64_t startTicks;

uint64_t PsxFieldCount(void)
{
    return fields;
}

uint64_t PsxScanlines(void)
{
    return lines;
}

static void Pace(void)
{
    uint64_t now, due;

    if (portConfig.fixedStep) {
        return;
    }
    if (!startTicks) {
        startTicks = SDL_GetTicksNS();
    }
    due = startTicks + (uint64_t)(fields * FIELD_SECONDS * 1e9);
    now = SDL_GetTicksNS();
    if (due > now) {
        SDL_DelayPrecise(due - now);
    } else if (now - due > 250000000ull) {
        /* far behind (the host stalled): don't rush to catch up */
        startTicks += now - due;
    }
}

void PsxVBlank(void)
{
    lines = (lines / LINES_PER_FIELD + 1) * LINES_PER_FIELD;
    fields++;
    PsxIoField(fields);
    if (vblankCallback) {
        vblankCallback();
    }
    PsxShowDisplay();
    if (!PortPumpEvents()) {
        exit(0);
    }
    Pace();
    if (portConfig.frameLimit && fields >= (uint64_t)portConfig.frameLimit) {
        exit(0);
    }
}

/* VSync(0) waits for the next field, VSync(n) for n fields; VSync(1) returns
   the scanline within the current field and VSync(-1) the field count,
   without waiting. */
int VSync(int mode)
{
    int i;

    if (mode < 0) {
        return (int)fields;
    }
    if (mode == 1) {
        return (int)(lines % LINES_PER_FIELD);
    }
    if (mode == 0) {
        mode = 1;
    }
    for (i = 0; i < mode; i++) {
        PsxVBlank();
    }
    return (int)(lines % LINES_PER_FIELD);
}

int VSyncCallback(void (*f)(void))
{
    vblankCallback = f;
    return 0;
}

/* Lockstep tests (WIPEOUT_VCOUNTS): the counter values a console recording
   read, one per line in call order. Each read moves the clock to the time
   the console measured, running the fields that passed meanwhile, so the
   port takes the console's slow frames and draws the same random seeds. */
static int *replayVcounts;
static size_t replayCount, replayNext;

static void LoadVcounts(void)
{
    const char *path = SDL_getenv("WIPEOUT_VCOUNTS");
    size_t capacity = 0;
    FILE *f;
    int value;

    replayCount = 0;
    if (!path) {
        return;
    }
    f = fopen(path, "r");
    if (!f) {
        fprintf(stderr, "libetc: cannot read %s\n", path);
        exit(1);
    }
    while (fscanf(f, "%d", &value) == 1) {
        if (replayCount == capacity) {
            capacity = capacity ? capacity * 2 : 4096;
            replayVcounts = SDL_realloc(replayVcounts, capacity * sizeof(*replayVcounts));
        }
        replayVcounts[replayCount++] = value;
    }
    fclose(f);
}

/* GsGetVcount / GsClearVcount: root counter 1 counting scanlines (16 bits). */
static int ReadVcount(void);

int GsGetVcount(void)
{
    int value = ReadVcount();

    PortTraceReturn(0x80082090u, (uint32_t)value);
    return value;
}

static int ReadVcount(void)
{
    static int loaded;

    if (!loaded) {
        loaded = 1;
        LoadVcounts();
    }
    if (replayNext < replayCount) {
        int value = replayVcounts[replayNext++];
        uint64_t due = counterBase + (uint64_t)value;

        while (lines / LINES_PER_FIELD < due / LINES_PER_FIELD) {
            PsxVBlank();
        }
        if (due > lines) {
            lines = due;
        }
        if (replayNext == replayCount) {
            PortLog("libetc: the recording's counter reads are used up\n");
        }
        return value;
    }
    return (int)((lines - counterBase) & 0xFFFF);
}

void GsClearVcount(void)
{
    counterBase = lines;
}

void GsInitVcount(void)
{
    counterBase = lines;
}
