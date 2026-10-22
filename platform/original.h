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
int PortOriginalTrackCount(void);

#ifdef __cplusplus
}
#endif

#endif
