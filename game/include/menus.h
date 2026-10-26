#ifndef MENUS_H
#define MENUS_H

#define MENU_MAIN                0
#define MENU_RACE_TYPE           1
#define MENU_RACING_CLASS        2
#define MENU_OPTIONS             3
#define MENU_TEAM                4
#define MENU_VECTOR_TRACK        5
#define MENU_VENOM_TRACK         6
#define MENU_RAPIER_TRACK        7
#define MENU_PHANTOM_TRACK       8
#define MENU_AUDIO               9
#define MENU_DIGITAL_PAD         10
#define MENU_LOAD_SAVE           12
#define MENU_BEST_TIMES          13
#define MENU_SHOW_PASSWORD       14
#define MENU_GAME_CONFIG         15
#define MENU_SCREEN              16
#define MENU_ARCADE_SCORES       17
#define MENU_TIME_TRIAL_SCORES   18
#define MENU_LINK                19
#define MENU_NEGCON_SELECT       20
#define MENU_NEGCON_TWIST        21
#define MENU_NEGCON_BUTTONS      22
#define MENU_ENTER_PASSWORD      23

#include "wipeout.h"

/* Load/save screen selection state (fields follow the menu's use of each offset). */
typedef struct SaveSelection {
    unsigned char pad0[0x74];
    short nameActive;
    char  fileName[42];
    short nameLength, loadMode, selected, enteringName, nameReady, confirmed, dupeCheck;
    short padAE;
    int   cardSelection[2];
    short timer;
} SaveSelection;

/* Rows of MusicScreen: the volume bars, the markers that slide along them and the arrows. */
enum {
    MUSIC_BAR, SFX_BAR, MUSIC_MARKER, SFX_MARKER, LEFT_ARROW, RIGHT_ARROW
};

/* Six rows of box polys (one pair per screen buffer) and the outline of the screen. */
typedef struct DrBox {
    short coords[24];
    POLY_FT4 rows[6][2];
} DrBox;


/* Six rows of two textured polys, one per screen buffer: the bars of the music screen. */
typedef struct MusicScreen {
    DrBox box;
    POLY_FT4 rows[6][2];
} MusicScreen;


/* Fourteen seed words that scramble and check the password digits. */
typedef struct PasswordSeeds {
    unsigned short values[14];
} PasswordSeeds;

/* A line of menu text and how it is drawn. */
typedef struct MenuLabel {
    char  text[26];
    short size;
    short colour;
} MenuLabel;

/* One selectable line of a menu: its text variants, position and the menu it leads to. */
typedef struct MenuOption {
    MenuLabel text[4];
    short     x;
    short     y;
    short     numText;
    short     font;
    short     nextMenu;
    short     selection;
    short     enabled;
    short     flags;
} MenuOption;

/* One 0x46E-byte record per menu level: the title, the option lines and the navigation state.
   mode is a second per-menu value: the cursor column of the password screen, for instance. */
typedef struct MenuEntry {
    MenuOption options[8];
    short      id;
    short      flags;
    MenuLabel  title;
    short      numOptions;
    short      prevMenu;
    short      nextMenu;
    short      selection;
    short      mode;
    short      updateCount;
} MenuEntry;

/* Unlock flags as the high-score screen reads them from the game data. */
typedef struct UnlockData {
    unsigned char pad0[62];
    unsigned char extraTracks;
    unsigned char pad3F[33];
    unsigned int unlockedTracks;
} UnlockData;

/* One best-time entry: race and lap records for a track. */
typedef struct HiScore {
    char name[4];
    int time;
    signed char ship;
    char lapName[7];
    int lapTime;
    signed char lapShip;
    char pad15[3];
} HiScore;

/* All polygons of the menu screens, one per screen buffer: the red and purple backdrops, the
   screen border, the selection shades, the pad and knob graphics, the list backgrounds and
   the popup, card-message and score boxes. */
typedef struct MenuPolys {
    POLY_G4  redA[2];
    POLY_G4  redB[2];
    POLY_G4  purple[2];
    LINE_F2  border[4][2];
    POLY_G4  shade0[2];
    DR_MODE  mode0[2];
    POLY_G4  shade1[2];
    DR_MODE  mode1[2];
    POLY_G4  shade2[2];
    DR_MODE  mode2[2];
    POLY_G4  shade3[2];
    DR_MODE  mode3[2];
    POLY_G4  shade4[2];
    DR_MODE  mode4[2];
    POLY_G4  selection[5][2];
    POLY_FT4 arrowTexture[3][2];
    LINE_F2  arrowLine[6][2];
    POLY_F4  background[30][2];
    POLY_FT4 backdrop[30][2];
    POLY_F4  boxFill[2];
    POLY_F4  boxBorder[2];
    POLY_F4  redBox[2];
    POLY_F4  lowerFill[2][2];
    POLY_F4  lowerBorder[2][2];
    POLY_F4  topFill[2];
    POLY_F4  topBorder[2];
} MenuPolys;

