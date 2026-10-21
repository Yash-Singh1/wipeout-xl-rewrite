/* The drawing backend under libgpu (gpu_gl.c): the PlayStation's
   primitives drawn at any resolution with OpenGL, textured straight from a
   copy of the console's video memory. */
#ifndef PSX_GPU_H
#define PSX_GPU_H

#include <stdint.h>

#define VRAM_WIDTH 1024
#define VRAM_HEIGHT 512

/* The console's video memory, 16-bit pixels; libgpu writes it, the backend
   textures from it. */
extern uint16_t psxVram[VRAM_HEIGHT][VRAM_WIDTH];
void GpuVramChanged(int x, int y, int w, int h);

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
    uint8_t background;  /* from a projection drawn behind everything (the sky): no depth test */
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
    int depthTest;     /* the port's depth buffer: 0 none (2D, the sky), 1 test and write, 2 test only */
} GpuDrawState;

int GpuInit(void);
/* The drawing area (a rectangle of video memory) the next primitives go to. */
void GpuSetDrawArea(int x, int y, int w, int h, int offsetX, int offsetY);
void GpuTriangles(const GpuDrawState *state, const GpuVertex *vertices, int count);
void GpuLine(const GpuDrawState *state, const GpuVertex *a, const GpuVertex *b);
/* Fills a rectangle of video memory (BLK_FILL, ClearImage, background clears). */
void GpuFill(int x, int y, int w, int h, uint8_t r, uint8_t g, uint8_t b);
/* Shows the given rectangle of video memory. */
void GpuPresent(int x, int y, int w, int h);
/* The depth buffer of an area is cleared (each ordering table starts afresh). */
void GpuClearDepth(int x, int y, int w, int h);
/* Two players on one machine (link.c): the area (x, y, w, h) of video memory in the upper half of the
   window and another picture (RGBA, rows from the bottom; lowerNew: changed since the last call) in the
   lower half; and an area read back at the framebuffer's scale (RGBA, rows from the bottom). */
void GpuPresentSplit(int x, int y, int w, int h, const uint8_t *lower, int lowerW, int lowerH, int lowerNew);
uint8_t *GpuReadArea(int x, int y, int w, int h, int *outW, int *outH);
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
/* Shows an RGB image (a movie frame) across the 4:3 picture, letterboxed
   when it is wider (square pixels). */
void GpuShowImage(const uint8_t *rgb, int width, int height);

/* Widescreen: how far the picture reaches beyond each side of the screen's 320 columns while a race is shown
   wide (0: the console's 4:3). Also the game's (port_platform.h), with PortWideScene. */
int PortScreenMargin(void);

#endif
