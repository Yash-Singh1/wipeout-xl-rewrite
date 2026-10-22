/* The player's settings (settings.h). The file is lines of "key = value"; unknown keys are kept out,
   missing ones take the defaults. */
#include <SDL3/SDL.h>
#include <stdio.h>
#include <string.h>

#include "platform.h"
#include "settings.h"

PortSettings portSettings = {0, 1, 1, 64000, 96000, 0, FRAME_RATE_UNCAPPED, 144, 0, 1, 0, ""};

static char path[1024];
static char defaultSaveDir[1024];

const char *PortSettingsPath(void)
{
    return path;
}

const char *PortSettingsDefaultSaveDir(void)
{
    return defaultSaveDir;
}

static void Set(const char *key, const char *value)
{
    int number = SDL_atoi(value);

    if (!strcmp(key, "show_fps")) {
        portSettings.showFps = number != 0;
    } else if (!strcmp(key, "far_view")) {
        portSettings.farView = number != 0;
    } else if (!strcmp(key, "fade")) {
        portSettings.fade = number != 0;
    } else if (!strcmp(key, "fade_near") && number > 0) {
        portSettings.fadeNear = number;
    } else if (!strcmp(key, "fade_far") && number > 0) {
        portSettings.fadeFar = number;
    } else if (!strcmp(key, "widescreen")) {
        portSettings.widescreen = number != 0;
    } else if (!strcmp(key, "frame_rate")) {
        portSettings.frameRate = SDL_clamp(number, FRAME_RATE_UNCAPPED, FRAME_RATE_CONSOLE);
    } else if (!strcmp(key, "frame_cap") && number > 0) {
        portSettings.frameCap = SDL_clamp(number, 30, 1000);
    } else if (!strcmp(key, "skip_intro")) {
        portSettings.skipIntro = number != 0;
    } else if (!strcmp(key, "record_frames")) {
        portSettings.recordFrames = SDL_clamp(number, RECORD_OFF, RECORD_WITH_TAB);
    } else if (!strcmp(key, "record_replays")) {
        portSettings.recordReplays = number != 0;
    } else if (!strcmp(key, "save_dir")) {
        SDL_strlcpy(portSettings.saveDir, value, sizeof(portSettings.saveDir));
    }
}

void PortSettingsLoad(void)
{
    char *base = SDL_GetPrefPath("Psygnosis", "WipeoutXL");
    char *text, *line, *next;
    const char *value;

    if (base) {
        SDL_snprintf(path, sizeof(path), "%swipeoutxl.ini", base);
        SDL_strlcpy(defaultSaveDir, base, sizeof(defaultSaveDir));
        SDL_free(base);
    }
    /* the tests' hidden runs keep the defaults: a player's file must not change them */
    if (path[0] && !portConfig.hidden && (text = SDL_LoadFile(path, NULL))) {
        for (line = text; line && *line; line = next) {
            char *equals, *end;

            next = strchr(line, '\n');
            if (next) {
                *next++ = 0;
            }
            if (*line == '#' || *line == ';' || !(equals = strchr(line, '='))) {
                continue;
            }
            *equals = 0;
            for (end = equals; end > line && (end[-1] == ' ' || end[-1] == '\t'); end--) {
                end[-1] = 0;
            }
            value = equals + 1;
            while (*value == ' ' || *value == '\t') {
                value++;
            }
            for (end = (char *)value + strlen(value); end > value && (end[-1] == '\r' || end[-1] == ' '); end--) {
                end[-1] = 0;
            }
            Set(line, value);
        }
        SDL_free(text);
    }
    /* the environment overrides the file for this run */
    if ((value = SDL_getenv("WIPEOUT_FAR_VIEW"))) {
        portSettings.farView = SDL_atoi(value) != 0;
    }
    if ((value = SDL_getenv("WIPEOUT_WIDESCREEN"))) {
        portSettings.widescreen = SDL_atoi(value) != 0;
    }
    if ((value = SDL_getenv("WIPEOUT_FRAME_RATE"))) {
        /* "uncapped", "vsync", "console" or a cap in pictures a second */
        if (!strcmp(value, "uncapped")) {
            portSettings.frameRate = FRAME_RATE_UNCAPPED;
        } else if (!strcmp(value, "vsync")) {
            portSettings.frameRate = FRAME_RATE_VSYNC;
        } else if (!strcmp(value, "console")) {
            portSettings.frameRate = FRAME_RATE_CONSOLE;
        } else if (SDL_atoi(value) > 0) {
            portSettings.frameRate = FRAME_RATE_CAPPED;
            portSettings.frameCap = SDL_clamp(SDL_atoi(value), 30, 1000);
        }
    }
    if ((value = SDL_getenv("WIPEOUT_FADE"))) {
        int a = 0, b = 0;

        if (SDL_sscanf(value, "%d,%d", &a, &b) == 2 && b > a && a > 0) {
            portSettings.fade = 1;
            portSettings.fadeNear = a;
            portSettings.fadeFar = b;
        } else {
            portSettings.fade = 0;
        }
    }
    if (portSettings.fadeFar <= portSettings.fadeNear) {
        portSettings.fadeFar = portSettings.fadeNear + 1000;
    }
}

int PortSettingsSave(void)
{
    FILE *file;

    if (!path[0] || !(file = fopen(path, "w"))) {
        return 0;
    }
    fprintf(file,
            "# Wipeout XL settings (the ~ key opens the debug console that changes them)\n"
            "show_fps = %d\nfar_view = %d\nfade = %d\nfade_near = %d\nfade_far = %d\n"
            "widescreen = %d\nframe_rate = %d\nframe_cap = %d\nskip_intro = %d\nrecord_frames = %d\nrecord_replays = %d\nsave_dir = %s\n",
            portSettings.showFps, portSettings.farView, portSettings.fade, portSettings.fadeNear, portSettings.fadeFar,
            portSettings.widescreen, portSettings.frameRate,
            portSettings.frameCap, portSettings.skipIntro,
            portSettings.recordFrames,
            portSettings.recordReplays,
            portSettings.saveDir);
    fclose(file);
    return 1;
}
