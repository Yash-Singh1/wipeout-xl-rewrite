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
/* An entry of the Saturn's archive by its path (art\track01\course01.rbh), read whole; the caller frees. */
unsigned char *PortSaturnEntry(const char *path, size_t *size);
/* Plays one of its movies (saturn_movie.c), by its path in the archive; 0 when it cannot. */
int SaturnPlayMovie(const char *path);
/* The sounds (saturn_sound.c): samples.wad from the Saturn's sound container's blocks, with the PlayStation's names
   in its order. The caller frees. */
unsigned char *SaturnMakeSamples(const unsigned char *const *blocks, const unsigned int *sizes, int blockCount,
                                 const char *const *names, size_t *size);
/* The font (saturn_font.c): the glyphs of a size (16, 12 or 8) drawn into a 4-bit page of 256 rows. */
void SaturnDrawFont(unsigned char *page, int pageWidth, int size);

#ifdef __cplusplus
}
#endif

#endif
