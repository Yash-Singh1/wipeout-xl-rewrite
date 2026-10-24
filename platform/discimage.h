/* Disc images as they are kept (discimage.c): a .cue with its .bin tracks, the .bin (or .img) of a disc without its
   .cue, or an archive holding either (.7z, .zip, .rar, .tar and the compressed kinds), used as they are. */
#ifndef PORT_DISCIMAGE_H
#define PORT_DISCIMAGE_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* The file kinds the disc can be chosen as (for file dialogs: separated by ';'). */
#define PORT_DISC_EXTENSIONS "cue;bin;img;ecm;7z;zip;rar;tar;gz;tgz;bz2;tbz2;xz;txz;zst;tzst"

/* Whether a path is an archive: by its first bytes (7z, zip, rar, tar, gzip, bzip2, xz, zstd), else by its name. */
int PortIsArchive(const char *path);
/* Whether a file's name is of one of PORT_DISC_EXTENSIONS's kinds. */
int PortIsDiscName(const char *name);
/* The .cue of the disc image at path: the .cue itself; for a .bin or .img without one, a .cue made for it (in the
   user's data folder, discs/); for an archive, its image's, extracted the first time (into discs/<the archive's
   name>/) and used as it is after that. 1 when there, else message says why. An archive's progress goes to the
   window's title (on the main thread) and to PortArchiveProgress. */
int PortDiscImage(const char *path, char *cue, size_t cueSize, char *message, size_t messageSize);
/* How far the extraction going on (another thread's) is, in thousandths. */
int PortArchiveProgress(void);

#ifdef __cplusplus
}
#endif

#endif
