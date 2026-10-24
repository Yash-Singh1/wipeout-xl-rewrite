#ifndef PORT_APE_H
#define PORT_APE_H

#include <stddef.h>

/* A Monkey's Audio file (.ape: a disc's audio track as old copies of PlayStation discs kept them) decoded to the
   track's raw samples (16-bit little-endian, as a .bin of the track holds them), written to out. Files of version
   3.99 and later (the format since 2004). 0, with why into message, when it cannot be. */
int PortApeDecode(const char *in, const char *out, char *message, size_t messageSize);

#endif
