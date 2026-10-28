/* The player's settings (settings.c): a small INI file in the user's data folder, changed from the
   debug console (debug_ui.cpp, the ~ key). Environment variables still override them for a
   run (WIPEOUT_FAR_VIEW, WIPEOUT_FAR_FLARES, WIPEOUT_FADE, WIPEOUT_WIDESCREEN, WIPEOUT_FRAME_RATE, WIPEOUT_SAVE_DIR, WIPEOUT_RENDER_SCALE,
   WIPEOUT_HD_PACK, WIPEOUT_MUSIC, WIPEOUT_TOUCH, WIPEOUT_SURROUND); the tests' hidden runs ignore the file. */
#ifndef PORT_SETTINGS_H
#define PORT_SETTINGS_H

#ifdef __cplusplus
extern "C" {
#endif

typedef struct PortSettings {
    int showFps;          /* the frame-rate overlay */
    int farView;          /* 0: the console's draw distance; 1: the whole track */
    int fade;             /* distant geometry fades into the sky */
    int fadeNear, fadeFar;
    int widescreen;       /* the picture fills a wider window: more of the scene at the sides (gpu_gl.c) */
    int frameRate;        /* the display: FRAME_RATE_*; the game itself keeps its 30 frames a second */
    int frameCap;         /* pictures a second with FRAME_RATE_CAPPED */
    int skipIntro;        /* no intro movie at start-up */
    int recordFrames;     /* the frame recorder (psx/framecap.h): RECORD_* */
    int recordReplays;    /* every session is recorded as a replay (trace.c, --record) */
    char saveDir[1024];   /* memory cards ("" = the user's data folder); takes effect at the next start */
    char discPath[1024];  /* the disc's .cue chosen last (main.c), for a start without --disc */
    char originalPath[1024]; /* the original Wipeout's disc image or data folder: its tracks (original.c) */
    char wipeout64Path[1024]; /* Wipeout 64's cartridge image: its tracks too (original.c) */
    int customMusic;      /* the player's music folder in place of the disc's soundtrack (music.c) */
    int musicShuffle;     /* ... its songs in a random order */
    char musicFolder[1024];
    char saveSlot[16];    /* the save in use: "card:name" (game/src/portsave.c), "" when none */
    int noAutoSave;       /* the game does not save itself */
    int noRumble;         /* the pads' motors stay still (port_platform.h's PortPadRumble) */
    int noFocusPause;     /* a race goes on when the window loses the focus (port_platform.h's PortFocusPause) */
    int rumbleStrength;   /* ... how hard they shake, in percent (0: 100) */
    int negconPad;        /* a neGcon is plugged in (psx/pad.h's PAD_NEGCON), not a digital pad */
    int renderScale;      /* the framebuffer's pixels per console pixel; 0: as many as the window shows (gpu_gl.c) */
    int hdTextures;       /* the HD texture pack is drawn (hdtex.c) */
    char hdPackPath[1024]; /* ... the player's pack: its .zip or folder */
    int touchControls;    /* the touch controls (touch.h): TOUCH_AUTO, TOUCH_ON or TOUCH_OFF */
    int touchSize;        /* ... their size, in percent (0: 100) */
    int touchOpacity;     /* ... how opaque, in percent (0: 60) */
    int consoleDepth;     /* no depth buffer: the game's ordering tables alone decide what is in front (gpu_gl.c) */
    int skipLogos;        /* no logo screens at start-up (Sony, the copyright, Dolby, Red Bull) */
    int recordRate;       /* the frame recorder's pictures a second, up to: 30 the game's frames, more the ones between
                             (psx/framecap.h; 0: 60) */
    int videoStandard;    /* VIDEO_BY_DISC, VIDEO_NTSC or VIDEO_PAL (region.c); takes effect at the next start */
    int farFlares;        /* the engine flares of the ships the whole track's view draws beyond the console's distance
                             (portfar.c; the console drew none there) */
    int surround;         /* the game's Stereo mode, a Dolby Surround encoding of its sound effects: SURROUND_* */
    int fullscreen;       /* the window was full screen when last left: the next start is too (video.c) */
} PortSettings;

/* PortSettings.surround */
enum {
    SURROUND_OFF,        /* in phase with the rest, as stereo speakers and headphones want (game/src/sfx.c) */
    SURROUND_RECEIVER,   /* the console's encoding, the right channel's phase inverted, for a Pro Logic receiver */
    SURROUND_HEADPHONES  /* decoded, its surround channel heard from behind on headphones (platform/psx/surround.c) */
};

/* PortSettings.videoStandard: the disc's (Wipeout XL NTSC, WipEout 2097 PAL), or either for both */
enum { VIDEO_BY_DISC, VIDEO_NTSC, VIDEO_PAL };

/* PortSettings.recordFrames: what the frame recorder keeps, and what Tab does */
enum {
    RECORD_OFF,
    RECORD_LAST_SECONDS, /* the last 3 seconds; Tab freezes the game to look at them */
    RECORD_WITH_TAB      /* from one Tab to the next (up to the last 10 seconds); the game goes on */
};

enum {
    FRAME_RATE_UNCAPPED, /* as many pictures as the computer draws, moving between the game's frames */
    FRAME_RATE_VSYNC,    /* ... one per refresh of the display */
    FRAME_RATE_CAPPED,   /* ... at most frameCap a second */
    FRAME_RATE_CONSOLE   /* the console's: each field shows the game's last frame */
};

extern PortSettings portSettings;

void PortSettingsLoad(void);
int PortSettingsSave(void);
const char *PortSettingsPath(void);  /* the file */
const char *PortSettingsDefaultSaveDir(void);

#ifdef __cplusplus
}
#endif

#endif
