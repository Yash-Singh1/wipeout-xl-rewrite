/* Program entry: configuration, the window, then the PlayStation game's own
   main(), which runs the whole game exactly as on the console. */
#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "platform.h"
#include "psx/gte.h"
#include "psx/psx_io.h"
#include "link.h"
#include "settings.h"
#include "trace.h"

PortConfig portConfig;

/* The game's main() (game/src/main.c, compiled under this name). */
int PsxMain(void);

static void Usage(void)
{
    fprintf(stderr,
            "usage: wipeoutxl [--disc FILE.cue] [--window WxH] [--fullscreen]\n"
            "  --disc     your Wipeout XL (USA) PlayStation disc image (.cue with its .bin tracks);\n"
            "             without it, the one chosen last time, else a file dialog asks for it\n"
            "environment: WIPEOUT_XL_DISC; tests: WIPEOUT_MUTE, WIPEOUT_HIDDEN,\n"
            "  WIPEOUT_FIXED_STEP, WIPEOUT_FRAMES, WIPEOUT_CAPTURE_DIR, WIPEOUT_CAPTURE_EVERY, WIPEOUT_CAPTURE_FROM,\n"
            "  WIPEOUT_TRACE (trace file), WIPEOUT_PADS (pad script), WIPEOUT_VCOUNTS (counter reads),\n"
            "  WIPEOUT_CD_SPEED (drive speed-up; 1 = the console's, default 1000), WIPEOUT_SAVE_DIR (memory cards; default: the user's data folder),\n"
            "  WIPEOUT_KEYS (scripted keys and gamepad), WIPEOUT_VRAM_DUMP (video memory picture at the end of a run),\n"
            "  WIPEOUT_FAR_VIEW (0: the console's draw distance; 2: the whole track even while tracing),\n"
            "  WIPEOUT_MOVIES (play the movies in fixed-step runs too)\n");
}

/* No disc given (--disc, WIPEOUT_XL_DISC): the one chosen last time, else the player picks the .cue in the
   system's file dialog, as when the program is started from the desktop. The web page passes --disc. */
static const char *chosenDisc;
static SDL_AtomicInt discDialogDone; /* the dialog may answer from a thread of its own (Windows) */

static void SDLCALL DiscChosen(void *user, const char *const *files, int filter)
{
    (void)user;
    (void)filter;
    if (files && files[0]) {
        chosenDisc = SDL_strdup(files[0]);
    }
    SDL_SetAtomicInt(&discDialogDone, 1);
}

static const char *ChooseDisc(void)
{
    static const SDL_DialogFileFilter cue[] = {{"Wipeout XL disc image (.cue)", "cue"}};
    SDL_PathInfo info;

    if (portSettings.discPath[0] && SDL_GetPathInfo(portSettings.discPath, &info)) {
        return portSettings.discPath;
    }
#ifdef __EMSCRIPTEN__
    return NULL; /* the page has no file dialog of SDL's: it starts the game with --disc */
#else
    if (portConfig.hidden) {
        return NULL;
    }
    SDL_ShowOpenFileDialog(DiscChosen, NULL, NULL, cue, SDL_arraysize(cue), NULL, false);
    while (!SDL_GetAtomicInt(&discDialogDone)) {
        SDL_Event event;

        if (SDL_WaitEventTimeout(&event, 100) && event.type == SDL_EVENT_QUIT) {
            break;
        }
    }
    if (chosenDisc) {
        SDL_strlcpy(portSettings.discPath, chosenDisc, sizeof(portSettings.discPath));
        PortSettingsSave();
    }
    return chosenDisc;
#endif
}

