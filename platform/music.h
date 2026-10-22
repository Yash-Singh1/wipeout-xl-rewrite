/* The player's own music (music.c): a folder of music files heard in place of the disc's soundtrack. */
#ifndef PORT_MUSIC_H
#define PORT_MUSIC_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Opens the settings' folder (or WIPEOUT_MUSIC's) and takes the drive's music; at start-up. */
void PortMusicInit(void);
/* Opens a folder (and those in it): returns the songs found (0: none, and message says why); "" forgets it. */
int PortMusicOpen(const char *folder, char *message, size_t messageSize);
const char *PortMusicFolder(void);
int PortMusicCount(void);
/* The song playing (its file's name), "" when the disc's music is heard. */
const char *PortMusicPlaying(void);
/* The next song now. */
void PortMusicSkip(void);
/* The settings changed (on or off, the order). */
void PortMusicSettingsChanged(void);

#ifdef __cplusplus
}
#endif

#endif