/* One box of a menu screen: its corners, the four outline lines per screen buffer and a flag. */
struct MenuBox {
    unsigned short x0, y0, x1, y1;
    LINE_F2        edge[4][2];
    short          active;
    short          pad;
};

/* The boxes of one menu, as stored in the menu line data file. */
typedef struct MenuLayout {
    MenuBox box[20];
    int    pad;
    int    numBoxes;
} MenuLayout;

/* The highlight bars behind the options of a menu and the title graphic of the main menu. */
typedef struct MenuBars {
    POLY_F4  options[8][2];
    POLY_FT4 title[2];
} MenuBars;

/* The orders in which the class and race types are shown. */
typedef struct ClassOrder {
    unsigned char order[4];
} ClassOrder;

typedef struct RaceOrder {
    unsigned char order[6];
} RaceOrder;

/* A whole menu data file, loaded from disk and copied into the menu-line table. */
typedef struct MenuFile {
    char bytes[0x10740];
} MenuFile;

/* Three 16-bit screen coordinates (x, y, and a third value) of the screen-offset menu. */
typedef struct Positions {
    short value[3];
} Positions;

/* A texture record as Load16BitTexture returns it: where the image sits in VRAM and its size. */
typedef struct LoadedImage {
    unsigned char pad0[0xC];
    short x;
    short y;
    unsigned short height;
    unsigned short width;
} LoadedImage;

/* The button codes a digital pad can be configured to use, and the ones for the analogue pad. */
typedef struct DigitalButtons {
    unsigned short button[8];
} DigitalButtons;

typedef struct AnalogButtons {
    unsigned short button[6];
} AnalogButtons;

/* Width of the highlight bar behind each key function name. */
typedef struct HighlightWidths {
    short value[6];
} HighlightWidths;

/* Digital and analogue key assignments: one 16-bit button code per control. */
typedef struct KeyMap {
    unsigned short key[9];
} KeyMap;

/* Button-list indices chosen for each control. */
typedef struct Calibration {
    short value[6];
} Calibration;

/* Pad key configuration: the key assignments of the digital [0] and analogue [1] pad and the
   indices of the chosen buttons in their lists. */
typedef struct KeyConfig {
    KeyMap      keys[2];
    signed char active;
    unsigned char pad25;
    Calibration calibration[2];
} KeyConfig;

/* Config-screen selection block: the per-player settings and key assignments of the menus that
   edit them (audio, pad, screen, card and team). Offsets follow the config screens' use. */
typedef struct MenuSelection {
    short         screenX;
    short         screenY;
    char          screenOffsetState;
    char          weapons;
    char          moveScreen;
    char          internalView;
    short         musicVolume;
    short         effectVolume;
    short         otherMusic;
    short         otherEffect;
    signed char   music;
    signed char   stereo;
    char          actTrackNum;
    char          trackNum;
    char          shipClass;
    signed char   ship;
    short         current;
    short         trackSelection;
    Calibration   calibration[2];
    KeyMap        keys[2];
    char          fileName[9];
    Password      password;
    signed char   passwordCheck;
    unsigned char negconMin;
    unsigned char negconCenter;
    signed char   negconType;
    char          negconFlags[3];
    unsigned char pad73;
    short         scoreIndex;
    char          scoreFlags[10];
    int          scoreName;
    short         scoreNameEnd;
    unsigned char pad86[26];
    short         record;
    short         highScoreScratchA2;
    short         highScoreScratchA4;
    short         highScoreScratchA6;
    short         highScoreScratchA8;
    short         highScoreScratchAA;
    short         highScoreScratchAC;
    unsigned char padAE[2];
    int           highScoreScratchB0;
    int           highScoreScratchB4;
    short         timer;
} MenuSelection;

/* The three load-menu labels. */
typedef struct LabelList {
    char *p[3];
} LabelList;

/* A countdown in frames and the value (message or action) it applies to. */
typedef struct CardTimer {
    short timer;
    short id;
} CardTimer;

/* Twist angle in degrees for each negcon twist setting. */
typedef struct TwistAngles {
    short value[4];
} TwistAngles;

/* Ship group of each team in the team menu. */
typedef struct TeamOrder {
    short order[5];
} TeamOrder;

#endif
