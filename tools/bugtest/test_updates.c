/* Exercise the launch lifecycle without network traffic or real dialogs. */
#include "../../platform/updates.c"
#include "../../platform/settings.c"
#include <assert.h>
#include <unistd.h>

PortConfig portConfig;
static int checks, alerts, opened, answer;
static const char *reply = PORT_RELEASES "/tag/v0.0.7";
SDL_Window *PortWindow(void) { return NULL; }
int PortUpdateFetch(char *url, size_t size)
{
    checks++;
    if (!reply) return 0;
    SDL_strlcpy(url, reply, size);
    return 1;
}
bool SDL_OpenURL(const char *url)
{
    assert(!strcmp(url, PORT_RELEASES "/tag/v0.0.7"));
    opened++;
    return true;
}
static void Complete(void)
{
    /* Test waits for the worker; the actual event pump only polls its completion flag. */
    if (worker) { SDL_WaitThread(worker, NULL); worker = NULL; }
    PortUpdatesPoll();
    if (PortUpdateAvailable()) {
        assert(!strcmp(PortUpdateAvailable(), "v0.0.7"));
        alerts++;
        if (answer) PortUpdateOpenRelease();
        PortUpdateDismiss();
    }
}
static void Relaunch(void)
{
    PortUpdatesShutdown();
    started = pending = 0;
    available[0] = 0;
    SDL_SetAtomicInt(&finished, 0);
}
int main(void)
{
    char tag[64], temp[] = "/tmp/wipeout-update-test-XXXXXX";
    assert(PortUpdateNewer("0.0.6", "v0.0.7"));
    assert(PortUpdateNewer("0.0.9", "v0.0.10"));
    assert(PortUpdateNewer("0.9.99", "1.0.0"));
    assert(!PortUpdateNewer("0.0.7", "v0.0.7"));
    assert(!PortUpdateNewer("0.0.8", "v0.0.7"));
    assert(PortUpdateNewer("0.0.7-rc.1", "v0.0.7"));
    assert(!PortUpdateNewer("0.0.7+build-name", "v0.0.7"));
    assert(!PortUpdateNewer("0.0.6-16-gc3f956d", "v0.0.6"));
    assert(!PortUpdateNewer("v0.0.6-16-gc3f956d-dirty", "v0.0.6"));
    assert(!PortUpdateNewer("0.0.6-c3f956d", "v0.0.6"));
    assert(!PortUpdateNewer("0.0.6-c3f956d-dirty", "v0.0.6"));
    assert(!PortUpdateNewer("0.0.6-dirty", "v0.0.6"));
    assert(PortUpdateNewer("0.0.6-c3f956d", "v0.0.7"));
    assert(!PortUpdateNewer("0.0.6-c3f956d", "v0.0.5"));
    assert(PortUpdateNewer("0.0.7-beta.2", "v0.0.7"));
    assert(PortUpdateNewer("0.0.7-alpha.1", "v0.0.7"));
    assert(!PortUpdateNewer("0.0.6", "v0.0.7-beta.1"));
    assert(!PortUpdateNewer("unknown", "v0.0.7"));
    assert(!PortUpdateNewer("0.0.6", "999999999999999999.1.0"));
    assert(!PortUpdateNewer("0.0.6", "1.2"));
    assert(PortUpdateTagFromUrl(reply, tag, sizeof(tag)) && !strcmp(tag, "v0.0.7"));
    assert(!PortUpdateTagFromUrl("https://example.com/tag/v0.0.7", tag, sizeof(tag)));
    assert(!PortUpdateTagFromUrl(PORT_RELEASES "/tag/v0.0.7/extra", tag, sizeof(tag)));
    assert(!PortUpdateTagFromUrl(reply, tag, 3));

    assert(!portSettings.noUpdateCheck); /* existing INIs also retain the enabled default */
    PortUpdatesStart(); PortUpdatesStart(); Complete();
    assert(checks == 1 && alerts == 1 && opened == 0);
    PortUpdatesPoll(); PortUpdatesStart(); assert(checks == 1 && alerts == 1);
    Relaunch(); answer = 1;
    PortUpdatesStart(); Complete(); assert(checks == 2 && alerts == 2 && opened == 1);
    Relaunch(); Set("check_updates", "0");
    PortUpdatesStart(); Complete(); assert(checks == 2);
    Set("check_updates", "1"); PortUpdatesStart(); Complete(); assert(checks == 2); /* next launch */
    Relaunch(); portConfig.hidden = 1;
    PortUpdatesStart(); Complete(); assert(checks == 2);
    Relaunch(); portConfig.hidden = 0; portConfig.secondPlayer = 1;
    PortUpdatesStart(); Complete(); assert(checks == 2);
    Relaunch(); portConfig.secondPlayer = 0; reply = NULL;
    PortUpdatesStart(); Complete(); assert(checks == 3 && alerts == 2); /* offline */
    Relaunch(); reply = PORT_RELEASES "/tag/v0.0.6";
    PortUpdatesStart(); Complete(); assert(checks == 4 && alerts == 2); /* current */
    Relaunch(); reply = PORT_RELEASES "/tag/v0.0.7";
    PortUpdatesStart(); Set("check_updates", "0"); Complete();
    assert(checks == 5 && alerts == 2); /* disabled while the request was in flight */
    Relaunch(); Set("check_updates", "1");
    PortUpdatesStart(); SDL_WaitThread(worker, NULL); worker = NULL; PortUpdatesPoll();
    assert(checks == 6 && PortUpdateAvailable());
    Set("check_updates", "0");
    assert(PortUpdateAvailable()); /* opting out in the alert keeps it open until dismissed */
    PortUpdateDismiss(); assert(!PortUpdateAvailable());

    assert(mkdtemp(temp));
    SDL_snprintf(path, sizeof(path), "%s/settings.ini", temp);
    assert(PortSettingsSave());
    {
        char *text = SDL_LoadFile(path, NULL);
        assert(text && strstr(text, "check_updates = 0\n"));
        SDL_free(text);
    }
    Set("check_updates", "1"); assert(PortSettingsSave());
    {
        char *text = SDL_LoadFile(path, NULL);
        assert(text && strstr(text, "check_updates = 1\n"));
        SDL_free(text);
    }
    unlink(path); rmdir(temp);
    Relaunch();
    puts("release notifications: numeric versions, persistence, one launch check, opt-out, isolation and silent failures: OK");
    return 0;
}
