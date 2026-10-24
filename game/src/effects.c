#include "effects.h"
#include "pc/pc.h"
#include "weapon.h"

/* Globals shared with other modules. */
extern short startBoomCount;
extern char nameStartBoom[];
extern char msgTooManyStartBooms[];
extern char nameRedLight[];
extern char nameLight[];
extern TrackSection *lightSections[];
extern Texture *textureTable[];
extern short CurrentScreen;
extern u_short waterfallBuf[16];
extern u_long commonPath[];

extern char trainCmpExt[];
extern char trainPrmExt[];
extern TrackSection *droidStartSections[];
extern TrackSection *droidPickSection;
extern short startLinePos[];
extern SideTable droidSideTable;
extern SideTable droidPickTable;
extern int ownShip;
extern int raceType;
extern RaceLine raceLines[];
extern ConfigData *gameData;
extern TrackData *track;
extern int zeppelinBaseX;
extern int zeppelinBaseY;
extern int zeppelinBaseZ;
extern Object *lightGroups[];
extern int startCountdown;
extern int remoteShip;
extern int lightUpdate;

/* Module-private state. */
extern Object *startBoomObjs[4];
extern int trainLightSeed;
extern int neonSignTick;
extern int redBullTick;
extern int redBullFrame;
extern int spriteTick;
extern int spriteFrame;
extern short lightPhase[2];
extern short advertAngle[4];
extern short radarAngle;
extern int lightUpdateTick;
extern int lightChaseStep;
extern short monoRailSettle;
extern int zeppelinDrift;
extern int shakeAmount;
extern int droidBlinkTick;
extern TrackSection *startDroidSection;
#include "functions.h"
#include "include_asm_g8.h"
#include "../../platform/psx/gte.h" /* Port: precise positions for the shadow's grid */

/* $gp-relative globals are tentative definitions; the rest are extern. */
extern u_char   pitCycleFlag;

void AddEffects(WeaponData *weapons, ShipData *ships)
{
    int                i;
    ShipData          *shipEnd;
    register ShipData *ship;
    register WeaponData *weapon;

    ship = ships;
    shipEnd = ships + 15;
ship_loop:
    if ((ship->attr & ELECTROED) && !(ship->combatAttr & DESTROYED)) {
        i = 0;
        do {
            weapon = (WeaponData *)&ship->ppivot;
            AddElectric((VECTOR *)weapon, 0);
            AddElectric((VECTOR *)weapon, 1);
            i += 1;
        } while (i < 2);
    }
    ship += 1;
    if (ship < shipEnd) goto ship_loop;

    weapon = weapons;
weapon_loop:
    if (weapon->type == 5 && weapon->update == UpdateElecBolt && weapon->count > 0) {
        AddEboltParticles(particleSlots, 8, &weapon->pos);
        i = 0;
        do {
            AddElectric(&weapon->pos, 0);
            AddElectric(&weapon->pos, 1);
            i += 1;
        } while (i < 3);
    }
    if (weapon->type == 2 && weapon->update == UpdateMissile && weapon->count > 0) {
        DrawMissileFlare(weapon->pos.vx, weapon->pos.vy, weapon->pos.vz);
    }
    if (weapon->type == 1 && weapon->update == UpdateRocket && weapon->count > 0) {
        DrawRocketFlare(weapon->pos.vx, weapon->pos.vy, weapon->pos.vz);
    }
    if (weapon->count == -1 || weapon->count == -2) {
        if (weapon->count == -1) {
            AddRing(weapon->pos.vx, weapon->pos.vy, weapon->pos.vz, 12);
        }
        if (weapon->type == 0xF) {
            AddMinigunExplode(weapon->pos.vx, weapon->pos.vy, weapon->pos.vz, -1);
        } else {
            AddExplode(weapon->pos.vx, weapon->pos.vy, weapon->pos.vz, -1);
        }
        weapon->count = 0;
    }
    weapon += 1;
    if (weapon < weapons + 30) goto weapon_loop;
}


void ColourStartBoom(int noToLight)
{
    int     i, j;
    char    r, g, b;
    Poly    poly;
    Object *obj;

    for (i = 0; i < startBoomCount; i++) {
        obj = startBoomObjs[i];
        poly.prim = obj->primitives;
        if (noToLight > 1) {
            for (j = 1; j < noToLight; j++) {
                poly.gt4 += 1;
            }
        }
        if (noToLight == 1) {
            r = 0xFF;
            g = 0;
            b = 0;
        } else if (noToLight == 2) {
            r = 0xFF;
            g = 0x80;
            b = 0;
        } else if (noToLight == 3) {
            r = 0;
            g = 0xFF;
            b = 0;
        }
        poly.gt4->colour[0].r = r;
        poly.gt4->colour[0].g = g;
        poly.gt4->colour[0].b = b;
        poly.gt4->colour[1].r = r;
        poly.gt4->colour[1].g = g;
        poly.gt4->colour[1].b = b;
        poly.gt4->colour[2].r = r;
        poly.gt4->colour[2].g = g;
        poly.gt4->colour[2].b = b;
        poly.gt4->colour[3].r = r;
        poly.gt4->colour[3].g = g;
        poly.gt4->colour[3].b = b;
    }
}


void ResetStartBoom(void)
{
    int     i, j;
    Object *obj;
    GT4    *poly;

    for (i = 0; i < startBoomCount; i++) {
        obj = startBoomObjs[i];
        poly = (GT4 *)obj->primitives;
        for (j = 0; j < 3; j++) {
            poly->colour[0].r = 0x28; poly->colour[0].g = 0x28; poly->colour[0].b = 0x28;
            poly->colour[1].r = 0x28; poly->colour[1].g = 0x28; poly->colour[1].b = 0x28;
            poly->colour[2].r = 0x28; poly->colour[2].g = 0x28; poly->colour[2].b = 0x28;
            poly->colour[3].r = 0x28; poly->colour[3].g = 0x28; poly->colour[3].b = 0x28;
            poly++;
        }
    }
}


void InitStartBoom(Object **prm, short prmCount)
{
    int     i;
    Object *obj;

    startBoomCount = 0;
    for (i = 0; i < prmCount; i++) {
        obj = prm[i];
        while (obj) {
            if (!strncmp(obj->name, nameStartBoom, 5)) {
                startBoomObjs[startBoomCount] = obj;
                startBoomCount++;
            }
            if (startBoomCount > 4) {
                Error(msgTooManyStartBooms, 2);
            }
            obj = obj->next;
        }
    }
}


void InitRedLight(Object **prm, short prmCount, Object **redLight)
{
    int     i;
    Object *obj;
    GT4    *poly;

    for (i = 0; i < prmCount; i++) {
        obj = prm[i];
        while (obj) {
            if (!strncmp(obj->name, nameRedLight, 3)) {
                *redLight = obj;
            }
            obj = obj->next;
        }
    }
    poly = (GT4 *)(*redLight)->primitives;
    poly->colour[0].g = 0;
    poly->colour[1].g = 0;
    poly->colour[2].g = 0;
    poly->colour[3].g = 0;
    poly->colour[0].b = 0;
    poly->colour[1].b = 0;
    poly->colour[2].b = 0;
    poly->colour[3].b = 0;
}


void InitTrainLights(Object **unused0, short unused1, Object **objects, int mode)
{
    Primitive *prim;
    short kind;
    TrainLight *light;
    int colour;
    int darkColour;
    int lastColour;
    int kindLight;
    int kindGlow;
    int white;
    int i;
    int j;
    Object **objp;

    colour = 0;
    j = 0;
    kindLight = 5;
    kindGlow = 7;
    white = 255;
    objp = objects;
    do {
        prim = (*objp)->primitives;
        i = 0;
        lastColour = colour;
        darkColour = colour - 0x32;
        light = (TrainLight *)prim;
        for (;;) {
            kind = prim->type;
            if (kind == kindLight) goto L_case5;
            if (kind == kindGlow) goto L_case7;
            goto L_inc;
L_case5:
            light->c1[0] = white;
            light->c1[1] = white;
            light->c1[2] = white;
            light->c2[0] = white;
            light->c2[1] = white;
            light->c2[2] = white;
            light->c3[0] = white;
            light->c3[1] = white;
            light->c3[2] = white;
            light = (TrainLight *)((u_char *)light + 0x18);
            prim = (Primitive *)((u_char *)prim + 0x18);
            goto L_inc;
L_case7:
            light->c1[0] = colour;
            light->c1[1] = colour;
            light->c1[2] = colour;
            light->c2[0] = darkColour;
            light->c2[1] = darkColour;
            light->c2[2] = darkColour;
            light->c3[0] = darkColour;
            light->c3[1] = darkColour;
            light->c3[2] = darkColour;
            light->c4[0] = lastColour;
            light->c4[1] = lastColour;
            light->c4[2] = lastColour;
            light = (TrainLight *)((u_char *)light + 0x1C);
            prim = (Primitive *)((u_char *)prim + 0x1C);
L_inc:
            i++;
            if (i < 12) {
                continue;
            }
            break;
        }
        if (mode == -1) {
            trainLightSeed = trainLightSeed + newRand() % 5;
            if (trainLightSeed >= 0x81) {
                trainLightSeed = 0;
            }
        }
        j++;
        colour = white - trainLightSeed;
        objp++;
    } while (j < 6);
}


