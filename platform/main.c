/* Program entry: configuration, the window, then the PlayStation game's own
   main(), which runs the whole game exactly as on the console. */
#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "platform.h"
#include "psx/disc.h"
#include "psx/gte.h"
#include "psx/pad.h"
#include "psx/psx_io.h"
#include "region.h"
#include "pc.h"
#include "saturn.h"
#include "debug_ui.h"
#include "discimage.h"
#include "link.h"
#include "mobile.h"
#include "music.h"
#include "settings.h"
#include "trace.h"

PortConfig portConfig;

/* The game's main() (game/src/main.c, compiled under this name). */
int PsxMain(void);

static void Usage(void)
{
    fprintf(stderr,
            "usage: wipeoutxl [--disc FILE.cue] [--window WxH] [--fullscreen]\n"
            "  --disc     your Wipeout XL (USA) or WipEout 2097 (Europe) PlayStation disc image: a .cue with its\n"
            "             .bin tracks, a .bin alone, or an archive of either (.7z, .zip, .rar, .tar, .gz, ...);\n"
            "             without it, the one chosen last time, else a file dialog asks for it\n"
            "environment: WIPEOUT_XL_DISC, WIPEOUT_RENDER_SCALE (0 auto, else 1-12), WIPEOUT_HD_PACK (a texture pack's\n"
            "  .zip or folder); tests: WIPEOUT_MUTE, WIPEOUT_HIDDEN,\n"
            "  WIPEOUT_FIXED_STEP, WIPEOUT_FRAMES, WIPEOUT_CAPTURE_DIR, WIPEOUT_CAPTURE_EVERY, WIPEOUT_CAPTURE_FROM,\n"
            "  WIPEOUT_TRACE (trace file), WIPEOUT_PADS (pad script), WIPEOUT_VCOUNTS (counter reads),\n"
            "  WIPEOUT_CD_SPEED (drive speed-up; 1 = the console's, default 1000), WIPEOUT_SAVE_DIR (memory cards; default: the user's data folder),\n"
            "  WIPEOUT_KEYS (scripted keys and gamepad), WIPEOUT_VRAM_DUMP (video memory picture at the end of a run),\n"
            "  WIPEOUT_RUMBLE_LOG (what the pads' motors are asked, one line a request),\n"
            "  WIPEOUT_FAR_VIEW (0: the console's draw distance; 2: the whole track even while tracing), WIPEOUT_FAR_FLARES,\n"
            "  WIPEOUT_MOVIES (play the movies in fixed-step runs too), WIPEOUT_WAV (the mixed audio, fixed-step),\n"
            "  WIPEOUT_MUSIC (a music folder in place of the disc's music)\n");
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
    static const SDL_DialogFileFilter cue[] = {
        {"Wipeout XL or WipEout 2097 disc image (.cue, .bin, or an archive of one)", PORT_DISC_EXTENSIONS}};
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
    chosenDisc = NULL;
    SDL_SetAtomicInt(&discDialogDone, 0);
    SDL_ShowOpenFileDialog(DiscChosen, NULL, NULL, cue, SDL_arraysize(cue), NULL, false);
    while (!SDL_GetAtomicInt(&discDialogDone)) {
        SDL_Event event;

        if (SDL_WaitEventTimeout(&event, 100) && event.type == SDL_EVENT_QUIT) {
            break;
        }
    }
    return chosenDisc; /* (kept in the settings once it opens) */
#endif
}

/* ---- the disc image before the game: unpacked or copied in on a thread of its own ---- */

typedef struct DiscJob {
    const char *path;
    char result[1200]; /* the .cue, or the copy */
    char message[512];
    int ok;
    SDL_AtomicInt done;
} DiscJob;

static int SDLCALL ImageJob(void *data)
{
    DiscJob *job = data;

    job->ok = PortDiscImage(job->path, job->result, sizeof(job->result), job->message, sizeof(job->message));
    SDL_SetAtomicInt(&job->done, 1);
    return 0;
}

