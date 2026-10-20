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

/* A vertex: position in the drawing area's pixels (fractional when the GTE
   supplied a precise one), w for perspective-correct texturing (1 when not
   known), colour 0-255 per channel (128 = unmodulated texture), texture
   coordinates in texels. */
typedef struct GpuVertex {
    float x, y, w;
    uint8_t r, g, b, a;
    float u, v;
} GpuVertex;

/* How a primitive is drawn. */
typedef struct GpuDrawState {
    int textured;
    int raw;           /* texture without colour modulation */
    int semi;          /* -1 opaque, else blend mode 0-3 */
    int depth;         /* texture colours: 0 = 4-bit, 1 = 8-bit, 2 = 15-bit */
    int pageX, pageY;  /* texture page origin in video memory */
    int clutX, clutY;  /* palette origin */
    int windowMaskX, windowMaskY, windowOffsetX, windowOffsetY; /* texture window, in 8-texel units */
    int dither;
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
/* Brings psxVram up to date with what was drawn into the rectangle (before
   the CPU reads or copies it). */
void GpuReadBack(int x, int y, int w, int h);
/* Finishes the frame's drawing (before presenting). */
void GpuFlush(void);
/* Shows an RGB image (a movie frame) filling the 4:3 picture. */
void GpuShowImage(const uint8_t *rgb, int width, int height);

#endif