void DoNeonSign(Object **objects, int count)
{
    int        i;
    Primitive *prim;
    GT4       *gt4;

    count = (short)count + 7;
    i = 7;
    if (count > i) {
        do {
next:
            prim = objects[i]->primitives;
            if (prim->type != 8) {
                i++;
                if (i < count) {
                    goto next;
                }
                break;
            }
            gt4 = (GT4 *)prim;
            if ((neonSignTick % 30) < 0xF) {
                gt4->colour[0].r = 0xFF; gt4->colour[0].g = 0xFF; gt4->colour[0].b = 0xFF;
                gt4->colour[1].r = 0xFF; gt4->colour[1].g = 0xFF; gt4->colour[1].b = 0xFF;
                gt4->colour[2].r = 0xFF; gt4->colour[2].g = 0xFF; gt4->colour[2].b = 0xFF;
                gt4->colour[3].r = 0xFF; gt4->colour[3].g = 0xFF; gt4->colour[3].b = 0xFF;
            } else {
                gt4->colour[0].r = 0x80; gt4->colour[0].g = 0x80; gt4->colour[0].b = 0x80;
                gt4->colour[1].r = 0x80; gt4->colour[1].g = 0x80; gt4->colour[1].b = 0x80;
                gt4->colour[2].r = 0x80; gt4->colour[2].g = 0x80; gt4->colour[2].b = 0x80;
                gt4->colour[3].r = 0x80; gt4->colour[3].g = 0x80; gt4->colour[3].b = 0x80;
            }
            i += 1;
        } while (i < count);
    }
    neonSignTick += 1;
}


void AnimateTexture(Object **objects, short objectCount, short period, short firstTexture, short textureCount, CVECTOR colour, int *frame, int *texCounter, short texStep)
{
    int lowerBound;
    int curFrame;
    int texPos;
    int i;
    int count;
    Object **objp;
    Texture *tex;
    Primitive *prim;
    TexFields *cursor;
    short primType;
    int k;

    curFrame = *frame;
    if ((curFrame % period) == 0) {
        *texCounter += (short)texStep;
    }
    texPos = *texCounter;
    tex = textureTable[firstTexture + (short)(texPos % (short)textureCount)];
    i = 0;
    lowerBound = 0;
    if (objectCount > lowerBound) {
        count = objectCount;
        objp = objects;
        do {
            ((PrimHeader *)(*objp)->primitives)->flag |= 4;
            prim = (*objp)->primitives;
            k = 0;
            cursor = (TexFields *)&((GT4 *)prim)->tsb;
inner:
            primType = prim->type;
            if (primType != TypeFT4) {
                if (primType == TypeGT4) {
                    cursor->u0 = tex->u0;
                    cursor->v0 = tex->v0;
                    cursor->u1 = tex->u1;
                    cursor->v1 = tex->v1;
                    cursor->u2 = tex->u2;
                    cursor->v2 = tex->v2;
                    cursor->u3 = tex->u3;
                    cursor->v3 = tex->v3;
                    cursor->tsb = tex->tsb;
                    ((short *)cursor)[-1] = tex->cba;
                    cursor->colour[0].r = colour.r;
                    cursor->colour[0].g = colour.g;
                    cursor->colour[0].b = colour.b;
                    cursor->colour[1].r = colour.r;
                    cursor->colour[1].g = colour.g;
                    cursor->colour[1].b = colour.b;
                    cursor->colour[2].r = colour.r;
                    cursor->colour[2].g = colour.g;
                    cursor->colour[2].b = colour.b;
                    cursor->colour[3].r = colour.r;
                    cursor->colour[3].g = colour.g;
                    prim = (Primitive *)((char *)prim + sizeof(GT4));
                    cursor->colour[3].b = colour.b;
                    cursor = (TexFields *)((char *)cursor + sizeof(GT4));
                }
            } else {
                cursor->u0 = tex->u0;
                cursor->v0 = tex->v0;
                cursor->u1 = tex->u1;
                cursor->v1 = tex->v1;
                cursor->u2 = tex->u2;
                cursor->v2 = tex->v2;
                cursor->u3 = tex->u3;
                cursor->v3 = tex->v3;
                ((short *)cursor)[-1] = tex->cba;
                cursor->tsb = tex->tsb;
                prim = (Primitive *)((char *)prim + sizeof(FT4));
                cursor = (TexFields *)((char *)cursor + sizeof(FT4));
            }
            k += 1;
            if (k <= 0) {
                goto inner;
            }
            i += 1;
            objp += 1;
        } while (i < count);
    }
    *frame += 1;
}


void RedBullAnimate(Object **objects, short objectCount, short period, short firstTexture, short textureCount)
{
    int lowerBound;

    int i;
    int remaining;
    Object **objp;
    GT4 *quads;
    Texture *tex;

    if (((int)redBullTick % period) == 0) {
        redBullFrame += 1;
    }
    tex = textureTable[firstTexture + (short)(redBullFrame % (short)textureCount)];
    i = 0;
    lowerBound = 0;
    if (objectCount > lowerBound) {
        remaining = objectCount;
        objp = objects;
        do {
            quads = (GT4 *)(*objp)->primitives;
            if (quads->type == TypeGT4) {
                quads[0].u0 = tex->u0;
                quads[0].v0 = tex->v0;
                quads[0].u1 = (u_char)tex->u1 - tex->textureW * 2;
                quads[0].v1 = tex->v1;
                quads[0].u2 = tex->u2;
                quads[0].v2 = (u_char)tex->v2 - ((int)((u_short)tex->textureH << 16) >> 17);
                quads[0].u3 = (u_char)tex->u3 - tex->textureW * 2;
                quads[0].v3 = (u_char)tex->v3 - ((int)((u_short)tex->textureH << 16) >> 17);
                quads[0].tsb = tex->tsb;
                quads[0].cba = tex->cba;
                quads[1].u0 = (u_char)tex->u0 + tex->textureW * 2;
                quads[1].v0 = tex->v0;
                quads[1].u1 = tex->u1;
                quads[1].v1 = tex->v1;
                quads[1].u2 = (u_char)tex->u2 + tex->textureW * 2;
                quads[1].v2 = (u_char)tex->v2 - ((int)((u_short)tex->textureH << 16) >> 17);
                quads[1].u3 = tex->u3;
                quads[1].v3 = (u_char)tex->v3 - ((int)((u_short)tex->textureH << 16) >> 17);
                quads[1].tsb = tex->tsb;
                quads[1].cba = tex->cba;
                quads[2].u0 = tex->u0;
                quads[2].v0 = ((int)((u_short)tex->textureH << 16) >> 17) + (u_char)tex->v0;
                quads[2].u1 = (u_char)tex->u1 - tex->textureW * 2;
                quads[2].v1 = ((int)((u_short)tex->textureH << 16) >> 17) + (u_char)tex->v1;
                quads[2].u2 = tex->u2;
                quads[2].v2 = tex->v2;
                quads[2].u3 = (u_char)tex->u3 - tex->textureW * 2;
                quads[2].v3 = tex->v3;
                quads[2].tsb = tex->tsb;
                quads[2].cba = tex->cba;
                quads[3].u0 = (u_char)tex->u0 + tex->textureW * 2;
                quads[3].v0 = ((int)((u_short)tex->textureH << 16) >> 17) + (u_char)tex->v0;
                quads[3].u1 = tex->u1;
                quads[3].v1 = ((int)((u_short)tex->textureH << 16) >> 17) + (u_char)tex->v1;
                quads[3].u2 = (u_char)tex->u2 + tex->textureW * 2;
                quads[3].v2 = tex->v2;
                quads[3].u3 = tex->u3;
                quads[3].v3 = tex->v3;
                quads[3].tsb = tex->tsb;
                quads[3].cba = tex->cba;
            }
            i += 1;
            objp += 1;
        } while (i < remaining);
    }
    redBullTick += 1;
}



void AnimateSprite(Object **objects, short count, short speed, int first, short frames)
{
    volatile int sp[2];
    int     firstFrame;
    int     frame;
    int     index;
    int     primitiveIndex;
    int     type;
    short   limit;
    Object **object;
    SPR    *primitive;

    firstFrame = first;
    if (spriteTick % speed == 0) {
        spriteFrame++;
    }
    limit = count;
    first = spriteFrame % frames;
    index = 0;
    if (limit > 0) {
        frame = firstFrame + first;
        object = objects;
        do {
            primitiveIndex = 0;
            primitive = (SPR *)(*object)->primitives;
            do {
                type = primitive->type;
                if (type < TypeLSF3) {
                    if (type >= TypeTSPR) {
                        primitive->texture = frame;
                        primitive++;
                    }
                }
                primitiveIndex++;
            } while (primitiveIndex <= 0);
            index++;
            object++;
        } while (index < limit);
    }
    spriteTick++;
}


