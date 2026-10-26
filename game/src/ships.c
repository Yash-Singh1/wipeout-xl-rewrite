#include "functions.h"
#include "ships.h"
#include "portfar.h" /* Port: the far view */
#include "portorig.h" /* Port: the original Wipeout's tracks */

/* $gp-relative globals are tentative definitions; the rest are extern. */
extern int           proximity;
extern int           sharkVertCount;
extern int           sharkWiggle;
extern u_char        sharkToggle;
extern short         shipTex;
extern short         bigGrid;

/* Port: the path plus ".cmp" is 29 bytes with its terminator; the console's 28-byte union overran by one byte into
   the unused `pad` array that follows it in loadShipData's frame, so the buffer is 32 bytes. */
typedef union ShipPath {
    char name[32];
    int  words[8];
} ShipPath;

typedef struct {
    u_short state[16];
} ShipStartStates;


extern TrackData  *track;
extern int         ownShip;
extern int         raceType;
extern int         remoteShip;
extern int         ghostShip;
extern int         shakeAmount;
extern char        errorString[];
extern char        shipCmpExt[];
extern char        shipPrmExt[];
extern const ShipStartStates startStateTable;
extern short       activeShipCount;
/* Port: phantomGridPos is part of gridPosition; the uses read it directly. */
extern short       gridPosition[];
extern ShipRank    shipRank[];
extern RaceLine    raceLines[];
extern Texture *textureTable[];
extern GhostClut   ghostCluts[];
/* Port: sharkBodyModel, sharkLegModel are parts of shipShapeTable; the uses read them directly. */
extern SharkVert   sharkBaseVerts[];
extern SharkVert   sharkFootVerts[];
extern int         sharkFootIndex[];
extern short       startLinePos[];
extern u_short     spreadScale0[];
extern u_short     spreadScale1[];
extern u_short     spreadScale2[];
extern u_short     spreadScale3[];

extern u_short shipMassClass0[], shipMassClass1[], shipMassClass2[], shipMassClass3[];
extern u_short shipMaxThrustClass0[], shipMaxThrustClass1[], shipMaxThrustClass2[], shipMaxThrustClass3[];
extern u_short shipSkidClass0[], shipSkidClass1[], shipSkidClass2[], shipSkidClass3[];
extern u_short shipResistanceClass0[], shipResistanceClass1[], shipResistanceClass2[], shipResistanceClass3[];
extern u_short shipHeadingIncClass0[], shipHeadingIncClass1[], shipHeadingIncClass2[], shipHeadingIncClass3[];
extern u_short shipMaxHeadingClass0[], shipMaxHeadingClass1[], shipMaxHeadingClass2[], shipMaxHeadingClass3[];
/* A per-frame heading rate from the tables above: read unsigned on NTSC; the European program reads the
   tables as signed shorts and raises the rate with PAL_SPEED. */
#define SHIP_RATE(rate) (portPal ? PAL_SPEED((short)(rate)) : (rate))
extern u_short shipRemoteMaxClass0[], shipRemoteMaxClass1[], shipRemoteMaxClass2[], shipRemoteMaxClass3[];
extern u_short shipRemoteMagClass0[], shipRemoteMagClass1[], shipRemoteMagClass2[], shipRemoteMagClass3[];

