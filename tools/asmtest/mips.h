/* A MIPS R3000 interpreter for the differential tests of the renderer's hand-written assembly.

   It runs the retail executable's own machine code (the routines of asm/800.s of the decomp and everything
   they call: SetSkeletonTransform, rsin, rcos ...) on 2 MB of emulated RAM plus the scratchpad, with the
   coprocessor-2 instructions going to the platform's GTE (platform/psx/gte.c).  The test programs build a
   scene in the emulated RAM and in native structures, call the retail routine here and its C port natively,
   and compare what both draw. */
#ifndef MIPS_H
#define MIPS_H

#include <stdint.h>

#define MIPS_RAM_SIZE 0x200000u
#define MIPS_BASE     0x80000000u

/* Loads the PlayStation executable (header 0x800 bytes, text at 0x80010000) and zeroes the rest of RAM. */
int MipsLoadExe(const char *path);

uint32_t MipsLoad32(uint32_t address);
uint16_t MipsLoad16(uint32_t address);
uint8_t MipsLoad8(uint32_t address);
void MipsStore32(uint32_t address, uint32_t value);
void MipsStore16(uint32_t address, uint16_t value);
void MipsStore8(uint32_t address, uint8_t value);
/* Host pointer to emulated memory (RAM or scratchpad), or NULL outside both. */
uint8_t *MipsPointer(uint32_t address);

/* Calls the routine at address with up to four register arguments (and stack words after them); returns $v0.
   Stops with a message and abort() after maxInstructions or on an unimplemented instruction. */
uint32_t MipsCall(uint32_t address, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3, const uint32_t *stackArgs, int stackCount);

/* Set when the last call stopped at a BREAK (the compiler's divide-by-zero trap; the console would
   have taken an exception there). */
extern int mipsTrapped;

/* Instructions executed by the last call. */
extern uint64_t mipsInstructionCount;
extern uint64_t mipsMaxInstructions;

/* Calls to these addresses are replaced by a host function (the library's hardware access):
   the hook gets the four argument registers and returns $v0.  */
typedef uint32_t (*MipsHook)(uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3);
void MipsSetHook(uint32_t address, MipsHook hook);

#endif