/* Sets the video panel quads for the current screen, depending on the track. */
void Video(Object **objects, int count)
{
    int lowerBound;
    u_short colour;

    Rect rectangles[2];

    short *prim;
    short hdr;
    int k;
    int i;
    Object **objp;
    void *cursor;

    objp = objects;
    rectangles[0] .w = 0xFF;
    rectangles[0] .h = SCREEN_HEIGHT - 1;
    if (CurrentScreen == 0) {
        rectangles[0] .x = 0;
        rectangles[0] .y = 0x100;
    } else {
        rectangles[0] .x = 0;
        rectangles[0] .y = 0;
    }
    lowerBound = 0;
    if ((count << 0x10) > lowerBound) {
        i = 0;
        do {
            prim = (short *)(*objp)->primitives;
            k = 0;
            cursor = (u_char *)prim + 0x10;
loop_6:
            hdr = *prim;
            if (hdr != 4) {
                if (hdr == 8) {
                    if (tempGameData->actTrackNum  == 6) {
                        (*(  signed char * )((signed char *)( cursor ) + (  2 )))  = 0;
                        (*(  signed char * )((signed char *)( cursor ) + (  3 )))  = 0;
                        colour = rectangles[0] .w;
                        (*(  signed char * )((signed char *)( cursor ) + (  5 )))  = 0;
                        (*(  signed char * )((signed char *)( cursor ) + (  6 )))  = 0;
                        (*(  signed char * )((signed char *)( cursor ) + (  4 )))  = (signed char) colour;
                        (*(  signed char * )((signed char *)( cursor ) + (  7 )))  = (signed char) rectangles[0] .h;
                        (*(  signed char * )((signed char *)( cursor ) + (  8 )))  = (signed char) rectangles[0] .w;
                        (*(  signed char * )((signed char *)( cursor ) + (  9 )))  = (signed char) rectangles[0] .h;
                    }
                    if (tempGameData->actTrackNum  == 7) {
                        (*(  signed char * )((signed char *)( cursor ) + (  2 )))  = 0;
                        colour = rectangles[0] .h;
                        (*(  signed char * )((signed char *)( cursor ) + (  4 )))  = 0;
                        (*(  signed char * )((signed char *)( cursor ) + (  5 )))  = 0;
                        (*(  signed char * )((signed char *)( cursor ) + (  3 )))  = (signed char) colour;
                        (*(  signed char * )((signed char *)( cursor ) + (  6 )))  = (signed char) rectangles[0] .w;
                        (*(  signed char * )((signed char *)( cursor ) + (  7 )))  = (signed char) rectangles[0] .h;
                        colour = rectangles[0] .w;
                        (*(  signed char * )((signed char *)( cursor ) + (  9 )))  = 0;
                        (*(  signed char * )((signed char *)( cursor ) + (  8 )))  = (signed char) colour;
                    }
                    (*(  short * )((signed char *)( cursor ) + (  -2 )))  = 0;
                    prim += 22;
                    (*(  short * )((signed char *)( cursor ) + (  0 )))  = (short) ((((int) (rectangles[0] .x << 0x10) >> 0x16) + ((rectangles[0] .y >> 4) & 0x10)) | 0x120);
                    cursor += 0x2C;
                }
            } else {
                if (tempGameData->actTrackNum  == 6) {
                    (*(  signed char * )((signed char *)( cursor ) + (  2 )))  = 0;
                    (*(  signed char * )((signed char *)( cursor ) + (  3 )))  = 0;
                    colour = rectangles[0] .w;
                    (*(  signed char * )((signed char *)( cursor ) + (  5 )))  = 0;
                    (*(  signed char * )((signed char *)( cursor ) + (  6 )))  = 0;
                    (*(  signed char * )((signed char *)( cursor ) + (  4 )))  = (signed char) colour;
                    (*(  signed char * )((signed char *)( cursor ) + (  7 )))  = (signed char) rectangles[0] .h;
                    (*(  signed char * )((signed char *)( cursor ) + (  8 )))  = (signed char) rectangles[0] .w;
                    (*(  signed char * )((signed char *)( cursor ) + (  9 )))  = (signed char) rectangles[0] .h;
                }
                if (tempGameData->actTrackNum  == 7) {
                    (*(  signed char * )((signed char *)( cursor ) + (  2 )))  = 0;
                    colour = rectangles[0] .h;
                    (*(  signed char * )((signed char *)( cursor ) + (  4 )))  = 0;
                    (*(  signed char * )((signed char *)( cursor ) + (  5 )))  = 0;
                    (*(  signed char * )((signed char *)( cursor ) + (  3 )))  = (signed char) colour;
                    (*(  signed char * )((signed char *)( cursor ) + (  6 )))  = (signed char) rectangles[0] .w;
                    (*(  signed char * )((signed char *)( cursor ) + (  7 )))  = (signed char) rectangles[0] .h;
                    colour = rectangles[0] .w;
                    (*(  signed char * )((signed char *)( cursor ) + (  9 )))  = 0;
                    (*(  signed char * )((signed char *)( cursor ) + (  8 )))  = (signed char) colour;
                }
                (*(  short * )((signed char *)( cursor ) + (  -2 )))  = 0;
                prim += 16;
                (*(  short * )((signed char *)( cursor ) + (  0 )))  = (short) ((((int) (rectangles[0] .x << 0x10) >> 0x16) + ((rectangles[0] .y >> 4) & 0x10)) | 0x120);
                cursor += 0x20;
            }
            k += 1;
            if (k <= 0) {
                goto loop_6;
            }
            lowerBound = (short)count;
            i += 1;
            objp += 1;
        } while (i < lowerBound);
    }
}


void WaterFall(void)
{
    RECT      rect;
    int       count;
    u_short  *source;
    u_short  *destination;
    u_short   first;

    first = waterfallBuf[15];
    count = 0xE;
    destination = &waterfallBuf[15];
    source = destination - 1;
    rect.x = 0x370;
    rect.y = 0xF1;
    rect.w = 0x10;
    rect.h = 1;
    do {
        *destination = *source;
        source--;
        destination--;
        count--;
    } while (count >= 4);
    waterfallBuf[4] = first;
    LoadImage(&rect, (u_long *)waterfallBuf);
}


void InitWaterFall(void)
{
    RECT rect;

    rect.x = 0x370;
    rect.y = 0xF1;
    rect.w = 0x10;
    rect.h = 1;
    StoreImage(&rect, (u_long *)waterfallBuf);
    DrawSync(0);
}


void ColourRedLight(Object *light)
{
    short  level;
    u_short phase;
    GT4   *gt4;

    phase = lightPhase[0] + 0x8C;
    lightPhase[0] = phase;
    level = (rsin((short)phase) >> 5) + 0x80;
    if (level >= 0x100) {
        level = 0xFF;
    }
    gt4 = (GT4 *)light->primitives;
    gt4->colour[0].r = level;
    gt4->colour[1].r = level;
    gt4->colour[2].r = level;
    gt4->colour[3].r = level;
}


void ColourDroidLights(Object *droid)
{
    short  r, g, b;
    short  rf, gf, bf;
    int    i;
    Poly   poly;

    lightPhase[1] += 140;
    rf = (rsin(lightPhase[1]) >> 5) + 128;
    if (rf > 255) {
        rf = 255;
    }
    gf = (rsin(lightPhase[1] + 75) >> 5) + 128;
    if (gf > 255) {
        gf = 255;
    }
    bf = (rsin((lightPhase[1] >> 1) + 32) >> 5) + 128;
    if (bf > 255) {
        bf = 255;
    }

    poly.prim = droid->primitives;
    for (i = 0; i < 11; i++) {
        if (i < 2) {
            r = 40;
            g = gf;
            b = 40;
        } else if (i < 6) {
            r = bf >> 1;
            b = bf;
            g = bf >> 1;
        } else {
            r = rf;
            b = 40;
            g = 40;
        }
        switch (poly.prim->type) {
        case 6:
            poly.gt3->colour[0].r = r;
            poly.gt3->colour[0].g = g;
            poly.gt3->colour[0].b = b;
            poly.gt3->colour[1].r = r;
            poly.gt3->colour[1].g = g;
            poly.gt3->colour[1].b = b;
            poly.gt3->colour[2].r = r;
            poly.gt3->colour[2].g = g;
            poly.gt3->colour[2].b = b;
            poly.gt3++;
            break;
        case 8:
            poly.gt4->colour[0].r = r;
            poly.gt4->colour[0].g = g;
            poly.gt4->colour[0].b = b;
            poly.gt4->colour[1].r = r;
            poly.gt4->colour[1].g = g;
            poly.gt4->colour[1].b = b;
            poly.gt4->colour[2].r = r;
            poly.gt4->colour[2].g = g;
            poly.gt4->colour[2].b = b;
            poly.gt4->colour[3].r = 40;
            poly.gt4->colour[3].g = 40;
            poly.gt4->colour[3].b = 40;
            poly.gt4++;
            break;
        }
    }
}


