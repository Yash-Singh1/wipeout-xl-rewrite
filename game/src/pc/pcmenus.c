/* The PC version's menus (menus.c): its main menu of three panels over a field of tiles, the race type, team, class
   and track screens with their turning models, the options (audio, controller, preferences, load and save, best
   times) and the quit question, each screen sliding in between the backdrop's bands with a burst of sparks.

   The PC's code, kept to its shape, with these differences (each marked "Port:"):
   - Its gameType is the PlayStation's raceType here (0 arcade, 1 time trial, 4 challenge, 99 attract: the PC's
     challenge is 2), which the race reads; the PlayStation's gameType stays 2 (one player).
   - The input is the port's (GetMenuKey and the pad's keys), for keyboards and gamepads alike: what the PC typed (a
     name) is entered letter by letter with Up and Down, and its prompts show the device's buttons.
   - Its controller screen shows the choice of device without effect (the port reads the keyboard and gamepads at
     once), and its key redefinition is left out: the port's controls are set in its own settings.
   - Its load and save screen keeps the port's saves (the memory card's, as the PlayStation's menus do) in place of
     its SAVEGAME.BIN, and its options are kept by the port's autosave in place of its CONFIG.DAT.
   - Only its Direct3D path is here (not its software renderer's fills), and its texture pages are the port's
     (pc.h); its 3D models are drawn by the port's object renderer, as the PlayStation's menus draw them. */
#include <string.h>

#include "pcmenus.h"
#include "../../include/ayrace.h"
#include "../../include/card.h"
#include "../../include/pad.h"
#include "../../include/port_platform.h"
#include "../../../platform/controls.h"

extern int currMenu;
extern MenuEntry *menu;
extern MenuLayout *menuLayout;
extern s32 raceType;
extern s32 ownShip;
extern int remoteShip;
extern s32 trackNo;
extern s16 raceFlag;
extern s32 quadIndex;
extern short CurrentScreen;
extern s16 fastFrame;
extern short lastTopOption;
extern s32 shipSpin;
extern void *preCalcObject;
extern SVECTOR VertexNormal[];
extern s32 envmapr;
extern s32 envmapg;
extern s32 envmapb;
extern u_char trackMedals[];
extern unsigned char raceTracks[];
extern short trackModelOrder[];
extern Object *menuModels[];
extern Object *shipModels[];
extern int cardNum;
extern int heading;
extern signed char cdPlaySeconds[];
extern struct WipeCdStatus cdStatus[];
extern u_char cdMusicIndex;
extern s32 cdMusicTrack;
extern s32 cdMusicCount;
extern int ambientVoice1;
extern short sixTracks;
extern short sixXPlayed;
extern short eightTracks;
extern short eightXPlayed;
extern CardSlot cardSlots[];
extern unsigned short keysDown;
void InitNewDisplay(ConfigData *gameData);

/* GetMenuKey's keys, and the pad's bits of the same keys (keysDown held, keysPressed pressed this frame). */
enum { MENU_UP, MENU_DOWN, MENU_LEFT, MENU_RIGHT, MENU_BACK, MENU_ACCEPT };
#define KEY_RIGHT 0x0020
#define KEY_LEFT 0x0080
#define KEY_BACK 0x1000
#define KEY_ACCEPT 0x4000

/* The menus' sounds (sound_samples.h). */
enum { SOUND_WEAPON_HIT = 5, SOUND_MENU_MOVE = 8, SOUND_MENU_SELECT = 9, SOUND_MENU_SLIDE = 49 };
#define Sound(sample) PlayNote((sample), 255, 15, 15, 256)

/* Port: the race types as the race reads them (raceType), in the race type menu's order. */
enum { RACE_ARCADE = 0, RACE_TIME_TRIAL = 1, RACE_CHALLENGE = 4, RACE_ATTRACT = 99 };

/* The PC's button icons (hud.c's buttonIcons, in HUD2): DoButton's icon numbers. */

/* The settings a screen changes until it is left (the PC's menuState). */
static struct {
    u_char weapons;
    u_char internalView;
    short musicVolume, effectVolume;
    short otherMusic, otherEffect;
    signed char music; /* the CD track: 1 plays them all, shuffled */
    signed char actTrackNum, trackNum, shipClass;
    signed char ship;    /* the team screen's ship */
    short current;       /* the screen's highlighted row */
    short trackSelection;
} menuState;

/* Port: a name the player enters (the preferences' default name, a save's name): letter by letter, the cursor's
   letter changed with Up and Down, the cursor moved with Left and Right. */
typedef struct NameEntry {
    char text[13];
    int length; /* its letters at most */
    int cursor;
    int editing;
} NameEntry;

/* The PC's save slots, here the first saves of Wipeout XL on the first memory card. */
#define SAVE_SLOTS 8
#define SAVE_NAME_LENGTH 8 /* (a memory card save's name) */
typedef struct SaveSlot {
    char name[SAVE_NAME_LENGTH + 1];
    int occupied;
} SaveSlot;

/* The screens' state between frames. */
static int leaving;          /* the race starts (or the attract mode) */
static int quitAsked;        /* the quit question is up (the PC's pauseRequested) */
static int quitSelection;    /* ... and YES is chosen */
static int screenCancelled;  /* a screen asks to go back (the PC's redefineCancelled) */
static int sliding;          /* the next screen slides in (menuSlideActive) */
static int slideFrame;
static short slideX, slideY, slideStep;
static int frameSteps;       /* the fields the last frame took (the PC's FR60) */
static int blinkFrame, blinkFlag;
static short savedRaceSelection, savedTeamSelection, savedClassSelection, savedTrackSelection;
static short savedMainSelection = 3;
static short lastClassSelection;
static int attractSettingsSaved, attractShip, attractTrack, attractTrackNum, attractActTrackNum;
static int deviceType, savedDeviceType;
static int saveState, saveCursor, saveMessageFrames, saveOk;
static SaveSlot saveSlots[SAVE_SLOTS];
static NameEntry defaultName, saveName;
static int cdState = 1;

/* The models (the controller screen's devices: CNTRL.PRM; harry and june are the PlayStation's menus' too). */
static Object *controllerModels[4];

/* The backdrop: the tiles over the whole screen (BACKGRND), the three bands of panels (TESTMENU) and their place,
   the lines of the menus' boxes. */
#define TILES_ACROSS 10 /* the PC's 640x480 in tiles of 64x60 */
#define TILES_DOWN 8
static POLY_FT4 menuTiles[TILES_ACROSS * TILES_DOWN];
static POLY_FT4 menuBackPolys[14];
static RECT menuBackdropRects[14];
static POLY_F4 boxEdge;

/* ---- drawing helpers (hud.c, ayrace.c) ---- */

/* A console icon for each of the PC's: the one the port's glyphs replace (controls.h). */
/* An option's backdrop in the main menu: a texel of HUD2 over the option's box, lighter when it is chosen. (The
   PC's software renderer fills the lower two instead.) */
static void PcDrawMenuOptionBackdrop(short x, short y, short width, short height, short option, short selected)
{
    POLY_FT4 quad;

    PcSetTextQuad(&quad, 0, 0, 130, option == selected ? 56 : 62, 1, 1, 32, 0);
    x *= 2, y *= 2, width *= 2, height *= 2;
    quad.x0 = x, quad.y0 = y;
    quad.x1 = x + width, quad.y1 = y;
    quad.x2 = x, quad.y2 = y + height;
    quad.x3 = x + width, quad.y3 = y + height;
    DrawScaledTexturedQuad(0.96f, &quad, 0);
}

/* The main menu's title: the WIPEOUT and XL logo of XLLOGO. */
static void DrawTitle(void)
{
    POLY_FT4 title;

    PcSetTextQuad(&title, 25, 38, 0, 0, 254, 64, 35, 0);
    DrawScaledTexturedQuad(pcTextDepth, &title, 0);
    PcSetTextQuad(&title, 151, 38, 0, 64, 210, 64, 35, 0);
    DrawScaledTexturedQuad(pcTextDepth, &title, 0);
}

/* Port: the PC's Direct3D view of its menus' models (render.c's view matrix, its viewport's scale): from
   PC_VIEW_BACK units further back than the console's, 320 of the console's pixels across and 240 down per unit of
   depth, where the console's GTE has 160 both ways. A model is drawn moved back and squeezed down to three
   quarters, for a GTE seeing 320 (PcMenuControl). */
#define PC_VIEW_BACK 1078 /* (10 / 38 of a Direct3D unit, 4096 of the console's) */

/* A model of the menus at (x, y, z), turned by dir; scale -1 draws it with the medal's environment map (the PC's
   DrawTeamShips). */
static void DrawMenuModel(int x, int y, int z, short dir, short scale, Object *obj)
{
    int i;

    SetSkeletonPosition(obj->skeleton, x, y * 3 / 4, z + PC_VIEW_BACK);
    SetSkeletonDirection(obj->skeleton, scale, dir, 0);
    for (i = 0; i < 3; i++) {
        obj->skeleton->relative.m[1][i] = (short)(obj->skeleton->relative.m[1][i] * 3 / 4);
    }
    if (scale == -1) {
        if (preCalcObject != obj) {
            PreCalcNormals(obj, VertexNormal);
            preCalcObject = obj;
        }
        DrawEnvMap(obj, 0, VertexNormal, -0x6D, -0xF, 0x120);
        return;
    }
    AsmDrawObject(obj, 0, 0);
}

/* The name of a track by its number on the disc. */
static char *PcGetTrackName(short type)
{
    switch (type) {
    case 1: return "TALON'S REACH";
    case 2: return "GARE D'EUROPA";
    case 6: return "VOSTOK ISLAND";
    case 7: return "SPILSKINANKE";
    case 8: return "SAGARMATHA";
    case 13: return "VALPARAISO";
    case 17: return "ODESSA KEYS";
    case 20: return "PHENITIA PARK";
    }
    return "";
}

/* ---- the models and the layout ---- */

