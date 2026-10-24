/* The PC CD-ROM version's data in place of the PlayStation disc (pc.c; docs/PC.md). The game's side (its file layer's
   PortPcFileInfo and PortPcRead, PortPcData) is in game/include/port_platform.h. */
#ifndef PORT_PC_H
#define PORT_PC_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

struct Disc;

/* Whether a disc image holds the PC version's files (its COMMON and TRACK01 folders at the root, no WIPEOUT2). */
int PortPcDiscIs(const struct Disc *disc);
/* Whether a folder holds them (the CD's files copied, or an installed copy with them). */
int PortPcFolderIs(const char *path);
/* Uses the PC version's files in a folder from now on, the game playing without music: an empty disc image (a data
   track with nothing on it) is made in the user's data folder to stand in for the CD, its path into disc. 0 (and
   why, into message) when it cannot be. */
int PortPcUseFolder(const char *path, char *disc, size_t discSize, char *message, size_t messageSize);
/* Whether the PC version's files are in use (the disc in use, or a folder given). */
int PortPcData(void);
/* The PC version's name, for the debug console. */
const char *PortPcTitle(void);
/* A file of the PC CD by its path from the CD's root (PICS\HI-RES\HUD2.TGA, any case), read whole; NULL when there
   is none. The caller frees (SDL_free). */
unsigned char *PortPcLoadFile(const char *path, size_t *size);
/* The PC version's texture pages (game/include/port_platform.h, PortPcLoadPages, PortPcLoadPicture). */
int PortPcLoadPages(const char *name, int firstPage, int pageCount);
int PortPcLoadPicture(const char *name, int firstPage);
/* Plays a movie of the PC CD (MPEG\WXLINTR.MPG), as movie.c plays the PlayStation's: skipped by Start or Cross, the
   sound from the file's user data (pc_movie.c). Returns 0 when it is not there. */
int PortPcPlayMovie(const char *path);
/* The game's samples.wad made from the PC's WAV sounds (pc_sound.c); NULL when one is missing. The caller frees. */
unsigned char *PcMakeSamples(size_t *size);

#ifdef __cplusplus
}
#endif

#endif
