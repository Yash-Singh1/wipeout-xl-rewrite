/* The sound, disc, controller and memory card side of the PlayStation, for the port's
   platform layer (see the libspu, libcd, libpad and libcard sources here for the PsyQ entry points the game calls).

   Threads. The game runs on one thread (it calls the libspu/libcd/libpad/libcard entry
   points and never returns from them while "waiting"). The audio device, when there is
   one, pulls samples from its own thread. A single lock protects the SPU, the CD drive
   and the event table; entry points take it briefly and never call out while holding it.

   Time. The SPU and the CD drive share one clock, the count of output samples (44100 a
   second). With an audio device the device advances it. In fixed-step mode (and when there
   is no device) it advances only when the game passes time: PsxIoField() once per video
   field, to exactly field * 44100 * 1001 / 60000 samples, so the audio is the same on every
   run. With queued audio (PsxIoConfig.queueAudio) time passes the same way and the samples go
   to the device. Waits inside the libraries (a CD read, a seek) pass the video fields that elapse
   while waiting by calling PsxVBlank(), as the console's vertical blanks go on during them. */
#ifndef PSX_IO_H
#define PSX_IO_H

#include <stdint.h>

typedef struct PsxIoConfig {
    const char *discPath;   /* the .cue of the PS1 disc image (required) */
    const char *savePath;   /* directory for the memory card images (NULL: current directory) */
    int fixedStep;          /* deterministic: audio time comes from PsxIoField only */
    int muted;              /* no audio output device (real-time runs use SDL's dummy driver) */
    int noDevice;           /* never open an audio device (tests); implied by fixedStep */
    double cdSpeed;         /* CD mechanics speed-up over the console's drive; 0 or 1 = as is */
    const char *padScript;  /* scripted pad input file (see pad.c); NULL for live input */
    const char *wavPath;    /* write the mixed audio here (fixed-step runs) */
    int noCdAudio;          /* the disc's audio tracks are not heard (the second player's game) */
    int oneCard;            /* no card in the second slot unless its image exists (the console recordings') */
    int queueAudio;         /* time follows the video fields and their samples are queued for the device, which
                               then never sets the time (the web, where the device's callback can only run while
                               the game waits) */
} PsxIoConfig;

/* Returns 1 on success; on failure fills error (when non-NULL). */
int PsxIoInit(const PsxIoConfig *config, char *error, int errorSize);
void PsxIoShutdown(void);

/* One video field has started (field is the running count). Call it from PsxVBlank() before
   the game's VSync callback runs: it advances audio time in fixed-step mode, delivers CD and
   card events (and runs event handlers the game installed) and samples the pad. */
void PsxIoField(uint64_t field);

/* The pad: input events from the window system (platform/pad_sdl.c) and the buttons
   currently held on port 0 or 1, active high, in the console's bit layout. */
uint16_t PsxPadButtons(int port);
/* Every SDL event (keyboard, gamepad added/removed/buttons/axes, focus) goes through here. */
union SDL_Event;
void PsxInputEvent(const union SDL_Event *event);

/* A hook for the movie programs: Exec(program) calls it when set. */
typedef int (*PsxExecHook)(void *block);
void PsxSetExecHook(PsxExecHook hook);

/* Headphone surround (surround.c): the sounds a game's Dolby Surround mode puts behind the player are heard from
   behind, on headphones; off, the output is the console's. */
void PsxAudioSetSurround(int headphones);

/* The lock (above), for the platform's own audio that the drive or the SPU call into (platform/music.c), while
   it changes what they play. */
void PsxLock(void);
void PsxUnlock(void);

/* The memory cards (libcard.c), for the debug console's saves: the files on a card (port 0 or 1; names of 20
   characters), and a file put onto one (replacing one of the same name; 1 when written). */
int PsxCardList(int port, char names[15][21], int sizes[15]);
int PsxCardWriteFile(int port, const char *name, const void *data, int size);

/* The disc image (disc.h), for the movie player; NULL before PsxIoInit. */
struct Disc;
const struct Disc *PsxIoDisc(void);

#endif
