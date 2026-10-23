/* The drawing backend under libgpu (gpu_gl.c): the PlayStation's
   primitives drawn at any resolution with OpenGL, textured straight from a
   copy of the console's video memory. */
#ifndef PSX_GPU_H
#define PSX_GPU_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define VRAM_WIDTH 1024
#define VRAM_HEIGHT 512

/* The console's video memory, 16-bit pixels; libgpu writes it, the backend
   textures from it. */
extern uint16_t psxVram[VRAM_HEIGHT][VRAM_WIDTH];
void GpuVramChanged(int x, int y, int w, int h);

/* The second half of video memory, as the PlayStation's 2 MB development GPU had: rows 512 to 1023, textures
   only (nothing is drawn there), reached with bit 11 of a texture page (its
   "Y base 2"). The console game never uses it; the original Wipeout's textures do when its tracks are raced
   (platform/original.c), as they do not fit beside XL's own. Wipeout 64's palettes go there too, reached with
   bit 15 of a palette's address (the row's tenth bit: rows 512 on), which the console's GPU ignores. */
extern uint16_t psxVramHigh[VRAM_HEIGHT][VRAM_WIDTH];
void GpuVramHighChanged(void);

/* The port's own page of texture memory, beyond the console's video memory, that a textured
   primitive reads when its texture page says colour depth 3 (the depth the console's GPU leaves
   reserved; the game never uses it). The button glyphs of the keyboard and of other gamepads
   live here. The page's coordinates are the primitive's UVs (256x256); its texels are four times
   finer, with smooth coverage, and are sampled filtered. */
#define GLYPH_PAGE_SIZE 256   /* the glyph page's texture coordinates (a quad's UV bytes) */
#define GLYPH_ATLAS_SCALE 4   /* ... and its texels per coordinate */
#define GLYPH_ATLAS_SIZE (GLYPH_PAGE_SIZE * GLYPH_ATLAS_SCALE)
void GpuSetGlyphMemory(const uint32_t *rgba); /* RGBA8, premultiplied by the coverage */

/* A vertex: position in the drawing area's pixels (fractional when the GTE
   supplied a precise one), w for perspective-correct texturing (1 when not
   known), colour 0-255 per channel (128 = unmodulated texture), texture
   coordinates in texels. */
typedef struct GpuVertex {
    float x, y, w;
    uint8_t r, g, b, a;
    float u, v;
    int noDepth;    /* with a depth (w): left out of the depth buffer (the sky; corners only found by their pixels) */
} GpuVertex;

/* How a primitive is drawn. */
typedef struct GpuDrawState {
    int textured;
    int raw;           /* texture without colour modulation */
    int semi;          /* -1 opaque, else blend mode 0-3 */
    int depth;         /* texture colours: 0 = 4-bit, 1 = 8-bit, 2 = 15-bit, 3 = the glyph page */
    int pageX, pageY;  /* texture page origin in video memory */
    int clutX, clutY;  /* palette origin */
    int windowMaskX, windowMaskY, windowOffsetX, windowOffsetY; /* texture window, in 8-texel units */
    int dither;
    int fade;          /* the port's distance fade: the vertices' alpha (GpuVertex.a) blends it out */
} GpuDrawState;

int GpuInit(void);
/* The framebuffer's scale (its pixels per console pixel), the largest it can be, and the window's (the
   settings' resolution 0: what it follows). */
int GpuScale(void);
int GpuMaxScale(void);
int GpuWindowScale(void);
/* The drawing area (a rectangle of video memory) the next primitives go to. */
void GpuSetDrawArea(int x, int y, int w, int h, int offsetX, int offsetY);
void GpuTriangles(const GpuDrawState *state, const GpuVertex *vertices, int count);
void GpuLine(const GpuDrawState *state, const GpuVertex *a, const GpuVertex *b);
/* Fills a rectangle of video memory (BLK_FILL, ClearImage, background clears). */
void GpuFill(int x, int y, int w, int h, uint8_t r, uint8_t g, uint8_t b);
/* ... of the drawn picture only (an interpolated picture's background: video memory keeps the game's). */
void GpuClearArea(int x, int y, int w, int h, uint8_t r, uint8_t g, uint8_t b);
/* Shows the given rectangle of video memory. */
void GpuPresent(int x, int y, int w, int h);
/* ... a picture between two of the game's frames (frame interpolation, libgpu.c): not a new frame of its own. */
void GpuPresentInterpolated(int x, int y, int w, int h);
/* What is drawn while on is such a picture. */
void GpuInterpolating(int on);
/* Two players on one machine (link.c): the area (x, y, w, h) of video memory in the upper half of the
   window and another picture (RGBA, rows from the bottom; lowerNew: changed since the last call) in the
   lower half; and an area read back at the framebuffer's scale (RGBA, rows from the bottom). */
