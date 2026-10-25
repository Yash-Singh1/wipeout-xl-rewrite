/* Disc images as they are kept (discimage.c): a .cue with its .bin tracks, an .iso, a CloneCD .ccd with its .img, an
   Alcohol 120% .mds with its .mdf, the .bin (or .img) of a disc without its .cue, a version 5 CD CHD, or an archive
   holding an image (.7z, .zip, .rar, .tar and the compressed kinds). */
#ifndef PORT_DISCIMAGE_H
#define PORT_DISCIMAGE_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* The file kinds the disc can be chosen as (for file dialogs: separated by ';'). */
#define PORT_DISC_EXTENSIONS "cue;bin;img;iso;ccd;mds;mdf;chd;ecm;7z;zip;rar;tar;gz;tgz;bz2;tbz2;xz;txz;zst;tzst"

/* Whether a path needs unpacking: an archive or CHD by its first bytes, else an archive by name. */
int PortIsArchive(const char *path);
/* Whether a file's name is of one of PORT_DISC_EXTENSIONS's kinds. */
int PortIsDiscName(const char *name);
/* The .cue of the disc image at path: the .cue itself (an .iso, .ccd and .mds too: DiscOpen opens them as they are); an
   .img or .mdf with its .ccd or .mds beside it, that; for a .bin or .img without one, a .cue made for it (in the
   user's data folder, discs/); for an archive, its image's, extracted the first time (into discs/<the archive's
   name>/) and used as it is after that; a CHD is made a bin/cue pair there in the same way. 1 when there, else
   message says why. Progress goes to the
   window's title (on the main thread) and to PortArchiveProgress. */
int PortDiscImage(const char *path, char *cue, size_t cueSize, char *message, size_t messageSize);
/* The file's name in path (after its last '/', or on Windows its last backslash). */
const char *PortPathName(const char *path);
/* How far the extraction going on (another thread's) is, in thousandths. */
int PortArchiveProgress(void);

#ifdef __cplusplus
}
#endif

#endif