void InitObject(Object **prm, short prmCount, Object **found, char *name, short len, short *count)
{
    int     i;
    Object *obj;

    *count = 0;
    for (i = 0; i < prmCount; i++) {
        obj = prm[i];
        while (obj) {
            if (!strncmp(obj->name, name, len)) {
                found[*count] = obj;
                (*count)++;
            }
            obj = obj->next;
        }
    }
}


void MoveAdvert(Object **objects, int count)
{
    int      index;
    int      lowest;
    Object **object;

    index = 0;
    lowest = 0;
    if (count > lowest) {
        object = objects;
        while (index < count) {
            SetSkeletonDirection((*object)->skeleton, 0, advertAngle[0], 0);
            object++;
            index++;
        }
    }
    advertAngle[0] = ((u_short)advertAngle[0]) + 0x14;
}


void MoveFan(Object **objects, int count)
{
    int      lowest;
    int      index;
    Object **object;

    lowest = 0;
    index = 0;
    if (count > lowest) {
        object = objects;
        while (index < count) {
            SetSkeletonDirection((*object)->skeleton, 0, 0, advertAngle[1]);
            object++;
            index++;
        }
    }
    advertAngle[1] = ((u_short)advertAngle[1]) + 0x64;
}


void InitMonoData(Object **objects, short unused, volatile MonoRailData *mono)
{
    VECTOR  first;
    VECTOR  last;
    Object *object;
    int     dx;
    int     dy;
    int     dz;
    int     duration;

    object = objects[0];
    first.vx = object->skeleton->relative.t[0];
    first.vy = object->skeleton->relative.t[1];
    first.vz = object->skeleton->relative.t[2];
    object = objects[16];
    last.vx = object->skeleton->relative.t[0];
    last.vy = object->skeleton->relative.t[1];
    last.vz = object->skeleton->relative.t[2];

    duration = 200;
    mono->totalX = last.vx - first.vx;
    dx = mono->totalX / duration;
    mono->totalY = last.vy - first.vy;
    dy = mono->totalY / duration;
    mono->totalZ = last.vz - first.vz;
    dz = mono->totalZ / duration;
    mono->stepX = dx;
    mono->stepY = dy;
    mono->stepZ = dz;
    mono->index = 0;
}


void ResetAnimatedObjects(void)
{
    ResetMonoRail();
    ResetZeppelin();
}


void ResetMonoRail(void)
{
    int     index;
    Object *current;

    index = 0;
    do {
        current = monoRailObjs[index];
        current->skeleton->relative.t[0] = posMonoRail[index].vx;
        current->skeleton->relative.t[1] = posMonoRail[index].vy;
        current->skeleton->relative.t[2] = posMonoRail[index].vz;
        index++;
    } while (index < 6);
    monoRail.index = 0;
}


/* Puts the zeppelin back at its base position through its skeleton. */
void ResetZeppelin(void)
{
    zeppelinObj->skeleton->relative.t[0] = zeppelinBaseX;
    zeppelinObj->skeleton->relative.t[1] = zeppelinBaseY;
    zeppelinObj->skeleton->relative.t[2] = zeppelinBaseZ;
}


void LoadTrain(Object **objects, char *trackDir, Skeleton *parent, VECTOR *positions)
{
    union {
        u_long align;
        char   bytes[264];
    } path;
    int      count;
    int      i;
    Object  *obj;
    Object **p;
    VECTOR  *dst;
    int      temp;
    u_short  textureStart;

    count = 0;
    __builtin_memcpy(path.bytes, commonPath, 20);
    strcat(path.bytes, trackDir);
    strcat(path.bytes, trainCmpExt);
    textureStart = TextureTableCount;
    LoadCmpFiles(LoadCompressedTextureSequence(path.bytes));
    __builtin_memcpy(path.bytes, commonPath, 20);
    strcat(path.bytes, trackDir);
    strcat(path.bytes, trainPrmExt);
    obj = LoadPrm(path.bytes, (short)textureStart);
    *objects = obj;
    if (obj != 0) {
        p = objects;
        do {
            obj->skeleton->super = parent;
            *p = obj;
            obj = obj->next;
            count++;
            p++;
        } while (obj != 0);
    }
    i = 0;
    if (count > 0) {
        p = objects;
        do {
            dst = positions + i;
            dst->vx = (*p)->skeleton->relative.t[0];
            dst->vy = (*p)->skeleton->relative.t[1];
            temp = (*p)->skeleton->relative.t[2];
            p++;
            dst->vz = temp;
            i++;
        } while (i < count);
    }
}



void UpdateStartDroid(Object **shapes, ShipData *ships, DroidData *droids, short textureBase, ConfigData *game, int player)
{
    VECTOR direction;
    SideTable side;
    short i,selected,distance;
    int angle;
    u_short ownSection;
    int texture;
    TrackSection *section,*previous;
    Object *shape;
    GT4 *poly;
    Texture *tex;
    side = droidSideTable;
    ownSection = raceLines[ships[ownShip].nearSecNo].distance;
    for (i=0;i<6;i++) {
        if (droids[i].active>0) {
            /* Port: a checkpoint cell with a time but section -1 (the file marks the unused cells so) indexed raceLines[-1];
               the console read the 16 bytes before the table, the (never written) end of quadPool: zeros. */
            distance = ownSection - (droids[i].section < 0 ? 0 : raceLines[droids[i].section].distance);
            if (distance<0) distance += numTrackSections;
            if (distance>numTrackSections>>1) distance -= numTrackSections;
            if ((distance<0 ? -distance : distance)<161) break;
        }
    }
    selected = i;
    for (i=(selected==6);i<2;i++) {
        if (i==0 && droids[selected].active==0) i++;
        if (i==0) section=droidStartSections[selected];
        else section=droidPickSection;
        previous=section->prevSection.ptr;
        shape=shapes[i];
        direction.vx=section->centre.vx-previous->centre.vx;
        direction.vy=section->centre.vy-previous->centre.vy;
        direction.vz=section->centre.vz-previous->centre.vz;
        angle=ratan2(direction.vx,direction.vz);
        angle=4096-angle;
        SetSkeletonDirectionHPR(shape->skeleton,(short)angle,0,0);
        SetSkeletonPosition(shape->skeleton,section->centre.vx,section->centre.vy-3000,section->centre.vz);
        texture=3;
        if(i==0) {
            if(ships[ownShip].lap==0) texture=0;
            else {
                texture=2;
                if(ships[ownShip].lap==classLaps[game->shipClass] && droids[selected].section==startLinePos[game->actTrackNum]) texture=1;
            }
        }
        poly=(GT4 *)shape->primitives;
        tex=textureTable[texture+textureBase];
        switch(poly->type) {
        case 8:
            if (droidBlinkTick%20>=10) {
            poly->colour[0].r=128;poly->colour[0].g=128;poly->colour[0].b=128;
            poly->colour[1].r=128;poly->colour[1].g=128;poly->colour[1].b=128;
            poly->colour[2].r=128;poly->colour[2].g=128;poly->colour[2].b=128;
            poly->colour[3].r=128;poly->colour[3].g=128;poly->colour[3].b=128;
            } else {
            poly->colour[0].r=255;poly->colour[0].g=255;poly->colour[0].b=255;
            poly->colour[1].r=255;poly->colour[1].g=255;poly->colour[1].b=255;
            poly->colour[2].r=255;poly->colour[2].g=255;poly->colour[2].b=255;
            poly->colour[3].r=255;poly->colour[3].g=255;poly->colour[3].b=255;
            }
            if (i && side.value[game->trackNum]==0) {
                poly->u0=tex->u1;poly->v0=tex->v0;poly->u1=tex->u0;poly->v1=tex->v1;
                poly->u2=tex->u3;poly->v2=tex->v2;poly->u3=tex->u2;
            } else {
                poly->u0=tex->u0;poly->v0=tex->v0;poly->u1=tex->u1;poly->v1=tex->v1;
                poly->u2=tex->u2;poly->v2=tex->v2;poly->u3=tex->u3;
            }
            poly->v3=tex->v3;poly->cba=tex->cba;poly->tsb=tex->tsb;
            break;
        case 4:
            if (droidBlinkTick%20>=10) {
            poly->colour[0].r=128;poly->colour[0].g=128;poly->colour[0].b=128;
            } else {
            poly->colour[0].r=255;poly->colour[0].g=255;poly->colour[0].b=255;
            }
            poly->u0=tex->u0;poly->v0=tex->v0;poly->u1=tex->u1;poly->v1=tex->v1;
            poly->u2=tex->u2;poly->v2=tex->v2;poly->u3=tex->u3;poly->v3=tex->v3;
            poly->cba=tex->cba;poly->tsb=tex->tsb;
            break;
        }
        shape->skeleton->update=1;
        AsmDrawObject(shape, cameraPtr->camPos, 0); /* Port: roll was never passed; 0 stands for the console's stale a2 */
    }
    if (player==-1) droidBlinkTick++;
}


