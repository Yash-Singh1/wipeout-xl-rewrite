/* Port: the original Wipeout's tracks and Wipeout 64's, raced as guests in Talon's Reach's slot (see
   platform/original.c).

   The RACING CLASS menu has a fifth line, WIPEOUT, when the player's copy of the original or of Wipeout 64
   has been found (the debug console's Files tab) and the race is an arcade race or a time trial. On it LEFT
   and RIGHT go through their tracks (the original's each raced at the class it was made for, Venom or
   Rapier; Wipeout 64's at Rapier). A chosen
   track takes the place of Talon's Reach (trackNum 0, folder 1): the platform answers the game's reads of
   that track's files with ones made from the original's, and the slot's start line, sky height and
   droid section are the guest's until an XL track is chosen again. Its times are not records: the
   records and medals of Talon's Reach stay its own. */
#include "functions.h"
#include "menus.h"
#include "effects.h"
#include "ttf.h"

extern s32       raceType;
extern short     startLinePos[];
extern int       skyPos[];
extern SideTable droidPickTable;
extern MenuEntry *menu;
extern short     CurrentScreen;
extern Texture  *textureTable[];

#define HOST_TRACK  0 /* trackNum: Talon's Reach */
#define HOST_FOLDER 1 /* actTrackNum: its folder, track01 */
#define WIPEOUT_OPTION 4

static int   saved;
static short savedStartLine, savedPick;
static int   savedSky;

/* Chooses an original track (index in PortOriginalTrack's list) or none (-1) for the next race. */
void PortOriginalChoose(int index)
{
    PortOriginalTrackSelect(index);
    if (index >= 0) {
        if (!saved) {
            savedStartLine = startLinePos[HOST_FOLDER];
            savedSky = skyPos[HOST_TRACK];
            savedPick = droidPickTable.value[HOST_TRACK];
            saved = 1;
        }
        startLinePos[HOST_FOLDER] = (short)PortOriginalStartLine();
        skyPos[HOST_TRACK] = PortOriginalSkyY();
        droidPickTable.value[HOST_TRACK] = (short)PortOriginalStartLine();
    } else if (saved) {
        startLinePos[HOST_FOLDER] = savedStartLine;
        skyPos[HOST_TRACK] = savedSky;
        droidPickTable.value[HOST_TRACK] = savedPick;
        saved = 0;
    }
}

/* A race begins: Talon's Reach's own values again if no original track is chosen any more (the player
   chose another copy of the original in the debug console meanwhile). */
void PortOriginalRaceBegins(void)
{
    if (PortOriginalTrack() < 0) {
        PortOriginalChoose(-1);
    }
}

/* Whether the class menu offers the original's tracks: there are some, in an arcade race or a time trial. */
int PortOriginalOffered(void)
{
    return PortOriginalTrackCount() > 0 && (raceType == 0 || raceType == 1);
}

/* The class menu's line moved up (step -1) or down (+1) from current over the lines shown: the classes
   (the fourth only when unlocked) and WIPEOUT. */
int PortClassMove(int current, int step, int classCount)
{
    int lines[5], count = 0, i, at = 0;

    for (i = 0; i < classCount; i++) {
        lines[count++] = i;
    }
    if (PortOriginalOffered()) {
        lines[count++] = WIPEOUT_OPTION;
    }
    for (i = 0; i < count; i++) {
        if (lines[i] == current) {
            at = i;
        }
    }
    at += step;
    if (at < 0) {
        at = 0;
    }
    if (at >= count) {
        at = count - 1;
    }
    return lines[at];
}

/* The class menu on the WIPEOUT line: how many tracks LEFT and RIGHT go through, and (draw) the track
   chosen, its name and class. Returns 1 when that line is the current one (the menu shows the guest
   instead of an XL track). */
int PortOriginalClassMenu(MenuSelection *selection, short *trackCount, int draw)
{
    char text[40];

    menu[2].options[WIPEOUT_OPTION].enabled = (short)PortOriginalOffered();
    if (selection->current != WIPEOUT_OPTION || !PortOriginalOffered()) {
        if (selection->current == WIPEOUT_OPTION) {
            selection->current = 0; /* (no longer offered) */
        }
        PortOriginalChoose(-1);
        return 0;
    }
    *trackCount = (short)PortOriginalTrackCount();
    if (selection->trackSelection >= *trackCount) {
        selection->trackSelection = *trackCount - 1;
    }
    if (selection->trackSelection < 0) {
        selection->trackSelection = 0;
    }
    if (!draw) {
        return 1;
    }
    PortOriginalChoose(selection->trackSelection);
    selection->trackNum = HOST_TRACK;
    selection->actTrackNum = HOST_FOLDER;
    selection->shipClass = (char)PortOriginalTrackClass(selection->trackSelection);
    PrintCX((u_char *)"ORIGINAL", 68, 186);
    sprintf(text, "FROM %s", PortOriginalTrackGame(selection->trackSelection));
    Print((u_char *)text, 120, 182, 8);
    sprintf(text, "%s CLASS  TRACK %d OF %d", selection->shipClass == 2 ? "RAPIER" : "VENOM",
            selection->trackSelection + 1, *trackCount);
    Print((u_char *)text, 120, 189, 8);
    return 1;
}

