#include <SDL3/SDL.h>
#include <limits.h>
#include <string.h>
#include "platform.h"
#include "settings.h"
#include "updates.h"

#ifdef __EMSCRIPTEN__
/* Web deployments update when the page is reloaded. No release requests or alerts. */
void PortUpdatesStart(void) {}
void PortUpdatesPoll(void) {}
void PortUpdatesShutdown(void) {}
const char *PortUpdateAvailable(void) { return NULL; }
void PortUpdateDismiss(void) {}
void PortUpdateOpenRelease(void) {}
#else
static SDL_Thread *worker;
static SDL_AtomicInt finished;
static int started, pending;
static char available[64];

static int Version(const char *text, int parts[3], int local)
{
    int i;
    if (*text == 'v') text++;
    for (i = 0; i < 3; i++) {
        unsigned number = 0;
        if (*text < '0' || *text > '9') return 0;
        do {
            unsigned digit = (unsigned)(*text++ - '0');
            if (number > ((unsigned)INT_MAX - digit) / 10) return 0;
            number = number * 10 + digit;
        } while (*text >= '0' && *text <= '9');
        parts[i] = (int)number;
        if (i != 2 && *text++ != '.') return 0;
    }
    /* Local git/prerelease suffixes are allowed; remote tags must be stable versions. */
    if (!*text) return 1;
    if (local && (*text == '-' || *text == '+') && text[1]) return *text == '-' ? 2 : 1;
    return 0;
}

int PortUpdateNewer(const char *current, const char *remote)
{
    int a[3], b[3], i, kind = Version(current, a, 1);
    if (!kind || !Version(remote, b, 0)) return 0;
    for (i = 0; i < 3; i++) {
        if (a[i] != b[i]) return b[i] > a[i];
    }
    return kind == 2;
}

int PortUpdateTagFromUrl(const char *url, char *tag, size_t size)
{
    const char prefix[] = PORT_RELEASES "/tag/";
    int parts[3];
    if (strncmp(url, prefix, sizeof(prefix) - 1)) return 0;
    url += sizeof(prefix) - 1;
    if (!Version(url, parts, 0) || strlen(url) >= size) return 0;
    SDL_strlcpy(tag, url, size);
    return 1;
}

static int CheckRelease(void *unused)
{
    char url[256];
    (void)unused;
    if (PortUpdateFetch(url, sizeof(url))) {
        char tag[64];
        if (PortUpdateTagFromUrl(url, tag, sizeof(tag)) && PortUpdateNewer(PORT_VERSION, tag))
            SDL_strlcpy(available, tag, sizeof(available));
    }
    SDL_SetAtomicInt(&finished, 1);
    return 0;
}

void PortUpdatesStart(void)
{
    if (started) return;
    started = 1;
    if (portConfig.hidden || portConfig.secondPlayer || portSettings.noUpdateCheck) return;
    worker = SDL_CreateThread(CheckRelease, "Release check", NULL);
}

void PortUpdatesPoll(void)
{
    if (portConfig.hidden || portConfig.secondPlayer) return;
    if (SDL_GetAtomicInt(&finished)) {
        SDL_SetAtomicInt(&finished, 0);
        if (worker) { SDL_WaitThread(worker, NULL); worker = NULL; }
        pending = available[0] != 0 && !portSettings.noUpdateCheck;
    }
}

const char *PortUpdateAvailable(void)
{
    return pending ? available : NULL;
}

void PortUpdateDismiss(void)
{
    pending = 0;
}

void PortUpdateOpenRelease(void)
{
    char url[256];
    if (!pending) return;
    SDL_snprintf(url, sizeof(url), PORT_RELEASES "/tag/%s", available);
    SDL_OpenURL(url);
}

void PortUpdatesShutdown(void)
{
    if (worker) { SDL_WaitThread(worker, NULL); worker = NULL; }
}

#endif
