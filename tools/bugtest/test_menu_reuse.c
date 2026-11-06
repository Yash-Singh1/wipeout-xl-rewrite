/* A reused image must expire before new packets, transfers or graphics settings. */
#define main OrderingRegressionMain
#include "test_ordering_cycle.c"
#undef main
PortSettings portSettings;
static int factor = 12, margin = 32, race, role;
int GpuRaceWide(void) { return race; }
int PortLinkRole(void) { return role; }
static unsigned textureEpoch;
int GpuScale(void) { return factor; }
int PortScreenMargin(void) { return margin; }
unsigned HdTexEpoch(void) { return textureEpoch; }
int main(void)
{
    menuPicture.valid = 1; menuPicture.scale = factor; menuPicture.margin = margin;
    menuPicture.settings = portSettings;
    assert(CanReuseMenuPicture());
    race = 1; assert(!CanReuseMenuPicture()); race = 0;
    role = LINK_MASTER; assert(!CanReuseMenuPicture()); role = LINK_NONE;
    recordingBetween = 1; assert(!CanReuseMenuPicture()); recordingBetween = 0;
    factor++; assert(!CanReuseMenuPicture()); factor--;
    margin++; assert(!CanReuseMenuPicture()); margin--;
    textureEpoch++; assert(!CanReuseMenuPicture()); textureEpoch--;
    portSettings.consoleDepth++; assert(!CanReuseMenuPicture()); portSettings.consoleDepth--;
    assert(CanReuseMenuPicture());
    ForgetDrawnFrame(); assert(!CanReuseMenuPicture());
    menuPicture.valid = 1;
    u_long table[2]; DRAWENV env = {0}; ClearOTagR(table, 2);
    PsxUpcomingTables(table + 1, NULL, NULL, &env);
    assert(!CanReuseMenuPicture());
    puts("menu image reuse expires for recording, settings, texture updates and new tables/transfers: OK");
    return 0;
}