void LoadStartDroid(Object **objects, char *names, Skeleton *parent)
{
    char    buf[256];
    short   n;
    Object *o;
    Object **p;

    n = TextureTableCount;
    LoadCmpFiles((TIMlist *)WADLoadCompressedTextureSequence(textureWad));
    o = WADLoadPrm(textureWad, n);
    *objects = o;
    if (o != 0) {
        p = objects;
        do {
            o->skeleton->super = parent;
            *p++ = o;
            o = o->next;
        } while (o != 0);
    }
}


void InitDroidThings(Object **objects, char *names, Skeleton *parent, Checkpoint *points, ConfigData *options)
{
    SideTable     table;
    short         count;
    short         spacing;
    int           step;
    int           i, j;
    TrackSection *section;

    table = droidPickTable;
    count = 3;
    spacing = 7;
    step = 7;
    if (strncmp(objects[0]->name, nameLight, 5) == 0) {
        objects[1] = NewInstance(objects[0]);
        objects[2] = NewInstance(objects[0]);
        lightUpdate = 0;
        section = track->sections;
        for (i = 0; i < track->sectionCount; i++) {
            if (section->secNo == startLinePos[options->actTrackNum]) {
                startDroidSection = section;
                break;
            }
            section = section->nextSection.ptr;
        }
        if (raceType == 1 || raceType == 3 || raceType == 99) {
            for (i = 0; i < 10; i++) {
                startDroidSection = startDroidSection->nextSection.ptr;
            }
            count = 1;
        } else {
            for (i = 0; i < 3; i++) {
                startDroidSection = startDroidSection->prevSection.ptr;
            }
        }
        for (i = 0; i < count; i++) {
            section = startDroidSection;
            for (j = 0; j < spacing; j++) {
                section = section->prevSection.ptr;
            }
            lightSections[i] = section;
            spacing += step;
        }
    } else {
        for (i = 0; i < 6; i++) {
            section = track->sections;
            for (j = 0; j < track->sectionCount; j++) {
                if (section->secNo == points[i].section) {
                    droidStartSections[i] = section;
                    break;
                }
                section = section->nextSection.ptr;
            }
        }
        section = track->sections;
        for (j = 0; j < track->sectionCount; j++) {
            if (section->secNo == table.value[options->trackNum]) {
                droidPickSection = section;
                break;
            }
            section = section->nextSection.ptr;
        }
    }
}


void ResetLightStart(void)
{
    short group;
    short index;
    FT4  *p;

    lightUpdate = 0;
    for (group = 0; group < 3; group++) {
        p = (FT4 *)lightGroups[group]->primitives;
        for (index = 0; index < 8; index++) {
            if (p->type == 4) {
                p->colour.r = 128;
                p->colour.g = 128;
                p->colour.b = 128;
                p++;
            }
        }
    }
}


void UpdateLightStart(Object **objects, ShipData *ships, DroidData *droids, short textureBase, ConfigData *game, int mode)
{
    short         count = 3;
    short         i, j;
    int           white;
    VECTOR        delta;
    TrackSection *section, *next;
    int           angle;
    Skeleton     *skeleton;
    Object       *object;
    FT4          *face;

    if (raceType == 1 || raceType == 99 || raceType == 3) {
        count = 1;
    }
    for (i = 0; i < count; i++) {
        white = 255;
        section = lightSections[i];
        next = section->prevSection.ptr;
        object = objects[i];
        delta.vx = section->centre.vx - next->centre.vx;
        delta.vy = section->centre.vy - next->centre.vy;
        delta.vz = section->centre.vz - next->centre.vz;
        angle = ratan2(delta.vx, delta.vz);
        skeleton = object->skeleton;
        angle = 4096 - angle;
        SetSkeletonDirectionHPR(skeleton, (short)angle, 0, 0);
        SetSkeletonPosition(object->skeleton, section->centre.vx, section->centre.vy - 3000, section->centre.vz);
        if (mode == -1) {
            face = (FT4 *)object->primitives;
            for (j = 0; j < 8; j++) {
                if (face->type == 4) {
                    if (lightUpdate < PAL_TIME(480)) {
                        if (startCountdown >= REGION(34, 42) && startCountdown <= REGION(66, 83)) {
                            face->colour.r = white;
                            face->colour.g = 0;
                            face->colour.b = 0;
                        } else if (startCountdown >= 1 && startCountdown <= REGION(33, 41)) {
                            face->colour.r = white;
                            face->colour.g = 128;
                            face->colour.b = 0;
                        } else if (startCountdown == 0) {
                            face->colour.r = 0;
                            face->colour.g = white;
                            face->colour.b = 0;
                            lightUpdate++;
                        }
                    } else {
                        lightUpdate++;
                        if (lightUpdate >= PAL_TIME(480)) {
                            if ((short)(lightChaseStep % 8) == j) {
                                face->colour.r = 0;
                                face->colour.g = white;
                                face->colour.b = 0;
                            } else {
                                face->colour.r = 0;
                                face->colour.g = j << 4;
                                face->colour.b = 0;
                            }
                        }
                    }
                    face++;
                }
            }
        }
        object->skeleton->update = 1;
        AsmDrawObject(object, cameraPtr->camPos, 0); /* Port: roll was never passed; 0 stands for the console's stale a2 */
    }
    if (mode == -1) {
        if (!(lightUpdateTick & 1)) {
            lightChaseStep++;
        }
        lightUpdateTick++;
    }
}


void MoveMonoRail(Object **objects, short count, MonoRailData *mono, ShipData *ships, int mode)
{
    int lowerBound;
    int remaining;
    int z, y, x;
    int dx;
    int dy;
    int dz;
    TrackCamera *camera;
    Skeleton *skeleton;
    int i;
    u_char shipByte;
    Object **unusedCursor;
    Skeleton *skel;
    ShipData *shipPtr;
    Object *obj;
    shipPtr = ships + ownShip;
    if (shipPtr->nearSecNo == 0xD2) {
        shipByte = shipPtr->lap;
        if (mono->index != (signed char) shipByte) {
            mono->index = (short) (signed char) shipByte;
            monoRailSettle = 0x76C;
        }
    }
    remaining = count;
    i = 0;
    lowerBound = 0;
    count = remaining;
    if (remaining > lowerBound) {
        unusedCursor = objects;
        do {
            obj = objects[i];
            if ((mode == -1) && (monoRailSettle != 0)) {
                if (!((ships + ownShip)->lap & 1)) {
                    skel = obj->skeleton;
                    dx = mono->stepX;
                    dy = mono->stepY;
                    dz = mono->stepZ;
                    x = skel->relative.t[0];
                    y = skel->relative.t[1];
                    z = skel->relative.t[2];
                    dx = x - (-dx);
                    dy = y - (-dy);
                    dz = z - (-dz);
                } else {
                    skel = obj->skeleton;
                    dx = mono->stepX;
                    dy = mono->stepY;
                    dz = mono->stepZ;
                    x = skel->relative.t[0];
                    y = skel->relative.t[1];
                    z = skel->relative.t[2];
                    dx = x - dx;
                    dy = y - dy;
                    dz = z - dz;
                }
                SetSkeletonPosition(skel, dx, dy, dz);
                monoRailSettle = (u_short) monoRailSettle - 1;
            }
            skeleton = obj->skeleton;
            camera = cameraPtr;
            skeleton->update = 1;
            unusedCursor = objects + i + 1;
            i += 1;
            AsmDrawObject(obj, camera->camPos, 0); /* Port: roll was never passed; 0 stands for the console's stale a2 */
        } while (i < count);
    }
}


void MoveZeppelin(Object **objects, int unused, ShipData *ships)
{
    Object *zeppelin;

    zeppelin = objects[0];
    if (ships[ownShip].nearTrkSect->secNo == 0x96 && ships[ownShip].prevShipSect->secNo == 0x95) {
        zeppelinDrift = 0;
    }
    if (ships[ownShip].nearTrkSect->secNo == 0xC9) {
        ResetZeppelin();
    }
    if ((u_long)((u_short)ships[ownShip].nearTrkSect->secNo - 0x96) < 0x33U && zeppelinDrift <= 0x1869F) {
        zeppelinDrift += 0x64;
    }
    SetSkeletonPosition(zeppelin->skeleton, zeppelinBaseX, zeppelinBaseY, zeppelinBaseZ + zeppelinDrift);
}


extern int cameraDroidHeight[];

