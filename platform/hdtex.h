/* HD textures (hdtex.c): the player's own texture pack made for DuckStation's texture replacement (none comes
   with the port), its images drawn in place of the console's textures. */
#ifndef PORT_HDTEX_H
#define PORT_HDTEX_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Opens a pack: its .zip as downloaded, or the folder it was extracted to (the one with config.yaml and
   replacements/, or a folder above it). Returns the number of replacements found (0: none, and message says
   why); "" closes the pack. */
int HdTexOpen(const char *path, char *message, size_t messageSize);
const char *HdTexSource(void);  /* the pack opened, "" when none */
int HdTexCount(void);           /* its replacements */
const char *HdTexAbout(void);   /* what the pack says about itself (config.yaml's heading), "" when nothing */
/* The pack is drawn: the setting is on and a pack is open (the settings' pack is opened the first time). */
int HdTexActive(void);

/* Video memory changed (any way: gpu_gl.c's GpuVramChanged). */
void HdTexChanged(int x, int y, int w, int h);
/* ... by an upload (LoadImage): a texture a replacement may be named after. */
void HdTexUploaded(int x, int y, int w, int h);
/* ... by a copy or a fill: the uploads it covers are gone. */
void HdTexOverwritten(int x, int y, int w, int h);

/* A replacement for a textured primitive: the image (HdTexImage), the texels of the page it covers and the
   part of the image there, in the replaced texture's texels (nameW x nameH of them across the whole image). */
typedef struct HdTexHit {
    int image;
    int x, y, w, h;
    int srcX, srcY;          /* (w x h texels from there) */
    int nameW, nameH;
} HdTexHit;
/* The replacement covering most of the texels (u0, v0) to (u1, v1) of a primitive textured from the page at
   (pageX, pageY), colour depth 0-2, palette at (clutX, clutY). 0 when none. */
int HdTexFind(int pageX, int pageY, int depth, int clutX, int clutY, float u0, float v0, float u1, float v1,
              HdTexHit *hit);
/* An image's pixels (RGBA; alpha 0 transparent, 128 semi-transparent, 255 opaque), or NULL while it is being
   decoded (asked for here). They change, as do their sizes, when HdTexEpoch does. */
const uint8_t *HdTexImage(int image, int *w, int *h);
int HdTexImageCount(void);
unsigned HdTexEpoch(void);
/* The images' scale: texels per console texel (the framebuffer's). */
void HdTexSetScale(int scale);

#ifdef __cplusplus
}
#endif

#endif