/* A model file of COMMON (name.prm) into models, each of its objects with a skeleton; with its own textures
   (name.cmp) when it has them (LoadModel, LoadMenuModel). */
static void PcLoadModel(Object **models, int capacity, const char *name, int ownTextures)
{
    char path[64];
    short firstTexture = (short)TextureTableCount;
    Object *object;
    int i = 0;

    heap = reserveHeap;
    if (ownTextures) {
        sprintf(path, "c:\\wipeout2\\common\\%s.cmp", name);
        LoadCmpFiles(LoadCompressedTextureSequence(path));
    }
    sprintf(path, "c:\\wipeout2\\common\\%s.prm", name);
    object = *models = LoadPrm(path, ownTextures ? firstTexture : 0);
    while (object && i < capacity) {
        object->skeleton = NewSkeleton();
        models[i++] = object;
        object = object->next;
    }
    heap = mainheapcopy;
}

/* MENU.DAT: the boxes of each menu (the PC's menu editor's), their corners put in order. */
static void PcLoadMenuLineData(void)
{
    char path[] = "c:\\wipeout2\\common\\menu.dat";
    size_t size = PC_MENU_LAYOUTS * sizeof(MenuLayout);
    int length = FileLength(path);
    char *buffer;
    int i, j;

    memset(menuLayout, 0, size);
    if (length <= 0) {
        return;
    }
    buffer = DAlloc(reserveHeap, length + 0x20);
    if (LoadFile(path, buffer) == length) {
        memcpy(menuLayout, buffer, (size_t)length < size ? (size_t)length : size);
    }
    DFree(reserveHeap, buffer);
    /* (NormaliseMenuBoxes) */
    for (i = 0; i < PC_MENU_LAYOUTS; i++) {
        for (j = 0; j < 20; j++) {
            MenuBox *box = &menuLayout[i].box[j];
            u_short x0 = box->x0, y0 = box->y0, x1 = box->x1, y1 = box->y1;

            box->x0 = x0 < x1 ? x0 : x1;
            box->x1 = x0 < x1 ? x1 : x0;
            box->y0 = y0 < y1 ? y0 : y1;
            box->y1 = y0 < y1 ? y1 : y0;
        }
    }
}
_Static_assert(sizeof(MenuLayout) == 0xAF8, "a layout of MENU.DAT");

/* ---- the menus' set-up ---- */

static void PcInitMenus(void)
{
    int m, o, line;

    memset(menu, 0, PC_MENU_COUNT * sizeof(MenuEntry));
    for (m = 0; m < PC_MENU_COUNT; m++) {
        const PcMenuDef *def = &pcMenuDefs[m];

        strcpy(menu[m].title.text, def->title);
        menu[m].title.size = 40; /* (the title's place: x 40, y 50) */
        menu[m].title.colour = 50;
        menu[m].numOptions = def->numOptions;
        menu[m].prevMenu = def->prevMenu;
        menu[m].nextMenu = def->nextMenu;
        menu[m].flags = m == PC_MENU_MAIN;
        for (o = 0; o < 6 && def->options[o].lines[0]; o++) {
            MenuOption *option = &menu[m].options[o];

            for (line = 0; line < 3 && def->options[o].lines[line]; line++) {
                strcpy(option->text[line].text, def->options[o].lines[line]);
                option->text[line].size = 8;
            }
            option->numText = line;
            option->x = def->options[o].x;
            option->y = def->options[o].y;
            option->font = 9;
            option->nextMenu = def->options[o].nextMenu;
            option->enabled = def->options[o].enabled;
        }
    }
}

/* What the player has opened: the challenge, the Phantom class (and the films already seen). */
static void PcSetupUnlocks(ConfigData *config)
{
    menu[PC_MENU_CLASS].selection = lastClassSelection;
    if (config->venomComplete) {
        menu[PC_MENU_RACE_TYPE].numOptions = 3;
        strcpy(menu[PC_MENU_RACE_TYPE].options[2].text[0].text, "CHALLENGE I");
    } else if (config->rapierComplete) {
        menu[PC_MENU_RACE_TYPE].numOptions = 3;
        strcpy(menu[PC_MENU_RACE_TYPE].options[2].text[0].text, "CHALLENGE II");
    }
    if (config->class1Unlocked || config->phantomTrackCheatActive) {
        menu[PC_MENU_CLASS].options[3].enabled = 1;
    }
    if (config->rapierComplete || config->trackCheatActive) {
        menu[PC_MENU_CLASS].options[3].enabled = 1;
    }
    sixXPlayed = 0;
    eightXPlayed = 0;
    sixTracks = 0;
    eightTracks = 0;
    if (config->venomComplete) {
        sixXPlayed = 1;
        sixTracks = 1;
    }
    if (config->rapierComplete) {
        eightXPlayed = 1;
        eightTracks = 1;
        sixXPlayed = 1;
        sixTracks = 1;
    }
}

static void PcInitMenuState(ConfigData *config)
{
    menuState.weapons = config->weaponsOn;
    menuState.internalView = config->internalView;
    menuState.musicVolume = config->musicVolume;
    menuState.effectVolume = config->effectVolume;
    menuState.otherMusic = config->otherMusicVolume;
    menuState.otherEffect = config->otherEffectVolume;
    menuState.music = config->cdTrack;
    menuState.actTrackNum = config->actTrackNum;
    menuState.shipClass = config->shipClass;
    menuState.ship = ownShip;
    menuState.trackSelection = 0;
    deviceType = savedDeviceType;
}

/* The tiles over the whole screen, in a checker of BACKGRND's two (InitMenuBackground). */
static void PcInitMenuBackground(void)
{
    int row, col;

    for (row = 0; row < TILES_DOWN; row++) {
        for (col = 0; col < TILES_ACROSS; col++) {
            POLY_FT4 *tile = &menuTiles[row * TILES_ACROSS + col];
            int x = col * 64, y = row * 60, v = (row + col) % 2 ? 60 : 0;

            SetPolyFT4(tile);
            tile->r0 = tile->g0 = tile->b0 = 0x80;
            tile->tpage = 5;
            tile->x0 = x, tile->y0 = y;
            tile->x1 = x + 64, tile->y1 = y;
            tile->x2 = x, tile->y2 = y + 60;
            tile->x3 = x + 64, tile->y3 = y + 60;
            tile->u0 = 0, tile->v0 = v;
            tile->u1 = 63, tile->v1 = v;
            tile->u2 = 0, tile->v2 = v + 59;
            tile->u3 = 63, tile->v3 = v + 59;
        }
    }
}

/* Each panel of the backdrop at its place, the slide's offset added (by band: the top, the bottom, the rest). */
static void PlaceBackPolys(void)
{
    int i;

    for (i = 0; i < 14; i++) {
        short offset = i < 3 ? slideX : i < 6 ? slideStep : slideY;
        short left = (menuBackdropRects[i].x + offset) * 2, right = left + menuBackdropRects[i].w * 2;
        short top = menuBackdropRects[i].y * 2, bottom = (menuBackdropRects[i].y + menuBackdropRects[i].h) * 2;

        menuBackPolys[i].x0 = left, menuBackPolys[i].y0 = top;
        menuBackPolys[i].x1 = right, menuBackPolys[i].y1 = top;
        menuBackPolys[i].x2 = left, menuBackPolys[i].y2 = bottom;
        menuBackPolys[i].x3 = right, menuBackPolys[i].y3 = bottom;
    }
}

/* The backdrop's fourteen panels: the top and bottom bands (TESTMENU's three pages across), the sides and the lines
   under and over the middle. */
static void PcInitMenuBackPolys(void)
{
    /* each panel: where (x, y, width, height in the console's 320x240), its page's texture code and its texels */
    static const struct {
        short x, y, w, h;
        u_char tpage, u0, v0, u1, v1;
    } panels[14] = {
        {20, 32, 128, 50, 6, 0, 0, 254, 98},     {148, 32, 128, 50, 7, 0, 0, 254, 98},
        {276, 32, 32, 50, 8, 0, 0, 64, 98},      {20, 180, 128, 36, 6, 0, 114, 254, 186},
        {148, 180, 128, 36, 7, 0, 114, 254, 186}, {276, 180, 32, 36, 8, 0, 114, 64, 186},
        {20, 82, 2, 98, 8, 88, 0, 91, 194},      {306, 82, 4, 98, 8, 98, 0, 102, 194},
        {20, 82, 128, 1, 6, 0, 100, 254, 101},   {148, 82, 128, 1, 7, 0, 100, 254, 101},
        {276, 82, 32, 1, 8, 0, 100, 64, 101},    {20, 179, 128, 1, 6, 0, 113, 254, 114},
        {148, 179, 128, 1, 7, 0, 113, 254, 114}, {276, 179, 32, 1, 8, 0, 113, 64, 114},
    };
    int i;

    for (i = 0; i < 14; i++) {
        POLY_FT4 *poly = &menuBackPolys[i];

        SetPolyFT4(poly);
        poly->r0 = poly->g0 = poly->b0 = 255;
        poly->tpage = panels[i].tpage;
        poly->u0 = poly->u2 = panels[i].u0;
        poly->u1 = poly->u3 = panels[i].u1;
        poly->v0 = poly->v1 = panels[i].v0;
        poly->v2 = poly->v3 = panels[i].v1;
        menuBackdropRects[i].x = panels[i].x;
        menuBackdropRects[i].y = panels[i].y;
        menuBackdropRects[i].w = panels[i].w;
        menuBackdropRects[i].h = panels[i].h;
    }
    PlaceBackPolys();
}

/* ---- the menus' moves ---- */

/* Port: the race type menu's option of a race type, and back. */
static int RaceSelection(int type)
{
    return type == RACE_CHALLENGE ? 2 : type == RACE_TIME_TRIAL ? 1 : 0;
}

