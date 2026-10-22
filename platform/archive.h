/* Disc images in archives (archive.c): a .7z holding a disc image (its .cue and .bin tracks), as such images are
   often kept, used as it is. */
#ifndef PORT_ARCHIVE_H
#define PORT_ARCHIVE_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Whether a path is an archive (by its name: .7z). */
int PortIsArchive(const char *path);
/* The disc image in an archive: extracted the first time (into the user's data folder, discs/), its .cue's path
   into cue; 1 when there, else message says why. Progress goes to the window's title. */
int PortArchiveDisc(const char *archive, char *cue, size_t cueSize, char *message, size_t messageSize);
/* How far the extraction going on (another thread's) is, in thousandths. */
int PortArchiveProgress(void);

#ifdef __cplusplus
}
#endif

#endif
