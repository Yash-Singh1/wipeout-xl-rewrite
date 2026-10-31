/* A slow split-screen presentation must not make the race clock run at half speed. */
#define SDL_GetTicksNS TestTicksNS
#define SDL_DelayPrecise TestDelayPrecise
#include "../../platform/psx/libetc.c"
#include <assert.h>

PortConfig portConfig;
PortSettings portSettings;
int portPal;
static uint64_t now = 1000000000ull;
static uint64_t presentationCost = 40000000ull;
static int presentations;
static int linkRole = LINK_MASTER, split = 1, interpolate;
uint64_t TestTicksNS(void) { return now; }
void TestDelayPrecise(uint64_t delay) { now += delay; }
int PortLinkRole(void) { return linkRole; }
int PortLinkSplit(void) { return split; }
int PortLinkField(void) { return 0; }
void PsxIoField(uint64_t field) {}
void PsxShowDisplay(void) { presentations++; now += presentationCost; }
int PortPumpEvents(void) { return 1; }
void PortSwapInterval(int vsync) {}
int PsxInterpolationTest(void) { return 0; }
int PsxCanInterpolate(void) { return interpolate; }
int PsxInterpolationOn(void) { return interpolate; }
void PsxDrawInterpolated(void) { PsxShowDisplay(); }
void PsxDrawInterpolatedTest(int picture, int total) {}
int FrameCapFrozen(void) { return 0; }
void FrameCapSave(void) {}
void FrameCapToggleFreeze(void) {}
void DebugUiShowRecorder(void) {}
void DebugUiTab(void) {}
void PsxAudioDevicePause(bool paused) {}
int PortShownFrames(void) { return presentations; }
void PortDumpVram(void) {}

static double Run(int updates)
{
    uint64_t began;
    fields = lines = counterBase = startTicks = paceField = 0;
    presentations = 0;
    began = now;
    for (int tick = 0; tick < updates; tick++) VSync(2);
    return (now - began) / 1e9;
}

int main(void)
{
    double elapsed = Run(60);
    printf("60 NTSC race updates under a 40ms presentation cost: %.3f seconds, %d pictures\n", elapsed, presentations);
    assert(elapsed > 1.9 && elapsed < 2.15);
    assert(presentations > 30 && presentations < 60);
    interpolate = 1;
    elapsed = Run(60); assert(elapsed > 1.9 && elapsed < 2.15);
    interpolate = 0;
    portPal = 1;
    elapsed = Run(50);
    assert(elapsed > 1.9 && elapsed < 2.15);
    portPal = 0;
    split = 0;
    Run(60); assert(presentations == 120); /* single-player presentation is unchanged */
    split = 1; linkRole = LINK_SLAVE;
    Run(60); assert(presentations == 120); /* the master alone paces the split window */
    linkRole = LINK_MASTER; portConfig.fixedStep = 1;
    Run(60); assert(presentations == 120); /* deterministic captures never omit fields */
    puts("split-screen NTSC/PAL race clocks keep their rate under presentation load: OK");
    return 0;
}