/* The settings between the game data and the screen's own: as a menu is entered (0) or left (leaving: 1). */
static void PcCopyGameData(ConfigData *config, int leaving)
{
    switch (currMenu) {
    case PC_MENU_RACE_TYPE:
        if (!leaving) {
            menu[currMenu].selection = RaceSelection(raceType);
        } else {
            switch (menu[currMenu].selection) {
            case 0:
                raceType = RACE_ARCADE;
                break;
            case 1:
                raceType = RACE_TIME_TRIAL;
                break;
            case 2:
                raceType = RACE_CHALLENGE;
                config->trackNum = 0;
                config->actTrackNum = 1;
                if (config->rapierComplete) {
                    config->class2Race = 0;
                } else {
                    config->class1Race = 0;
                }
                break;
            }
        }
        break;
    case PC_MENU_CLASS:
        if (!leaving) {
            menuState.shipClass = config->shipClass;
            menuState.actTrackNum = config->actTrackNum;
            menuState.trackNum = config->trackNum;
            menu[PC_MENU_CLASS].selection = lastClassSelection;
            menuState.trackSelection = menu[PC_MENU_VECTOR + config->shipClass].selection;
        } else {
            config->shipClass = menuState.shipClass;
            config->actTrackNum = menuState.actTrackNum;
            config->trackNum = menuState.trackNum;
            lastClassSelection = menu[PC_MENU_CLASS].selection;
            menu[PC_MENU_VECTOR + menuState.shipClass].selection = menuState.trackSelection;
        }
        break;
    case PC_MENU_TEAM:
        if (!leaving) {
            menuState.ship = ownShip;
            menuState.current = menu[currMenu].selection;
        } else {
            ownShip = menuState.ship;
            menu[currMenu].selection = menuState.current;
        }
        break;
    case PC_MENU_VECTOR:
    case PC_MENU_VENOM:
    case PC_MENU_RAPIER:
    case PC_MENU_PHANTOM:
        if (!leaving) {
            menuState.shipClass = config->shipClass;
            menuState.actTrackNum = config->actTrackNum;
            menuState.current = menu[currMenu].selection;
        } else {
            config->shipClass = menuState.shipClass;
            config->actTrackNum = menuState.actTrackNum;
            menu[currMenu].selection = menuState.current;
        }
        break;
    case PC_MENU_AUDIO:
        if (!leaving) {
            menuState.musicVolume = config->musicVolume;
            menuState.otherMusic = config->otherMusicVolume;
            menuState.effectVolume = config->effectVolume;
            menuState.otherEffect = config->otherEffectVolume;
            menuState.music = config->cdTrack;
        } else {
            config->musicVolume = menuState.musicVolume;
            config->otherMusicVolume = menuState.otherMusic;
            config->effectVolume = menuState.effectVolume;
            config->otherEffectVolume = menuState.otherEffect;
            config->cdTrack = menuState.music;
        }
        menuState.current = 0;
        menu[currMenu].selection = 0;
        SetCdVolume(config->musicVolume * 255 / 100);
        SetSoundVolume(config->effectVolume * 255 / 100);
        /* Port: the shield's hum (the effects' sample) is the PlayStation's controlled sound */
        TerminateSound();
        StartControlledSound();
        SetVolume(ambientVoice1, 0);
        SfxEffectLRPan(ambientVoice1, 15);
        SfxEffectFBPan(ambientVoice1, 15);
        break;
    case PC_MENU_CONTROLLER:
        if (!leaving) {
            deviceType = savedDeviceType;
            menu[currMenu].selection = 0;
            menuState.current = 0;
        } else {
            savedDeviceType = deviceType;
        }
        break;
    case PC_MENU_PREFERENCES:
        if (!leaving) {
            menuState.weapons = config->weaponsOn;
            menuState.internalView = config->internalView;
            memset(&defaultName, 0, sizeof(defaultName));
            strncpy(defaultName.text, config->name, 3);
            defaultName.length = 3;
            menuState.current = 0;
            menu[currMenu].selection = 0;
        } else {
            config->weaponsOn = menuState.weapons;
            config->internalView = menuState.internalView;
            strcpy(config->name, defaultName.text);
            menu[currMenu].selection = 0;
            menuState.current = 0;
        }
        break;
    case PC_MENU_LOAD_SAVE:
        if (!leaving) {
            saveState = 0;
        }
        break;
    }
}

/* The screen slides from one menu to the next: the backdrop's bands part and close again, sparks flying from
   their edges, with a crash at the end. */
static void PcSlideScreen(void)
{
    int steps = frameSteps;

    while (steps != 0) {
        short left, right;
        int i;

        if (slideFrame >= 0 && slideFrame < 30) {
            slideX += 12;
            slideY -= 12;
            slideStep += 12;
        } else {
            slideX -= 12;
            slideY += 12;
            slideStep -= 12;
        }
        right = slideY + 308;
        left = slideStep + 20;
        if (slideFrame > 0 && slideFrame < 15 && right - left > 0) {
            for (i = 20; i != 0; i--) {
                Insert2DParticle(left, 82);
                Insert2DParticle(left, 180);
                Insert2DParticle(right, 82);
                Insert2DParticle(right, 180);
            }
        }
        slideFrame++;
        if (slideFrame == 60) {
            slideFrame = 0;
            sliding = 0;
            Sound(SOUND_WEAPON_HIT);
            steps = 1;
        }
        steps--;
    }
    PlaceBackPolys();
}

/* Into another menu: the screen slides. */
static void EnterMenu(int next)
{
    currMenu = next;
    slideX = 0;
    slideY = 0;
    slideStep = 0;
    slideFrame = 0;
    sliding = 1;
}

/* ---- the screens ---- */

/* Port: a name entered letter by letter (the PC's typed EnterName): Up and Down change the cursor's letter, Left
   and Right move the cursor, Accept or Back ends the entry (Accept then goes on to the menu). Returns whether it is
   still being entered. */
