/* The web entry points must never start a request, worker or notification. */
#define __EMSCRIPTEN__ 1
#include "../../platform/updates.c"
#include <assert.h>
PortConfig portConfig;
PortSettings portSettings;
int PortUpdateFetch(char *url, size_t size) { (void)url; (void)size; assert(0); return 0; }
int main(void)
{
    for (int i = 0; i < 3; i++) {
        PortUpdatesStart(); PortUpdatesPoll(); assert(!PortUpdateAvailable());
        PortUpdateOpenRelease(); PortUpdateDismiss(); PortUpdatesShutdown();
    }
    puts("web builds never check for releases or show update alerts: OK");
    return 0;
}
