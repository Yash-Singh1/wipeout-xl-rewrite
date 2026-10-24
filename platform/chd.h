/* A version 5 CD CHD made its raw .bin and .cue, in the first-use conversion folder (discimage.c). */
#ifndef PORT_CHD_H
#define PORT_CHD_H

#include <stddef.h>

int PortChdDecode(const char *in, const char *bin, const char *cue, void (*progress)(int),
                  char *message, size_t messageSize);

#endif