int main(int argc, char **argv)
{
    const char *value;
    int i, w, h, recordReplay = 0;

    for (i = 1; i < argc; i++) {
        if (!strcmp(argv[i], "--link-slave")) {
            /* the second player's game: the first's test settings are not for it */
            static const char *const testSettings[] = {
                "WIPEOUT_KEYS", "WIPEOUT_CAPTURE_DIR", "WIPEOUT_FRAMES", "WIPEOUT_TRACE", "WIPEOUT_PADS",
                "WIPEOUT_VCOUNTS", "WIPEOUT_WRITES", "WIPEOUT_LOADS", "WIPEOUT_VRAM_DUMP",
                "WIPEOUT_SAVE_DIR", "WIPEOUT_FIXED_STEP", "WIPEOUT_HIDDEN"};
            size_t k;

            for (k = 0; k < SDL_arraysize(testSettings); k++) {
                SDL_UnsetEnvironmentVariable(SDL_GetEnvironment(), testSettings[k]);
            }
        }
    }

    portConfig.discPath = SDL_getenv("WIPEOUT_XL_DISC");
    portConfig.windowWidth = 1280;
    portConfig.windowHeight = 960;
    portConfig.muted = SDL_getenv("WIPEOUT_MUTE") != NULL;
    portConfig.hidden = SDL_getenv("WIPEOUT_HIDDEN") != NULL;
    portConfig.fixedStep = SDL_getenv("WIPEOUT_FIXED_STEP") != NULL;
    value = SDL_getenv("WIPEOUT_FRAMES");
    portConfig.frameLimit = value ? SDL_atoi(value) : 0;
    portConfig.captureDir = SDL_getenv("WIPEOUT_CAPTURE_DIR");
    value = SDL_getenv("WIPEOUT_CAPTURE_EVERY");
    portConfig.captureEvery = value ? SDL_atoi(value) : 1;
    value = SDL_getenv("WIPEOUT_CAPTURE_FROM");
    portConfig.captureFrom = value ? SDL_atoi(value) : 0;
    value = SDL_getenv("WIPEOUT_CAPTURE_TO");
    portConfig.captureTo = value ? SDL_atoi(value) : 0;
    portConfig.executable = argv[0];
    PortLinkNoteProgram(argv[0]);
    portConfig.savePath = SDL_getenv("WIPEOUT_SAVE_DIR");
    for (i = 1; i < argc; i++) {
        if (!strcmp(argv[i], "--disc") && i + 1 < argc) {
            portConfig.discPath = argv[++i];
        } else if (!strcmp(argv[i], "--window") && i + 1 < argc && SDL_sscanf(argv[i + 1], "%dx%d", &w, &h) == 2) {
            portConfig.windowWidth = w;
            portConfig.windowHeight = h;
            i++;
        } else if (!strcmp(argv[i], "--fullscreen")) {
            portConfig.fullscreen = 1;
        } else if (!strcmp(argv[i], "--link-slave") && i + 1 < argc) {
            /* the second player's game, started by the first's (link.c): no window, no music */
            portConfig.linkName = argv[++i];
            portConfig.hidden = 1;
        } else if (!strcmp(argv[i], "--record")) {
            recordReplay = 1;
        } else if (!strcmp(argv[i], "--save-dir") && i + 1 < argc) {
            portConfig.savePath = argv[++i];
        } else {
            Usage();
            return 1;
        }
    }
    PortSettingsLoad();
    if (!portConfig.savePath && portSettings.saveDir[0]) {
        static char saveDir[1040];

        /* the memory cards' folder from the settings (the file names follow it directly) */
        SDL_snprintf(saveDir, sizeof(saveDir), "%s%s", portSettings.saveDir,
                     portSettings.saveDir[strlen(portSettings.saveDir) - 1] == '/' ? "" : "/");
        SDL_CreateDirectory(saveDir);
        portConfig.savePath = saveDir;
    }
    if (!portConfig.savePath) {
        portConfig.savePath = SDL_GetPrefPath("Psygnosis", "WipeoutXL");
    }
    if (portConfig.linkName && !PortLinkSlaveInit(portConfig.linkName)) {
        return 1;
    }
    if (portConfig.muted) {
        SDL_SetHint(SDL_HINT_AUDIO_DRIVER, "dummy");
    }
    if (portConfig.linkName) {
        /* the second player's game is part of the first's window: no Dock icon, never the focus */
        SDL_SetHint(SDL_HINT_MAC_BACKGROUND_APP, "1");
    }
    /* WIPEOUT_NO_GAMEPAD: the controllers are not touched at all (only a test script's virtual pads start
       them, pad_sdl.c), so the game starts with the keyboard even when the system's controller service
       hangs every program that asks it */
    if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_EVENTS |
                  (SDL_getenv("WIPEOUT_NO_GAMEPAD") && !SDL_getenv("WIPEOUT_KEYS") ? 0 : SDL_INIT_GAMEPAD))) {
        SDL_Log("SDL: %s", SDL_GetError());
        return 1;
    }
    if (!portConfig.discPath && !(portConfig.discPath = ChooseDisc())) {
        Usage();
        return 1;
    }
    if (!PortVideoInit()) {
        return 1;
    }
    {
        PsxIoConfig io;
        char error[256];

        memset(&io, 0, sizeof(io));
        io.discPath = portConfig.discPath;
        io.savePath = portConfig.savePath;
        io.fixedStep = portConfig.fixedStep;
        io.muted = portConfig.muted;
        io.noCdAudio = portConfig.linkName != NULL;
#ifdef __EMSCRIPTEN__
        io.queueAudio = 1; /* the page's audio callback runs only while the game waits (psx_io.h) */
#else
        io.queueAudio = SDL_getenv("WIPEOUT_QUEUE_AUDIO") != NULL; /* tests: as the web does */
#endif
        value = SDL_getenv("WIPEOUT_CD_SPEED");
        /* data reads as fast as the disc image allows (the console's drive: WIPEOUT_CD_SPEED=1) */
        io.cdSpeed = value ? SDL_atof(value) : 1000.0;
        if (!PsxIoInit(&io, error, sizeof(error))) {
            SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "Wipeout XL", error, NULL);
            fprintf(stderr, "%s\n", error);
            return 1;
        }
        atexit(PsxIoShutdown);
    }
    PortTraceInit();
    if (recordReplay || (portSettings.recordReplays && !portConfig.hidden)) {
        /* a replay of this session: <data folder>/replays/<date and time>/ */
        char dir[1200];
        char *base = SDL_GetPrefPath("Psygnosis", "WipeoutXL");
        SDL_Time now;
        SDL_DateTime t;

        SDL_GetCurrentTime(&now);
        SDL_TimeToDateTime(now, &t, true);
        SDL_snprintf(dir, sizeof(dir), "%sreplays", base ? base : "./");
        SDL_CreateDirectory(dir);
        SDL_snprintf(dir, sizeof(dir), "%sreplays/%04d-%02d-%02d_%02d-%02d-%02d", base ? base : "./", t.year, t.month,
                     t.day, t.hour, t.minute, t.second);
        if (PortRecordStart(dir, portConfig.savePath, PortSettingsPath())) {
            fprintf(stderr, "recording a replay to %s\n", dir);
        }
        SDL_free(base);
    }
    atexit(PortLinkShutdown);
    PsxMain();
    PortVideoShutdown();
    SDL_Quit();
    return 0;
}

