/* Shared between the platform modules (not included by game code). */
#ifndef PORT_PLATFORM_H
#define PORT_PLATFORM_H

#include <stdint.h>

typedef struct PortConfig {
    const char *discPath;    /* the PS1 disc image's .cue */
    const char *pcDataPath;  /* optional: the PC release (intro movies, keyboard glyphs) */
    const char *savePath;    /* writable: memory card files */
    int windowWidth, windowHeight, fullscreen;
    int muted;               /* tests: no audio device */
    int hidden;              /* tests: no visible window */
    int fixedStep;           /* tests: no real-time pacing; time advances only with the game */
    int frameLimit;          /* tests: stop after this many video fields (0 = no limit) */
    const char *captureDir;  /* tests: save shown frames as PPM */
    int captureEvery;
} PortConfig;
extern PortConfig portConfig;

/* video.c */
int PortVideoInit(void);
void PortVideoShutdown(void);
void PortVideoPresent(void);
void PortDrawableSize(int *width, int *height);
/* Pumps window and input events; returns 0 when the user closed the window. */
int PortPumpEvents(void);

/* libetc.c: one video field passes (VSync, and waits inside the libraries such
   as CD reads): the clock advances, the game's vblank callback runs, the
   display is shown and events are pumped. */
void PsxVBlank(void);
uint64_t PsxFieldCount(void);
uint64_t PsxScanlines(void);  /* since power-on */

/* libgpu.c */
void PsxShowDisplay(void);

/* Diagnostics */
void PortLog(const char *format, ...);

#endif