/* Runs a job on a thread of its own while the window says what is going on and how far it is (thousandths). */
static void RunShowing(SDL_ThreadFunction run, DiscJob *job, const char *text, int (*progress)(void))
{
    SDL_Thread *thread = SDL_CreateThread(run, "disc", job);
    SDL_Event event;

    if (!thread) {
        run(job);
        return;
    }
    while (!SDL_GetAtomicInt(&job->done)) {
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_EVENT_QUIT) {
                exit(0);
            }
        }
        DebugUiSplash(text, progress() / 1000.0f);
        SDL_Delay(50);
    }
    SDL_WaitThread(thread, NULL);
}

#ifdef PORT_MOBILE
/* A phone or tablet: the disc chosen before, else a disc image in the app's folder (mobile.c). Without one the
   player is told where to put it (Android: or picks it, and it is copied in), and the folder is looked at again. */
static int SDLCALL ImportJob(void *data)
{
    DiscJob *job = data;

    job->ok = PortImportFile(job->path, job->result, sizeof(job->result), job->message, sizeof(job->message));
    SDL_SetAtomicInt(&job->done, 1);
    return 0;
}

static char **picked;
static int pickedCount;
static SDL_AtomicInt pickDone;

static void SDLCALL Picked(void *user, const char *const *files, int filter)
{
    (void)user;
    (void)filter;
    for (; files && *files; files++) {
        picked = SDL_realloc(picked, (size_t)(pickedCount + 1) * sizeof(char *));
        picked[pickedCount++] = SDL_strdup(*files);
    }
    SDL_SetAtomicInt(&pickDone, 1);
}

/* Android: the system's picker; the files chosen are copied into the app's folder. */
static void PickAndCopy(void)
{
    SDL_Event event;
    int i;

    SDL_SetAtomicInt(&pickDone, 0);
    SDL_ShowOpenFileDialog(Picked, NULL, PortWindow(), NULL, 0, NULL, true);
    while (!SDL_GetAtomicInt(&pickDone)) {
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_EVENT_QUIT) {
                exit(0);
            }
        }
        DebugUiSplash("Choose the disc image: its .cue together with its .bin files, or a .zip or .7z of them.", -1);
        SDL_Delay(50);
    }
    for (i = 0; i < pickedCount; i++) {
        static DiscJob job;
        char text[300];

        memset(&job, 0, sizeof(job));
        job.path = picked[i];
        SDL_snprintf(text, sizeof(text), "Copying the disc image into the game's folder (%d of %d)...", i + 1, pickedCount);
        RunShowing(ImportJob, &job, text, PortImportProgress);
        if (!job.ok) {
            SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "Wipeout XL", job.message, PortWindow());
        }
        SDL_free(picked[i]);
    }
    SDL_free(picked);
    picked = NULL;
    pickedCount = 0;
}

static const char *MobileDisc(void)
{
    static char found[1024];
    char text[1200];
    SDL_PathInfo info;

    for (;;) {
        const SDL_MessageBoxButtonData buttons[] = {
            {SDL_MESSAGEBOX_BUTTON_RETURNKEY_DEFAULT, 1, "Look again"},
#ifdef SDL_PLATFORM_ANDROID
            {0, 2, "Choose files..."},
#endif
            {SDL_MESSAGEBOX_BUTTON_ESCAPEKEY_DEFAULT, 0, "Quit"}};
        SDL_MessageBoxData box;
        int button = 0;

        if (portSettings.discPath[0] && SDL_GetPathInfo(portSettings.discPath, &info)) {
            return portSettings.discPath;
        }
        if (PortFindDisc(found, sizeof(found))) {
            SDL_strlcpy(portSettings.discPath, found, sizeof(portSettings.discPath));
            PortSettingsSave();
            return found;
        }
#ifdef SDL_PLATFORM_IOS
        SDL_snprintf(text, sizeof(text),
                     "Wipeout XL needs your copy of the PlayStation disc: Wipeout XL (USA) or WipEout 2097 (Europe).\n\n"
                     "In the Files app, put its .cue and .bin files (or a .zip or .7z of them) in On My iPhone (or iPad) "
                     "> Wipeout XL, then tap Look again.");
#else
        SDL_snprintf(text, sizeof(text),
                     "Wipeout XL needs your copy of the PlayStation disc: Wipeout XL (USA) or WipEout 2097 (Europe).\n\n"
                     "Choose its .cue together with its .bin files (or a .zip or .7z of them) on this device, or copy them "
                     "from a computer into %s, then tap Look again.", PortImportFolder() ? PortImportFolder() : "the app's folder");
#endif
        DebugUiSplash("", -1);
        SDL_zero(box);
        box.flags = SDL_MESSAGEBOX_INFORMATION;
        box.window = PortWindow();
        box.title = "Wipeout XL";
        box.message = text;
        box.numbuttons = (int)SDL_arraysize(buttons);
        box.buttons = buttons;
        if (!SDL_ShowMessageBox(&box, &button) || button == 0) {
            return NULL;
        }
        if (button == 2) {
            PickAndCopy();
        }
    }
}
#endif

