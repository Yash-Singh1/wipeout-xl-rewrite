/* Program entry: configuration, the window, then the PlayStation game's own
   main(), which runs the whole game exactly as on the console. */
#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "platform.h"
#include "psx/psx_io.h"
#include "trace.h"

PortConfig portConfig;

/* The game's main() (game/src/main.c, compiled under this name). */
int PsxMain(void);

static void Usage(void)
{
    fprintf(stderr,
            "usage: wipeoutxl [--disc FILE.cue] [--pc-data DIR] [--window WxH] [--fullscreen]\n"
            "  --disc     your Wipeout XL (USA) PlayStation disc image (.cue with its .bin tracks)\n"
            "  --pc-data  optional: the Wipeout XL PC CD, for its intro movies and keyboard art\n"
            "environment: WIPEOUT_XL_DISC, WIPEOUT_XL_PC_DATA; tests: WIPEOUT_MUTE, WIPEOUT_HIDDEN,\n"
            "  WIPEOUT_FIXED_STEP, WIPEOUT_FRAMES, WIPEOUT_CAPTURE_DIR, WIPEOUT_CAPTURE_EVERY, WIPEOUT_CAPTURE_FROM,\n"
            "  WIPEOUT_TRACE (trace file), WIPEOUT_PADS (pad script), WIPEOUT_VCOUNTS (counter reads),\n"
            "  WIPEOUT_CD_SPEED (drive speed-up; 1 = the console's, default 1000), WIPEOUT_SAVE_DIR (memory cards; default: the user's data folder),\n"
            "  WIPEOUT_KEYS (scripted keys and gamepad), WIPEOUT_VRAM_DUMP (video memory picture at the end of a run),\n"
            "  WIPEOUT_FAR_VIEW (0: the console's draw distance; 2: the whole track even while tracing)\n");
}

int main(int argc, char **argv)
{
    const char *value;
    int i, w, h;

    portConfig.discPath = SDL_getenv("WIPEOUT_XL_DISC");
    portConfig.pcDataPath = SDL_getenv("WIPEOUT_XL_PC_DATA");
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
    for (i = 1; i < argc; i++) {
        if (!strcmp(argv[i], "--disc") && i + 1 < argc) {
            portConfig.discPath = argv[++i];
        } else if (!strcmp(argv[i], "--pc-data") && i + 1 < argc) {
            portConfig.pcDataPath = argv[++i];
        } else if (!strcmp(argv[i], "--window") && i + 1 < argc && SDL_sscanf(argv[i + 1], "%dx%d", &w, &h) == 2) {
            portConfig.windowWidth = w;
            portConfig.windowHeight = h;
            i++;
        } else if (!strcmp(argv[i], "--fullscreen")) {
            portConfig.fullscreen = 1;
        } else {
            Usage();
            return 1;
        }
    }
    if (!portConfig.discPath) {
        Usage();
        return 1;
    }
    portConfig.savePath = SDL_getenv("WIPEOUT_SAVE_DIR");
    if (!portConfig.savePath) {
        portConfig.savePath = SDL_GetPrefPath("Psygnosis", "WipeoutXL");
    }
    if (portConfig.muted) {
        SDL_SetHint(SDL_HINT_AUDIO_DRIVER, "dummy");
    }
    if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_GAMEPAD | SDL_INIT_EVENTS)) {
        SDL_Log("SDL: %s", SDL_GetError());
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
    PsxMain();
    PortVideoShutdown();
    SDL_Quit();
    return 0;
}

int PortFarViewEnabled(void)
{
    const char *value = SDL_getenv("WIPEOUT_FAR_VIEW");

    if (value && SDL_atoi(value) == 2) {
        return 1; /* tests: also while tracing (that the game's own results stay the same) */
    }
    if (SDL_getenv("WIPEOUT_TRACE")) {
        return 0; /* the lockstep tests compare the console's drawing */
    }
    return !value || SDL_atoi(value) != 0;
}
