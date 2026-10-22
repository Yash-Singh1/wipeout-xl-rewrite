/* The original Wipeout's tracks raced in XL (original.c): the player's copy of the original, for the debug
   console. The game's side is in game/include/port_platform.h (PortOriginalTrack...). */
#ifndef PORT_ORIGINAL_H
#define PORT_ORIGINAL_H

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

#ifdef __cplusplus
}
#endif

#endif