void MoveCameraDroid(Object **objects, short count, ShipData *ships, int *bobCount)
{
    int i;
    Object *obj;
    VECTOR delta;
    int heading, pitch;

    for (i = 0; i < count; i++) {
        obj = objects[i];

        delta.vx = obj->skeleton->relative.t[0] - ships[ownShip].ppivot.vx;
        delta.vy = obj->skeleton->relative.t[1] - ships[ownShip].ppivot.vy;
        delta.vz = obj->skeleton->relative.t[2] - ships[ownShip].ppivot.vz;
        heading = ratan2(delta.vx, delta.vz);
        pitch = ratan2(delta.vy, SquareRoot0(delta.vx * delta.vx + delta.vz * delta.vz));
        if (ships[ownShip].attr & 8) {
            pitch = 0x1000 - pitch;
            heading = 0x1000 - heading;
        } else {
            pitch = -pitch;
            heading = -heading;
        }
        SetSkeletonDirectionHPR(obj->skeleton, heading, pitch, 0);
        if (*bobCount == 0) {
            cameraDroidHeight[i] = obj->skeleton->relative.t[1];
        }
        SetSkeletonPosition(obj->skeleton, obj->skeleton->relative.t[0],
                            cameraDroidHeight[i] + (rsin(*bobCount) >> 6), obj->skeleton->relative.t[2]);
    }
    *bobCount += 0xC8;
}


void MoveRadar(Object **objects, short count)
{
    int index;

    for (index = 0; index < count; index++) {
        SetSkeletonDirection(objects[index]->skeleton, 0, radarAngle, 0);
    }
    radarAngle = (u_short)radarAngle + 0x50;
}


void InitShadowCluts(void)
{
    u_short colours[16];
    RECT    rect;
    int     i;
    int     white;

    white = 0x8C63;
    i = 1;
    rect.x = 400;
    rect.y = 450;
    rect.w = 16;
    rect.h = 1;
    colours[0] = 0;
    do {
        if (i & 1) {
            colours[i] = white;
        } else {
            colours[i] = 0;
        }
        i++;
    } while (i < 16);
    LoadImage(&rect, (u_long *)colours);
    DrawSync(0);
    i = 1;
    rect.y++;
    do {
        if (i & 2) {
            colours[i] = white;
        } else {
            colours[i] = 0;
        }
        i++;
    } while (i < 16);
    LoadImage(&rect, (u_long *)colours);
    DrawSync(0);
    i = 1;
    rect.y++;
    do {
        if (i & 4) {
            colours[i] = white;
        } else {
            colours[i] = 0;
        }
        i++;
    } while (i < 16);
    LoadImage(&rect, (u_long *)colours);
    DrawSync(0);
    i = 1;
    rect.y++;
    do {
        if (i & 8) {
            colours[i] = white;
        } else {
            colours[i] = 0;
        }
        i++;
    } while (i < 16);
    LoadImage(&rect, (u_long *)colours);
    DrawSync(0);
    rect.y++;
}


extern char texShadow1[];
extern char texShadow2[];
extern char texSilly1[];
extern char texSilly2[];
extern Texture *shadowTexture;

Shadow *InitShadow(void)
{
    u_char u[32];
    u_char v[32];
    Shadow *shadows;
    char *secondName;
    POLY_FT4 *poly;
    u_char *cellPtr;
    u_char *shipBase;
    u_char *rowBase;
    POLY_FT4 *rowPoly;
    int ship, shipOffset;
    int screen, screenOffset;
    int cell, cellOffset;
    int row, rowIndex;
    int col;
    int rowScale, rowOffset, gridOffset;
    int quadOffset, uvIndex;

    InitShadowCluts();
    if (sillyShips == 0) {
        shadowTexture = LoadTexture(texShadow1, 1);
        ShowLoadMessage();
        secondName = texShadow2;
    } else {
        shadowTexture = LoadTexture(texSilly1, 1);
        ShowLoadMessage();
        secondName = texSilly2;
    }
    LoadTexture(secondName, 1);
    ship = 0;
    ShowLoadMessage();
    shadows = DAlloc(heap, 15 * sizeof(Shadow) + 32);
    u[0] = shadowTexture->u0;
    u[4] = shadowTexture->u1;
    u[2] = (u[0] + u[4]) >> 1;
    u[1] = (u[0] + u[2]) >> 1;
    u[3] = (u[2] + u[4]) >> 1;
    u[5] = u[0];
    u[6] = u[1];
    u[7] = u[2];
    u[8] = u[3];
    u[9] = u[4];
    u[10] = u[0];
    u[11] = u[1];
    u[12] = u[2];
    u[13] = u[3];
    u[14] = u[4];
    u[15] = u[0];
    u[16] = u[1];
    u[17] = u[2];
    u[18] = u[3];
    u[19] = u[4];
    u[20] = u[0];
    u[21] = u[1];
    u[22] = u[2];
    u[23] = u[3];
    u[24] = u[4];
    v[0] = shadowTexture->v0;
    v[20] = shadowTexture->v2;

    shipOffset = 0;
    v[10] = (v[0] + v[20]) >> 1;
    v[5] = (v[0] + v[10]) >> 1;
    v[15] = (v[10] + v[20]) >> 1;
    v[1] = v[0];
    v[2] = v[0];
    v[3] = v[0];
    v[4] = v[0];
    v[6] = v[5];
    v[7] = v[5];
    v[8] = v[5];
    v[9] = v[5];
    v[11] = v[10];
    v[12] = v[10];
    v[13] = v[10];
    v[14] = v[10];
    v[16] = v[15];
    v[17] = v[15];
    v[18] = v[15];
    v[19] = v[15];
    v[21] = v[20];

    v[22] = v[20];
    v[23] = v[20];
    v[24] = v[20];
    do {
        screen = 0;
        screenOffset = 0;
        do {
            cell = 0;
            cellOffset = 0;
            shipBase = (u_char *)shadows + shipOffset;
            cellPtr = shipBase;
        nextCell:
            poly = (POLY_FT4 *)(cellPtr + screenOffset);
            SetPolyFT4(poly);
            poly->tpage = GetTPage(0, 2, shadowTexture->textureX, shadowTexture->textureY);
            poly->clut = GetClut(0x190, 0x1C2);
            if ((u_int)(ship - 3) < 3) {
                poly->clut = GetClut(0x190, 0x1C3);
            }
            if ((u_int)(ship - 6) < 3) {
                poly->clut = GetClut(0x190, 0x1C4);
            }
            if ((u_int)(ship - 9) < 3) {
                poly->clut = GetClut(0x190, 0x1C5);
            }
            SetShadeTex(poly, 1);
            SetSemiTrans(poly, 1);
            ((POLY_FT4 *)(shipBase + (screenOffset + cellOffset)))->r0 = 0;
            ((POLY_FT4 *)(shipBase + (screenOffset + cellOffset)))->g0 = 0;
            ((POLY_FT4 *)(shipBase + (screenOffset + cellOffset)))->b0 = 0;
            cellPtr += sizeof(POLY_FT4) * 2;
            cellOffset += sizeof(POLY_FT4) * 2;
            cell++;
            if (cell < 16) {
                goto nextCell;
            }
            row = 0;
            gridOffset = screenOffset;
            rowIndex = 0;
        nextRow:
            col = 0;
            rowScale = (row * 2) * 2;
            rowOffset = rowIndex;
            do {
                rowBase = (u_char *)shadows + shipOffset;
                quadOffset = (rowScale + col) * (sizeof(POLY_FT4) * 2);
                uvIndex = rowOffset + col;
                rowPoly = (POLY_FT4 *)(rowBase + (gridOffset + quadOffset));
                rowPoly->u0 = u[uvIndex];
                rowPoly->v0 = v[uvIndex];
                rowPoly->u1 = u[uvIndex + 1];
                rowPoly->v1 = v[uvIndex + 1];
                rowPoly->u2 = u[uvIndex + 5];
                rowPoly->v2 = v[uvIndex + 5];
                rowPoly->u3 = u[uvIndex + 6];
                rowPoly->v3 = v[uvIndex + 6];
                if (ship >= 12) {
                    rowPoly->u0 += 0x80;
                    rowPoly->u1 += 0x80;
                    rowPoly->u2 += 0x80;
                    rowPoly->u3 += 0x80;
                }
                col++;
            } while (col < 4);
            row++;
            rowIndex += 5;
            if (row < 4) {
                goto nextRow;
            }
            screen++;
            screenOffset += sizeof(POLY_FT4);
        } while (screen < 2);
        ship++;
        shipOffset += sizeof(Shadow);
    } while (ship < 15);
    return shadows;
}


