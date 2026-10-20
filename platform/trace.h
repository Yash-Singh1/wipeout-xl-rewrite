/* Test recordings in the console recorder's format (trace.c). */
#ifndef PORT_TRACE_H
#define PORT_TRACE_H

#include <stdint.h>

/* The boundary markers: the console functions whose return ends an
   iteration of the game's loop. */
#define TRACE_SWAP 0x800684C4u
#define TRACE_CALLBACK_SWAP 0x800686E8u

void PortTraceInit(void);
int PortTracing(void);
/* One drawing packet or video memory transfer, as the GPU receives it. */
void PortTraceGp0(const uint32_t *words, int count);
/* A display control command. */
void PortTraceGp1(uint32_t word);
/* The end of a game-loop iteration. */
void PortTraceBoundary(uint32_t marker);

/* Scripted pad input (WIPEOUT_PADS): the game's fixed-pad read calls this
   with its packet before using it. */
int PortPadScripted(void);
void PortScriptFixedPad(unsigned char *packet);

#endif
