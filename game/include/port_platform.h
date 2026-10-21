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

/* The window keeps showing its last picture until the area of video memory on display has been drawn
   again (an ordering table drawn there): for a wipe of video memory between two pictures that the player
   is not meant to see. */
void PsxHoldPicture(void);

/* The HUD's place in the ordering tables Swap draws next: the last entry of the first and the start of
   the second (drawn at half size in a split window, platform/link.c). */
void PsxHudTables(u_long *lastEntry, u_long *secondTable);

/* A buffer of packets that gets link numbers of its own (libgpu numbers every packet address it links;
   a large buffer whose packets move from frame to frame would use up its table). */
void PsxLinkRegion(void *start, size_t bytes);

/* Whether the port draws the whole track in view (portfar.h): not in the lockstep tests (WIPEOUT_TRACE)
   or with WIPEOUT_FAR_VIEW=0. */
int PortFarViewEnabled(void);

#endif
