/* Shared between the platform modules (not included by game code). */
#ifndef PORT_PLATFORM_H
#define PORT_PLATFORM_H

#include <stdint.h>

typedef struct PortConfig {
    const char *discPath;    /* the PS1 disc image's .cue */
    const char *savePath;    /* writable: memory card files */
    int windowWidth, windowHeight, fullscreen;
    int muted;               /* tests: no audio device */
    int hidden;              /* tests: no visible window */
    int fixedStep;           /* tests: no real-time pacing; time advances only with the game */
    int frameLimit;          /* tests: stop after this many video fields (0 = no limit) */
    const char *captureDir;  /* tests: save shown frames as PPM */
    int captureEvery;
    int captureFrom;         /* the first shown frame saved */
    int captureTo;           /* ... and the last (0: no end) */
    const char *executable;  /* argv[0]: the second player's game is started from it */
    const char *linkName;    /* the second player's game: the shared memory of the link (link.c) */
} PortConfig;
extern PortConfig portConfig;

/* video.c */
int PortVideoInit(void);
void PortVideoShutdown(void);
void PortVideoPresent(void);
void PortSwapInterval(int vsync); /* whether pictures wait for the display's refresh */
void PortDumpVram(void);
void PortDrawableSize(int *width, int *height);
/* Whether the window has the input focus (always, for the hidden test window). */
int PortWindowFocused(void);
/* Pumps window and input events; returns 0 when the user closed the window. */
int PortPumpEvents(void);

/* libetc.c: one video field passes (VSync, and waits inside the libraries such
   as CD reads): the clock advances, the game's vblank callback runs, the
   display is shown and events are pumped. */
void PsxVBlank(void);
/* The fields that have passed by the wall clock, without waiting: while the second player's game waits
   for the first's over the link (link.c), as the console's vertical blanks go on during a transfer. */
void PsxPassFields(void);
uint64_t PsxFieldCount(void);
uint64_t PsxScanlines(void);  /* since power-on */

/* libgpu.c */
void PsxShowDisplay(void);
/* Frame interpolation (libgpu.c): whether the display draws pictures between the game's frames (the settings, not
   in tests or a linked game); whether there is a frame to move towards now; one such picture, drawn and shown. */
int PsxInterpolationOn(void);
int PsxCanInterpolate(void);
void PsxDrawInterpolated(void);
void PsxDrawInterpolatedAt(float along); /* 0: the frame before .. 1: the frame to come */
int PsxInterpolationTest(void);
void PsxDrawInterpolatedTest(int k, int n);

/* controls.c: scripted key and gamepad events (WIPEOUT_KEYS), fed to SDL's queue each field */
void PortInputScript(uint64_t field);

/* main.c: whether the whole track is drawn (game/include/portfar.h) */
int PortFarViewEnabled(void);

/* Diagnostics */
void PortLog(const char *format, ...);
/* The pictures presented so far (video.c). */
int PortShownFrames(void);

#endif