void GpuPresentSplit(int x, int y, int w, int h, const uint8_t *lower, int lowerW, int lowerH, int lowerNew);
/* The band of console rows each player's half shows (libgpu.c): below the middle, where the ship is, and a few
   rows more than half the picture (a little more of the view, with widescreen's sides to match). */
#define GPU_SPLIT_TOP 76
#define GPU_SPLIT_ROWS 136
uint8_t *GpuReadArea(int x, int y, int w, int h, int *outW, int *outH);
/* The depth buffer in that area of video memory starts afresh (each ordering table: libgpu's DrawOTag). */
void GpuClearDepth(int x, int y, int w, int h);
/* Brings psxVram up to date with what was drawn into the rectangle (before
   the CPU reads or copies it). */
void GpuReadBack(int x, int y, int w, int h);
/* Puts pixels the CPU wrote to video memory (LoadImage, MoveImage) into the
   drawn picture as well. */
void GpuWriteFramebuffer(int x, int y, int w, int h);
/* MoveImage's copy in the drawn picture. */
void GpuMoveFramebuffer(int x, int y, int w, int h, int toX, int toY);
/* Finishes the frame's drawing (before presenting). */
void GpuFlush(void);
/* The window's framebuffer, when it is not 0 (iOS: SDL's view draws into one of its own). */
void GpuSetWindowFramebuffer(unsigned int number);
/* Clears the window to a colour, its framebuffer bound for drawing over (the start-up's messages, DebugUiSplash). */
void GpuClearWindow(float r, float g, float b);
/* Shows an RGB image (a movie frame) across the 4:3 picture, letterboxed
   when it is wider (square pixels). */
void GpuShowImage(const uint8_t *rgb, int width, int height);

/* Widescreen: how far the picture reaches beyond each side of the screen's 320 columns while a race or a menu is
   shown wide (0: the console's 4:3). Also the game's (port_platform.h), with PortWideScene. */
int PortScreenMargin(void);
/* Widescreen: the picture being drawn is a race's (everything reaches the sides, the HUD goes to its edges), not
   a menu's. */
int GpuRaceWide(void);
/* Widescreen: how far the picture shown from (x, w) reaches beyond each side (0: it is 4:3). */
int GpuShownMargin(int x, int w);
/* The picture at (x, y, w, h) of video memory, with `margin` columns beyond each side (GpuShownMargin), as
   outW x outH RGBA, top row first (the frame recorder's). */
uint8_t *GpuReadPicture(int x, int y, int w, int h, int margin, int outW, int outH);
/* ... the same, without waiting for the GPU to finish the picture: one at a time for each reader (0 while one is on
   its way); GpuReadPictureDone gives it to `use` once it is back (1; with `wait`, waiting for it), else 0. */
enum { GPU_READ_LINK, GPU_READ_RECORDER, GPU_READERS };
int GpuReadPictureStart(int reader, int x, int y, int w, int h, int margin, int outW, int outH);
int GpuReadPictureDone(int reader, int wait, void (*use)(const uint8_t *pixels, int width, int height, void *user),
                       void *user);
int GpuReadPictureBusy(int reader);
/* The second player's game handed its picture over (the split window) instead of showing it. */
void GpuPictureHandedOver(void);
/* A menu's backdrop was built this frame (PortWideBackdrop in libgpu): the menu is shown wide. */
void GpuWideMenu(void);
/* A loading screen is being built (PortWideLoadScreen): shown wide, from this picture on. */
void GpuWideLoadScreen(void);
/* The primitives that follow are (1) or are not (0) a menu's backdrop, which widescreen enlarges to fill the
   sides. */
void GpuBackdrop(int on);

#ifdef __cplusplus
}
#endif

#endif
