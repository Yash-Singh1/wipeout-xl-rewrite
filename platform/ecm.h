#ifndef PORT_ECM_H
#define PORT_ECM_H

#include <stddef.h>

/* An ECM file (Neill Corlett's error code modeler: a disc image with the sectors' error codes taken out, as old
   copies of PlayStation discs were kept, "... (Track 01).bin.ecm") made a disc image again, written to out. 0, with
   why into message, when it cannot be. */
int PortEcmDecode(const char *in, const char *out, char *message, size_t messageSize);

/* Whether the file begins as an ECM file does. */
int PortIsEcm(const char *path);

#endif
