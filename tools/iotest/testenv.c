#include "testenv.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include "../../platform/link.h"
#include "../../platform/psx/psx_io.h"

#ifndef REPO_DIR
#define REPO_DIR "."
#endif

static uint64_t fields;
static char saveDir[256];

uint64_t test_fields(void)
{
    return fields;
}

/* The platform's video timing: one field passes. */
void PsxVBlank(void)
{
    fields++;
    PsxIoField(fields);
}

/* ... and the fields due by the clock while the game reads the disc: none (the tests' time is the fields'). */
void PsxPassFields(void)
{
}

/* The platform's two-player link and window, as without them: no link, the window in focus. */
int PortLinkRole(void)
{
    return LINK_NONE;
}

void PortLinkSetInput(const LinkInput *input)
{
    (void)input;
}

int PortLinkGetInput(LinkInput *input)
{
    (void)input;
    return 0;
}

int PortWindowFocused(void)
{
    return 1;
}

void PortLog(const char *format, ...)
{
    va_list ap;

    va_start(ap, format);
    vfprintf(stderr, format, ap);
    va_end(ap);
}

static int exists(const char *path)
{
    struct stat st;

    return stat(path, &st) == 0;
}

const char *test_disc_path(void)
{
    static char path[1024];
    const char *env = getenv("WIPEOUT_XL_DISC");

    if (env) {
        return exists(env) ? env : NULL;
    }
    snprintf(path, sizeof(path), "%s/../wipeout-xl/Wipeout XL (USA).cue", REPO_DIR);
    return exists(path) ? path : NULL;
}

const char *test_root_dir(void)
{
    static char path[1024];
    const char *env = getenv("WIPEOUT_XL_ROOT");

    if (env) {
        return exists(env) ? env : NULL;
    }
    snprintf(path, sizeof(path), "%s/../wipeout-xl/disc/root", REPO_DIR);
    return exists(path) ? path : NULL;
}

const char *test_save_dir(void)
{
    return saveDir;
}

static double lastSpeed = 1.0;
static const char *lastScript;
static int started;

int test_io_restart(void)
{
    PsxIoConfig config;
    char error[256] = "";

    PsxIoShutdown();
    memset(&config, 0, sizeof(config));
    config.discPath = test_disc_path();
    config.savePath = saveDir;
    config.fixedStep = 1;
    config.cdSpeed = lastSpeed;
    config.padScript = lastScript;
    fields = 0;
    return PsxIoInit(&config, error, sizeof(error));
}

int test_io_start(double cdSpeed, const char *padScript)
{
    PsxIoConfig config;
    char error[256] = "";
    const char *disc = test_disc_path();

    if (!disc) {
        return 0;
    }
    snprintf(saveDir, sizeof(saveDir), "/tmp/psxio-test-XXXXXX");
    if (!mkdtemp(saveDir)) {
        perror("mkdtemp");
        exit(1);
    }
    memset(&config, 0, sizeof(config));
    config.discPath = disc;
    config.savePath = saveDir;
    config.fixedStep = 1;
    config.cdSpeed = cdSpeed;
    config.padScript = padScript;
    lastSpeed = cdSpeed;
    lastScript = padScript;
    started = 1;
    fields = 0;
    if (!PsxIoInit(&config, error, sizeof(error))) {
        fprintf(stderr, "PsxIoInit: %s\n", error);
        exit(1);
    }
    return 1;
}

void test_io_stop(void)
{
    char command[300];

    (void)started;
    PsxIoShutdown();
    snprintf(command, sizeof(command), "rm -rf '%s'", saveDir);
    if (saveDir[0]) {
        if (system(command) != 0) {
            fprintf(stderr, "cleanup failed\n");
        }
    }
}