void DrawShips(ShipData *shipIndex, Object **shipShapes, int camShip, TrackCamera camera, int flg)
{
    int       i;
    VECTOR    distance;
    VECTOR    v2;
    SVECTOR   toCentre;
    int       firstMag, secondMag, firstShip, secondShip;
    Object   *obj;
    int       mag;
    int      rflag;
    u_short   baseIndex, attr2;
    int       diff, rawDiff;
    int       v;
    u_char    flags;
    Face     *face;
    CVECTOR  *win;

    proximity = 0;
    baseIndex = (u_short)raceLines[camera.section->secNo].distance;
    firstMag = 3000;
    secondMag = 3000;
    firstShip = -1;
    secondShip = -1;
    i = 0;
    for (;;) {
        shipIndex[i].combatAttr |= 0x80;
        distance.vx = (shipIndex[i].ppivot.vx + camera.camPos->relative.t[0]) >> 3;
        distance.vy = (shipIndex[i].ppivot.vy + camera.camPos->relative.t[1]) >> 3;
        distance.vz = (shipIndex[i].ppivot.vz + camera.camPos->relative.t[2]) >> 3;
        mag = GetMagnitude(distance);
        toCentre.vx = shipIndex[i].ppivot.vx + camera.camPos->relative.t[0];
        toCentre.vy = shipIndex[i].ppivot.vy + camera.camPos->relative.t[1];
        toCentre.vz = shipIndex[i].ppivot.vz + camera.camPos->relative.t[2];
        SetSkeletonTransform(camera.camPos->super);
        RotTrans(&toCentre, &v2, &rflag);
        if (mag < 2900) {
            proximity |= 1 << i;
            shipIndex[i].attr |= VISIBLE;
            if (!(shipIndex[i].combatAttr & DESTROYED) && (i != ownShip)) {
                if (mag < firstMag) {
                    firstMag = mag;
                    firstShip = i;
                } else if (mag < secondMag) {
                    secondMag = mag;
                    secondShip = i;
                }
            }
        }
        if (((mag < 1200) && (v2.vz > 0)) || (i == camShip)) {
            obj = shipShapes[shipIndex[i].highResTableIndex];
            SetSkeletonPosition(obj->skeleton, shipIndex[i].ppivot.vx, shipIndex[i].ppivot.vy, shipIndex[i].ppivot.vz);
            SetSkeletonDirectionHPR(obj->skeleton, shipIndex[i].hdg, shipIndex[i].pitch, shipIndex[i].roll);
            attr2 = shipIndex[i].combatAttr;
            if (((attr2 & DESTROYED) == 0) || (shipTargetIndex[i] >= 11)) {
              if ((shipIndex[i].attr & (REMOTE_VIEW | VIEW_IN)) != VIEW_IN) {
                rawDiff = baseIndex - (u_short)raceLines[shipIndex[i].nearTrkSect->secNo].distance;
                if ((short)rawDiff < 0) {
                    diff = numTrackSections + rawDiff;
                } else diff = rawDiff;
                if ((short)diff > (short)numTrackSections / 2) {
                    diff = diff - numTrackSections;
                    }
                if ((u_short)(diff + 39) < 79) {
                    PortFarMarkShip(i); /* Port: drawn here, not by the far view */
                    shipIndex[i].combatAttr = attr2 & ~0x80;
                    face = track->faces + shipIndex[i].nearTrkSect->faceStart;
                    do {
                        flags = face->flags;
                        face++;
                    } while (!(flags & 1));
                    face--;
                    if (!(shipIndex[i].attr & LSIDE)) face++;
                    win = (CVECTOR *)&face->red;
                    PortInterpInstance(i); /* Port: frame interpolation: the model's drawing is this ship's */
                    if (raceType == 3) {
                        if ((i == remoteShip) || (i == ownShip)) {
                            AsmDrawObjectLit(obj, camera.camPos, 0, win);
                        }
                    } else if (raceType == 1) {
                        if (i == ownShip) {
                            AsmDrawObjectLit(obj, camera.camPos, 0, win);
                        }
                        if (i == ghostShip) {
                            InitTransShip(obj, 1);
                            AsmDrawObjectLit(obj, camera.camPos, 0, win);
                            InitTransShip(obj, 0);
                        }
                    } else {
                        AsmDrawObjectLit(obj, camera.camPos, 0, win);
                    }
                    PortInterpInstance(-1);
                }
              }
            }
        } else if (!(mag < 2900)) {
            shipIndex[i].attr &= ~VISIBLE;
        }
        i++;
        if (i >= 15) {
            break;
        }
    }
    if (flg == -1) {
        AdjustRemoteEngineSound(firstShip, secondShip, firstMag, secondMag);
    }
}


