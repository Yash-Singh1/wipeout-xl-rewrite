/* The original Wipeout's tracks raced in XL (original.c): the player's copy of the original, for the debug
   console. The game's side is in game/include/port_platform.h (PortOriginalTrack...). */
#ifndef PORT_ORIGINAL_H
#define PORT_ORIGINAL_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Opens the original's disc image (.cue) or data folder; returns the number of its tracks found (0: none,
   and message says why) and says so in message. */
int PortOriginalOpen(const char *path, char *message, int messageSize);
void PortOriginalClose(void);
const char *PortOriginalSource(void); /* the path opened, "" when none */
int PortOriginalTrackCount(void);   /* the guests: the original's tracks found, then Wipeout 64's */
int PortOriginalFoundCount(void);   /* ... the original's */

/* Wipeout 64's tracks (the player's cartridge image: .z64, .n64 or .v64), raced the same way. */
int PortWipeout64Open(const char *path, char *message, int messageSize);
void PortWipeout64Close(void);
const char *PortWipeout64Source(void); /* "" when none */
int PortWipeout64TrackCount(void);

/* The games' compression of texture sequences (.cmp): unpacked into out (size bytes at most; returns how many),
   and the same bytes as a stream without back references (the caller frees). */
size_t PortLzssDecompress(const uint8_t *in, size_t length, uint8_t *out, size_t size);
uint8_t *PortLzssStore(const uint8_t *in, size_t length, size_t *size);

#ifdef __cplusplus
}
#endif

#endif
