/* Test recordings in the console recorder's format (trace.c). */
#ifndef PORT_TRACE_H
#define PORT_TRACE_H

#include <stdint.h>

/* The boundary markers: the console functions whose return ends an
   iteration of the game's loop. */
#define TRACE_SWAP 0x800684C4u
#define TRACE_CALLBACK_SWAP 0x800686E8u

void PortTraceInit(void);
/* --record: a replay of the session (trace.c) */
int PortRecordStart(const char *dir, const char *saveDir, const char *settingsPath);
const char *PortRecordDir(void);
void PortRecordVcount(int value);
int PortTracing(void);
/* One drawing packet or video memory transfer, as the GPU receives it. */
void PortTraceGp0(const uint32_t *words, int count);
/* Port-only events for the ordering-table timing (libgpu.c, DrawOTag):
   a table drawn (link number and word count of its packets in order), and when the game
   next waits for the GPU each table's first packet and every packet with
   its next link as they are then. */
#define TRACE_TABLE 43
#define TRACE_LATE_TABLE 44
#define TRACE_LATE_PACKET 42
void PortTraceEvent(uint32_t kind, const uint32_t *words, int count);
/* A display control command. */
void PortTraceGp1(uint32_t word);
/* A value the game read from a library call (the recorder's return event). */
void PortTraceReturn(uint32_t function, uint32_t value);
/* The end of a game-loop iteration. */
void PortTraceBoundary(uint32_t marker);

/* WIPEOUT_AUTOPILOT (all, or until a pad-read count): the console recorder's
   autopilot. While it lasts the player's ship hands itself to the drone code
   (PortAutopilot); for the whole run the drone code never counts the
   autopilot down (PortAutopilotPatched). */
int PortAutopilot(void);
int PortAutopilotPatched(void);

/* Loading screens (WIPEOUT_LOADS: the loading-screen frames of each load in
   a console recording, one number per line). The game draws its loading
   screen from the vertical-blank callback, which also advances the random
   seed, so the number of loading frames changes the race that follows. The
   disc and the console's decompression set that number; when a load ends
   early the port goes on with loading frames until it is the console's. */
void PortLoadingStarts(void);
void PortLoadingEnds(void);

/* Scripted pad input (WIPEOUT_PADS): the game's fixed-pad read calls this
   with its packet before using it, counting the reads... */
int PortPadScripted(void);
/* Scripted writes (WIPEOUT_WRITES, made from the recording's writes.txt by
   tools/trace/writes.py): UpdateShips calls this first with the player's
   ship and the game's configuration. */
void PortScriptWrites(void *ship, void *config, int owner);
void PortScriptFixedPad(unsigned char *packet);
/* ... and so does buildGameKeyPad, without advancing the count. */
void PortScriptGamePad(unsigned char *packet);

#endif
