#ifndef PORT_PLATFORM_H_GAME
#define PORT_PLATFORM_H_GAME

/* What the game needs from the platform beyond the PsyQ libraries.  Implemented by platform/. */

/* Plays the intro/outro movie that the console played by loading a movie-player executable
   (fileName is that executable's name, e.g. "c:\\xtro1.exe"); returns when it has finished
   or was skipped. */
void PsxPlayMovie(const char *fileName);

/* The console's 1 KB scratchpad (0x1F800000), which the game uses for transformed vertices
   (screen coordinates at +0, depths at +0x200, clip flags at +0x300).  Defined in game/src/object.c. */
extern unsigned char PsxScratchpad[1024];

#endif
