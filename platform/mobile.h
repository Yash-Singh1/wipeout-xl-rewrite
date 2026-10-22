/* Phones and tablets (mobile.c): the folder the player's disc image is put in, and Android's picked documents
   copied into it. */
#ifndef PORT_MOBILE_H
#define PORT_MOBILE_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* The folder the player puts files in, with a separator at the end: iOS's Documents (the Files app), Android's
   app folder on the device's storage; NULL elsewhere. */
const char *PortImportFolder(void);
/* A disc image in it (a .cue before an archive before a .bin, the nearest the top): 1 and its path. */
int PortFindDisc(char *out, size_t size);
/* A file to use: a content:// document (Android's picker) is copied into the folder first, under its name;
   anything else is used as it is. 1 and the local path in out, else message says why. */
int PortImportFile(const char *uri, char *out, size_t outSize, char *message, size_t messageSize);
/* How far the copy going on (another thread's) is, in thousandths. */
int PortImportProgress(void);

#ifdef __cplusplus
}
#endif

#endif
