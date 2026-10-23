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
#include "../settings.h"
#include "framecap.h"
#include "internal.h"
#include "../debug_ui.h"
#include "psx_io.h"
#include "../trace.h"
#include "../link.h"
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

static int hurry; /* the second player's game starts up unseen: no waiting for the fields (link.c) */
static uint64_t paceField; /* the field the real-time pacing counts from (after such a start-up) */

static void Pace(void)
{
    uint64_t now, due;

    if (hurry) {
        startTicks = 0; /* the real-time fields start again from where the hurry ends */
        paceField = fields;
        return;
    }
    if (portConfig.fixedStep) {
        int k, n = PsxInterpolationTest();

        for (k = 1; k <= n && PsxCanInterpolate(); k++) {
            PsxDrawInterpolatedTest(k, n); /* tests: WIPEOUT_INTERPOLATE */
        }
        return;
    }
    if (!startTicks) {
        startTicks = SDL_GetTicksNS();
    }
    due = startTicks + (uint64_t)((fields - paceField) * FIELD_SECONDS * 1e9);
    now = SDL_GetTicksNS();
    PortSwapInterval(portSettings.frameRate == FRAME_RATE_CONSOLE || portSettings.frameRate == FRAME_RATE_VSYNC ||
                     !PsxInterpolationOn());
    if (due > now && PsxCanInterpolate()) {
        /* frame interpolation: pictures between the game's frames until the field is due, as many as the
           settings allow (each waits for the display's refresh with VSync) */
        static uint64_t lastPicture, pictureCost = 2000000;

        while ((now = SDL_GetTicksNS()) < due) {
            uint64_t next = portSettings.frameRate == FRAME_RATE_CAPPED
                                ? lastPicture + 1000000000ull / (uint64_t)SDL_max(portSettings.frameCap, 1)
                                : now;

            if (next > now || now + pictureCost > due) {
                /* not yet (the cap), or no time for one before the field: the game's time comes first */
                SDL_DelayPrecise(((next > now && next < due) ? next : due) - now);
                continue;
            }
            lastPicture = now;
            PsxDrawInterpolated();
            /* what a picture takes, drawing and showing it (with VSync, the wait for the display's refresh) */
            pictureCost = (pictureCost * 7 + (SDL_GetTicksNS() - now)) / 8;
        }
    } else if (due > now) {
        SDL_DelayPrecise(due - now);
    } else if (now - due > 250000000ull) {
        /* far behind (the host stalled): don't rush to catch up */
        startTicks += now - due;
    }
}

/* One field passes. */
static void Field(void)
{
    lines = (lines / LINES_PER_FIELD + 1) * LINES_PER_FIELD;
    fields++;
    hurry = PortLinkField();
    PsxIoField(fields);
    if (vblankCallback) {
        vblankCallback();
    }
    PsxShowDisplay();
    if (!PortPumpEvents()) {
        exit(0);
    }
    {
        /* tests: WIPEOUT_FRAMECAP_SAVE_AT=n saves the frame recorder's frames at picture n */
        static int saveAt = -1;

        if (saveAt < 0) {
            const char *value = SDL_getenv("WIPEOUT_FRAMECAP_SAVE_AT");
            saveAt = value ? SDL_atoi(value) : 0;
        }
        if (saveAt > 0 && PortShownFrames() == saveAt) {
            FrameCapSave();
        }
    }
    {
        /* tests: WIPEOUT_FRAMECAP_FREEZE_AT=n freezes as Tab does at picture n */
        static int freezeAt = -1;

        if (freezeAt < 0) {
            const char *value = SDL_getenv("WIPEOUT_FRAMECAP_FREEZE_AT");
            freezeAt = value ? SDL_atoi(value) : 0;
        }
        if (freezeAt > 0 && PortShownFrames() == freezeAt) {
            FrameCapToggleFreeze();
            DebugUiShowRecorder();
        }
    }
    {
        /* tests: WIPEOUT_FRAMECAP_TAB_AT=a,b presses Tab at pictures a and b (and with WIPEOUT_DEBUG_UI opens the
           console after b, as ~ would) */
        static int tabAt[2] = {-1, -1};

        if (tabAt[0] < 0) {
            const char *value = SDL_getenv("WIPEOUT_FRAMECAP_TAB_AT");

            tabAt[0] = tabAt[1] = 0;
            if (value) {
                SDL_sscanf(value, "%d,%d", &tabAt[0], &tabAt[1]);
            }
        }
        if ((tabAt[0] > 0 && PortShownFrames() == tabAt[0]) || (tabAt[1] > 0 && PortShownFrames() == tabAt[1])) {
            DebugUiTab();
            if (PortShownFrames() == tabAt[1] && SDL_getenv("WIPEOUT_DEBUG_UI")) {
                DebugUiShowRecorder();
            }
        }
    }
    /* Tab (the frame recorder): the game waits here, silent, while the window and the debug console go on */
    if (FrameCapFrozen()) {
        PsxAudioDevicePause(true);
        while (FrameCapFrozen()) {
            PsxShowDisplay();
            if (!PortPumpEvents()) {
                exit(0);
            }
            SDL_Delay(16);
        }
        PsxAudioDevicePause(false);
    }
}

void PsxVBlank(void)
{
    Field();
    Pace();
    if (portConfig.frameLimit && fields >= (uint64_t)portConfig.frameLimit) {
        fprintf(stderr, "%d pictures shown in %llu fields\n", PortShownFrames(), (unsigned long long)fields); /* tests */
        PortDumpVram();
        exit(0);
    }
}

void PsxPassFields(void)
{
    static int passing; /* (a field's vblank callback may read the disc itself) */
    uint64_t now, due;

    if (portConfig.fixedStep || passing) {
        return;
    }
    if (!startTicks) {
        startTicks = SDL_GetTicksNS();
    }
    now = SDL_GetTicksNS();
    due = startTicks + (uint64_t)((fields + 1 - paceField) * FIELD_SECONDS * 1e9);
    if (due + 250000000ull < now) {
        startTicks += now - due; /* far behind (the host stalled): one field, not a rush to catch up, as Pace */
    }
    passing = 1;
    while (startTicks + (uint64_t)((fields + 1 - paceField) * FIELD_SECONDS * 1e9) <= now) {
        Field();
    }
    passing = 0;
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
    if (vblankCallback && !f) {
        PortLoadingEnds(); /* lockstep tests: the console's number of loading frames */
    }
    vblankCallback = f;
    if (f) {
        PortLoadingStarts();
    }
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
    PortRecordVcount(value);
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
