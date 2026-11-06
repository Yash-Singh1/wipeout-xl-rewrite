/* Slow presentations must not slow the single-player or split-screen race clock. */
#define SDL_GetTicksNS TestTicksNS
#define SDL_DelayPrecise TestDelayPrecise
#ifndef TIMING_SOURCE
#define TIMING_SOURCE "../../platform/psx/libetc.c"
#endif
#include TIMING_SOURCE
#include <assert.h>

PortConfig portConfig;
PortSettings portSettings;
int portPal;
static uint64_t now = 1000000000ull;
static uint64_t presentationCost = 40000000ull;
static int presentations, servicedFields, pumpedEvents, jitter;
static uint64_t simulationCost;
static int stallInterpolation;
static int linkRole = LINK_MASTER, split = 1, interpolate;
uint64_t TestTicksNS(void) { return now; }
void TestDelayPrecise(uint64_t delay) { now += delay; }
int PortLinkRole(void) { return linkRole; }
int PortLinkSplit(void) { return split; }
int PortLinkField(void) { return 0; }
void PsxIoField(uint64_t field) { servicedFields++; }
void PsxShowDisplay(void) { if (!PsxBeginPresentation()) return; presentations++; now += presentationCost + (jitter && !(presentations % 10) ? 80000000ull : 0); PsxEndPresentation(); }
int PortPumpEvents(void) { pumpedEvents++; return 1; }
void PortSwapInterval(int vsync) {}
int PsxInterpolationTest(void) { return 0; }
int PsxCanInterpolate(void) { return interpolate; }
int PsxInterpolationOn(void) { return interpolate; }
void PsxDrawInterpolated(void)
{
    if (stallInterpolation && presentations >= 100) {
        now += 500000000ull;
        stallInterpolation = 0;
    }
    PsxShowDisplay();
}
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
    presentations = servicedFields = pumpedEvents = 0;
    presentationPending = presentationCap = 0; nextPresentation = 0;
    began = now;
    for (int tick = 0; tick < updates; tick++) { now += simulationCost; VSync(2); }
    assert(servicedFields == updates * 2 && pumpedEvents == updates * 2);
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
    presentationCost = 2000000ull;
    const int caps[] = {15, 20, 30, 45, 60, 75, 90};
    for (int i = 0; i < 7; i++) {
        int cap = caps[i];
        portSettings.frameRate = FRAME_RATE_CAPPED;
        portSettings.frameCap = cap;
        elapsed = Run(300);
        double fps = presentations / elapsed;
        printf("cap %d: %.2f displayed FPS, %.2f race updates/sec\n", cap, fps, 300 / elapsed);
        assert(fps <= cap + 0.2 && fps > cap - 1.0);
        assert(elapsed > 9.95 && elapsed < 10.15);
    }
    portSettings.frameCap = 60;
    PsxNextPresentation();
    nextPresentation = now + 1000000000ull / 60;
    assert(!PsxBeginPresentation());
    now = nextPresentation;
    assert(PsxBeginPresentation() && PsxBeginPresentation()); /* nested interpolation shares the field's slot */
    now += 2000000ull;
    PsxEndPresentation(); assert(!PsxBeginPresentation());
    now += 1000000000ull; /* a stall drops old deadlines, rather than presenting a burst to catch up */
    assert(PsxBeginPresentation()); PsxEndPresentation(); assert(!PsxBeginPresentation());
    for (int i = 0; i < 5; i++) {
        portSettings.frameCap = caps[i];
        for (int cost = 8; cost <= 80; cost *= 2) {
            presentationCost = (uint64_t)cost * 1000000ull;
            simulationCost = 5000000ull;
            elapsed = Run(300);
            assert(elapsed > 9.9 && elapsed < 10.25);
        }
    }
    jitter = 1; presentationCost = 2000000ull;
    elapsed = Run(300); assert(elapsed > 9.9 && elapsed < 10.25);
    jitter = 0; simulationCost = 0;
    portPal = 1;
    for (int i = 0; i < 5; i++) {
        portSettings.frameCap = caps[i];
        elapsed = Run(250);
        assert(elapsed > 9.9 && elapsed < 10.25);
        assert(presentations / elapsed <= caps[i] + 0.2);
    }
    portPal = 0;
    portSettings.frameRate = FRAME_RATE_UNCAPPED;
    presentationCost = 40000000ull;
    interpolate = 0;
    portPal = 1;
    elapsed = Run(50);
    assert(elapsed > 1.9 && elapsed < 2.15);
    portPal = 0;
    split = 0; linkRole = LINK_NONE;
    elapsed = Run(60); assert(elapsed > 1.9 && elapsed < 2.15);
    assert(presentations > 30 && presentations < 60);
    portPal = 1;
    elapsed = Run(50); assert(elapsed > 1.9 && elapsed < 2.15);
    portPal = 0;
    presentationCost = 2000000ull;
    Run(60); assert(presentations == 120); /* inexpensive pictures still show every field */
    split = 1; linkRole = LINK_SLAVE;
    Run(60); assert(presentations == 120); /* the master alone paces the split window */
    linkRole = LINK_MASTER; portConfig.fixedStep = 1;
    Run(60); assert(presentations == 120); /* deterministic captures never omit fields */
    portConfig.fixedStep = 0; linkRole = LINK_NONE; split = 0; interpolate = 1;
    portSettings.frameRate = FRAME_RATE_UNCAPPED;
    const int costs[] = {8, 12, 20, 40};
    for (int i = 0; i < 4; i++) {
        int cost = costs[i];
        presentationCost = (uint64_t)cost * 1000000ull;
        elapsed = Run(300);
        printf("uncapped %dms pictures: %.1f displayed FPS, %.2f race updates/sec\n",
               cost, presentations / elapsed, 300 / elapsed);
        assert(presentations / elapsed > 0.95 * 1000 / cost);
        assert(elapsed > 9.9 && elapsed < 10.3);
    }
    presentationCost = 1000000ull;
    for (int mode = 0; mode < 2; mode++) {
        portSettings.frameRate = mode ? FRAME_RATE_CAPPED : FRAME_RATE_UNCAPPED;
        portSettings.frameCap = 300;
        for (int pal = 0; pal < 2; pal++) {
            portPal = pal;
            stallInterpolation = 1;
            elapsed = Run(pal ? 250 : 300);
            printf("%s %s interpolation after a 500ms stall: %.1f displayed FPS, %.3f seconds\n",
                   pal ? "PAL" : "NTSC", mode ? "capped" : "uncapped", presentations / elapsed, elapsed);
            assert(!stallInterpolation);
            assert(presentations / elapsed > (mode ? 260 : 700));
            assert(elapsed > 9.9 && elapsed < 10.8);
        }
    }
    puts("single-player and split-screen NTSC/PAL race clocks keep their rate under presentation load: OK");
    return 0;
}
