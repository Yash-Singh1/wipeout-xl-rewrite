/* The Sega Saturn's Wipeout XL / WipEout 2097 disc in place of the PlayStation's (saturn.c). The game's side
   (its file layer's PortSaturnFileInfo and PortSaturnRead) is in game/include/port_platform.h. */
#ifndef PORT_SATURN_H
#define PORT_SATURN_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

struct Disc;

/* Whether a disc image is the Saturn's Wipeout XL (Japan) or WipEout 2097 (Europe). */
int PortSaturnDiscIs(const struct Disc *disc);
/* Whether the disc in use is; its name. */
int PortSaturnDisc(void);
const char *PortSaturnTitle(void);
/* Whether it is WipEout 2097 (Europe). */
int PortSaturnEurope(void);
/* An entry of the Saturn's archive by its path (art\track01\course01.rbh), read whole; the caller frees. */
unsigned char *PortSaturnEntry(const char *path, size_t *size);
/* Plays one of its movies (saturn_movie.c), by its path in the archive; 0 when it cannot. */
int SaturnPlayMovie(const char *path);
/* The sounds (saturn_sound.c): samples.wad from the Saturn's sound container's blocks, with the PlayStation's names
   in its order. The caller frees. */
unsigned char *SaturnMakeSamples(const unsigned char *const *blocks, const unsigned int *sizes, int blockCount,
                                 const char *const *names, size_t *size);
/* Its video memory images and containers (saturn.c): a .vri loaded into a copy of the VDP1 and VDP2 memories and
   colour RAM (SDL_free it), the memory at a Saturn address; a container's block, unpacked (the caller frees). */
typedef struct SaturnVideo SaturnVideo;
SaturnVideo *SaturnLoadVideo(const char *path);
unsigned char *SaturnVideoAt(SaturnVideo *v, unsigned int address, size_t bytes);
unsigned char *SaturnBlock(const char *path, int block, size_t *size);
/* The Saturn's art for one of the PlayStation's TIMs (saturn_art.c), by its name (of a .cmp, its index): its texels
   (palette indices for depth 0 and 1, colours for 2) and palette. 0 when the Saturn has none. */
int SaturnArtTim(const char *name, int index, int depth, int w, int h, unsigned short *clut, int clutSize,
                 unsigned short *texels);
/* A glyph of the PlayStation's font (A-Z, 0-9, ':', '.') as the Saturn's HUD draws it (saturn_art.c), white RGBA, its
   big letters (16 lines) or small (8). */
int SaturnFontGlyph(int glyph, int small, unsigned int *rgba, int w, int h);
/* The font (saturn_font.c): the glyphs of a size (16, 12 or 8) drawn into a 4-bit page of 256 rows. */
void SaturnDrawFont(unsigned char *page, int pageWidth, int size);

#ifdef __cplusplus
}
#endif

#endif
