/* The player's settings (settings.c): a small INI file in the user's data folder, changed from the
   debug console (debug_ui.cpp, the backtick key). Environment variables still override them for a
   run (WIPEOUT_FAR_VIEW, WIPEOUT_FADE, WIPEOUT_DEPTH, WIPEOUT_LARGE_POLYGONS, WIPEOUT_SAVE_DIR); the tests' hidden runs
   ignore the file. */
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
    int depthTest;        /* the per-pixel depth test */
    int largePolygons;    /* near polygons the console's GPU drops for their size are drawn */
    int skipIntro;        /* no intro movie at start-up */
    char saveDir[1024];   /* memory cards ("" = the user's data folder); takes effect at the next start */
} PortSettings;

extern PortSettings portSettings;

void PortSettingsLoad(void);
int PortSettingsSave(void);
const char *PortSettingsPath(void);  /* the file */
const char *PortSettingsDefaultSaveDir(void);

#ifdef __cplusplus
}
#endif

#endif
