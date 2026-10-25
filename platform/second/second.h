/* Two players in one program: the second player's game is a second copy of the whole game (the game, the console's
   libraries, the platform layer) compiled into the program under names of its own (tools/second_game.py), which
   runs on a thread of its own beside the first. The two talk as two consoles over the link cable did (link.c);
   the first shows the second's picture in the lower half of its window.

   host.c (the first game's side) starts and stops it; entry.c is its entry point and stands in for what would
   end or change the whole program. */
#ifndef PORT_SECOND_H
#define PORT_SECOND_H

#include <SDL3/SDL.h>

/* Built into this program (the desktop): else (the web, a phone) there is no second game. */
int PortSecondGameBuilt(void);

/* The first game's side. Start: from the main thread (it makes the second game's hidden window); the arguments
   are its command line, the memory is what the two share. Running: until its thread has ended. Stop: waits for
   its thread to end (after it was told to, link.c) up to the time given; 0 when it did not end. */
int PortSecondGameStart(void *shared, int argc, const char **argv);
int PortSecondGameRunning(void);
int PortSecondGameStop(int milliseconds);

/* The second game's side: what the first made for it. */
void *PortSecondGameShared(void);
SDL_Window *PortSecondGameWindow(void);
SDL_GLContext PortSecondGameContext(void);

/* entry.c: the second game's main(), and what its sources call in place of the C library's (exit, atexit, getenv,
   malloc, calloc, realloc, free) and SDL's (SDL_getenv) */
int PortSecondGameMain(int argc, char **argv);
int PortSecondGameMainEntry(int argc, char **argv); /* its own main(), renamed */
void PortSecondExit(int code);
int PortSecondAtExit(void (*function)(void));
char *PortSecondGetenv(const char *name);
void *PortSecondMalloc(size_t size);
void *PortSecondCalloc(size_t count, size_t size);
void *PortSecondRealloc(void *block, size_t size);
void PortSecondFree(void *block);
const char *PortSecondSDLGetenv(const char *name);

#endif