/* The guests' track menu models (the original's common/track.prm and Wipeout 64's, served as origtrk.prm:
   platform/original.c), loaded with the menus' own models when the player's copies are in use. */
#define GUEST_MODELS 32 /* (LoadModel fills as many as the file has: the original's and Wipeout 64's seven) */
static Object *originalModels[GUEST_MODELS];
static int originalModelCount;
void LoadModel(Object **modelShapes, char *fileName);

static short pictureBase = -1; /* the original's pictures of its circuits: the first's texture, or -1 */

void PortOriginalModelsLoad(void)
{
    int i;

    originalModelCount = 0;
    memset(originalModels, 0, sizeof(originalModels));
    pictureBase = -1;
    if (PortOriginalHasPictures()) {
        pictureBase = (short)TextureTableCount;
        LoadCmpFiles(LoadCompressedTextureSequence("c:\\wipeout2\\common\\origpics.cmp"));
    }
    if (!PortOriginalHasModels()) {
        return;
    }
    LoadModel(originalModels, "origtrk");
    for (i = 0; i < GUEST_MODELS && originalModels[i]; i++) {
        originalModelCount++;
    }
}

/* The picture of the WIPEOUT line's track in the class menu, as its own game's track menu showed it (the original
   Wipeout's; Wipeout 64's have their models): 1 when it was drawn. */
int PortOriginalDrawPicture(int index, int x, int y)
{
    static POLY_FT4 picture[2];
    POLY_FT4 *p = &picture[CurrentScreen];
    int number = PortOriginalTrackPicture(index);
    Texture *texture;

    if (pictureBase < 0 || number < 0) {
        return 0;
    }
    texture = textureTable[pictureBase + number];
    SetPolyFT4(p);
    p->r0 = p->g0 = p->b0 = 128;
    p->x0 = p->x2 = (short)x;
    p->x1 = p->x3 = (short)(x + texture->textureW * 2);
    p->y0 = p->y1 = (short)y;
    p->y2 = p->y3 = (short)(y + texture->textureH);
    p->u0 = p->u2 = (u_char)texture->u0;
    p->u1 = p->u3 = (u_char)(texture->u1 + 1);
    p->v0 = p->v1 = (u_char)texture->v0;
    p->v2 = p->v3 = (u_char)(texture->v2 + 1);
    p->tpage = texture->tsb;
    p->clut = texture->cba;
    AddPrim(OT[CurrentScreen], p);
    return 1;
}

/* The model of the WIPEOUT line's track, or NULL. */
Object *PortOriginalModel(int index)
{
    const char *name = PortOriginalTrackModel(index);
    int i;

    for (i = 0; name && i < originalModelCount; i++) {
        if (!strncmp(originalModels[i]->name, name, 15)) {
            return originalModels[i];
        }
    }
    return NULL;
}

/* Back in the class menu after a race on an original track: its line and track again. */
void PortOriginalMenuRestore(MenuSelection *selection)
{
    if (PortOriginalTrack() >= 0 && PortOriginalOffered()) {
        selection->current = WIPEOUT_OPTION;
        selection->trackSelection = (short)PortOriginalTrack();
    }
}

/* The name of the track of the coming race (a guest's in Talon's Reach's folder). */
char *PortRaceTrackName(short actTrackNum)
{
    if (actTrackNum == HOST_FOLDER && PortOriginalTrack() >= 0) {
        return (char *)PortOriginalTrackName(PortOriginalTrack());
    }
    return GetTrackName(actTrackNum, 0, 0, 0);
}

/* The track's surface textures go to the first slots of the texture table, below the ones the game loaded at its
   start and keeps (the HUD's and the menus' fonts first, at slot 0x113, main.c): as many slots as XL's own
   libraries need. A guest's with more (Wipeout 64's Sokana, Dyroness and Machaon II: 15 tiles of 21 pieces each)
   would overwrite those, and the race's own textures would then overwrite its last tiles: it goes after the
   textures loaded so far instead (the table has room for it, game/data), and its tiles name their pieces there. */
#define LIBRARY_SLOTS 0x113
static short libraryBase;

