#include <SDL3/SDL.h>
#include <limits.h>
#include <string.h>
#include "platform.h"
#include "settings.h"
#include "updates.h"

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

#ifdef __EMSCRIPTEN__
#include <emscripten.h>
EMSCRIPTEN_KEEPALIVE void PortUpdateReceived(const char *tag)
{
    if (strlen(tag) < sizeof(available) && PortUpdateNewer(PORT_VERSION, tag))
        SDL_strlcpy(available, tag, sizeof(available));
    SDL_SetAtomicInt(&finished, 1);
}
EM_JS(int, UpdateAlert, (const char *latest, const char *current), {
    return confirm('Wipeout XL ' + UTF8ToString(latest) + ' is available.\nYou are running ' +
                   UTF8ToString(current) + '.\n\nOpen the GitHub release page?') ? 1 : 0;
});
EM_JS(void, CheckRelease, (void), {
    const controller = new AbortController();
    const timeout = setTimeout(() => controller.abort(), 8000);
    fetch('https://api.github.com/repos/Yash-Singh1/wipeout-xl-rewrite/releases/latest',
          {signal: controller.signal, credentials: 'omit'})
      .then(response => response.ok ? response.json() : null)
      .then(release => {
          if (release && !release.draft && !release.prerelease && typeof release.tag_name === 'string') {
              const tag = stringToNewUTF8(release.tag_name);
              _PortUpdateReceived(tag);
              _free(tag);
          }
      }).catch(() => {}).finally(() => clearTimeout(timeout));
});
#else
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
#endif

void PortUpdatesStart(void)
{
    if (started) return;
    started = 1;
    if (portConfig.hidden || portConfig.secondPlayer || portSettings.noUpdateCheck) return;
#ifdef __EMSCRIPTEN__
    CheckRelease();
#else
    worker = SDL_CreateThread(CheckRelease, "Release check", NULL);
#endif
}

void PortUpdatesPoll(void)
{
    if (portConfig.hidden || portConfig.secondPlayer) return;
    if (SDL_GetAtomicInt(&finished)) {
        SDL_SetAtomicInt(&finished, 0);
        if (worker) { SDL_WaitThread(worker, NULL); worker = NULL; }
        pending = available[0] != 0;
    }
    if (pending) {
        char message[192], url[256];
        int button = 0;
        const SDL_MessageBoxButtonData buttons[] = {
            {SDL_MESSAGEBOX_BUTTON_ESCAPEKEY_DEFAULT | SDL_MESSAGEBOX_BUTTON_RETURNKEY_DEFAULT, 0, "Later"},
            {0, 1, "View release"}
        };
        SDL_MessageBoxData box = {0};
        pending = 0;
        if (portSettings.noUpdateCheck) return;
        SDL_snprintf(message, sizeof(message), "Wipeout XL %s is available.\nYou are running %s.", available, PORT_VERSION);
        box.flags = SDL_MESSAGEBOX_INFORMATION;
        box.window = PortWindow();
        box.title = "Wipeout XL update available";
        box.message = message;
        box.numbuttons = SDL_arraysize(buttons);
        box.buttons = buttons;
#ifdef __EMSCRIPTEN__
        button = UpdateAlert(available, PORT_VERSION);
        if (button == 1) {
#else
        if (SDL_ShowMessageBox(&box, &button) && button == 1) {
#endif
            SDL_snprintf(url, sizeof(url), PORT_RELEASES "/tag/%s", available);
            SDL_OpenURL(url);
        }
    }
}

void PortUpdatesShutdown(void)
{
    if (worker) { SDL_WaitThread(worker, NULL); worker = NULL; }
}
