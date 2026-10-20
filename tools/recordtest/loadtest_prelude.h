/* Included before every game source that tools/recordtest/loadtest links: the game's heap calls go
   through the test's recording allocator, with the source line of the call. */
#ifndef LOADTEST_PRELUDE_H
#define LOADTEST_PRELUDE_H

#include "functions.h"

void *HarnessAlloc(HeapBlock *head, int size, const char *file, int line);
void HarnessFree(HeapBlock *head, char *ptr, const char *file, int line);
void HarnessInitHeap(HeapBlock *head, int size, const char *file, int line);

#define DAlloc(head, size) HarnessAlloc((head), (size), __FILE__, __LINE__)
#define DFree(head, ptr) HarnessFree((head), (char *)(ptr), __FILE__, __LINE__)
#define InitDynamicMem(head, size) HarnessInitHeap((head), (size), __FILE__, __LINE__)

/* The patched PsxMain (stage 0, start-up done), MenuControl (stage 1, before its menu loop) and race
   (stage 2, before its main loop) call this where their loading is done; a nonzero result makes them
   return. */
extern int LoadtestHook(int stage);
extern char *LoadtestMainHeap(void);
extern char *LoadtestReserveHeap(void);

/* The game stops on a fatal error with exit(0); the test wants to know. */
extern void LoadtestExit(int status);
#define exit(status) LoadtestExit(status)

#endif