void DrawShadow(void *shapes, ShipData *ships, Shadow *shadow)
{
    VECTOR a,b,c,d,facePoint;
    DVECTOR screen[4];
    int x[26];
    int y[26];
    int depth[4];
    SVECTOR vertex[4];
    int p,flag;
    int i;
    int row;
    int col;
    int alpha;
    Face *face;
    unsigned char faceFlags;
    int grid;
    int gridRow;
    int rowStart;
    int quadRow;
    int buffer;
    int bufferOffset;
    int polyIndex;
    int x0;
    Shadow *shadowQuads;
    POLY_FT4 *poly;
    ShipData *ship;
    Shadow *shadowEntry;
    uint32_t gridHandles[25]; /* Port: the precise positions of the grid points (gte.h) */
    cameraPtr->camPos->super->update=1;
    SetSkeletonTransform(cameraPtr->camPos->super);
    for(i=0;i<15;i++) {
        ship=ships+i;shadowEntry=shadow+i;
        if((raceType==3 && (i==ownShip || i==remoteShip)) || (raceType==1 && i==ownShip) || (raceType!=1 && raceType!=3)) {
            if((ship->attr & (FLYING | VISIBLE)) != VISIBLE) continue;
            if (ship->combatAttr & 0x8080) continue;
            face=track->faces+ship->nearTrkSect->faceStart;
            do { faceFlags=face->flags;face++;if(faceFlags & 1) {face--;break;} } while(1);
            a.vx=(ship->ppivot.vx+((ship->unitVecNose.vx*3)>>5))-(ship->unitVecWing.vx>>4);
            a.vy=(ship->ppivot.vy+((ship->unitVecNose.vy*3)>>5))-(ship->unitVecWing.vy>>4);
            a.vz=(ship->ppivot.vz+((ship->unitVecNose.vz*3)>>5))-(ship->unitVecWing.vz>>4);
            b.vx=(ship->ppivot.vx+((ship->unitVecNose.vx*3)>>5))+(ship->unitVecWing.vx>>4);
            b.vy=(ship->ppivot.vy+((ship->unitVecNose.vy*3)>>5))+(ship->unitVecWing.vy>>4);
            b.vz=(ship->ppivot.vz+((ship->unitVecNose.vz*3)>>5))+(ship->unitVecWing.vz>>4);
            c.vx=(ship->ppivot.vx-((ship->unitVecNose.vx*3)>>5))-(ship->unitVecWing.vx>>4);
            c.vy=(ship->ppivot.vy-((ship->unitVecNose.vy*3)>>5))-(ship->unitVecWing.vy>>4);
            c.vz=(ship->ppivot.vz-((ship->unitVecNose.vz*3)>>5))-(ship->unitVecWing.vz>>4);
            d.vx=(ship->ppivot.vx-((ship->unitVecNose.vx*3)>>5))+(ship->unitVecWing.vx>>4);
            d.vy=(ship->ppivot.vy-((ship->unitVecNose.vy*3)>>5))+(ship->unitVecWing.vy>>4);
            d.vz=(ship->ppivot.vz-((ship->unitVecNose.vz*3)>>5))+(ship->unitVecWing.vz>>4);
            facePoint.vx=track->vertices[face->vertex[0]].vx;
            facePoint.vy=track->vertices[face->vertex[0]].vy;
            facePoint.vz=track->vertices[face->vertex[0]].vz;
            alpha=PlaneLineCollision(facePoint,a,face->normal);
            a.vx-=(alpha*face->normal.vx)>>12;
            a.vy-=(alpha*face->normal.vy)>>12;
            a.vz-=(alpha*face->normal.vz)>>12;
            alpha=PlaneLineCollision(facePoint,b,face->normal);
            b.vx-=(alpha*face->normal.vx)>>12;
            b.vy-=(alpha*face->normal.vy)>>12;
            b.vz-=(alpha*face->normal.vz)>>12;
            alpha=PlaneLineCollision(facePoint,c,face->normal);
            c.vx-=(alpha*face->normal.vx)>>12;
            c.vy-=(alpha*face->normal.vy)>>12;
            c.vz-=(alpha*face->normal.vz)>>12;
            alpha=PlaneLineCollision(facePoint,d,face->normal);
            d.vx-=(alpha*face->normal.vx)>>12;
            d.vy-=(alpha*face->normal.vy)>>12;
            d.vz-=(alpha*face->normal.vz)>>12;
            vertex[0].vx=a.vx+cameraPtr->camPos->relative.t[0];
            vertex[0].vy=a.vy+cameraPtr->camPos->relative.t[1];
            vertex[0].vz=a.vz+cameraPtr->camPos->relative.t[2];
            vertex[1].vx=b.vx+cameraPtr->camPos->relative.t[0];
            vertex[1].vy=b.vy+cameraPtr->camPos->relative.t[1];
            vertex[1].vz=b.vz+cameraPtr->camPos->relative.t[2];
            vertex[2].vx=c.vx+cameraPtr->camPos->relative.t[0];
            vertex[2].vy=c.vy+cameraPtr->camPos->relative.t[1];
            vertex[2].vz=c.vz+cameraPtr->camPos->relative.t[2];
            vertex[3].vx=d.vx+cameraPtr->camPos->relative.t[0];
            vertex[3].vy=d.vy+cameraPtr->camPos->relative.t[1];
            vertex[3].vz=d.vz+cameraPtr->camPos->relative.t[2];
            depth[0]=RotTransPers(&vertex[0],(int *)&screen[0],&p,&flag);
            depth[1]=RotTransPers(&vertex[1],(int *)&screen[1],&p,&flag);
            depth[2]=RotTransPers(&vertex[2],(int *)&screen[2],&p,&flag);
            depth[3]=RotTransPers(&vertex[3],(int *)&screen[3],&p,&flag);
            if(flag & 0x80460000) continue;
            x[0]=screen[0].vx;
            x[4]=screen[1].vx;
            x[20]=screen[2].vx;
            x[24]=screen[3].vx;
            y[0]=screen[0].vy;
            y[4]=screen[1].vy;
            y[20]=screen[2].vy;
            y[24]=screen[3].vy;
            x[2]=(x[0]+x[4])>>1;
            y[2]=(y[0]+y[4])>>1;
            x[10]=(x[0]+x[20])>>1;
            y[10]=(y[0]+y[20])>>1;
            x[14]=(x[4]+x[24])>>1;
            y[14]=(y[4]+y[24])>>1;
            x[22]=(x[20]+x[24])>>1;
            y[22]=(y[20]+y[24])>>1;
            x[12]=(x[10]+x[14])>>1;
            y[12]=(y[10]+y[14])>>1;
            x[1]=(x[0]+x[2])>>1;
            y[1]=(y[0]+y[2])>>1;
            x[5]=(x[0]+x[10])>>1;
            y[5]=(y[0]+y[10])>>1;
            x[7]=(x[2]+x[12])>>1;
            y[7]=(y[2]+y[12])>>1;
            x[11]=(x[10]+x[12])>>1;
            y[11]=(y[10]+y[12])>>1;
            x[6]=(x[5]+x[7])>>1;
            y[6]=(y[5]+y[7])>>1;
            x[3]=(x[2]+x[4])>>1;
            y[3]=(y[2]+y[4])>>1;
            x[9]=(x[4]+x[14])>>1;
            y[9]=(y[4]+y[14])>>1;
            x[13]=(x[12]+x[14])>>1;
            y[13]=(y[12]+y[14])>>1;
            x[8]=(x[7]+x[9])>>1;
            y[8]=(y[7]+y[9])>>1;
            x[15]=(x[10]+x[20])>>1;
            y[15]=(y[10]+y[20])>>1;
            x[17]=(x[12]+x[22])>>1;
            y[17]=(y[12]+y[22])>>1;
            x[21]=(x[20]+x[22])>>1;
            y[21]=(y[20]+y[22])>>1;
            x[16]=(x[15]+x[17])>>1;
            y[16]=(y[15]+y[17])>>1;
            x[19]=(x[14]+x[24])>>1;
            y[19]=(y[14]+y[24])>>1;
            x[23]=(x[22]+x[24])>>1;
            y[23]=(y[22]+y[24])>>1;
            x[18]=(x[17]+x[19])>>1;
            y[18]=(y[17]+y[19])>>1;
            {
                /* Port: the grid was halved on whole pixels; the renderer places it between the corners' exact positions */
                uint32_t corners[4];
                const void *cornerAddress[4];
                corners[0]=(u_short)screen[0].vx|(uint32_t)(u_short)screen[0].vy<<16;
                corners[1]=(u_short)screen[1].vx|(uint32_t)(u_short)screen[1].vy<<16;
                corners[2]=(u_short)screen[2].vx|(uint32_t)(u_short)screen[2].vy<<16;
                corners[3]=(u_short)screen[3].vx|(uint32_t)(u_short)screen[3].vy<<16;
                cornerAddress[0]=&screen[0];cornerAddress[1]=&screen[1];cornerAddress[2]=&screen[2];cornerAddress[3]=&screen[3];
                GteRecordGrid(corners,cornerAddress,x,y,5,gridHandles);
            }
            depth[0]-=175;
            if(depth[0]<0) depth[0]=0;
            depth[1]=depth[0]-20;
            if(depth[1]<0) depth[1]=0;
            depth[2]-=100;
            if(depth[2]<0) depth[2]=0;
            row=0;
            shadowQuads=shadowEntry;
            gridRow=0;
            for(;row<4;row++,gridRow+=5) {
                col=0;
                quadRow=row*4;
                rowStart=gridRow;
                for(;col<4;col++) {
                    buffer=CurrentScreen;
                    polyIndex=quadRow+col;
                    grid=rowStart+col;
                    bufferOffset=buffer*40;
                    polyIndex*=80;
                    x0=x[grid];
                    poly=(POLY_FT4 *)((u_char *)shadowQuads+(polyIndex+bufferOffset));
                    poly->x0=x0;poly->y0=y[grid];
                    poly->x1=x[grid+1];poly->y1=y[grid+1];
                    poly->x2=x[grid+5];poly->y2=y[grid+5];
                    poly->x3=x[grid+6];poly->y3=y[grid+6];
                    GteBindPrecise(&poly->x0,gridHandles[grid]);GteBindPrecise(&poly->x1,gridHandles[grid+1]); /* Port: precise positions */
                    GteBindPrecise(&poly->x2,gridHandles[grid+5]);GteBindPrecise(&poly->x3,gridHandles[grid+6]);
                    if(row==0) AddPrim(OT[CurrentScreen]+depth[0],&shadowQuads->quad[col][CurrentScreen]);
                    else if(row==1) AddPrim(OT[CurrentScreen]+depth[1],&shadowQuads->quad[4+col][CurrentScreen]);
                    else AddPrim(OT[CurrentScreen]+depth[2],poly);
                }
            }
        }
    }
}