void CopySharkVerts(Object *ship)
{
    int     modelIndex;
    int     vertexIndex;
    Object *model;
    int     lowestY;
    short   vertexY;
    short   vertexCount;

    for (modelIndex = 0; modelIndex < 5; modelIndex++) {
        model = shipShapeTable[modelIndex];
        for (vertexIndex = 0; vertexIndex < model->vertexCount; vertexIndex++) {
            model->vertices[vertexIndex].vy -= 0x30;
        }
    }
    vertexCount = ship->vertexCount;
    sharkVertCount = vertexCount;
    if (vertexCount > 0) {
        modelIndex = 0;
        do {
            sharkBaseVerts[modelIndex].x = ship->vertices[modelIndex].vx;
            sharkBaseVerts[modelIndex].y = ship->vertices[modelIndex].vy;
            sharkBaseVerts[modelIndex].z = ship->vertices[modelIndex].vz;
            modelIndex++;
        } while (modelIndex < vertexCount);
    }
    model = shipShapeTable[4];
    for (modelIndex = 0; modelIndex < 4; modelIndex++) {
        lowestY = 0;
        for (vertexIndex = 0; vertexIndex < model->vertexCount; vertexIndex++) {
            vertexY = model->vertices[vertexIndex].vy;
            if (vertexY < lowestY) {
                sharkFootIndex[modelIndex] = vertexIndex;

                sharkFootVerts[modelIndex].x = model->vertices[vertexIndex].vx;
                sharkFootVerts[modelIndex].y = model->vertices[vertexIndex].vy;
                sharkFootVerts[modelIndex].z = model->vertices[vertexIndex].vz;
                lowestY = vertexY;
            }
        }
        model->vertices[sharkFootIndex[modelIndex]].vy = 0;
    }
}


void UpdateSharkModel(void)
{
    int     i;
    int     j;
    Object *body;
    Object *legs;
    short   pad[4];

    body = shipShapeTable[2];
    i = 0;
    if (sharkVertCount > 0) {
        do {
            body->vertices[i].vx = (short)(sharkBaseVerts[i].x + (rsin(sharkWiggle + sharkBaseVerts[i].z * 4) >> 6));
            i++;
        } while (i < sharkVertCount);
    }
    sharkWiggle = (sharkWiggle + 0x80) & 0xFFF;
    legs = shipShapeTable[4];
    if (sharkToggle == 0) {
        sharkToggle = 1;
        for (j = 0; j < 4; j++) {
            legs->vertices[sharkFootIndex[j]].vy = sharkFootVerts[j].y;
        }
    } else {
        sharkToggle = 0;
        for (j = 0; j < 4; j++) {
            legs->vertices[sharkFootIndex[j]].vy = (short)(sharkFootVerts[j].y + 0x40);
        }
    }
}


