/* TEMPORARY, only with cmake -DPORT_ASM_STUBS=ON: the hand-written assembly routines of the renderer
   (asm/800.s of the decomp) have not been ported to C yet.  Each one reports its first call and returns,
   so that the rest of the game can be started and tested.  Nothing in a normal build uses this file. */
#include <stdio.h>

#define NOT_PORTED(name) \
    void name(void) { static int reported; if (!reported) { reported = 1; fprintf(stderr, "asm routine " #name " is not ported: nothing is drawn by it\n"); } }

NOT_PORTED(AsmDrawObject)
NOT_PORTED(AsmDrawObjectLit)
NOT_PORTED(AsmDrawObjectFixed)
NOT_PORTED(AsmDrawParticles)
NOT_PORTED(AsmTrackLo)
NOT_PORTED(AsmTrackMed)
NOT_PORTED(AsmTrackHi)
