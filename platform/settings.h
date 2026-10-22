/* The player's settings (settings.c): a small INI file in the user's data folder, changed from the
   debug console (debug_ui.cpp, the ~ key). Environment variables still override them for a
   run (WIPEOUT_FAR_VIEW, WIPEOUT_FADE, WIPEOUT_WIDESCREEN, WIPEOUT_FRAME_RATE, WIPEOUT_SAVE_DIR, WIPEOUT_RENDER_SCALE,
   WIPEOUT_HD_PACK); the tests' hidden runs ignore the file. */
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
    int renderScale;      /* the framebuffer's pixels per console pixel; 0: as many as the window shows (gpu_gl.c) */
    int hdTextures;       /* the HD texture pack is drawn (hdtex.c) */
    char hdPackPath[1024]; /* ... the player's pack: its .zip or folder */
} PortSettings;

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
