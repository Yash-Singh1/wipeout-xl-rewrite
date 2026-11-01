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
static Object *guestScene;
static TrackSection *boardSections[8];
static int boardHeights[8], boardTops[8], boardBottoms[8], boardScales[8], boardCount;

int PortOriginalLooks(void)
{
    return PortOriginalTrack() >= 0 && !PortOriginalTrackIs64();
}

int PortOriginalEightShips(void)
{
    return PortOriginalLooks() && gameType == 2 && raceType != 1;
}

/* Wipeout 64's tracks share one sky (Talon's Reach's model: an open cylinder about the camera, 2956 high), and
   Wipeout 64 keeps its heights in its code, not its data. Its lower rim goes at the camera's height, so that it fills
   all above the horizon: placed by the track's heights, it hung thousands of units above some tracks' cameras and
   the black background showed below it (Velocitar's and Qoron IV's walls of panels missing, popping in). */
void PortOriginalPlaceSky(Object *sky)
{
    int i, lowest = 0;

    if (!PortOriginalTrackIs64()) {
        return;
    }
    for (i = 0; i < (u_short)sky->vertexCount; i++) {
        if (sky->vertices[i].vy > lowest) lowest = sky->vertices[i].vy; /* (y grows downward) */
    }
    SetSkeletonPosition(sky->skeleton, 0, -lowest, 0);
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

    guestScene = prmCount ? prm[0] : NULL;
    boardCount = 0;
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

/* Height below an overhead polygon. The winding test rejects walls and roofs beside the
   checkpoint; y grows downward, so the board's top must have a larger y than the roof. */
static int BoardBelowRoof(int y, int top, TrackSection *section, VECTOR *points, int count)
{
    int ceiling = (-0x7fffffff - 1), positive = 0, negative = 0, j;
    int64_t area = 0;

    for (j = 0; j < count; j++) {
        VECTOR *v = &points[j], *next = &points[(j + 1) % count];
        int64_t side = (int64_t)(next->vx - v->vx) * (section->centre.vz - v->vz) -
                       (int64_t)(next->vz - v->vz) * (section->centre.vx - v->vx);

        positive |= side > 0;
        negative |= side < 0;
        area += (int64_t)v->vx * next->vz - (int64_t)next->vx * v->vz;
        if (v->vy > ceiling) ceiling = v->vy;
    }
    if (area && !(positive && negative) && ceiling < section->centre.vy - 512 && y + top < ceiling + 128) {
        y = ceiling + 128 - top;
    }
    return y;
}

/* XL's board height can intersect a guest's tunnel. Wipeout 64's roofs are scenery,
   rather than track faces. Test their polygons once per board and keep its height for the race. */
static void ScaleBoard(Object *shape, int scale)
{
    int row, col;
    /* DrawDroid resets the rotation before each call, so the scale cannot accumulate. */
    for (row = 0; row < 3; row++) {
        for (col = 0; col < 3; col++) {
            shape->skeleton->relative.m[row][col] =
                (short)((shape->skeleton->relative.m[row][col] * scale) / 4096);
        }
    }
}

int PortOriginalFitDroid(Object *shape, TrackSection *section)
{
    Object *object;
    int y = section->centre.vy - 3000, top = 0, bottom = 0, scale = 4096, i, j;

    if (!PortOriginalTrackIs64()) return y;
    for (i = 0; i < (u_short)shape->vertexCount; i++) {
        if (shape->vertices[i].vy < top) top = shape->vertices[i].vy;
        if (shape->vertices[i].vy > bottom) bottom = shape->vertices[i].vy;
    }
    for (i = 0; i < boardCount; i++) {
        if (boardSections[i] == section && boardTops[i] == top && boardBottoms[i] == bottom) {
            ScaleBoard(shape, boardScales[i]);
            return boardHeights[i];
        }
    }
    for (object = guestScene; object; object = object->next) {
        Poly poly;
        poly.prim = object->primitives;
        for (i = 0; i < (u_short)object->primitiveCount; i++) {
            const short *coords;
            int count, size;
            VECTOR points[4];
            MATRIX *m = &object->skeleton->relative;

            switch (poly.prim->type) {
            case TypeF3: coords = poly.f3->coords; count = 3; size = sizeof(F3); break;
            case TypeFT3: coords = poly.ft3->coords; count = 3; size = sizeof(FT3); break;
            case TypeG3: coords = poly.g3->coords; count = 3; size = sizeof(G3); break;
            case TypeGT3: coords = poly.gt3->coords; count = 3; size = sizeof(GT3); break;
            case TypeF4: coords = poly.f4->coords; count = 4; size = sizeof(F4); break;
            case TypeFT4: coords = poly.ft4->coords; count = 4; size = sizeof(FT4); break;
            case TypeG4: coords = poly.g4->coords; count = 4; size = sizeof(G4); break;
            case TypeGT4: coords = poly.gt4->coords; count = 4; size = sizeof(GT4); break;
            case TypeTSPR: case TypeBSPR: poly.ptr += sizeof(SPR); continue;
            default: size = count = 0; coords = NULL; break;
            }
            if (!size) break;
            for (j = 0; j < count; j++) {
                /* The quad's file order is a triangle strip, not a perimeter. */
                int corner = count == 4 && j >= 2 ? 5 - j : j;
                const SVECTOR *v = &object->vertices[coords[corner]];
                points[j].vx = ((m->m[0][0]*v->vx + m->m[0][1]*v->vy + m->m[0][2]*v->vz) >> 12) + m->t[0];
                points[j].vy = ((m->m[1][0]*v->vx + m->m[1][1]*v->vy + m->m[1][2]*v->vz) >> 12) + m->t[1];
                points[j].vz = ((m->m[2][0]*v->vx + m->m[2][1]*v->vy + m->m[2][2]*v->vz) >> 12) + m->t[2];
            }
            y = BoardBelowRoof(y, top, section, points, count);
            poly.ptr += size;
        }
    }
    /* An XL board can be taller than a guest tunnel. Keep its proportions while
       fitting both edges between the roof clearance and the racing surface. */
    if (y + bottom > section->centre.vy - 512 && bottom > top) {
        int roof = y + top, floor = section->centre.vy - 512;
        if (floor > roof) {
            scale = (floor - roof) * 4096 / (bottom - top);
            y = roof - top * scale / 4096;
        }
    }
    ScaleBoard(shape, scale);
    if (boardCount < 8) {
        boardSections[boardCount] = section;
        boardTops[boardCount] = top;
        boardBottoms[boardCount] = bottom;
        boardScales[boardCount] = scale;
        boardHeights[boardCount++] = y;
    }
    return y;
}

/* Qirex's XL Rapier profile outruns the original circuits' ramps. Use its
   original Rapier profile when racing those circuits with the original model. */
void PortOriginalShipPhysics(ShipData *ship, int number, int shipClass)
{
    if (PortOriginalLooks() && number >= 6 && number < 9 && shipClass == 2) {
        ship->mass = 150;
        ship->maxThrust = 1400;
        ship->resistance = 130;
        ship->skid = 16;
        ship->headingInc = PAL_SPEED(140);
        ship->maxHeading = PAL_SPEED(1920);
    }
}
