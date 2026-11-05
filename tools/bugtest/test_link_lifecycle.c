/* Each game enters its own race after loading; the first game alone controls the split window. */
#include "../../platform/link.c"
#include <assert.h>
PortConfig portConfig;
static int pauseBlocks;
void PortInputRaceStart(void) { pauseBlocks++; }
int main(void)
{
    LinkInput sent = {0}, received;
    role = LINK_MASTER;
    shared = MapShared(1);
    assert(shared);
    assert(PortLinkDownsample() == -1);
    PortLinkSetBand(16, 1280, 360, 3, DOWNSAMPLE_ADAPTIVE);
    role = LINK_SLAVE;
    assert(PortLinkDownsample() == 3 && PortLinkDownsampleStyle() == DOWNSAMPLE_ADAPTIVE);
    role = LINK_MASTER;
    PortLinkSetBand(16, 1280, 360, 0, DOWNSAMPLE_BOX);
    role = LINK_SLAVE;
    assert(PortLinkDownsample() == 0 && PortLinkDownsampleStyle() == DOWNSAMPLE_BOX);
    role = LINK_MASTER;
    PortLinkSetSplit(1);
    assert(PortLinkRaceActive() && PortLinkSplit() && pauseBlocks == 1);
    PortLinkSetSplit(1); assert(pauseBlocks == 1);
    /* The other game is still loading while the first has enabled the split window. */
    role = LINK_SLAVE; raceActive = 0;
    assert(PortLinkSplit() && !PortLinkRaceActive());
    PortLinkSetSplit(0); assert(PortLinkSplit() && !PortLinkRaceActive());
    PortLinkSetSplit(1); assert(PortLinkRaceActive() && pauseBlocks == 2);
    PortLinkSetSplit(0); assert(PortLinkSplit());
    role = LINK_MASTER;
    sent.pad.buttons = PAD_CROSS; sent.pad.twist = 173; sent.pad.analogSteering = true;
    PortLinkSetInput(&sent); assert(PortLinkGetInput(&received));
    assert(received.pad.buttons == PAD_CROSS && received.pad.twist == 173 && received.pad.analogSteering);
    PortLinkSetSplit(0); assert(!PortLinkSplit());
    UnmapShared();
    puts("each linked game's race starts independently; controller snapshot stays intact: OK");
    return 0;
}