/* Screen rows of the weapon icon; the PAL layout puts it 8 lines higher. */
#define WEAPON_ICON_TOP    REGION(0x14, 0x0C)
#define WEAPON_ICON_BOTTOM REGION(0x43, 0x3B)

void InitWeaponIcons(POLY_FT4 *icons, Texture **textures)
{
    short loc;
    int i;
    POLY_FT4 *back;

    loc = TextureTableCount;
    LoadCmpFiles(WADLoadCompressedTextureSequence(textureWad));
    textures[6] = textureTable[loc++];
    textures[4] = textureTable[(short)loc];
    textures[5] = textureTable[(short)(loc + 1)];
    textures[3] = textureTable[(short)(loc + 2)];
    textures[2] = textureTable[(short)(loc + 3)];
    textures[14] = textureTable[(short)(loc + 4)];
    textures[1] = textureTable[(short)(loc + 5)];
    textures[12] = textureTable[(short)(loc + 6)];
    textures[11] = textureTable[(short)(loc + 8)];
    textures[0] = textureTable[(short)(loc + 9)];
    textures[7] = textureTable[(short)(loc + 10)];
    textures[13] = textureTable[(short)(loc + 11)];
    textures[8] = textureTable[(short)(loc + 12)];

    for (i = 0; i < 2; i++) {
        SetPolyFT4(&icons[i]);
        icons[i].r0 = 0x80;
        icons[i].g0 = 0x80;
        icons[i].b0 = 0x80;
        icons[i].x0 = 0x90;
        icons[i].y0 = WEAPON_ICON_TOP;
        icons[i].x1 = 0xBF;
        icons[i].y1 = WEAPON_ICON_TOP;
        icons[i].x2 = 0x90;
        icons[i].y2 = WEAPON_ICON_BOTTOM;
        icons[i].x3 = 0xBF;
        icons[i].y3 = WEAPON_ICON_BOTTOM;
        icons[i].tpage = GetTPage(0, 0, textures[0]->textureX, textures[0]->textureY);

        back = &weaponIconPrims[i];
        SetPolyFT4(back);
        weaponIconPrims[i].r0 = 0x80;
        weaponIconPrims[i].g0 = 0x80;
        weaponIconPrims[i].b0 = 0x80;
        weaponIconPrims[i].x0 = 0x90;
        weaponIconPrims[i].y0 = WEAPON_ICON_TOP;
        weaponIconPrims[i].x1 = 0xBF;
        weaponIconPrims[i].y1 = WEAPON_ICON_TOP;
        weaponIconPrims[i].x2 = 0x90;
        weaponIconPrims[i].y2 = WEAPON_ICON_BOTTOM;
        weaponIconPrims[i].x3 = 0xBF;
        weaponIconPrims[i].y3 = WEAPON_ICON_BOTTOM;
        back->code |= 2;
        back->tpage = GetTPage(0, 3, textures[3]->textureX, textures[3]->textureY);
        back->u0 = textures[3]->u0;
        back->v0 = textures[3]->v0;
        back->u1 = textures[3]->u1;
        back->v1 = textures[3]->v1;
        back->u2 = textures[3]->u2;
        back->v2 = textures[3]->v2;
        back->u3 = textures[3]->u3;
        back->v3 = textures[3]->v3;
        weaponIconPrims[i].clut = textures[3]->cba;
    }
}


void DrawWeaponIcons(POLY_FT4 *icons, Texture **textures, ShipData *ship)
{
    int       textureIndex;
    u_short   tpage;
    POLY_FT4 *poly;

    if (PortPcData()) {
        PcDrawWeaponIcons(ship); /* Port: the PC version's weapon panel (pc/pchud.c) */
        return;
    }
    if (ship->weapon != 0) {
        textureIndex = ship->weapon - 1;
        icons[CurrentScreen].u0 = textures[textureIndex]->u0;
        icons[CurrentScreen].v0 = textures[textureIndex]->v0;
        icons[CurrentScreen].u1 = textures[textureIndex]->u1;
        icons[CurrentScreen].v1 = textures[textureIndex]->v1;
        icons[CurrentScreen].u2 = textures[textureIndex]->u2;
        icons[CurrentScreen].v2 = textures[textureIndex]->v2;
        icons[CurrentScreen].u3 = textures[textureIndex]->u3;
        icons[CurrentScreen].v3 = textures[textureIndex]->v3;
        icons[CurrentScreen].clut = textures[textureIndex]->cba;
        tpage = GetTPage(0, 1, textures[textureIndex]->textureX, textures[textureIndex]->textureY);
        poly = &icons[CurrentScreen];
        poly->code |= 2;
        poly->tpage = tpage;
        AddPrim(OT[CurrentScreen], &icons[CurrentScreen]);
    }
}


void SetShake(int amount)
{
    shakeAmount = amount;
}


void ShakeScreen(void)
{
    int offsetY;
    int offsetX;
    int rnd;
    int rnd2;
    int halfShake;
    u_short posY;
    u_short posX;
    u_short posY2;
    u_short posX2;

    if (shakeAmount >= 2) {
        rnd = newRand();
        offsetX = rnd % (int) shakeAmount - DISPLAY_SHIFT_X;
        posX = *(u_short *)&gameData->screenX + (((int) shakeAmount >> 1) - offsetX);
        DispEnv1.screen.x = posX;
        DispEnv0.screen.x = posX;
        rnd2 = newRand();
        offsetY = rnd2 % (int) shakeAmount - DISPLAY_SHIFT_Y;
        halfShake = (int) shakeAmount >> 1;
        posY = *(u_short *)&gameData->screenY + (halfShake - offsetY);
        shakeAmount -= 1;
        DispEnv1.screen.y = posY;
        DispEnv0.screen.y = posY;
        PortDisplayShake((short)(posX - (gameData->screenX + DISPLAY_SHIFT_X)),
                         (short)(posY - (gameData->screenY + DISPLAY_SHIFT_Y))); /* Port: no TV to move it on */
    } else if (shakeAmount == 1) {
        posX2 = *(u_short *)&gameData->screenX + DISPLAY_SHIFT_X;
        DispEnv1.screen.x = posX2;
        DispEnv0.screen.x = posX2;
        posY2 = *(u_short *)&gameData->screenY + DISPLAY_SHIFT_Y;
        shakeAmount = 0;
        DispEnv1.screen.y = posY2;
        DispEnv0.screen.y = posY2;
        PortDisplayShake(0, 0); /* Port */
    }
}
void InitPitCycle(void)
{
    TrackSection *sec;
    Face         *p;
    Face         *arr;
    int           i;
    TrackData    *t;

    i = 0;
    sec = track->sections;
    t = track;
    arr = t->faces;
    for (;;) {
        p = arr + sec->faceStart;
        for (;;) {
            if (p->flags & 1) {
                break;
            }
            p++;
        }
        if (p->flags & 0x40) {
            p--;
            i++;
            sec++;
            if (!(t->sectionCount < i)) {
                continue;
            }
        } else {
            goto second;
        }
        break;
    }
    pitCycleFlag = 0;
second:
    t = track;
    arr = t->faces;
    do {
        p = arr + sec->faceStart;
        for (;;) {
            if (p->flags & 1) {
                p--;
                break;
            }
            p++;
        }
        i++;
        sec++;
    } while (i < t->sectionCount);
    pitCycleFlag = 0;
}


void InitTrackCycle(void)
{
    ConfigData   *config = gameData;
    TrackSection *seg;
    short         key;
    int           delay;

    key = startLinePos[config->actTrackNum];
    seg = track->sections;
    do {
    } while (seg++->secNo != key);
    delay = 0x1D;
    do {
        delay--;
    } while (delay >= 0);
    InitPitCycle();
}