void InitTransShip(Object *lights, short flag)
{
    short i;
    Poly  prm;

    prm.prim = lights->primitives;
    i = 0;
    if (lights->primitiveCount > 0) {
        do {
            switch ((short)prm.prim->type) {
            case TypeF3:
                prm.f3 += 1;
                break;
            case TypeF4:
                prm.f4 += 1;
                break;
            case TypeFT3:
                prm.ft3->tsb = GetTPage(0, 0, textureTable[prm.ft3->texture]->textureX, textureTable[prm.ft3->texture]->textureY);
                if (!flag) {
                    prm.ft3->cba = (textureTable[prm.ft3->texture]->clutY << 6) | ((short)textureTable[prm.ft3->texture]->clutX >> 4);
                    prm.ft3->flag &= ~4;
                } else {
                    prm.ft3->cba = (ghostCluts[prm.ft3->texture - shipTex].clutY << 6) | ((short)ghostCluts[prm.ft3->texture - shipTex].clutX >> 4);
                    prm.ft3->flag |= 4;
                }
                prm.ft3 += 1;
                break;
            case TypeFT4:
                prm.ft4->tsb = GetTPage(0, 0, textureTable[prm.ft4->texture]->textureX, textureTable[prm.ft4->texture]->textureY);
                if (!flag) {
                    prm.ft4->cba = (textureTable[prm.ft4->texture]->clutY << 6) | ((short)textureTable[prm.ft4->texture]->clutX >> 4);
                    prm.ft4->flag &= ~4;
                } else {
                    prm.ft4->cba = (ghostCluts[prm.ft4->texture - shipTex].clutY << 6) | ((short)ghostCluts[prm.ft4->texture - shipTex].clutX >> 4);
                    prm.ft4->flag |= 4;
                }
                prm.ft4 += 1;
                break;
            case TypeG3:
                prm.g3 += 1;
                break;
            case TypeG4:
                prm.g4 += 1;
                break;
            case TypeGT3:
                prm.gt3->tsb = GetTPage(0, 0, textureTable[prm.gt3->texture]->textureX, textureTable[prm.gt3->texture]->textureY);
                if (!flag) {
                    prm.gt3->cba = (textureTable[prm.gt3->texture]->clutY << 6) | ((short)textureTable[prm.gt3->texture]->clutX >> 4);
                    prm.gt3->flag &= ~4;
                } else {
                    prm.gt3->cba = (ghostCluts[prm.gt3->texture - shipTex].clutY << 6) | ((short)ghostCluts[prm.gt3->texture - shipTex].clutX >> 4);
                    prm.gt3->flag |= 4;
                }
                prm.gt3 += 1;
                break;
            case TypeGT4:
                prm.gt4 += 1;
                break;
            default:
                sprintf(errorString, "ships.c::InitTransShip:Bad primitive type %x\n", prm.prim->type);
                Error(errorString, Fatal);
                break;
            }
            i++;
        } while (i < lights->primitiveCount);
    }
}


void loadShipData(TrackSection *section, ShipData *shipIndex, Object **shipShapes, Skeleton *camPos)
{
    Object   *obj;
    ShipPath  shipFile;
    short     shipTextures;
    short     droidTextures;
    short     count = 0;
    char      pad[0x1DD];

    if (sillyShips) {
        highResTable[1] = 5;
        highResTable[2] = 10;
        highResTable[3] = 1;
        highResTable[4] = 6;
        highResTable[5] = 11;
        highResTable[6] = 3;
        highResTable[7] = 8;
        highResTable[8] = 13;
        highResTable[9] = 4;
        highResTable[10] = 9;
        highResTable[11] = 14;
        highResTable[12] = 2;
        highResTable[13] = 7;
        highResTable[0] = 0;
        highResTable[14] = 12;
    }
    if (!sillyShips) {
        __builtin_memcpy(shipFile.name, "c:\\wipeout2\\common\\terry", 25);
    } else {
        __builtin_memcpy(shipFile.name, "c:\\wipeout2\\common\\wierd", 25);
    }
    strcat(shipFile.name, shipCmpExt);
    shipTextures = TextureTableCount;
    shipTex = shipTextures;
    GhostLoadCmpFiles(LoadCompressedTextureSequence(shipFile.name));
    droidTextures = TextureTableCount;
    LoadCmpFiles(LoadCompressedTextureSequence("c:\\wipeout2\\common\\rescu.cmp"));
    if (!sillyShips) {
        __builtin_memcpy(shipFile.name, "c:\\wipeout2\\common\\terry", 25);
    } else {
        __builtin_memcpy(shipFile.name, "c:\\wipeout2\\common\\wierd", 25);
    }
    strcat(shipFile.name, shipPrmExt);
    obj = (shipShapes[0] = LoadPrm(shipFile.name, shipTextures));
    while (obj) {
        obj->skeleton->super = camPos;
        shipShapes[count] = obj;
        count++;
        obj = obj->next;
    }

    count = 0;
    while (count < 15) {
        shipIndex[count].highResTableIndex = highResTable[count];
        count++;
    }

    count = 15;
    shipShapes[5] = NewInstance(shipShapes[0]);
    shipShapes[6] = NewInstance(shipShapes[1]);
    shipShapes[7] = NewInstance(shipShapes[2]);
    shipShapes[8] = NewInstance(shipShapes[3]);
    shipShapes[9] = NewInstance(shipShapes[4]);
    shipShapes[10] = NewInstance(shipShapes[0]);
    shipShapes[11] = NewInstance(shipShapes[1]);
    shipShapes[12] = NewInstance(shipShapes[2]);
    shipShapes[13] = NewInstance(shipShapes[3]);
    shipShapes[14] = NewInstance(shipShapes[4]);
    __builtin_memcpy(shipFile.name, "c:\\wipeout2\\common\\telco", 25);
    strcat(shipFile.name, shipPrmExt);
    obj = LoadPrm(shipFile.name, shipTex);
    shipShapes[15] = obj;
    while (obj) {
        shipIndex[count - 15].lowResTableIndex = count;
        obj->skeleton->super = camPos;
        shipShapes[count] = obj;
        count++;
        obj = obj->next;
    }

    obj = (shipShapes[30] = LoadPrm("c:\\wipeout2\\common\\rescu.prm", droidTextures));
    while (obj) {
        obj->skeleton->super = camPos;
        obj = obj->next;
    }

    if (sillyShips) {
        CopySharkVerts(shipShapes[2]);
    }
}


