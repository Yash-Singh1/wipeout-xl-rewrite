/* The frame recorder (framecap.c): the last few seconds of the rendering pipeline, kept in memory so that
   a glitch can be taken apart after it was seen. For every frame: each primitive libgpu was given, in the
   order drawn, as drawn (its corners with their precise positions and depths, colours, texture, blending,
   whether the GPU's size limit skipped it) and which game code built it (FrameCapTag); the video memory
   it textured from; and the picture shown (with widescreen, the whole wide picture). The settings choose
   what it keeps (RECORD_*): the last 3 seconds, which Tab (debug_ui.cpp) freezes the game on and saves, or
   what is drawn from one Tab to the next while the game goes on. The debug console's Recorder tab steps
   through a recording and explains any pixel. tools/framecap.py reads the saved recordings. */
#ifndef PSX_FRAMECAP_H
#define PSX_FRAMECAP_H

#include <stdint.h>

#include "gpu.h"

#ifdef __cplusplus
extern "C" {
#endif

enum {
    FRAMECAP_POLYGON,
    FRAMECAP_LINE,
    FRAMECAP_RECTANGLE,
    FRAMECAP_FILL
};

/* FrameCapPrim.flags */
#define FRAMECAP_SKIP0 1     /* the first triangle was skipped (the GPU's size limit) */
#define FRAMECAP_SKIP1 2     /* the second triangle of a quad was */
#define FRAMECAP_FAR 4       /* from the port's far view (portfar.h) */
#define FRAMECAP_PRECISE 8   /* drawn from precise projections (perspective-correct) */
#define FRAMECAP_DROPPED 16  /* not drawn at all (a corner behind the camera among corners without depth) */

typedef struct FrameCapPrim {
    uint32_t serial;     /* the order drawn in the frame */
    uint8_t kind, code, flags, count;
    uint16_t tpage, clut;
    int16_t ix[4], iy[4]; /* the packet's integer coordinates */
    GpuVertex v[4];       /* as drawn: video-memory coordinates (the drawing offset added) */
    GpuDrawState draw;
    uint64_t address;     /* the packet */
    char tag[48];         /* what built it (FrameCapTag), or "" */
} FrameCapPrim;

typedef struct FrameCapFrame {
    uint64_t gameFrame;   /* the game's Swap count */
    int shown;            /* the presented frame on which it first appeared (0: not shown) */
    int offsetX, offsetY; /* the drawing offset of its tables */
    int areaX, areaY, areaW, areaH;
    int vram;             /* its video-memory snapshot (FrameCapVram) */
    FrameCapPrim *prims;
    int count, capacity;
    uint8_t *picture;     /* RGBA, pictureW x pictureH, top row first: the picture shown */
    int pictureW, pictureH;
    int pictureX;         /* the column of the picture's left edge, from the area's (widescreen: -the margin) */
} FrameCapFrame;

/* Recording (libgpu). */
void FrameCapTable(int offsetX, int offsetY, int areaX, int areaY, int areaW, int areaH);
FrameCapPrim *FrameCapAdd(void); /* the next primitive's record (NULL when not recording); fill it in */
void FrameCapVramChanged(void);
void FrameCapShown(int x, int y, int w, int h);
void FrameCapTagRecord(FrameCapPrim *prim); /* its tag, from its address */

/* What built the packets from start to end (game code, through PortTagPrims). */
void FrameCapTag(const void *start, const void *end, const char *what, const char *name, int index);

int FrameCapRecording(void);
int FrameCapMode(void); /* RECORD_*: the settings' (the tests': WIPEOUT_FRAMECAP) */
int FrameCapFrozen(void);
/* Tab: with RECORD_LAST_SECONDS freezes the game (libetc's fields wait) and saves the recording, again
   resumes (FrameCapToggleFreeze); with RECORD_WITH_TAB starts a new recording, again stops it. */
void FrameCapTab(void);
void FrameCapToggleFreeze(void);
int FrameCapTaking(void);   /* RECORD_WITH_TAB: a recording is being made */
int FrameCapViewable(void); /* the recording holds still: frozen, or a stopped RECORD_WITH_TAB recording */
int FrameCapGeneration(void); /* changes when the recording held changes (a new one held, or cleared) */
void FrameCapClear(void);   /* forgets the recording (and resumes a frozen game): the settings changed */
const char *FrameCapSavedDir(void);
void FrameCapSave(void);

/* The recording, oldest first. */
int FrameCapFrameCount(void);
const FrameCapFrame *FrameCapFrameAt(int index);
const uint16_t *FrameCapVram(int snapshot);

/* A primitive's triangle that covers a pixel, and what it puts there. */
typedef struct FrameCapHit {
    int prim, triangle;
    float u, v;           /* texture coordinates at the pixel */
    int texel;            /* the 16-bit colour sampled (-1: untextured) */
    float r, g, b, alpha; /* the colour it writes (0-255), alpha the port's fade */
    int transparent;      /* texel 0: nothing written */
    int skipped;          /* the triangle was not drawn (size limit, dropped) */
} FrameCapHit;
/* The triangles of frame `frame` covering video-memory pixel (x, y), in drawing order. */
int FrameCapHits(int frame, int x, int y, FrameCapHit *hits, int max);

#ifdef __cplusplus
}
#endif

#endif
