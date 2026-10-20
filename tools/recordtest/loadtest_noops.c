/* PsyQ functions that do nothing in the load test: the CD drive, events, sound, controller, memory card
   and the draw hardware. No game header is included on purpose: the stand-ins take whatever arguments the
   game passes. Any other undefined function stops the program when the game calls it (build_loadtest.py). */
#include <stddef.h>

#define NOOP(name) int name() { return 0; }
#define RETURNS(name, value) int name() { return (value); }

/* events and interrupts */
NOOP(ResetCallback) NOOP(StopCallback) NOOP(SetMem) NOOP(EnterCriticalSection) NOOP(ExitCriticalSection)
RETURNS(OpenEvent, 1) RETURNS(EnableEvent, 1) RETURNS(CloseEvent, 1) RETURNS(TestEvent, 1)
NOOP(VSync) NOOP(VSyncCallback)
NOOP(GsInitVcount) NOOP(GsClearVcount)
int GsGetVcount(void)
{
    static int vcount;
    return vcount += 3;
}
/* CD drive */
RETURNS(CdInit, 1) NOOP(CdControl) NOOP(CdControlB) NOOP(CdGetToc) NOOP(CdIntToPos) NOOP(CdPosToInt) NOOP(CdMix) NOOP(CdSetDebug)
/* controller, memory card, serial port */
NOOP(InitPAD) NOOP(StartPAD) NOOP(StopPAD) NOOP(InitCARD) NOOP(StartCARD) NOOP(_bu_init) NOOP(_card_clear)
NOOP(_card_info) NOOP(_card_load) NOOP(_card_write) NOOP(_new_card) NOOP(ChangeClearPAD) NOOP(ChangeClearSIO)
NOOP(_comb_control) NOOP(AddCOMB)
NOOP(firstfile) NOOP(nextfile) NOOP(delete) NOOP(format)
RETURNS(PsyqOpen, -1) RETURNS(PsyqClose, -1) RETURNS(PsyqRead, -1) RETURNS(PsyqWrite, -1) RETURNS(PsyqRename, -1)
/* sound */
NOOP(SpuInit) NOOP(SpuInitMalloc) NOOP(SpuMalloc) NOOP(SpuFree) NOOP(SpuQuit) NOOP(SpuStart) NOOP(SpuSetKey)
NOOP(SpuSetVoiceAttr) NOOP(SpuGetVoiceAttr) NOOP(SpuGetKeyStatus) NOOP(SpuSetCommonAttr) NOOP(SpuSetReverb)
NOOP(SpuSetReverbModeParam) NOOP(SpuSetReverbVoice) NOOP(SpuClearReverbWorkArea) NOOP(SpuReserveReverbWorkArea)
NOOP(SpuSetTransferMode) NOOP(SpuSetTransferStartAddr) NOOP(SpuWrite) RETURNS(SpuIsTransferCompleted, 1)
/* the draw hardware (the platform's libgpu keeps the VRAM image and calls these) */
NOOP(GpuInit) NOOP(GpuFill) NOOP(GpuFlush) NOOP(GpuLine) NOOP(GpuPresent) NOOP(GpuReadBack) NOOP(GpuSetDrawArea)
NOOP(GpuTriangles) NOOP(GpuVramChanged) NOOP(PortTraceGp0) NOOP(PortTraceGp1) NOOP(PortTraceBoundary) NOOP(PortScriptFixedPad)
NOOP(PortTracing)

/* the movie player and the hand-written assembly drawing routines */
NOOP(PsxPlayMovie) NOOP(AsmDrawObject) NOOP(AsmDrawObjectFixed) NOOP(AsmDrawObjectLit) NOOP(AsmDrawParticles)
NOOP(AsmTrackHi) NOOP(AsmTrackLo) NOOP(AsmTrackMed)
