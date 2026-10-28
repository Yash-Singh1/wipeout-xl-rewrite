/* The player's settings (settings.h). The file is lines of "key = value"; unknown keys are kept out,
   missing ones take the defaults. */
#include <SDL3/SDL.h>
#include <stdio.h>
#include <string.h>

#include "platform.h"
#include "settings.h"

PortSettings portSettings = {0, 1, 1, 64000, 96000, 0, FRAME_RATE_UNCAPPED, 144, 0, RECORD_OFF, 0, ""};

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
    } else if (!strcmp(key, "far_flares")) {
        portSettings.farFlares = number != 0;
    } else if (!strcmp(key, "fade")) {
        portSettings.fade = number != 0;
    } else if (!strcmp(key, "fade_near") && number > 0) {
        portSettings.fadeNear = number;
    } else if (!strcmp(key, "fade_far") && number > 0) {
        portSettings.fadeFar = number;
    } else if (!strcmp(key, "depth_buffer")) {
        portSettings.consoleDepth = number == 0;
    } else if (!strcmp(key, "widescreen")) {
        portSettings.widescreen = number != 0;
    } else if (!strcmp(key, "frame_rate")) {
        portSettings.frameRate = SDL_clamp(number, FRAME_RATE_UNCAPPED, FRAME_RATE_CONSOLE);
    } else if (!strcmp(key, "frame_cap") && number > 0) {
        portSettings.frameCap = SDL_clamp(number, 30, 1000);
    } else if (!strcmp(key, "skip_intro")) {
        portSettings.skipIntro = number != 0;
    } else if (!strcmp(key, "record_rate")) {
        portSettings.recordRate = SDL_clamp(number, 30, 240);
    } else if (!strcmp(key, "skip_logos")) {
        portSettings.skipLogos = number != 0;
    } else if (!strcmp(key, "video_standard")) {
        portSettings.videoStandard = SDL_clamp(number, VIDEO_BY_DISC, VIDEO_PAL);
    } else if (!strcmp(key, "record_frames")) {
        portSettings.recordFrames = SDL_clamp(number, RECORD_OFF, RECORD_WITH_TAB);
    } else if (!strcmp(key, "record_replays")) {
        portSettings.recordReplays = number != 0;
    } else if (!strcmp(key, "save_dir")) {
        SDL_strlcpy(portSettings.saveDir, value, sizeof(portSettings.saveDir));
    } else if (!strcmp(key, "disc")) {
        SDL_strlcpy(portSettings.discPath, value, sizeof(portSettings.discPath));
    } else if (!strcmp(key, "original")) {
        SDL_strlcpy(portSettings.originalPath, value, sizeof(portSettings.originalPath));
    } else if (!strcmp(key, "wipeout64")) {
        SDL_strlcpy(portSettings.wipeout64Path, value, sizeof(portSettings.wipeout64Path));
    } else if (!strcmp(key, "custom_music")) {
        portSettings.customMusic = number != 0;
    } else if (!strcmp(key, "music_shuffle")) {
        portSettings.musicShuffle = number != 0;
    } else if (!strcmp(key, "music_folder")) {
        SDL_strlcpy(portSettings.musicFolder, value, sizeof(portSettings.musicFolder));
    } else if (!strcmp(key, "save_slot")) {
        SDL_strlcpy(portSettings.saveSlot, value, sizeof(portSettings.saveSlot));
    } else if (!strcmp(key, "autosave")) {
        portSettings.noAutoSave = number == 0;
    } else if (!strcmp(key, "render_scale")) {
        portSettings.renderScale = SDL_clamp(number, 0, 16);
    } else if (!strcmp(key, "hd_textures")) {
        portSettings.hdTextures = number != 0;
    } else if (!strcmp(key, "hd_pack")) {
        SDL_strlcpy(portSettings.hdPackPath, value, sizeof(portSettings.hdPackPath));
    } else if (!strcmp(key, "touch_controls")) {
        portSettings.touchControls = SDL_clamp(number, 0, 2);
    } else if (!strcmp(key, "touch_size")) {
        portSettings.touchSize = SDL_clamp(number, 50, 200);
    } else if (!strcmp(key, "touch_opacity")) {
        portSettings.touchOpacity = SDL_clamp(number, 10, 100);
    } else if (!strcmp(key, "vibration")) {
        portSettings.noRumble = number == 0;
    } else if (!strcmp(key, "pause_unfocused")) {
        portSettings.noFocusPause = number == 0;
    } else if (!strcmp(key, "surround")) {
        portSettings.surround = SDL_clamp(number, SURROUND_OFF, SURROUND_HEADPHONES);
    } else if (!strcmp(key, "fullscreen")) {
        portSettings.fullscreen = number != 0;
    } else if (!strcmp(key, "vibration_strength") && number > 0) {
        portSettings.rumbleStrength = SDL_clamp(number, 50, 300);
    } else if (!strcmp(key, "negcon")) {
        portSettings.negconPad = number != 0;
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
#ifdef PORT_MOBILE
    portSettings.widescreen = 1; /* a phone or tablet: its wide screen filled (until the player says otherwise) */
#endif
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
    if ((value = SDL_getenv("WIPEOUT_SAVE_SLOT"))) {
        /* the save in use ("card:name"; tests) */
        SDL_strlcpy(portSettings.saveSlot, value, sizeof(portSettings.saveSlot));
    }
    if ((value = SDL_getenv("WIPEOUT_RENDER_SCALE"))) {
        portSettings.renderScale = SDL_clamp(SDL_atoi(value), 0, 16);
    }
    if ((value = SDL_getenv("WIPEOUT_HD_PACK"))) {
        /* a pack's .zip or folder, drawn ("" not) */
        SDL_strlcpy(portSettings.hdPackPath, value, sizeof(portSettings.hdPackPath));
        portSettings.hdTextures = value[0] != 0;
    }
    if ((value = SDL_getenv("WIPEOUT_TOUCH"))) {
        portSettings.touchControls = SDL_clamp(SDL_atoi(value), 0, 2); /* 0 automatic, 1 on, 2 off */
    }
    if ((value = SDL_getenv("WIPEOUT_FRAMECAP_RATE"))) {
        portSettings.recordRate = SDL_clamp(SDL_atoi(value), 30, 240);
    }
    if ((value = SDL_getenv("WIPEOUT_FAR_FLARES"))) {
        portSettings.farFlares = SDL_atoi(value) != 0;
    }
    if ((value = SDL_getenv("WIPEOUT_SKIP_LOGOS"))) {
        portSettings.skipLogos = SDL_atoi(value) != 0;
    }
    if ((value = SDL_getenv("WIPEOUT_VIDEO_STANDARD"))) {
        /* "ntsc", "pal", anything else the disc's */
        portSettings.videoStandard = !SDL_strcasecmp(value, "ntsc") ? VIDEO_NTSC
                                     : !SDL_strcasecmp(value, "pal") ? VIDEO_PAL : VIDEO_BY_DISC;
    }
    if ((value = SDL_getenv("WIPEOUT_SURROUND"))) {
        /* "off", "receiver" or "headphones" */
        portSettings.surround = !strcmp(value, "receiver")     ? SURROUND_RECEIVER
                                : !strcmp(value, "headphones") ? SURROUND_HEADPHONES : SURROUND_OFF;
    }
    if ((value = SDL_getenv("WIPEOUT_DEPTH_BUFFER"))) {
        portSettings.consoleDepth = SDL_atoi(value) == 0;
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

    if (portConfig.hidden) {
        return 0; /* the tests' hidden runs neither read the player's file nor write it */
    }
    if (!path[0] || !(file = fopen(path, "w"))) {
        return 0;
    }
    fprintf(file,
            "# Wipeout XL settings (the ~ key opens the debug console that changes them)\n"
            "show_fps = %d\nfar_view = %d\nfade = %d\nfade_near = %d\nfade_far = %d\n"
            "widescreen = %d\nframe_rate = %d\nframe_cap = %d\nskip_intro = %d\nrecord_frames = %d\nrecord_replays = %d\nsave_dir = %s\ndisc = %s\noriginal = %s\n"
            "render_scale = %d\nhd_textures = %d\nhd_pack = %s\nwipeout64 = %s\n"
            "custom_music = %d\nmusic_shuffle = %d\nmusic_folder = %s\nsave_slot = %s\nautosave = %d\n"
            "touch_controls = %d\ntouch_size = %d\ntouch_opacity = %d\ndepth_buffer = %d\nskip_logos = %d\nrecord_rate = %d\n"
            "video_standard = %d\nvibration = %d\nvibration_strength = %d\nfar_flares = %d\npause_unfocused = %d\n"
            "surround = %d\nfullscreen = %d\nnegcon = %d\n",
            portSettings.showFps, portSettings.farView, portSettings.fade, portSettings.fadeNear, portSettings.fadeFar,
            portSettings.widescreen, portSettings.frameRate,
            portSettings.frameCap, portSettings.skipIntro,
            portSettings.recordFrames,
            portSettings.recordReplays,
            portSettings.saveDir, portSettings.discPath, portSettings.originalPath, portSettings.renderScale,
            portSettings.hdTextures, portSettings.hdPackPath, portSettings.wipeout64Path, portSettings.customMusic,
            portSettings.musicShuffle, portSettings.musicFolder, portSettings.saveSlot, !portSettings.noAutoSave,
            portSettings.touchControls, portSettings.touchSize ? portSettings.touchSize : 100,
            portSettings.touchOpacity ? portSettings.touchOpacity : 60, !portSettings.consoleDepth, portSettings.skipLogos,
            portSettings.recordRate ? portSettings.recordRate : 60, portSettings.videoStandard, !portSettings.noRumble,
            portSettings.rumbleStrength ? portSettings.rumbleStrength : 100, portSettings.farFlares,
            !portSettings.noFocusPause, portSettings.surround, portSettings.fullscreen,
            portSettings.negconPad);
    fclose(file);
    return 1;
}