int main(int argc, char **argv)
{
    const char *value;
    int i, w, h, recordReplay = 0;
    int discFromSettings = 0; /* the disc is the settings' (or the dialog's), not one given (--disc) */

    for (i = 1; i < argc; i++) {
        if (!strcmp(argv[i], "--link-slave")) {
            /* the second player's game: the first's test settings are not for it */
            static const char *const testSettings[] = {
                "WIPEOUT_KEYS", "WIPEOUT_CAPTURE_DIR", "WIPEOUT_FRAMES", "WIPEOUT_TRACE", "WIPEOUT_PADS",
                "WIPEOUT_VCOUNTS", "WIPEOUT_WRITES", "WIPEOUT_LOADS", "WIPEOUT_VRAM_DUMP",
                "WIPEOUT_SAVE_DIR", "WIPEOUT_FIXED_STEP", "WIPEOUT_HIDDEN", "WIPEOUT_RUMBLE_LOG"};
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
    PortMusicInit();
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
#ifndef PORT_MOBILE
    discFromSettings = !portConfig.discPath;
    if (!portConfig.discPath && !(portConfig.discPath = ChooseDisc())) {
        Usage();
        return 1;
    }
#endif
    if (!PortVideoInit()) {
        return 1;
    }
#ifdef PORT_MOBILE
    if (!portConfig.discPath && !(portConfig.discPath = MobileDisc())) {
        return 1;
    }
#endif
    {
        /* the image's .cue: an archive's unpacked the first time (the window showing how far), a .bin's made for
           it (discimage.c) */
        static DiscJob image;

        for (;;) {
            image.path = portConfig.discPath;
            if (PortPcFolderIs(image.path)) {
                /* the PC CD-ROM version's files in a folder (pc.c): an empty disc stands in for its CD */
                image.ok = PortPcUseFolder(image.path, image.result, sizeof(image.result), image.message,
                                           sizeof(image.message));
            } else if (PortIsArchive(image.path) && !portConfig.hidden) {
                RunShowing(ImageJob, &image, "Unpacking the disc image (only this once)...", PortArchiveProgress);
            } else {
                ImageJob(&image);
            }
            if (image.ok) {
                /* its cue sheet read now, before it is kept as the disc to start with */
                Disc *disc = DiscOpen(image.result, image.message, sizeof(image.message));
                int32_t lba;
                uint32_t size;

                /* ... and it is the game: the sound bank of its \WIPEOUT2 tree (on the disc of every release:
                   region.c's two, the Japanese and the prototype's), the Sega Saturn disc (saturn.c), or the PC
                   CD-ROM version's (pc.c). */
                image.ok = disc && (PortSaturnDiscIs(disc) || PortPcDiscIs(disc) || PortPcData() ||
                                    DiscFindFile(disc, "\\WIPEOUT2\\SOUND\\SAMPLES.WAD", &lba, &size));
                if (disc && !image.ok) {
                    SDL_snprintf(image.message, sizeof(image.message),
                                 "%s is not Wipeout XL (SCUS-94351) or WipEout 2097 (SLES-00327).\n\n"
                                 "The game's files are not on it, nor the Sega Saturn's or the PC CD-ROM version's.",
                                 PortPathName(image.path));
                }
                DiscClose(disc);
            }
            if (image.ok) {
                break;
            }
            SDL_Log("%s", image.message);
#ifndef PORT_MOBILE
            if (discFromSettings && !portConfig.hidden) {
                /* the one chosen last time, or just now: forgotten first (else the game would stop with it at every
                   start), then the player picks another */
                portSettings.discPath[0] = 0;
                PortSettingsSave();
            }
#endif
            if (!portConfig.hidden) {
                SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "Wipeout XL", image.message, NULL);
            }
#ifndef PORT_MOBILE
            if (discFromSettings && !portConfig.hidden && (portConfig.discPath = ChooseDisc())) {
                continue;
            }
#endif
            return 1;
        }
#ifndef PORT_MOBILE
        if (discFromSettings && strcmp(portSettings.discPath, portConfig.discPath) != 0) {
            SDL_strlcpy(portSettings.discPath, portConfig.discPath, sizeof(portSettings.discPath));
            PortSettingsSave();
        }
#endif
        portConfig.discGiven = portConfig.discPath;
        portConfig.discPath = image.result;
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
        io.oneCard = SDL_getenv("WIPEOUT_TRACE") != NULL; /* (the console recordings were made with one card) */
        io.wavPath = SDL_getenv("WIPEOUT_WAV"); /* tests: the mixed audio of a fixed-step run */
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
    PortRegionStart(); /* the disc's region and the video standard (version.h) */
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
#ifdef __EMSCRIPTEN__
int PortRestart(const char *discPath)
{
    (void)discPath;
    return 0; /* (a page cannot start a program: it is reloaded) */
}
#else
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
#endif

int PortLogoFields(int fields)
{
    /* the setting; never in the tests' recorded runs, which compare the console's start-up */
    return portSettings.skipLogos && !SDL_getenv("WIPEOUT_TRACE") ? 0 : fields;
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

int PortFarFlaresEnabled(void)
{
    return portSettings.farFlares;
}

int PortFadeRange(int *nearDistance, int *farDistance)
{
    *nearDistance = portSettings.fadeNear;
    *farDistance = portSettings.fadeFar;
    return portSettings.fade;
}

/* The level at the strength the player asked for (0: the scale's own), in the game's units. */
static int RumbleLevel(int level)
{
    int strength = portSettings.rumbleStrength > 0 ? portSettings.rumbleStrength : 100;

    return level * strength / 100;
}

/* What the game asks of the pads' motors (WIPEOUT_RUMBLE_LOG), one line a request. A hidden run has no
   pad to feel and a trace records nothing of them, so this is how the jolts and the launch thrust's
   buzz are checked. */
static void RumbleLog(const char *kind, int level)
{
    static int enabled = -1;

    if (enabled < 0) {
        enabled = SDL_getenv("WIPEOUT_RUMBLE_LOG") != NULL;
    }
    if (enabled) {
        fprintf(stderr, "rumble: %s %d\n", kind, level);
    }
}

/* A request of the game's: the second player's game sends it over the link to the master, whose window has
   their controller; this game's runs the player's own motors at the player's setting, and never in the
   tests' hidden runs (which is also what stops a jolt or a held level that was running when either of them
   changed). */
static void Rumble(int kind, const char *name, int level)
{
    RumbleLog(name, level);
    if (PortLinkRole() == LINK_SLAVE) {
        PortLinkPutRumble(kind, level);
        return;
    }
    PsxPadRumble(0, kind, portSettings.noRumble || portConfig.hidden ? 0 : RumbleLevel(level));
}

void PortPadRumble(int amount)
{
    Rumble(PAD_RUMBLE_JOLT, "jolt", amount);
}

void PortPadRumbleLevel(int level)
{
    Rumble(PAD_RUMBLE_LEVEL, "level", level);
}

void PortPadRumbleScrape(int level)
{
    Rumble(PAD_RUMBLE_SCRAPE, "scrape", level);
}

void PortPadSecondRumble(int kind, int level)
{
    PsxPadRumble(1, kind, portSettings.noRumble || portConfig.hidden ? 0 : RumbleLevel(level));
}