/* Where the track's library is loaded (into the texture table): its first slot. */
short PortOriginalLibraryBase(short loaded)
{
    libraryBase = PortOriginalTrack() >= 0 && PortOriginalLibraryTextures() > LIBRARY_SLOTS ? loaded : 0;
    return libraryBase;
}

/* The library and its tiles loaded: the tiles' pieces where it went, and the slot the race's other textures follow
   from (as the game's own, the slot they followed from before). */
short PortOriginalLibraryPlaced(TexTemplate *tiles, short tileCount, short loaded)
{
    int i, k;

    if (!libraryBase) {
        return loaded;
    }
    for (i = 0; i < tileCount; i++) {
        for (k = 0; k < 16; k++) {
            tiles[i].tex16[k] += libraryBase;
        }
        for (k = 0; k < 4; k++) {
            tiles[i].tex4[k] += libraryBase;
        }
        tiles[i].tex1[0] += libraryBase;
    }
    return (short)TextureTableCount;
}

/* ---- the original's scenery ----

   A track of the original Wipeout (not Wipeout 64's) looks as in its own race. Its start gantries ("start...")
   light their three lamps one by one for the count, red at two, amber at one and green at go (the original's
   code, which XL kept, ColourStartBoom, but no longer called); its beacons ("redl...") pulse once a second and
   its oil pumps ("donkey...") nod. XL's own markers are not drawn over it: the droids with the start lights,
   the lap boards and the checkpoint signs, and the white of its checkpoint sections (dynam.c). */

#define MAX_BEACONS 8
#define MAX_PUMPS   8

static Object *beacons[MAX_BEACONS], *pumps[MAX_PUMPS];
static int     beaconCount, pumpCount, sceneryFrame;

int PortOriginalLooks(void)
{
    return PortOriginalTrack() >= 0 && !PortOriginalTrackIs64();
}

int PortOriginalEightShips(void)
{
    return PortOriginalLooks() && gameType == 2 && raceType != 1;
}

static void Collect(Object **prm, short prmCount, const char *name, Object **found, int *count, int max)
{
    Object *obj;
    int     i;

    *count = 0;
    for (i = 0; i < prmCount; i++) {
        for (obj = prm[i]; obj; obj = obj->next) {
            if (!strncmp(obj->name, name, strlen(name)) && *count < max) {
                found[(*count)++] = obj;
            }
        }
    }
}

/* A race begins: its scenery's moving parts (none but on the original's tracks: XL's own start gantries are
   left as they are). */
void PortOriginalSceneInit(Object **prm, short prmCount)
{
    int looks = PortOriginalLooks();

    InitStartBoom(prm, looks ? prmCount : 0);
    ResetStartBoom();
    Collect(prm, looks ? prmCount : 0, "redl", beacons, &beaconCount, MAX_BEACONS);
    Collect(prm, looks ? prmCount : 0, "donkey", pumps, &pumpCount, MAX_PUMPS);
    sceneryFrame = 0;
}

/* The count before the start (GeneralStartProcs): the gantries' lamps. */
void PortOriginalCount(int updateCount)
{
    if (!PortOriginalLooks()) {
        return;
    }
    if (updateCount == REGION(0x42, 83)) {
        ColourStartBoom(1);
    } else if (updateCount == REGION(0x21, 41)) {
        ColourStartBoom(2);
    } else if (updateCount == 0) {
        ColourStartBoom(3);
    }
}

/* Each frame of the race (but while paused): the beacons and the pumps; on Wipeout 64's tracks, its animated
   scenery. */
void PortOriginalSceneUpdate(int paused)
{
    int i, red, angle;

    if (!paused) {
        sceneryFrame++;
        PortOriginalAnimate(sceneryFrame);
    }
    red = 128 + rsin((short)((sceneryFrame * 4096 / PAL_TIME(30)) & 4095)) * 127 / 4096;
    for (i = 0; i < beaconCount; i++) {
        Poly     poly;
        CVECTOR *colour = NULL;
        int      corners = 0, k;

        poly.prim = beacons[i]->primitives;
        if (poly.prim->type == TypeGT4) {
            colour = poly.gt4->colour;
            corners = 4;
        } else if (poly.prim->type == TypeGT3) {
            colour = poly.gt3->colour;
            corners = 3;
        }
        for (k = 0; k < corners; k++) {
            colour[k].r = (u_char)red;
            colour[k].g = 0;
            colour[k].b = 0;
        }
    }
    /* (a radian each way, every eight seconds) */
    angle = rsin((short)((sceneryFrame * 4096 / PAL_TIME(240)) & 4095)) * 652 / 4096;
    for (i = 0; i < pumpCount; i++) {
        SetSkeletonDirection(pumps[i]->skeleton, (short)angle, 0, 0);
    }
}