/* The debug console's choice of disc: the game starts again with it (a running game cannot change discs), in a
   window of the same size. Returns only when the new game could not be started. */
int PortRestart(const char *discPath)
{
    const char *args[8];
    char window[32];
    int n = 0, width, height;
    SDL_Process *process;

    PortWindowSize(&width, &height);
    SDL_snprintf(window, sizeof(window), "%dx%d", width, height);
    args[n++] = portConfig.executable;
    args[n++] = "--disc";
    args[n++] = discPath;
    args[n++] = "--window";
    args[n++] = window;
    if (PortWindowFullscreen()) {
        args[n++] = "--fullscreen";
    }
    args[n] = NULL;
    process = SDL_CreateProcess(args, false);
    if (!process) {
        fprintf(stderr, "restart: %s\n", SDL_GetError());
        return 0;
    }
    SDL_DestroyProcess(process); /* (the object only: the new game goes on) */
    exit(0);
}

int PortFarViewEnabled(void)
{
    static int mode = -1; /* 2: forced on (tests), 0: off while tracing, 1: the setting */

    if (mode < 0) {
        const char *value = SDL_getenv("WIPEOUT_FAR_VIEW");

        /* WIPEOUT_FAR_VIEW=2, tests: also while tracing (that the game's own results stay the same);
           the lockstep tests compare the console's drawing */
        mode = value && SDL_atoi(value) == 2 ? 2 : SDL_getenv("WIPEOUT_TRACE") ? 0 : 1;
    }
    return mode == 2 || (mode == 1 && portSettings.farView);
}

int PortFadeRange(int *nearDistance, int *farDistance)
{
    *nearDistance = portSettings.fadeNear;
    *farDistance = portSettings.fadeFar;
    return portSettings.fade;
}