static int EditName(NameEntry *name)
{
    static const char letters[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789- ";
    char *at = &name->text[name->cursor];
    const char *letter;
    int count = (int)sizeof(letters) - 1, k;

    if (*at == 0) {
        *at = ' ';
    }
    letter = strchr(letters, *at);
    k = letter ? (int)(letter - letters) : count - 1;
    if (GetMenuKey(MENU_UP)) {
        *at = letters[(k + 1) % count];
        Sound(SOUND_MENU_MOVE);
    } else if (GetMenuKey(MENU_DOWN)) {
        *at = letters[(k + count - 1) % count];
        Sound(SOUND_MENU_MOVE);
    }
    if (GetMenuKey(MENU_LEFT) && name->cursor > 0) {
        Sound(SOUND_MENU_MOVE);
        name->cursor--;
    }
    if (GetMenuKey(MENU_RIGHT) && name->cursor < name->length - 1) {
        Sound(SOUND_MENU_MOVE);
        name->cursor++;
        if (name->text[name->cursor] == 0) {
            name->text[name->cursor] = ' ';
        }
    }
    if (GetMenuKey(MENU_ACCEPT) || GetMenuKey(MENU_BACK)) {
        name->editing = 0;
    }
    keysPressed &= ~KEY_BACK; /* (Back ends the name, not the screen; Accept goes on to the menu's, as the PC's Enter) */
    return name->editing;
}

/* The default name of the preferences (EnterName): the cursor's letter yellow over it while it is entered. */
static void DrawDefaultName(void)
{
    char before[13], letter[2];

    PcAddText(defaultName.text, 180, 130, 8, 0);
    if (defaultName.editing) {
        memcpy(before, defaultName.text, (size_t)defaultName.cursor);
        before[defaultName.cursor] = 0;
        letter[0] = defaultName.text[defaultName.cursor];
        letter[1] = 0;
        PcAddText(letter, 180 + PcTextWidth(before, 8), 130, 8, 1);
    }
}

/* The best times of a track, with the team of each pilot (raceMode 4 arcade, 5 time trial). */
static void PcDispHiScores(ConfigData *config, int raceMode)
{
    static const char *const teams[] = {"AG SYSTEMS", "AURICOM", "QIREX", "FEISAR", "PIRANHA", "WIPEOUT"};
    static const char *const classes[] = {"VECTOR", "VENOM", "RAPIER", "PHANTOM"};
    short maxTracks = 6, track, shownTrack, i;

    if (config->class1Unlocked || config->trackCheatActive || config->phantomTrackCheatActive) {
        maxTracks = 8;
    }
    if (GetMenuKey(MENU_RIGHT)) {
        menu[currMenu].selection++;
        if (maxTracks <= menu[currMenu].selection) {
            menu[currMenu].selection = maxTracks - 1;
        } else {
            Sound(SOUND_MENU_MOVE);
        }
    } else if (GetMenuKey(MENU_LEFT)) {
        if (--menu[currMenu].selection < 0) {
            menu[currMenu].selection = 0;
        } else {
            Sound(SOUND_MENU_MOVE);
        }
    }
    if (blinkFlag) {
        if (menu[currMenu].selection < maxTracks - 1) {
            PcDoButton(275, 184, PC_KEY_RIGHT, "");
        }
        if (menu[currMenu].selection > 0) {
            PcDoButton(30, 184, PC_KEY_LEFT, "");
        }
    }
    shownTrack = menu[currMenu].selection;
    PcPrintCX(PcGetTrackName(raceTracks[shownTrack]), 160, 186, 0);
    track = raceMode == 5 ? shownTrack + 8 : shownTrack;
    PcPrint("BEST", 30, 90, 0, 0);
    PcPrint("RACE", 30, 97, 0, 0);
    PcPrint("TIME", 30, 104, 0, 0);
    PcPrint("BEST", 30, 135, 0, 0);
    PcPrint("LAP", 30, 142, 0, 0);
    PcPrint("TIME", 30, 149, 0, 0);
    for (i = 0; i < 4; i++) {
        /* (the two slower classes do not race the last two tracks) */
        if (!(i < 2 && (track % 8 == 6 || track % 8 == 7))) {
            HiScore *score = &hiScoreTable[track][i];

            PcAddText(score->name, 75, 90 + i * 10, 8, 1);
            PcAddTime(score->time, 130, 90 + i * 10, 8, 1, 1);
            PcAddText(score->lapName, 75, 135 + i * 10, 8, 1);
            PcAddTime(score->lapTime, 130, 135 + i * 10, 8, 1, 1);
            if (score->ship / 3 >= 0 && score->ship / 3 < 6) {
                PcPrint(teams[score->ship / 3], 240, 92 + i * 10, 0, 0);
            }
            if (score->lapShip / 3 >= 0 && score->lapShip / 3 < 6) {
                PcPrint(teams[score->lapShip / 3], 240, 137 + i * 10, 0, 0);
            }
        }
        PcPrint(classes[i], 190, 92 + i * 10, 0, 0);
        PcPrint(classes[i], 190, 137 + i * 10, 0, 0);
    }
    DrawMenuModel(0, 0, 1500, (short)heading, 220, shipModels[trackModelOrder[shownTrack]]);
}

/* The preferences: weapons on or off, the default view and the default name. */
static void PcDisplayConfig(ConfigData *config)
{
    int wasEditing = defaultName.editing;

    if (wasEditing) {
        EditName(&defaultName);
    } else {
        if (GetMenuKey(MENU_UP)) {
            menuState.current--;
            if (menuState.current < 0) {
                menuState.current = 0;
            } else {
                Sound(SOUND_MENU_MOVE);
            }
        }
        if (GetMenuKey(MENU_DOWN)) {
            menuState.current++;
            if (menuState.current > 2) {
                menuState.current = 2;
            } else {
                Sound(SOUND_MENU_MOVE);
            }
        }
    }
    PcAddText("ON", 180, 90, 8, menuState.weapons ? 1 : 0);
    PcAddText("OFF", 210, 90, 8, menuState.weapons ? 0 : 1);
    PcAddText("INT", 181, 110, 8, menuState.internalView ? 1 : 0);
    PcAddText("EXT", 210, 110, 8, menuState.internalView ? 0 : 1);
    switch (menuState.current) {
    case 0:
        if (blinkFlag) {
            PcAddText("WEAPONS", 30, 90, 8, 1);
        }
        PcAddText("DEFAULT VIEW", 30, 110, 8, 0);
        PcAddText("DEFAULT NAME", 30, 130, 8, 0);
        if (keysPressed & KEY_RIGHT) {
            if (menuState.weapons) {
                Sound(SOUND_MENU_MOVE);
            }
            menuState.weapons = 0;
        }
        if (keysPressed & KEY_LEFT) {
            if (!menuState.weapons) {
                Sound(SOUND_MENU_MOVE);
            }
            menuState.weapons = 1;
        }
        break;
    case 1:
        PcAddText("WEAPONS", 30, 90, 8, 0);
        if (blinkFlag) {
            PcAddText("DEFAULT VIEW", 30, 110, 8, 1);
        }
        PcAddText("DEFAULT NAME", 30, 130, 8, 0);
        if ((keysPressed & KEY_RIGHT) && menuState.internalView == 1) {
            Sound(SOUND_MENU_MOVE);
            menuState.internalView = 0;
        }
        if ((keysPressed & KEY_LEFT) && menuState.internalView == 0) {
            Sound(SOUND_MENU_MOVE);
            menuState.internalView = 1;
        }
        break;
    case 2:
        PcAddText("WEAPONS", 30, 90, 8, 0);
        PcAddText("DEFAULT VIEW", 30, 110, 8, 0);
        if (blinkFlag) {
            PcAddText("DEFAULT NAME", 30, 130, 8, 1);
        }
        PcPrintCX("NAME IS USED AS DEFAULT HIGH SCORE ENTRY", 160, 186, 0);
        /* Port: Accept here enters the name (the PC typed it on this row); Accept again accepts the screen */
        if (!wasEditing && GetMenuKey(MENU_ACCEPT)) {
            Sound(SOUND_MENU_SELECT);
            defaultName.editing = 1;
            defaultName.cursor = 0;
            keysPressed &= ~KEY_ACCEPT;
        }
        break;
    }
    DrawDefaultName();
}

/* The controller: the keyboard alone or with a joystick or a mouse, its model turning. Port: the choice is shown
   but changes nothing (the port reads the keyboard and every gamepad at once), and the PC's second row, its
   redefinition of the keys and buttons, is left out: the port's controls are its own settings'. */
static void PcSelectController(void)
{
    static const short models[3] = {1, 2, 0}; /* (CNTRL.PRM's keyboard, joystick and mouse) */
    char name[24];

    strcpy(name, "KEYBOARD");
    if (deviceType == 1) {
        strcat(name, " AND JOYSTICK");
    } else if (deviceType == 2) {
        strcat(name, " AND MOUSE");
    }
    if (GetMenuKey(MENU_RIGHT)) {
        deviceType++;
        if (deviceType > 2) {
            deviceType = 2;
        } else {
            Sound(SOUND_MENU_MOVE);
        }
    } else if (GetMenuKey(MENU_LEFT)) {
        deviceType--;
        if (deviceType < 0) {
            deviceType = 0;
        } else {
            Sound(SOUND_MENU_MOVE);
        }
    }
    if (blinkFlag) {
        if (deviceType < 2) {
            PcDoButton(275, 110, PC_KEY_RIGHT, "");
        }
        if (deviceType > 0) {
            PcDoButton(30, 110, PC_KEY_LEFT, "");
        }
    }
    PcCentreText(name, 0, 110, 8, blinkFlag ? 0 : 1);
    if (controllerModels[models[deviceType]]) {
        DrawMenuModel(0, 0, 6000, (short)heading, deviceType == 2 ? 220 : 0, controllerModels[models[deviceType]]);
    }
}

/* The audio screen: the music's and the effects' volume, and the CD track. */
static void PcAudioConfig(ConfigData *config)
{
    POLY_FT4 bar;
    signed char savedTrack;
    int musicX, sfxX, shown;

    savedTrack = config->cdTrack;
    config->cdTrack = menuState.music;
    SetInGameCDTrack(cdPlaySeconds, cdStatus, config, &cdState);
    config->cdTrack = savedTrack;
    if (GetMenuKey(MENU_UP)) {
        menuState.current--;
        if (menuState.current < 0) {
            menuState.current = 0;
        } else {
            Sound(SOUND_MENU_MOVE);
        }
    }
    if (GetMenuKey(MENU_DOWN)) {
        menuState.current++;
        if (menuState.current >= 3) {
            menuState.current = 2;
        } else {
            Sound(SOUND_MENU_MOVE);
        }
    }
    if (menuState.current != 1) {
        SetVolume(ambientVoice1, 0);
        SfxEffectLRPan(ambientVoice1, 15);
        SfxEffectFBPan(ambientVoice1, 15);
    }
    switch (menuState.current) {
    case 0:
        if (blinkFlag) {
            PcAddText("MUSIC", 30, 108, 8, 1);
        }
        PcAddText("SFX", 30, 147, 8, 0);
        PcAddText("CD TRACK", 25, 184, 8, 0);
        if (keysDown & KEY_RIGHT) {
            menuState.musicVolume++;
            if (menuState.musicVolume >= 100) {
                menuState.musicVolume = 100;
            }
        }
        if (keysDown & KEY_LEFT) {
            menuState.musicVolume--;
            if (menuState.musicVolume < 0) {
                menuState.musicVolume = 0;
            }
        }
        menuState.otherMusic = menuState.musicVolume;
        SetCdVolume(menuState.otherMusic * 255 / 100);
        break;
    case 1:
        PcAddText("MUSIC", 30, 108, 8, 0);
        if (blinkFlag) {
            PcAddText("SFX", 30, 147, 8, 1);
        }
        PcAddText("CD TRACK", 25, 184, 8, 0);
        if (keysDown & KEY_RIGHT) {
            menuState.effectVolume++;
            if (menuState.effectVolume >= 100) {
                menuState.effectVolume = 100;
            }
        }
        if (keysDown & KEY_LEFT) {
            menuState.effectVolume--;
            if (menuState.effectVolume < 0) {
                menuState.effectVolume = 0;
            }
        }
        menuState.otherEffect = menuState.effectVolume;
        SetSoundVolume(menuState.otherEffect * 255 / 100);
        SetVolume(ambientVoice1, 255);
        SfxEffectLRPan(ambientVoice1, 15);
        SfxEffectFBPan(ambientVoice1, 15);
        break;
    case 2:
        PcAddText("MUSIC", 30, 108, 8, 0);
        PcAddText("SFX", 30, 147, 8, 0);
        if (blinkFlag) {
            PcAddText("CD TRACK", 25, 184, 8, 1);
        }
        if (GetMenuKey(MENU_LEFT) || GetMenuKey(MENU_RIGHT)) {
            Sound(SOUND_MENU_MOVE);
            menuState.music += GetMenuKey(MENU_LEFT) ? -1 : 1;
            if (menuState.music < 1) {
                menuState.music = cdMusicCount;
            } else if (menuState.music > cdMusicCount) {
                menuState.music = 1;
            }
            if (menuState.music != 1) {
                CdMusicPlay(menuState.music, 0);
            } else {
                InitShufflePlay();
                cdMusicIndex = 0;
                cdMusicTrack = cdMusicTable[0];
                CdMusicPlay(cdMusicTrack, 0);
            }
        }
        break;
    }
    shown = menuState.music == 0 ? 0 : menuState.music - 1;
    if (shown >= 0 && shown < 13) {
        PcPrint(pcCdTrackNames[shown], 210, 190, 0, 0);
        PcAddNumber(shown, 135, 184, 8, 0);
        PcPrint(pcCdTrackArtists[shown], 210, 182, 0, 0);
    }
    PcPrint("ARTIST", 170, 182, 0, 0);
    PcPrint("TITLE", 170, 190, 0, 0);
    musicX = menuState.musicVolume * 6430 >> 12;
    sfxX = menuState.effectVolume * 6430 >> 12;
    PcSetTextQuad(&bar, musicX + 105, 103, 116, 178, 32, 36, 35, 0);
    DrawScaledTexturedQuad(pcTextDepth, &bar, 0);
    PcSetTextQuad(&bar, sfxX + 105, 142, 116, 178, 32, 36, 35, 0);
    DrawScaledTexturedQuad(pcTextDepth, &bar, 0);
    PcSetTextQuad(&bar, 100, 100, 0, 128, 254, 48, 35, 0);
    DrawScaledTexturedQuad(pcTextDepth + 0.1f, &bar, 0);
    PcSetTextQuad(&bar, 227, 100, 0, 178, 114, 48, 35, 0);
    DrawScaledTexturedQuad(pcTextDepth + 0.1f, &bar, 0);
    PcSetTextQuad(&bar, 100, 139, 0, 128, 254, 48, 35, 0);
    DrawScaledTexturedQuad(pcTextDepth + 0.1f, &bar, 0);
    PcSetTextQuad(&bar, 227, 139, 0, 178, 114, 48, 35, 0);
    DrawScaledTexturedQuad(pcTextDepth + 0.1f, &bar, 0);
}

/* The five performance bars of the team of the team screen's ship at (x, y). */
static void PcShipParameters(short x, short y)
{
    static const char *const names[5] = {"THRUST", "TOP SPEED", "TURNING ABILITY", "SHIELD ENERGY", "AERODYNAMICS"};
    POLY_FT4 bar;
    int i;

    for (i = 0; i < 5; i++) {
        PcPrint(names[i], x, y + i * 7, 0, 0);
    }
    for (i = 0; i < 5; i++) {
        short width = pcTeamRatings[menuState.ship / 3][i] * 7, barY = y + i * 7;

        /* the bar to the rating, the rest of its 198 dark (the width's byte as the PC passes it) */
        PcSetTextQuad(&bar, x + 0x4D, barY, 0, 100, (u_char)(width * 2), 10, 32, 0);
        DrawScaledTexturedQuad(pcTextDepth, &bar, 0);
        PcSetTextQuad(&bar, x + 0x4D + width, barY, 0, 112, (u_char)((198 - width) * 2), 10, 32, 0);
        DrawScaledTexturedQuad(pcTextDepth, &bar, 0);
    }
}

/* The team screen: the teams, the ship of the chosen one turning, its description and its performance. */
static void PcTeamSelect(ConfigData *config)
{
    static const short teamOrder[5] = {3, 0, 1, 2, 4};
    static const char *const teams[5] = {"FEISAR", "AG SYSTEMS", "AURICOM", "QIREX", "PIRANHA"};
    static const char *const descriptions[5][2] = {
        {"EXCELLENT TURNING ABILITY AND EASY TO CONTROL", "BUT NOT TOO QUICK.  [BEGINNER]"},
        {"GOOD ACCELERATION AND TURNING ABILITY BUT", "WEAK SHIELD.  [AMATEUR]"},
        {"GOOD ALL ROUNDER BUT AVERAGE PERFORMANCE.", "[INTERMEDIATE]"},
        {"FAST WITH GOOD SHIELD CAPABILITY BUT SLIGHTLY", "HEAVY AT TURNING.  [EXPERT]"},
        {"VERY FAST WITH EXCELLENT SHIELD BUT NO", "WEAPON TECHNOLOGY.  [MASTER]"},
    };
    signed char teamCount = 4;
    int i;

    if (config->class2Unlocked) {
        teamCount = 5;
    }
    if (config->teamCheatActive) {
        teamCount = 5;
        PcPrintCX("TEAM CHEAT ACTIVE", 160, 204, 0);
    }
    if (GetMenuKey(MENU_DOWN)) {
        menuState.current++;
        if (menuState.current >= teamCount) {
            menuState.current = teamCount - 1;
        } else {
            Sound(SOUND_MENU_MOVE);
        }
    }
    if (GetMenuKey(MENU_UP)) {
        menuState.current--;
        if (menuState.current < 0) {
            menuState.current = 0;
        } else {
            Sound(SOUND_MENU_MOVE);
        }
    }
    menuState.ship = 3 * teamOrder[menuState.current];
    DrawMenuModel(450, sillyShips == 1 ? 0 : -50, 1200, (short)heading, 0, shipShapeTable[highResTable[menuState.ship]]);
    for (i = 0; i < 4; i++) {
        PcAddText(teams[i], 30, 90 + i * 10, 8, menuState.current == i);
    }
    if (menuState.current == 4) {
        PcAddText(teams[4], 30, 130, 8, 1);
    } else if (config->class2Unlocked || config->teamCheatActive) {
        PcAddText(teams[4], 30, 130, 8, 0);
    }
    PcPrint(descriptions[menuState.current][0], 30, 182, 0, 0);
    PcPrint(descriptions[menuState.current][1], 30, 189, 0, 0);
    PcShipParameters(155, 142);
}

/* The class screen: the tracks of the chosen class, the turning model of the track, its medal. */
static void PcClassSelect(ConfigData *config)
{
    static const u_char classOrder[4] = {1, 0, 3, 5};
    static const char *const classNames[4] = {"NOVICE", "INTERMEDIATE", "EXPERT", "SPEED DEMON"};
    static const char *const descriptions[8][2] = {
        {"A MAJOR CANADIAN INDUSTRIAL", "COMPLEX. [EASY]"},
        {"A SNOWY TIBETAN MOUNTAIN CIRCUIT.", "[EASY]"},
        {"A SOUTH AMERICAN JUNGLE CIRCUIT.", "[TRICKY]"},
        {"A NEWLY CONSTRUCTED GERMAN", "COMMERCIAL PARK.  [TRICKY]"},
        {"A DISUSED FRENCH METRO SYSTEM.", "[HARD]"},
        {"A HUGE SUSPENDED CIRCUIT OVER THE", "BLACK SEA. [HARD]"},
        {"AN OBSCURE VOLCANIC ISLAND LOCATED", "IN THE SOUTH PACIFIC.  [VERY HARD]"},
        {"AN AMERICAN CITY CIRCUIT AFFECTED", "BY SEISMIC ACTIVITY.  [VERY HARD]"},
    };
    static const char *const medals[4] = {"", "GOLD", "SILVER", "BRONZE"};
    int classSelection = menu[PC_MENU_CLASS].selection;
    int allTracks = config->class1Unlocked || config->rapierComplete || config->phantomTrackCheatActive ||
                    config->trackCheatActive;
    signed char track, trackCount = 2, medal;

    if (config->class1Unlocked || config->phantomTrackCheatActive) {
        trackCount = 6;
        if (menu[currMenu].selection == 3) {
            trackCount = 2;
        }
    }
    if (config->phantomTrackCheatActive) {
        PcPrintCX("PHANTOM TRACK CHEAT ACTIVE", 210, 75, 0);
    }
    if (config->rapierComplete || config->trackCheatActive) {
        trackCount = 8;
        if (menu[currMenu].selection == 0 || menu[currMenu].selection == 1) {
            trackCount = 6;
        }
        if (config->trackCheatActive) {
            PcPrintCX("TRACK CHEAT ACTIVE", 160, 204, 0);
        }
    }
    if (GetMenuKey(MENU_RIGHT)) {
        menuState.trackSelection++;
        if (trackCount > menuState.trackSelection) {
            Sound(SOUND_MENU_MOVE);
        }
    }
    if (menuState.trackSelection >= trackCount - 1) {
        menuState.trackSelection = trackCount - 1;
    }
    if (GetMenuKey(MENU_LEFT)) {
        menuState.trackSelection--;
        if (menuState.trackSelection < 0) {
            menuState.trackSelection = 0;
        } else {
            Sound(SOUND_MENU_MOVE);
        }
    }
    if (allTracks) {
        menuState.trackNum = menuState.trackSelection;
        menuState.actTrackNum = raceTracks[menuState.trackSelection];
    }
    /* the class's own two tracks while the others are closed (the first class's are the first two either way) */
    track = menuState.trackSelection + 2 * classSelection;
    if (classSelection == 0 ||
        (classSelection == 3 ? !config->rapierComplete && !config->trackCheatActive : !allTracks)) {
        menuState.trackNum = track;
        menuState.actTrackNum = raceTracks[track];
    }
    menuState.shipClass = classSelection;
    PcPrintCX(classNames[classSelection], 68, 186, 0);
    PcPrint(descriptions[menuState.trackNum][0], 117, 182, 0, 0);
    PcPrint(descriptions[menuState.trackNum][1], 117, 189, 0, 0);
    if (blinkFlag) {
        if (menuState.trackSelection < trackCount - 1) {
            PcDoButton(280, 163, PC_KEY_RIGHT, "");
        }
        if (menuState.trackSelection > 0) {
            PcDoButton(125, 163, PC_KEY_LEFT, "");
        }
    }
    PortTwoSided(1); /* Port: the track model's surfaces shown from behind too (as the PlayStation's menus) */
    DrawMenuModel(400, 100, 1800, (short)heading, 220, shipModels[trackModelOrder[menuState.trackNum]]);
    PortTwoSided(0);
    PcPrintCX(PcGetTrackName(menuState.actTrackNum), 160, 90, 0);
    medal = trackMedals[menuState.trackNum];
    if (medal >= 1 && medal <= 3) {
        static const u_char colours[4][3] = {{0, 0, 0}, {0xFF, 0x80, 0}, {0xE0, 0xE0, 0xE0}, {0xC0, 0x20, 0}};

        if (blinkFlag) {
            PcPrintCX(medals[medal], 260, 90, 0);
            PcPrintCX("AWARDED", 260, 97, 0);
        }
        envmapr = colours[medal][0];
        envmapg = colours[medal][1];
        envmapb = colours[medal][2];
    }
    DrawMenuModel(-700, 300, 1400, (short)heading, medal ? -1 : 0, menuModels[classOrder[menu[currMenu].selection]]);
}

/* Port: the saves of Wipeout XL on the first memory card, the first eight as the PC's slots. */
static void ReadSaveSlots(void)
{
    int i, slot = 0;

    memset(saveSlots, 0, sizeof(saveSlots));
    if (GetCardInfo(0) != 0) {
        return;
    }
    ClearEventHW();
    _card_clear(0);
    CheckHWCardEvents();
    LoadCard(0);
    if (CardContentStatus() != 0) {
        return;
    }
    LoadCardFiles(cardSlots, 0);
    for (i = 0; i < 15 && slot < SAVE_SLOTS; i++) {
        if (cardSlots[0].entry[i].type == 4) {
            strncpy(saveSlots[slot].name, cardSlots[0].entry[i].name, SAVE_NAME_LENGTH);
            saveSlots[slot].occupied = 1;
            slot++;
        }
    }
}

/* Port: the save in a slot made or written again under the name entered (renamed when it had another). */
static int WriteSaveSlot(ConfigData *config, SaveSlot *slot, char *name)
{
    int ok;

    if (!slot->occupied) {
        ok = SaveCardFile(name, 0, config);
    } else if (strcmp(slot->name, name) != 0) {
        ok = RenameFile1(name, slot->name, 0, config);
    } else {
        ok = RewriteCardFile(name, 0, config);
    }
    if (ok) {
        strcpy(slot->name, name);
        slot->occupied = 1;
        PortSaveChosen(0, name, config);
    }
    return ok;
}

/* The load and save screen: load or save, the slot, the save's name; then a word on how it went. */
static void PcLoadSave(ConfigData *config)
{
    char label[32], prompt[48];
    int flash = 1, x = 0, i;

    switch (saveState) {
    case 0:
        flash = blinkFlag;
        if (GetMenuKey(MENU_LEFT)) {
            if (menuState.current > 0) {
                menuState.current = 0;
                Sound(SOUND_MENU_MOVE);
            }
        } else if (GetMenuKey(MENU_RIGHT)) {
            if (menuState.current < 1) {
                menuState.current = 1;
                Sound(SOUND_MENU_MOVE);
            }
        }
        if (GetMenuKey(MENU_ACCEPT)) {
            saveState = 1;
            saveCursor = 0;
            Sound(SOUND_MENU_SELECT);
            ReadSaveSlots();
        }
        if (GetMenuKey(MENU_BACK)) {
            screenCancelled = 1;
        }
        break;
    case 1:
        if (GetMenuKey(MENU_UP) && saveCursor > 0) {
            saveCursor--;
            Sound(SOUND_MENU_MOVE);
        }
        if (GetMenuKey(MENU_DOWN) && saveCursor < SAVE_SLOTS - 1) {
            saveCursor++;
            Sound(SOUND_MENU_MOVE);
        }
        if (GetMenuKey(MENU_ACCEPT)) {
            if (menuState.current == 0) {
                if (saveSlots[saveCursor].occupied) {
                    saveState = 3;
                    Sound(SOUND_MENU_SELECT);
                }
            } else {
                saveState = 2;
                memset(&saveName, 0, sizeof(saveName));
                strcpy(saveName.text, saveSlots[saveCursor].name);
                saveName.length = SAVE_NAME_LENGTH;
                saveName.editing = 1;
                Sound(SOUND_MENU_SELECT);
            }
        }
        if (GetMenuKey(MENU_BACK)) {
            Sound(SOUND_MENU_SELECT);
            saveState = 0;
        }
        break;
    case 2:
        /* Port: the name is entered letter by letter; Accept saves under it, Back goes back to the slots */
        if (GetMenuKey(MENU_BACK)) {
            Sound(SOUND_MENU_SELECT);
            saveState = 1;
        } else if (GetMenuKey(MENU_ACCEPT)) {
            char *end;

            end = strchr(saveName.text, ' ');
            if (end) {
                *end = 0; /* (the name ends at its first blank) */
            }
            if (saveName.text[0]) {
                Sound(SOUND_MENU_SELECT);
                PcPrintCX("SAVING GAME", 270, 182, 0);
                saveState = 3;
            } else {
                saveName.cursor = 0;
            }
        } else {
            EditName(&saveName);
            saveName.editing = 1;
        }
        break;
    case 3:
        if (menuState.current == 0) {
            /* the progress of the save loaded in place of the game's (the unlocks taken away first) */
            if (raceType == RACE_CHALLENGE && (config->venomComplete || config->rapierComplete)) {
                raceType = RACE_ARCADE;
            }
            menu[PC_MENU_RACE_TYPE].numOptions = 2;
            menu[PC_MENU_RACE_TYPE].selection = 0;
            menu[PC_MENU_CLASS].options[3].enabled = 0;
            menu[PC_MENU_CLASS].selection = 0;
            menu[PC_MENU_CLASS].mode = 0;
            config->phantomTrackCheatActive = 0;
            config->trackCheatActive = 0;
            config->teamCheatActive = 0;
            config->venomComplete = 0;
            config->rapierComplete = 0;
            config->class1Unlocked = 0;
            config->class2Unlocked = 0;
            saveOk = LoadCardData(saveSlots[saveCursor].name, 0, config);
            if (saveOk) {
                PortSaveChosen(0, saveSlots[saveCursor].name, config);
            }
            PcSetupUnlocks(config);
        } else {
            saveOk = WriteSaveSlot(config, &saveSlots[saveCursor], saveName.text);
        }
        saveState = 4;
        saveMessageFrames = 0;
        break;
    case 4:
        if (saveMessageFrames < 180) {
            saveMessageFrames += frameSteps;
            if (menuState.current == 0) {
                PcPrintCX(saveOk ? "LOADED GAME OK" : "COULD NOT LOAD GAME", 70, 182, 0);
            } else {
                PcPrintCX(saveOk ? "SAVED GAME OK" : "COULD NOT SAVE GAME", 260, 182, 0);
            }
        } else {
            screenCancelled = 1;
        }
        break;
    }
    if (menuState.current != 0) {
        x = 142;
    }
    PcAddText("LOAD GAME", 38, 90, 8, menuState.current == 0 && !flash);
    PcAddText("SAVE GAME", 182, 90, 8, menuState.current != 0 && !flash);
    if (blinkFlag && saveState == 2) {
        PcPrint("-", x + saveName.cursor * 6 + 80, saveCursor * 8 + 114, 1, 0);
    }
    for (i = 0; i < SAVE_SLOTS; i++) {
        if ((saveState == 1 && (i != saveCursor || blinkFlag)) || saveState == 2) {
            sprintf(label, "GAME %d", i + 1);
            PcPrint(label, x + 36, 110 + i * 8, 0, 0);
            PcPrint(saveState == 2 && i == saveCursor ? saveName.text : saveSlots[i].name, x + 80, 110 + i * 8, 0, 1);
        }
    }
    /* Port: the prompts name the device's Accept (the PC's Enter) and keep to its keys' directions */
    if (saveState == 1) {
        sprintf(prompt, "USE UP/DOWN AND %s", PortButtonLabel(PORT_BUTTON_ACCEPT, "X"));
        PcPrint(prompt, 30, 182, 0, 0);
        PcPrint(menuState.current == 0 ? "TO SELECT THE GAME SLOT TO LOAD." : "TO SELECT THE SAVE GAME SLOT.", 30, 189,
                0, 0);
    } else if (saveState == 2) {
        PcPrint("ENTER THE NAME FOR THIS SAVE GAME.", 30, 182, 0, 0);
        PcPrint("UP/DOWN CHANGE A LETTER, LEFT/RIGHT MOVE.", 30, 189, 0, 0);
    }
}

/* The screen of the menu shown, after its options (RunMenuScreen). */
static void PcRunMenuScreen(ConfigData *config)
{
    switch (currMenu) {
    case PC_MENU_TEAM: PcTeamSelect(config); break;
    case PC_MENU_VECTOR:
    case PC_MENU_VENOM:
    case PC_MENU_RAPIER:
    case PC_MENU_PHANTOM: PcClassSelect(config); break;
    case PC_MENU_AUDIO: PcAudioConfig(config); break;
    case PC_MENU_CONTROLLER: PcSelectController(); break;
    case PC_MENU_PREFERENCES: PcDisplayConfig(config); break;
    case PC_MENU_LOAD_SAVE: PcLoadSave(config); break;
    case PC_MENU_ARCADE_TIMES: PcDispHiScores(config, 4); break;
    case PC_MENU_TIME_TRIAL_TIMES: PcDispHiScores(config, 5); break;
    }
}

/* The captions: the main menu's team, race type and track under its panels, every screen's mode over them. */
static void PcAddTextBits(ConfigData *config)
{
    static const char *const modes[PC_MENU_COUNT] = {
        "ESSENTIAL OPTIONS", "STRUCTURE MODE", "RACING MODE", "OPTIONS MODE", "TEAM MODE", "TRACK MODE",
        "TRACK MODE", "TRACK MODE", "TRACK MODE", "PRECISION LEVELS", "INTERACTION MODE", "PREFERENCES MODE",
        "DATA MODE", "ARCADE MODE", "TIME TRIAL MODE",
    };
    static const char *const teams[5] = {"AG SYSTEMS", "AURICOM", "QIREX", "FEISAR", "PIRANHA"};

    if (currMenu == PC_MENU_MAIN) {
        const char *type = "ARCADE";

        if (ownShip >= 0 && ownShip <= 14) {
            PcPrintCX(teams[ownShip / 3], 160, 172, 0);
        }
        if (raceType == RACE_TIME_TRIAL) {
            type = "TIME TRIAL";
        } else if (raceType == RACE_CHALLENGE) {
            type = config->venomComplete ? "CHALLENGE I" : "CHALLENGE II";
        }
        PcPrintCX(type, 63, 172, 0);
        PcPrintCX(PcGetTrackName(config->actTrackNum), 256, 172, 0);
    }
    PcPrint(modes[currMenu], 30, 75, 0, 0);
}

/* The lines round the menu's boxes (the layout's, but those through the screen's middle: unused ones). */
static void DrawMenuBoxes(void)
{
    MenuLayout *layout = &menuLayout[currMenu];
    int i;

    for (i = 0; i < layout->numBoxes && i < 20; i++) {
        const MenuBox *box = &layout->box[i];
        /* the top, right, bottom and left lines, a pixel wide */
        const short edges[4][4] = {
            {box->x0, box->y0, box->x1, box->y0 + 1},
            {box->x1, box->y0, box->x1 + 1, box->y1 + 1},
            {box->x0, box->y1, box->x1 + 1, box->y1 + 1},
            {box->x0, box->y0, box->x0 + 1, box->y1},
        };
        int e;

        if (box->x0 == 160 || box->y0 == 120 || box->x1 == 160 || box->y1 == 120) {
            continue;
        }
        for (e = 0; e < 4; e++) {
            boxEdge.x0 = boxEdge.x2 = edges[e][0];
            boxEdge.x1 = boxEdge.x3 = edges[e][2];
            boxEdge.y0 = boxEdge.y1 = edges[e][1];
            boxEdge.y2 = boxEdge.y3 = edges[e][3];
            DrawPolyF4(0.13f, &boxEdge);
        }
    }
}

/* The menu shown: its options, its prompts, its boxes, its models and its title; the moves between options. */
static void PcShowMenu(ConfigData *config)
{
    static const u_char classOrder[4] = {1, 0, 3, 5};
    static const u_char raceOrder[3] = {9, 2, 8};
    MenuEntry *shown = &menu[currMenu];
    short i, j;

    for (i = 0; i < shown->numOptions; i++) {
        if (currMenu == PC_MENU_MAIN) {
            if (i == 3 || i == 4 || i == shown->selection) {
                const MenuBox *box = &menuLayout[currMenu].box[i];

                PcDrawMenuOptionBackdrop(box->x0 + 1, box->y0 + 1, box->x1 - box->x0, box->y1 - box->y0, i,
                                       shown->selection);
            }
        }
        for (j = 0; j < shown->options[i].numText; j++) {
            const char *text = shown->options[i].text[j].text;
            int colour = i == shown->selection;

            if (currMenu == PC_MENU_MAIN) {
                PcCentreText2(text, shown->options[i].x, shown->options[i].y + j * 10, 8, 0);
            } else if (shown->options[i].enabled) {
                if (currMenu == PC_MENU_CLASS) {
                    PcCentreText2(text, 68, shown->options[0].y + i * 10, 8, colour);
                } else if (currMenu == PC_MENU_OPTIONS) {
                    PcCentreText(text, 68, shown->options[0].y + i * 14, 8, colour);
                } else {
                    PcAddText(text, shown->options[0].x, shown->options[0].y + i * 10, 8, colour);
                }
            }
        }
    }
    if (currMenu == PC_MENU_AUDIO || currMenu == PC_MENU_CONTROLLER || currMenu == PC_MENU_PREFERENCES) {
        PcDoButton(30, 198, PC_KEY_ENTER, "ACCEPT");
    } else if (currMenu != PC_MENU_ARCADE_TIMES && currMenu != PC_MENU_TIME_TRIAL_TIMES) {
        PcDoButton(30, 198, PC_KEY_ENTER, "SELECT");
    }
    PcDoButton(278, 198, PC_KEY_ESCAPE, currMenu != PC_MENU_MAIN ? "BACK" : "QUIT");
    if (currMenu == PC_MENU_CLASS) {
        PcClassSelect(config);
    }
    if (currMenu == PC_MENU_RACE_TYPE) {
        switch (shown->selection) {
        case 0:
            PcPrint("SINGLE RACE AGAINST A FULL", 30, 182, 0, 0);
            PcPrint("GRID OF COMPETITORS.", 30, 189, 0, 0);
            break;
        case 1:
            PcPrint("SINGLE RACE TIME TRIAL.", 30, 182, 0, 0);
            PcPrint("NO COMPETITORS.", 30, 189, 0, 0);
            break;
        case 2:
            if (config->venomComplete) {
                PcPrint("PHANTOM CHALLENGE. SIX RACES", 30, 182, 0, 0);
                PcPrint("AT RAPIER CLASS. WEAPONS ARE ACTIVATED.", 30, 189, 0, 0);
            } else if (config->rapierComplete) {
                PcPrint("PIRANHA CHALLENGE. EIGHT RACES", 30, 182, 0, 0);
                PcPrint("AT PHANTOM CLASS. WEAPONS ARE ACTIVATED.", 30, 189, 0, 0);
            }
            break;
        }
    }
    DrawMenuBoxes();
    if (currMenu == PC_MENU_OPTIONS) {
        DrawMenuModel(0, 80, 150, (short)heading, 0, menuModels[7]);
    }
    if (currMenu == PC_MENU_MAIN) {
        DrawMenuModel(0, 140, 1800, (short)cardNum, 173, shipShapeTable[highResTable[ownShip]]);
        switch (raceType) {
        case RACE_TIME_TRIAL:
            DrawMenuModel(-600, 110, 1000, (short)cardNum, 0, menuModels[raceOrder[1]]);
            break;
        case RACE_CHALLENGE:
            if (config->venomComplete) {
                DrawMenuModel(-600, 110, 1000, (short)cardNum, 0, menuModels[6]);
            } else {
                DrawMenuModel(-655, 110, 1200, (short)cardNum, 0, menuModels[4]);
            }
            break;
        default:
            DrawMenuModel(-540, 110, 800, (short)cardNum, 0, menuModels[raceOrder[0]]);
        }
        DrawMenuModel(620, 150, 1000, (short)cardNum, 0, menuModels[classOrder[config->shipClass & 3]]);
    } else if (currMenu == PC_MENU_RACE_TYPE) {
        if (shown->selection == 2) {
            DrawMenuModel(440, 100, 800, (short)cardNum, 0, menuModels[config->venomComplete ? 6 : 4]);
        } else {
            DrawMenuModel(440, 100, 800, (short)cardNum, 0, menuModels[raceOrder[shown->selection]]);
        }
    }
    if (currMenu == PC_MENU_MAIN) {
        DrawTitle();
    } else {
        /* (the title's size and colour fields hold its place) */
        PcAddText(shown->title.text, shown->title.size, shown->title.colour, 8, 0);
    }
    PcAddTextBits(config);

    if (shown->numOptions <= 0) {
        return;
    }
    if (currMenu == PC_MENU_MAIN) {
        /* the top row's three options left and right, START and OPTIONS below them */
        if (GetMenuKey(MENU_RIGHT)) {
            shown->selection++;
            if (!shown->options[shown->selection].enabled) {
                shown->selection++;
            }
            if (shown->selection >= shown->numOptions) {
                shown->selection = shown->numOptions - 1;
            } else {
                Sound(SOUND_MENU_MOVE);
            }
        } else if (GetMenuKey(MENU_LEFT)) {
            shown->selection--;
            if (shown->selection < 0) {
                shown->selection = 0;
                if (!shown->options[0].enabled) {
                    shown->selection = -1;
                }
            } else {
                Sound(SOUND_MENU_MOVE);
            }
        } else if (GetMenuKey(MENU_UP)) {
            if (shown->selection >= shown->numOptions - 2) {
                shown->selection--;
                if (shown->selection == 2) {
                    shown->selection = lastTopOption;
                }
                Sound(SOUND_MENU_MOVE);
            }
        } else if (GetMenuKey(MENU_DOWN)) {
            if (shown->selection >= 0 && shown->selection <= 2) {
                lastTopOption = shown->selection;
                shown->selection = 3;
                Sound(SOUND_MENU_MOVE);
            } else {
                shown->selection++;
                if (shown->selection >= shown->numOptions) {
                    shown->selection = 4;
                } else {
                    Sound(SOUND_MENU_MOVE);
                }
            }
        }
    } else {
        int blocked = 0;

        if (GetMenuKey(MENU_DOWN)) {
            shown->selection++;
            if (shown->selection < shown->numOptions && !shown->options[shown->selection].enabled) {
                blocked = 1;
                shown->selection--;
            }
            if (shown->selection >= shown->numOptions) {
                shown->selection = shown->numOptions - 1;
            } else if (!blocked) {
                Sound(SOUND_MENU_MOVE);
            }
        }
        if (GetMenuKey(MENU_UP)) {
            shown->selection--;
            if (shown->selection < 0) {
                shown->selection = 0;
            } else {
                Sound(SOUND_MENU_MOVE);
            }
        }
    }
}

/* The question on the main menu's Back: quit the game? */
static void QuitQuestion(u_short keys)
{
    pcTextDepth = 0.05f;
    PcDrawMenuBox(120, 160, 520, 300, 0.06f, 0.07f);
    PcCentreText("ARE YOU SURE YOU", 77, 95, 8, 0);
    PcCentreText("WANT TO QUIT ?", 105, 105, 8, 0);
    if (keys & KEY_LEFT) {
        if (!quitSelection) {
            Sound(SOUND_MENU_MOVE);
        }
        quitSelection = 1;
    }
    if (keys & KEY_RIGHT) {
        if (quitSelection) {
            Sound(SOUND_MENU_MOVE);
        }
        quitSelection = 0;
    }
    if (keys & KEY_BACK) {
        quitSelection = 0;
        quitAsked = 0;
        Sound(SOUND_MENU_SELECT);
    }
    if (keys & KEY_ACCEPT) {
        if (quitSelection) {
            SetSoundVolume(0);
            PortQuit(); /* Port: the game closes as when its window is closed (the PC posted WM_QUIT) */
        } else {
            Sound(SOUND_MENU_SELECT);
            quitAsked = 0;
        }
    }
    PcAddText("YES", 75, 120, 8, quitSelection && blinkFlag);
    PcAddText("NO", 220, 120, 8, !quitSelection && blinkFlag);
    pcTextDepth = 0.1f;
}

/* ---- the menus ---- */

int PcMenuControl(ConfigData *config, int timeout)
{
    int inputDelay = 2, attractMode = 0, idleCount = 0, steps, i;

    (void)timeout;
    InitDynamicMem(reserveHeap, RESERVE_HEAP_SIZE);
    cardNum = 0;
    heading = 0;
    leaving = 0;
    quitAsked = 0;
    sliding = 0;
    screenCancelled = 0;
    tempGameData = config;
    SfxSetReverbDepth(0);
    KillControlledSound();
    SetSkeletonPosition(raceCamera.camPos, 0, 0, 0);
    SetSkeletonDirection(raceCamera.camPos, 0, 0, 0);
    SetSkeletonPosition(upperSkeleton, 0, 0, 0);
    SetSkeletonDirection(upperSkeleton, 0, 0, 0);
    /* Port: the PC's models are seen from the screen's centre (its InitMenuDisplay set the console's geometry offset,
       150 down, or 110 for the team screen, which its Direct3D view does not use), at its scale (DrawMenuModel) */
    SetGeomOffset(160, 120);
    SetGeomScreen(320);
    for (i = 0; i < 15; i++) {
        InitMatrix(&shipShapeTable[i]->skeleton->relative);
        InitMatrix(&shipShapeTable[i]->skeleton->absolute);
        shipShapeTable[i]->skeleton->update = 1;
    }
    UpdateNotes(0);
    menu = DAlloc(reserveHeap, PC_MENU_COUNT * sizeof(MenuEntry));
    menuLayout = DAlloc(reserveHeap, PC_MENU_LAYOUTS * sizeof(MenuLayout));
    PcInitMenus();
    PcSetupUnlocks(config);
    if (!raceFlag) {
        if (attractSettingsSaved) {
            ownShip = attractShip;
            config->actTrackNum = attractActTrackNum;
            config->trackNum = attractTrackNum;
            trackNo = attractTrack;
        } else {
            config->trackNum = 0;
            trackNo = 0;
            ownShip = 9;
            config->actTrackNum = raceTracks[0];
        }
    } else {
        raceFlag = 0;
    }
    menu[PC_MENU_RACE_TYPE].selection = savedRaceSelection;
    menu[PC_MENU_TEAM].selection = savedTeamSelection;
    menu[PC_MENU_CLASS].selection = savedClassSelection;
    menu[PC_MENU_VECTOR + (config->shipClass & 3)].selection = savedTrackSelection;
    menu[PC_MENU_MAIN].selection = savedMainSelection;
    raceType = savedRaceSelection == 2 ? RACE_CHALLENGE : savedRaceSelection == 1 ? RACE_TIME_TRIAL : RACE_ARCADE;
    currMenu = PC_MENU_MAIN;
    SetShake(0);
    PcInitMenuState(config);
    Init2DParticle();
    PcInitMenuBackground();
    SetPolyF4(&boxEdge);
    boxEdge.r0 = boxEdge.g0 = boxEdge.b0 = 255;
    LoadCardTexture(); /* Port: the memory card saves' icon (the PlayStation's: blank with the PC's data) */
    PcLoadModel(controllerModels, 4, "cntrl", 1);
    PcLoadMenuLineData();
    PcLoadModel(menuModels, 10, "harry", 0);
    PcLoadModel(shipModels, 8, "june", 0);
    if (raceType == RACE_CHALLENGE) {
        config->shipClass = config->savedClass;
        config->actTrackNum = config->savedActTrackNum;
        config->trackNum = config->savedTrackNum;
    }
    SetCdVolume(config->musicVolume * 255 / 100);
    if (config->cdTrack == 1) {
        cdMusicIndex = (cdMusicIndex + 1) % (cdMusicCount - 1);
        cdMusicTrack = cdMusicTable[cdMusicIndex];
        CdMusicPlay(cdMusicTrack, 0);
    } else {
        CdMusicPlay(config->cdTrack, 0);
    }
    PcInitMenuBackPolys();
    SetSoundVolume(config->effectVolume * 255 / 100);
    pcBackdropDepth = 0.9f;
    fastFrame = 1;
    frameSteps = 2;
    while (!leaving) {
        SfxDoSfx();
        buildFixedPad(padBuf1);
        UpdateCheatSequence(1); /* Port: the PlayStation's button sequences (the PC typed its cheats' words) */
        quadIndex = CurrentScreen * 300;
        ClearOTagR(OT[CurrentScreen], 0x2000);
        ClearOTagR(OT2[CurrentScreen], 0x640);
        if (currMenu != PC_MENU_AUDIO) {
            /* Port: the next track when one ends, as the PlayStation's menus play them */
            SetInGameCDTrack(cdPlaySeconds, cdStatus, config, &cdState);
        }
        if (quitAsked) {
            u_short keys = keysPressed;

            keysPressed = 0; /* (the question takes the keys: the PC's menus see none while it is up) */
            QuitQuestion(keys);
        }
        /* Port: the tiles first, the panels over them at the same depth (the PC draws its pages in order) */
        for (i = 0; i < TILES_ACROSS * TILES_DOWN; i++) {
            DrawPixelTexturedQuad(0.99f, &menuTiles[i], 0);
        }
        for (i = 0; i < 14; i++) {
            DrawScaledTexturedQuad(0.99f, &menuBackPolys[i], 0);
        }
        Update2DParticle();
        if (!sliding) {
            PcShowMenu(config);
            PcRunMenuScreen(config);
        } else {
            PcSlideScreen();
        }
        if (inputDelay > 0) {
            steps = 2;
            inputDelay--;
        } else {
            steps = frameSteps;
        }
        cardNum -= steps * 30;
        heading -= steps * 15;
        ShakeScreen();
        Swap();
        PortAutoSave(config, 1); /* Port: the game saves itself (portsave.c) */
        frameSteps = menuSendFlags;
        for (i = 0; i < frameSteps; i++) {
            blinkFrame++;
        }
        blinkFlag = blinkFrame % 30 < 20;

        if (GetMenuKey(MENU_ACCEPT) && !sliding &&
            !(currMenu == PC_MENU_MAIN && menu[currMenu].selection == 2 && raceType == RACE_CHALLENGE)) {
            if (currMenu != PC_MENU_LOAD_SAVE && currMenu != PC_MENU_ARCADE_TIMES &&
                currMenu != PC_MENU_TIME_TRIAL_TIMES) {
                Sound(SOUND_MENU_SELECT);
            }
            if (currMenu == PC_MENU_MAIN && menu[currMenu].selection == 3) {
                savedRaceSelection = menu[PC_MENU_RACE_TYPE].selection;
                leaving = 1;
                savedTeamSelection = menu[PC_MENU_TEAM].selection;
                savedClassSelection = menu[PC_MENU_CLASS].selection;
                savedTrackSelection = menu[PC_MENU_VECTOR + (config->shipClass & 3)].selection;
                savedMainSelection = menu[PC_MENU_MAIN].selection;
            } else if (menu[currMenu].numOptions) {
                PcCopyGameData(config, 1);
                currMenu = menu[currMenu].options[menu[currMenu].selection].nextMenu;
                PcCopyGameData(config, 0);
                EnterMenu(currMenu);
            } else if (currMenu != PC_MENU_LOAD_SAVE && currMenu != PC_MENU_ARCADE_TIMES &&
                       currMenu != PC_MENU_TIME_TRIAL_TIMES) {
                PcCopyGameData(config, 1);
                currMenu = menu[currMenu].nextMenu;
                PcCopyGameData(config, 0);
                EnterMenu(currMenu);
            }
        }
        if (screenCancelled) {
            Sound(SOUND_MENU_SLIDE);
            PcCopyGameData(config, 1);
            currMenu = menu[currMenu].prevMenu;
            PcCopyGameData(config, 0);
            screenCancelled = 0;
            EnterMenu(currMenu);
        } else if (GetMenuKey(MENU_BACK)) {
            if (currMenu != PC_MENU_MAIN) {
                if (menu[currMenu].prevMenu != 999 && currMenu != PC_MENU_LOAD_SAVE) {
                    Sound(SOUND_MENU_SLIDE);
                    PcCopyGameData(config, 0);
                    EnterMenu(menu[currMenu].prevMenu);
                }
            } else {
                Sound(SOUND_MENU_SELECT);
                quitSelection = 0;
                quitAsked ^= 1;
            }
        }
        if (currMenu == PC_MENU_MAIN && !quitAsked) {
            if (keysDown) {
                idleCount = 0;
            } else {
                idleCount += frameSteps;
            }
            if (idleCount >= 1800) {
                attractMode = 1;
                leaving = 1;
            }
        }
    }
    pcBackdropDepth = 2.0f;
    if (raceType == RACE_TIME_TRIAL) {
        cheatWeapon = 0;
        cheatDamage = 0;
        cheatRain = 0;
        cheatTime = 0;
        cheatMinigun = 0;
    }
    ownShip = menuState.ship;
    if (raceType != RACE_CHALLENGE) {
        continuesLeft = 0;
    } else {
        continuesLeft = 3;
        config->savedClass = config->shipClass;
        config->savedActTrackNum = config->actTrackNum;
        config->savedTrackNum = config->trackNum;
    }
    config->class1Race = 0;
    config->class2Race = 0;
    /* Port: one player, no link (the PlayStation's menus leave these for the race) */
    gameType = 2;
    remoteShip = -1;
    DrawSync(0);
    InitNewDisplay(config);
    CdMusicStop();
    fastFrame = 0;
    Close2DParticle();
    if (attractMode) {
        attractSettingsSaved = 1;
        attractTrackNum = config->trackNum;
        savedRaceSelection = menu[PC_MENU_RACE_TYPE].selection;
        raceType = RACE_ATTRACT;
        attractTrack = trackNo;
        attractShip = ownShip;
        attractActTrackNum = config->actTrackNum;
        return 1;
    }
    for (i = 0; i < 5; i++) {
        UpdateNotes(0);
        VSync(0);
    }
    return 0;
}