typedef struct ShipStartLocals {
    VECTOR        delta;
    VECTOR        point;
    TrackSection *starts[15];
    int           pad;
    u_char        side[16];
    short         order[16];
    ShipStartStates states;
    int           padding[2];
} ShipStartLocals;

void initShipData(TrackSection *section, ShipData *shipIndex, ConfigData *options)
{
    ShipStartLocals locals;
    short         i, j, slot, groups;
    int           side, index;
    short         next;
    u_short       position, sectionNum;
    short         launch[15]; /* Port: the place each ship's launch is set by */
    ShipData   *ship;
    TrackSection *aheadSection;
    Face         *face;
    u_char        flags;

    locals.states = startStateTable;
    shakeAmount = 0;
    bigGrid = options->shipClass == 3;
    if ((u_long)(raceType - 2) < 2) {
        if (!(ownShip < 12 && remoteShip < 12)) bigGrid = 1;
    } else if (ownShip >= 12) {
        bigGrid = 1;
    }
    {
        int wantedSection = startLinePos[options->actTrackNum];
    find_section:
        if (section->secNo == wantedSection) goto found_section;
        section++;
        goto find_section;
    found_section:;
    }
    if (PortOriginalTrack() >= 0) {
        /* Port: a guest track has boxes painted on its grid, where its own game put its ships: the original
           Wipeout's eight from four sections behind the line, Wipeout 64's fifteen from three; then by turns two
           and one section further back, on the track's two faces by turns (the original's first on the second
           face, Wipeout 64's on the first). XL's longer grid goes on behind them the same way. */
        int from64 = PortOriginalTrackIs64();

        for (i = 0; i < (from64 ? 3 : 4); i++) section = section->prevSection.ptr;
        side = !from64;
        for (i = 0; i < 15; i++) {
            locals.starts[i] = section;
            section = section->prevSection.ptr;
            if (!(i & 1)) section = section->prevSection.ptr;
            locals.side[i] = side;
            side ^= 1;
        }
    } else {
        section = section->prevSection.ptr;
        side = 0;
        for (i = 0; i < 15; i++) {
            locals.starts[i] = section;
            section = section->prevSection.ptr;
            section = section->prevSection.ptr;
            locals.side[i] = side;
            side ^= 1;
        }
    }
    if (gameType != 2) {
        if (raceType == 3) {
            slot = 0;
        } else if (raceType == 2) {
            slot = 10;
            if (bigGrid) slot = 13;
        }
        locals.starts[slot + 1] = locals.starts[slot];
    }
    if (raceType == 1) {
        gridPosition[ghostShip] = 0;
        gridPosition[ownShip] = 0;
    } else if (raceType == 3) {
        if (gameType == 1) {
            gridPosition[ownShip] = 1;
            gridPosition[remoteShip] = 0;
        } else {
            gridPosition[ownShip] = 0;
            gridPosition[remoteShip] = 1;
        }
    } else {
        groups = 4;
        if (bigGrid) groups = 5;
        for (i = 0; i < 3; i++) {
            locals.order[i * groups] = 2 - i;
            locals.order[i * groups + 1] = 5 - i;
            locals.order[i * groups + 2] = 8 - i;
            locals.order[i * groups + 3] = 11 - i;
            locals.order[i * groups + 4] = 14 - i;
        }
        next = 0;
        i = 0;
        if (groups * 3 != 0) do {
            int orderIndex = locals.order[i];
            if (orderIndex != ownShip && orderIndex != remoteShip) {
                gridPosition[orderIndex] = next;
                next++;
            }
            i++;
        } while (i < groups * 3);
        if (gameType == 2) {
            gridPosition[ownShip] = (int)next;
            next = (int)next + 1;
        } else {
            if (gameType == 0) {
                gridPosition[ownShip] = next;
                next++;
                gridPosition[remoteShip] = next;
                next++;
            } else {
                gridPosition[remoteShip] = next;
                next++;
                gridPosition[ownShip] = next;
                next++;
            }
        }
        if (!bigGrid) {
            gridPosition[12 + 1] = next + 1;
            gridPosition[12 + 0] = next;
            gridPosition[12 + 2] = next + 2;
        }
    }
    for (i = 0; i < 15; i++) launch[i] = gridPosition[i];
    if (PortOriginalEightShips()) {
        /* Port: on a track of the original Wipeout eight ships race, as in the original: two of each team (the
           player and one other in the player's), on the grid's eight boxes, the player in the last, as the
           original's player started last. The other ships sit the race out. XL's grid order is its ships'
           order of strength (each team's third pilot the fastest, then its second, then its first), and its
           ships are picked to spread evenly over the eleven the player would race in XL rather than be its
           fastest seven. Each keeps the launch (numToSpeed) of its place in XL's grid. */
        short field[12], count = 0, teamShips[4], best = -1, racing = 0, out = 8;
        int pick, error, bestError = 0, k, shift;

        for (i = 0; i < groups * 3; i++) {
            index = locals.order[i];
            if (index < 12 && index != ownShip) field[count++] = index;
        }
        for (pick = 0; pick < 1 << count; pick++) {
            for (j = 0; j < 4; j++) teamShips[j] = ownShip / 3 == j;
            error = 0;
            k = 0;
            for (j = 0; j < count; j++) {
                if (!(pick >> j & 1)) continue;
                teamShips[field[j] / 3]++;
                shift = j * 6 - k++ * (count - 1); /* (its place - the even spread's) * 6 */
                error += shift * shift;
            }
            if (k != 7 || teamShips[0] > 2 || teamShips[1] > 2 || teamShips[2] > 2 || teamShips[3] > 2) continue;
            if (best < 0 || error < bestError) {
                best = pick;
                bestError = error;
            }
        }
        for (i = 0; i < 15; i++) gridPosition[i] = -1;
        for (j = 0; j < count; j++) {
            if (best >> j & 1) gridPosition[field[j]] = racing++;
        }
        gridPosition[ownShip] = 7;
        for (i = 0; i < 15; i++) {
            if (gridPosition[i] < 0) gridPosition[i] = out++;
        }
    }
    for (i = 0; i < 15; i++) {
        position = gridPosition[i];
        shipIndex[i].attr = 0x248;
        shipIndex[i].vpivot.vx = 0;
        shipIndex[i].vpivot.vy = 0;
        shipIndex[i].vpivot.vz = 0;
        shipIndex[i].apivot.vx = 0;
        shipIndex[i].apivot.vy = 0;
        shipIndex[i].apivot.vz = 0;
        shipIndex[i].hdg = 0;
        shipIndex[i].pitch = 0;
        shipIndex[i].roll = 0;
        shipIndex[i].vhdg = 0;
        shipIndex[i].vpitch = 0;
        shipIndex[i].vroll = 0;
        shipIndex[i].ahdg = 0;
        shipIndex[i].r_brake_rot = 0;
        shipIndex[i].l_brake_rot = 0;
        shipIndex[i].speed = 0;
        shipIndex[i].shipInitializationValue = 0;
        shipIndex[i].thrust = 0;
        shipIndex[i].thrustCount = 0;
        shipIndex[i].rotationCorrectionDisabled = 0;
        shipIndex[i].weapon = 0;
        shipIndex[i].lastWeapon = 0;
        shipIndex[i].targetShip = -1;
        shipIndex[i].rearTarget = -1;
        shipIndex[i].electroCount = 0;
        shipIndex[i].autopilotCount = 0;
        shipIndex[i].stallCount = 0;
        shipIndex[i].lap = 0;
        shipIndex[i].reverseLap = 0;
        for (j = 0; j < 5; j++) shipIndex[i].lapTimes[j] = 0;
        shipIndex[i].updateCount = START_COUNTDOWN_FRAMES;
        if (i == ownShip) {
            shipIndex[i].combatAttr = 0;
        } else if (i == remoteShip) {
            shipIndex[i].combatAttr = 2;
        } else if (raceType == 1) {
            if (i != ghostShip) shipIndex[i].combatAttr |= 0x8000;
        } else if (raceType == 3) {
            shipIndex[i].combatAttr |= 0x8000;
        } else {
            shipIndex[i].combatAttr = locals.states.state[i];
            if (!bigGrid && i >= 12) shipIndex[i].combatAttr |= 0x8000;
            if (PortOriginalEightShips() && position >= 8) shipIndex[i].combatAttr |= 0x8000; /* Port */
        }
        switch (options->shipClass) {
        case 0: shipIndex[i].mass = shipMassClass0[i]; shipIndex[i].maxThrust = shipMaxThrustClass0[i]; shipIndex[i].skid = shipSkidClass0[i]; shipIndex[i].resistance = shipResistanceClass0[i]; break;
        case 1: shipIndex[i].mass = shipMassClass1[i]; shipIndex[i].maxThrust = shipMaxThrustClass1[i]; shipIndex[i].skid = shipSkidClass1[i]; shipIndex[i].resistance = shipResistanceClass1[i]; break;
        case 2: shipIndex[i].mass = shipMassClass2[i]; shipIndex[i].maxThrust = shipMaxThrustClass2[i]; shipIndex[i].skid = shipSkidClass2[i]; shipIndex[i].resistance = shipResistanceClass2[i]; break;
        case 3: shipIndex[i].mass = shipMassClass3[i]; shipIndex[i].maxThrust = shipMaxThrustClass3[i]; shipIndex[i].skid = shipSkidClass3[i]; shipIndex[i].resistance = shipResistanceClass3[i]; break;
        }
        switch (options->shipClass) {
        case 0: shipIndex[i].headingInc = SHIP_RATE(shipHeadingIncClass0[i]); shipIndex[i].maxHeading = SHIP_RATE(shipMaxHeadingClass0[i]); break;
        case 1: shipIndex[i].headingInc = SHIP_RATE(shipHeadingIncClass1[i]); shipIndex[i].maxHeading = SHIP_RATE(shipMaxHeadingClass1[i]); break;
        case 2: shipIndex[i].headingInc = SHIP_RATE(shipHeadingIncClass2[i]); shipIndex[i].maxHeading = SHIP_RATE(shipMaxHeadingClass2[i]); break;
        case 3: shipIndex[i].headingInc = SHIP_RATE(shipHeadingIncClass3[i]); shipIndex[i].maxHeading = SHIP_RATE(shipMaxHeadingClass3[i]); break;
        }
        if (i == ownShip || i == remoteShip || (i == ghostShip && raceType == 1)) {
            shipIndex[i].update = UpdatePlayerShipStart;
        } else {
            shipIndex[i].update = UpdateDroneShipStart;
        }
        switch (options->shipClass) {
        case 0: shipIndex[i].remoteMax = shipRemoteMaxClass0[i]; shipIndex[i].remoteMagnitude = shipRemoteMagClass0[i]; break;
        case 1: shipIndex[i].remoteMax = shipRemoteMaxClass1[i]; shipIndex[i].remoteMagnitude = shipRemoteMagClass1[i]; break;
        case 2: shipIndex[i].remoteMax = shipRemoteMaxClass2[i]; shipIndex[i].remoteMagnitude = shipRemoteMagClass2[i]; break;
        case 3: shipIndex[i].remoteMax = shipRemoteMaxClass3[i]; shipIndex[i].remoteMagnitude = shipRemoteMagClass3[i]; break;
        }
        if (launch[i] < 2) shipIndex[i].remoteMax += options->shipClass * 30;
        SetSpread(shipIndex, options, launch[i], i, bigGrid);
        shipIndex[i].nearTrkSect = locals.starts[(short)position];
        shipIndex[i].prevShipSect = locals.starts[(short)position];
        face = track->faces + locals.starts[(short)position]->faceStart;
        do {
            flags = face->flags;
            face++;
        } while (!(flags & 1));
        face--;
        if (locals.side[(short)position]) face++;
        locals.point.vx = (track->vertices[face->vertex[0]].vx + track->vertices[face->vertex[2]].vx) >> 1;
        locals.point.vy = (track->vertices[face->vertex[0]].vy + track->vertices[face->vertex[2]].vy) >> 1;
        locals.point.vz = (track->vertices[face->vertex[0]].vz + track->vertices[face->vertex[2]].vz) >> 1;
        ship = &shipIndex[i];
        ship->ppivot.vx = locals.point.vx + ((face->normal.vx * 75) >> 10);
        ship->ppivot.vy = locals.point.vy + ((face->normal.vy * 75) >> 10);
        ship->ppivot.vz = locals.point.vz + ((face->normal.vz * 75) >> 10);
        sectionNum = locals.starts[(short)position]->secNo;
        ship->nearSecNo = sectionNum;
        ship->prevSecNo = sectionNum;
        ship->distance = (short)raceLines[(short)sectionNum].distance;
        ship->trackDistance = ship->distance;
        aheadSection = locals.starts[(short)position]->nextSection.ptr;
        locals.delta.vx = aheadSection->centre.vx - locals.starts[(short)position]->centre.vx;
        locals.delta.vy = aheadSection->centre.vy - locals.starts[(short)position]->centre.vy;
        locals.delta.vz = aheadSection->centre.vz - locals.starts[(short)position]->centre.vz;
        ship->hdg = -ratan2(locals.delta.vx, locals.delta.vz);
    }
    activeShipCount = 0;
    i = 0;
    do {
        if (!(shipIndex[i].combatAttr & 0x8000)) {
            short distance = raceLines[shipIndex[i].nearSecNo].distance;
            short nextCount;
            index = activeShipCount;
            nextCount = index + 1;
            shipRank[index].shipNo = i;
            activeShipCount = nextCount;
            shipRank[index].position = distance;
        }
        i++;
    } while (i < 15);
    finishPosition = gridPosition[ownShip] + 1;
}


void SetSpread(ShipData *shipIndex, ConfigData *snd, int vol, short idx, short a4)
{

    vol = (a4 ? 15 : 12) - vol;
    switch (snd->shipClass) {
    case 0:
        shipIndex[idx].numToSpeed = vol * spreadScale0[snd->trackNum];
        break;
    case 1:
        shipIndex[idx].numToSpeed = vol * spreadScale1[snd->trackNum];
        break;
    case 2:
        shipIndex[idx].numToSpeed = vol * spreadScale2[snd->trackNum];
        break;
    case 3:
        shipIndex[idx].numToSpeed = vol * spreadScale3[snd->trackNum];
        break;
    }
    shipIndex[idx].numToSpeed = PAL_TIME(shipIndex[idx].numToSpeed);
    if (idx == ownShip || idx == remoteShip) {
        shipIndex[idx].numToSpeed = 0;
    }
}


const ShipStartStates startStateTable = {
    {0x10, 0x20, 0x40, 0x10, 0x20, 0x40, 0x10, 0x20, 0x40, 0x10, 0x20, 0x40, 0x10, 0x20, 0x40, 0x10}
};
