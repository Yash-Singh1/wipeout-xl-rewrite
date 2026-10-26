#include "functions.h"
#include "weapon.h"
#include "scene.h"
#include "sparks.h"
#include "port_widen.h"

/* Frames between minigun bursts. */
#define MINIGUN_FIRE_INTERVAL REGION(30, 23)

/* Weapon lifetimes in frames.  The European values follow no single 5/6 rule, so each is
   the value the European program uses. */
#define ROCKET_FRAMES   REGION(0xC8, 0x8C)
#define MISSILE_FRAMES  REGION(0xC8, 0x96)
#define QUAKE_FRAMES    REGION(0x46, 0x3C)
#define PLASMA_FRAMES   REGION(0xDC, 0x99)
#define ELECBOLT_FRAMES REGION(0x8C, 0x64)
/* The plasma shot charges while its count is above this; the European charge starts lower. */
#define PLASMA_CHARGE_FRAMES REGION(200, 140)

/* $gp-relative globals are tentative definitions; the rest are extern. */
extern short fadeAmount;
extern u_char fadeActive;
extern u_char perfectLap;
extern int olddam;
extern short fadeRed;
extern short fadeGreen;
extern short fadeBlue;
extern int seedWep;
extern short missileLockTimer;
extern u_char rearLockCycle;
extern u_short minigunAmmo;
extern u_char minigunFrame;

extern short cheatfade;
extern short CurrentScreen;
extern DR_MODE fadedrm[];
extern char msgWeaponTableFull[];
extern u_char plasmaShotBusy;
extern int remoteShip;
extern int ownShip;
extern ShipData *shipIndex;
extern int shipDamage[];
extern int fracdamage[];
extern POLY_G4 plasmacharge[];
extern int attractDelayFrames;
extern int raceContext;
extern int ghostShip;
extern int shieldRed[];
extern int shieldGreen[];
extern TILE fadeTile[];
extern DR_MODE shieldParams[][2];
extern int raceType;
extern int proximity;
extern u_char weaponChance[][11];
extern u_char weaponDrone[];
extern RaceLine raceLines[];
extern TrackData *track;
extern ConfigData *gameData;
extern int snowCount;
extern int flashNozzleShip;
extern int ambientVoice1;
extern int primsDrawn;
extern DVECTOR StaticScreen[];
extern unsigned short StaticDepth[];
extern unsigned short StaticFlags[];

/* Runs every live weapon's update routine, then the fade overlay. */
void UpdateWeapons(ShipData *ships, WeaponShape *shapes, WeaponData *weapons)
{
    int         pad[2];
    int         i;
    WeaponData *p;

    fadeActive = 0;
    if (cheatfade > 0) {
        fadeActive = 1;
    }
    i = 0;
    p = weapons;
    do {
        if (p->count > 0) {
            p->update(ships, shapes, p, weapons);
        }
        i++;
        p++;
    } while (i < 0x1E);
    if (fadeAmount > 0) {
        fadeActive = 1;
    }
    if (fadeActive != 0) {
        UpdateFadeScreen();
    }
    AddPrim(OT[CurrentScreen], &fadedrm[CurrentScreen]);
}


/* Launches a weapon of the given type from the first free slot. Some types act on the ship directly. */
void InsertWeapon(ShipData *ship, WeaponData *weapons, int owner, u_char type)
{
    WeaponData *weapon;
    int delay = 0;
    int lifetime = PAL_TIME(40);
    int frequency = 256;
    int offset;
    int *shield;
    int *damage = shipDamage;
    int *energy;

    offset = owner * 4;
    shield = (int *)((u_char *)damage + offset);
    weapon = weapons;
again:
    if (weapon->count == 0) {
        switch (type & 255) {
        case 1:
            weapon->owner = owner;
            weapon->update = InitRocket;
            weapon->count = lifetime;
            if (owner == ownShip) PlayNote(12, 160, 15, 15, frequency);
            weapon->type = 1;
            ship->weapon = 0;
            return;
        case 13:
            if (plasmaShotBusy) return;
            weapon->update = InitPlasma;
            weapon->count = lifetime;
            weapon->owner = owner;
            PlayNote(39, 255, 15, 15, frequency);
            weapon->type = 13;
            ship->weapon = 0;
            return;
        case 12:
            weapon->owner = owner;
            weapon->update = InitQuake;
            weapon->count = lifetime;
            if (owner == ownShip) PlayNote(38, 255, 15, 15, frequency);
            weapon->type = 12;
            ship->weapon = 0;
            return;
        case 10:
            weapon->update = InitRocketLeft;
            weapon->count = lifetime;
            weapon->owner = owner;
            weapon->type = 1;
            ship->weapon = 0;
            return;
        case 11:
            weapon->update = InitRocketRight;
            weapon->count = lifetime;
            weapon->owner = owner;
            weapon->type = 1;
            ship->weapon = 0;
            return;
        case 2:
            weapon->owner = owner;
            weapon->update = InitMissile;
            weapon->count = lifetime;
            if (owner == ownShip) PlayNote(12, 160, 15, 15, frequency);
            weapon->type = 2;
            ship->weapon = 0;
            return;
        case 15:
            weapon->update = InitMinigun;
            weapon->count = lifetime;
            weapon->owner = owner;
            weapon->type = 15;
            return;
        case 3:
            weapon->owner = owner;
            delay += 3;
            weapon->update = InitMine;
            if ((owner & 255) != ownShip && (owner & 255) != remoteShip)
                weapon->count = delay + PAL_TIME(40);
            else weapon->count = delay;
            weapon->type = 3;
            weapon++;
            if (delay < 15) goto bound;
            ship->weapon = 0;
            return;
        case 14:
            weapon->update = InitSmartBomb;
            weapon->count = PAL_TIME(24);
            weapon->owner = owner;
            weapon->type = 14;
            ship->weapon = 0;
            return;
        case 5:
            weapon->update = InitElecBolt;
            weapon->count = lifetime;
            weapon->owner = owner;
            weapon->type = 5;
            ship->weapon = 0;
            if (owner == ownShip) {
                PlayNote(1, 255, 15, 15, frequency);
                PlayNote(12, 255, 15, 15, 192);
            }
            return;
        case 7:
            ship->weapon = 0;
            ship->autopilotCount = PAL_TIME(120);
            ship->update = UpdateDroneShip;
            humanToDroneSpeed(shipIndex, owner);
            if (owner == ownShip) PlayNote(16, 480, 15, 15, frequency);
            return;
        case 6:
            energy = (int *)((u_char *)fracdamage + offset);
            *shield -= 32;
            *energy -= 8192;
            if (*shield < 0) *shield = 0;
            ship->weapon = 0;
            if (owner == ownShip) {
                PlayNote(47, 255, 30, 15, frequency);
                PlayNote(47, 255, 0, 15, frequency);
            }
            return;
        case 8:
            weapon->update = InitShield;
            weapon->count = REGION(200, 150);
            weapon->owner = owner;
            weapon->type = 8;
            ship->weapon = 0;
            return;
        case 9:
            ship->weapon = 0;
            ship->thrustCount = 13;
            ship->thrust = ship->maxThrust;
            if (owner == ownShip) PlayNote(34, 200, 15, 15, frequency);
            return;
        }
    }
    weapon++;
bound:
    if (weapon < weapons + 30) goto again;
    printf("%s", msgWeaponTableFull); /* Port: the message is not a format string */
}


/* Fires a weapon of the given type; a rocket type also launches its two side rockets. */
void FireWeapon(ShipData *ship, WeaponData *weapons, int owner, u_char type)
{
    if (((owner == ownShip) || (owner == remoteShip)) && (type == 2) && (ship->rearFire != 0)) {
        if (ship->rearTarget != -1) {
            ship->targetShip = ship->rearTarget;
        }
    }
    InsertWeapon(ship, weapons, owner, type);
    if (type == 1) {
        InsertWeapon(ship, weapons, owner, 0xA);
        InsertWeapon(ship, weapons, owner, 0xB);
    }
}
/* Locks the nearest ship in front (within range) as this ship's missile target. */
void MissileLock(int shipNo, ShipData *ships)
{
    ShipData *s;
    int       sec;
    int       diff;
    int       nearShip;

    s = &ships[shipNo];
    sec = raceLines[s->nearSecNo].distance;
    if (s->targetShip != -1) {
        diff = raceLines[ships[s->targetShip].nearSecNo].distance - sec;
        if (diff < 0) {
            diff += numTrackSections;
        }
        if ((u_long)(diff - 1) >= 0x27) {
            s->targetShip = -1;
        }
        if (s->targetShip != -1) {
            return;
        }
    }
    nearShip = (short)getShipInfrontLAP(ships, shipNo);
    if (nearShip == -1) {
        return;
    }
    diff = raceLines[ships[nearShip].nearSecNo].distance - sec;
    if (diff < 0) {
        diff += numTrackSections;
    }
    if ((u_long)(diff - 5) >= 0x1F) {
        return;
    }
    missileLockTimer = 10;
    if (shipNo == ownShip) {
        PlayNote(0x25, 0x80, 0xF, 0xF, 0x100);
    }
    s->targetShip = nearShip;
}


/* Tracks the nearest ship behind as the rear-missile target (or none). */
void RearMissileLock(int shipNo, ShipData *ships)
{
    int       diff;
    int       section;
    int       nearShip;
    ShipData *s;

    nearShip = (short)getShipBehindLAP(ships, shipNo);
    s = &ships[shipNo];
    if (nearShip != -1) {
        section = raceLines[ships[nearShip].nearSecNo].distance;
        diff = raceLines[s->nearSecNo].distance - section;
        if (diff < 0) {
            diff += numTrackSections;
        }
        if ((u_long)(diff - 1) < 0x27) {
            s->rearTarget = nearShip;
            if (shipNo == ownShip) {
                rearLockCycle = (rearLockCycle + 1) % 20;
            }
            return;
        }
    }
    s->rearTarget = -1;
    if (shipNo == ownShip) {
        rearLockCycle = 0;
    }
}


/* Copies the master shape into the following slots; each copy gets its own skeleton, vertices,
   normals and primitives packed after the object header. */
#define CLONE_SHAPES(master, first, end)                                               \
    shapes[master].shapePtr = WADLoadPrm(textureWad, (short)textureStart);             \
    length = portLastPrmImageLength; /* Port: the length of the widened PRM image */   \
    for (i = first; i < end; i++) {                                                    \
        start = DAlloc(heap, length + 32);                                             \
        prm.ptr = start;                                                               \
        shapes[i].shapePtr = prm.object;                                               \
        memcpy(start, shapes[master].shapePtr, length);                                \
        object = prm.object++;                                                         \
        object->skeleton = prm.skeleton++;                                             \
        object->vertices = prm.svector;                                                \
        prm.svector += object->vertexCount;                                            \
        object->normals = prm.svector;                                                 \
        prm.svector += object->normalCount;                                            \
        object->primitives = prm.primitive;                                            \
    }

/* Loads the weapon shapes (rockets, mines, shields, missiles, bolts), clears the weapon table and
   builds the semi-transparent draw-mode packets used by the shield. */
void initWeapons(WeaponShape *shapes, Skeleton *camPos, WeaponData *weapons)
{
    int     i;
    int     j;
    int     k;
    int    length;
    short   textureStart;
    Prm     prm;
    char   *start;
    Object *object;
    Object *obj;
    Poly    poly;
    short   primitiveCount;

    textureStart = TextureTableCount;
    LoadCmpFiles(WADLoadCompressedTextureSequence(textureWad));
    CLONE_SHAPES(0, 1, 6)
    CLONE_SHAPES(6, 7, 21)
    CLONE_SHAPES(21, 22, 25)
    CLONE_SHAPES(25, 26, 28)
    CLONE_SHAPES(28, 29, 31)
    for (i = 0; i < 31; i++) {
        if (i >= 25 && i < 28) {
            poly.prim = shapes[i].shapePtr->primitives;
            primitiveCount = shapes[i].shapePtr->primitiveCount;
            for (k = 0; k < primitiveCount; k++) {
                switch (poly.prim->type) {
                case TypeG3:
                    for (j = 0; j < 2; j++) {
                        poly.g3->colour[0].b = 255;
                        poly.g3->colour[1].b = 255;
                        poly.g3->colour[2].b = 255;
                    }
                    poly.g3 += 1;
                    break;
                case TypeG4:
                    for (j = 0; j < 2; j++) {
                        poly.g4->colour[0].b = 255;
                        poly.g4->colour[1].b = 255;
                        poly.g4->colour[2].b = 255;
                        poly.g4->colour[3].b = 255;
                    }
                    poly.g4 += 1;
                    break;
                }
            }
        }
        shapes[i].used = 0;
        obj = shapes[i].shapePtr;
        while (obj) {
            obj->skeleton->super = camPos;
            obj = obj->next;
        }
    }
    for (i = 0; i < 30; i++) {
        weapons[i].count = 0;
        weapons[i].type = 0;
    }
    for (i = 0; i < 192; i++) {
        for (j = 0; j < 2; j++) {
            SetDrawMode(&shieldParams[i][j], 1, 1, GetTPage(0, 1, 0, 0) & 0xFFFF, 0);
        }
    }
}


/* Raises the three track points of a bump by one step. */
void BumpUp(TrackSection *bump)
{
    Face *nodes;

    if (bump != 0 && bump->bump < 8) {
        nodes = &track->faces[bump->faceStart];
        while ((nodes->flags & 1) == 0) {
            nodes++;
        }
        track->vertices[nodes->vertex[0]].vy -= 0xC8;
        track->vertices[nodes->vertex[1]].vy -= 0xC8;
        track->vertices[(nodes + 1)->vertex[0]].vy -= 0xC8;
        bump->bump += 1;
    }
}


/* Lowers one bump step. */
void BumpDown(TrackSection *bump)
{
    Face *nodes;

    if (bump != 0 && bump->bump > 0) {
        nodes = &track->faces[bump->faceStart];
        while ((nodes->flags & 1) == 0) {
            nodes++;
        }
        track->vertices[nodes->vertex[0]].vy += 0xC8;
        track->vertices[nodes->vertex[1]].vy += 0xC8;
        track->vertices[(nodes + 1)->vertex[0]].vy += 0xC8;
        bump->bump -= 1;
    }
}


/* Lowers the whole bump to its base level. */
void BumpFlatten(TrackSection *bump)
{
    Face *nodes;
    Face *first;
    u_char flag;

    if (bump != 0 && bump->bump > 0) {
        nodes = &track->faces[bump->faceStart];
        while (((flag = nodes->flags) & 1) == 0) {
            nodes++;
        }
        first = nodes;
        if (bump->bump > 0) {
            do {
                nodes = first;
                track->vertices[nodes->vertex[0]].vy += 0xC8;
                track->vertices[nodes->vertex[1]].vy += 0xC8;
                nodes++;
                track->vertices[nodes->vertex[0]].vy += 0xC8;
                bump->bump -= 1;
            } while (bump->bump > 0);
        }
    }
}


void FlattenTrack(void)
{
    TrackSection *s;
    int i;

    s = track->sections;
    for (i = 0; i < track->sectionCount; i++) {
        BumpFlatten(s);
        s++;
    }
}


/* Marks the sections that carry a bump on each circuit. */
void InitialiseBumps(void)
{
    int           i;
    int           pad[2];
    int           section;
    int           count;
    unsigned char circuit;
    TrackSection *p;

    count = track->sectionCount;
    p = track->sections;
    i = 0;
    if (count > 0) {
        do {
            p->bump = 0;
            circuit = gameData->actTrackNum;
            section = p->secNo;
            if ((circuit == 2 && ((u_long)(section - 0x67) < 0x37 || (u_long)(section - 0xBF) < 0x1E || (u_long)(section - 0x10E) < 0x74)) ||
                (circuit == 1 && ((u_long)section < 0x49 || (u_long)(section - 0x57) < 0x11 || (u_long)(section - 0xB9) < 0x29 || (u_long)(section - 0x86) < 0x27)) ||
                (circuit == 8 && ((u_long)(section - 0x23) < 0x1E || (u_long)(section - 0x99) < 0x12 || (u_long)(section - 0xFB) < 0x10 || (u_long)(section - 0x11B) < 0xC)) ||
                (circuit == 13 && ((u_long)(section - 0x7D) < 0x5B || (u_long)(section - 0x105) < 0x14 || (u_long)(section - 0x12B) < 0x2C || (u_long)(section - 0x19D) < 0x13)) ||
                (circuit == 17 && ((u_long)(section - 0xF) < 0x11 || (u_long)(section - 0x4A) < 0x20 || (u_long)(section - 0x82) < 0x26 || (u_long)(section - 0xE6) < 0x2C)) ||
                (circuit == 6 && ((u_long)(section - 0x13) < 0x6 || (u_long)(section - 0xC3) < 0x49 || (u_long)(section - 0x166) < 0x46)) ||
                (circuit == 20 && ((u_long)(section - 0xD5) < 0xA || (u_long)(section - 0xE7) < 0x1A || (u_long)(section - 0x130) < 0x1E)) ||
                (circuit == 7 && ((u_long)(section - 0x1B) < 0xB || (u_long)(section - 0x5F) < 0x8 || (u_long)(section - 0x6D) < 0x22 || (u_long)(section - 0xAB) < 0x3C || (u_long)(section - 0x144) < 0x11 || (u_long)(section - 0x4A) < 0xA))) {
                p->snow = 1;
            } else {
                p->snow = 0;
            }
            i++;
            p++;
        } while (i < track->sectionCount);
    }
}


/* Shakes the track under a quake weapon: knocks the nearby ships about, raises or lowers the
   bumps around the impact and counts the quake down. */
void UpdateQuake(ShipData *ships, WeaponShape *unused, WeaponData *weapon, WeaponData *weapons)
{
    s32 spare0[4];
    VECTOR faceVertex;
    VECTOR centre;
    s32 spare1[18];
    s32 faceNo;
    u8 flag;
    s16 step;
    s16 stepNow;
    s32 explodeX;
    s32 explodeY;
    TrackSection *ownerSection;
    s32 bumpCount;
    s32 i;
    s32 j;
    s32 sectionCount;
    s32 sectionCount2;
    s32 shipNo;
    TrackSection *nextSection;
    Face *facePtr;
    Face *nextFace;
    Face *riseFace;
    TrackSection *nearest;
    TrackSection *prevOfNext;
    TrackSection *junction;
    TrackSection *nearSection;
    TrackSection *nearRemote;
    TrackSection *junction2;
    TrackSection *bumpSection;
    TrackSection *bumpSection2;
    TrackSection *section;
    ShipData *ship;

    nearest = FindNearestSection(weapon->section, weapon->pos);
    weapon->section = nearest;
    faceNo = nearest->faceStart;
    nextSection = nearest->nextSection.ptr;
    facePtr = track->faces + faceNo;
    nextFace = track->faces + nextSection->faceStart;
    if (facePtr->flags & 1) goto second_scan;
    facePtr++;
first_scan:
    flag = facePtr->flags;
    facePtr++;
    if (!(flag & 1)) goto first_scan;
    facePtr--;
second_scan:
    do {
        flag = nextFace->flags;
        nextFace++;
        if (flag & 1) {
            nextFace--;
            break;
        }
    } while (1);
    riseFace = nextFace;
    AddRisingParticles(particleSlots, 0xA, track->vertices + riseFace->vertex[0], 8);
    AddRisingParticles(particleSlots, 0xA, track->vertices + riseFace->vertex[1], 8);
    AddRisingParticles(particleSlots, 0xA, track->vertices + (riseFace + 1)->vertex[0], 8);
    faceVertex.vx = track->vertices[facePtr->vertex[0]].vx;
    faceVertex.vy = track->vertices[facePtr->vertex[0]].vy;
    faceVertex.vz = track->vertices[facePtr->vertex[0]].vz;
    shipNo = 0;
    section = weapon->section;
    do {
        ship = ships + shipNo;
        if ((section == ship->nearTrkSect) && (shipNo != weapon->owner) && !(ship->combatAttr & DESTROYED)) {
            ship->speed >>= 2;
            ship->vpivot.vx >>= 2;
            ship->vpivot.vy >>= 2;
            ship->vpivot.vz >>= 2;
            if (shipNo == ownShip) {
                SetShake(8);
            }
            PlayDirectionalNote(5, 0, 0x28, ship->ppivot);
            explodeX = ship->ppivot.vx + (newRand() & 0x3FF) - 0x200;
            explodeY = ship->ppivot.vy - (newRand() & 0x1FF);
            AddExplode(explodeX, explodeY, ship->ppivot.vz + (newRand() & 0x3FF) - 0x200, -1);
            DamageCollide(0xC, shipNo);
        }
        shipNo += 1;
    } while (shipNo < 0xF);
    bumpCount = 0;
    if (weapon->quakeBumpStep > 0) {
        do {
            BumpUp(section);
            bumpCount += 1;
        } while (bumpCount < weapon->quakeBumpStep);
    }
    section = weapon->section;
    sectionCount = 0;
    do {
        junction = section->junction.ptr;
        /* Port: junction.ptr != (void *)-1 tested the 32-bit slot; compare the slot as the index (-1 = no junction). */
        if (section->junction.index != -1) {
            bumpSection = junction;
            if (junction->flags & 8) {
                i = 0;
                do {
                    BumpDown(bumpSection);
                    bumpSection = bumpSection->prevSection.ptr;
                    i += 1;
                } while (i < 0xA);
            }
        }
        section = section->prevSection.ptr;
        sectionCount += 1;
        BumpDown(section);
    } while (sectionCount < 0x14);
    centre.vx = nextSection->centre.vx;
    centre.vy = nextSection->centre.vy;
    centre.vz = nextSection->centre.vz;
    weapon->count = (u16)(weapon->count - 1);
    nearSection = ships[ownShip].nearTrkSect;
    if (nearSection->bump > 0) {
        BumpFlatten(nearSection);
        BumpFlatten(ships[ownShip].nearTrkSect->nextSection.ptr);
        BumpFlatten(ships[ownShip].nearTrkSect->prevSection.ptr);
    }
    if ((u32)(raceType - 2) < 2U) {
        nearRemote = ships[remoteShip].nearTrkSect;
        if (nearRemote->bump > 0) {
            BumpFlatten(nearRemote);
            BumpFlatten(ships[remoteShip].nearTrkSect->nextSection.ptr);
            BumpFlatten(ships[remoteShip].nearTrkSect->prevSection.ptr);
        }
    }
    if (weapon->count & 1) {
        weapon->pos.vx = nextSection->centre.vx;
        weapon->pos.vy = nextSection->centre.vy;
        weapon->pos.vz = nextSection->centre.vz;
        ownerSection = ships[weapon->owner].nearTrkSect;
        if ((ownerSection == nextSection) || (prevOfNext = nextSection->prevSection.ptr, ownerSection == prevOfNext) || (ownerSection == prevOfNext->prevSection.ptr)) {
            weapon->pos.vx = nextSection->nextSection.ptr->centre.vx;
            weapon->pos.vy = nextSection->nextSection.ptr->centre.vy;
            weapon->pos.vz = nextSection->nextSection.ptr->centre.vz;
        }
        step = weapon->quakeBumpStep;
        if ((step < 8) && ((s16)weapon->count >= 9)) {
            weapon->quakeBumpStep = step + 2;
        }
        stepNow = weapon->quakeBumpStep;
        if ((stepNow > 0) && ((s16)weapon->count < 9)) {
            weapon->quakeBumpStep = stepNow - 1;
        }
    }
    sectionCount2 = 0;
    if (weapon->count == 0) {
        section = weapon->section;
        do {
            junction2 = section->junction.ptr;
            if (section->junction.index != -1) {
                bumpSection2 = junction2;
                if (junction2->flags & 8) {
                    j = 0;
                    do {
                        BumpFlatten(bumpSection2);
                        bumpSection2 = bumpSection2->prevSection.ptr;
                        j += 1;
                    } while (j < 0x14);
                }
            }
            BumpFlatten(section);
            section = section->prevSection.ptr;
            sectionCount2 += 1;
        } while (sectionCount2 < 0x14);
    }
}



void UpdateMissile(ShipData *ships, WeaponShape *shapes, WeaponData *weapon, WeaponData *weapons)
{
    VECTOR    distance;
    VECTOR    targetVector;
    VECTOR    bestPath;
    VECTOR    facePoint;
    VECTOR    nextPoint;
    VECTOR    path;
    VECTOR    offset;
    Face     *facePtr;
    Face     *nextFace;
    ShipData *ship;
    int       i;
    int       mag;
    int       temp;
    int       vhdg;
    int       vpitch;
    int       alpha;
    int       height;
    u_char    flag;

    if (weapon->target == -1) {
        UpdateRocket(ships, shapes, weapon, weapons);
        return;
    }
    targetVector.vx = (ships[weapon->target].ppivot.vx - weapon->pos.vx) >> 3;
    targetVector.vy = (ships[weapon->target].ppivot.vy - weapon->pos.vy) >> 3;
    targetVector.vz = (ships[weapon->target].ppivot.vz - weapon->pos.vz) >> 3;
    temp = SquareRoot0(targetVector.vx * targetVector.vx + targetVector.vz * targetVector.vz);
    vhdg = -ratan2(targetVector.vx, targetVector.vz) - weapon->hdg;
    vpitch = -ratan2(targetVector.vy, temp) - weapon->pitch;
    weapon->section = FindNearestSection(weapon->section, weapon->pos);
    if (vhdg > 2048) {
        vhdg -= 4096;
    } else if (vhdg < -2048) {
        vhdg += 4096;
    }
    if (vpitch <= 2048) {
        goto lowPitch;
    }
    vpitch -= 4096;
    goto steer;
expired:
    weapon->count = -2;
    shapes[weapon->shapeNo].used = 0;
    PlayDirectionalNote(6, 0, 40, weapon->pos);
    return;
lowPitch:
    if (vpitch < -2048) {
        vpitch += 4096;
    }
steer:
    weapon->hdg += vhdg >> 1;
    weapon->pitch += vpitch >> 1;
    weapon->hdg = ang(weapon->hdg);
    weapon->pitch = ang(weapon->pitch);
    weapon->accel.vx = (-rsin(weapon->hdg) * rcos(weapon->pitch)) >> 16;
    weapon->accel.vy = -rsin(weapon->pitch) >> 4;
    weapon->accel.vz = (rcos(weapon->hdg) * rcos(weapon->pitch)) >> 16;
    PAL_SPEED_VECTOR(weapon->accel);
    weapon->velocity.vx += weapon->accel.vx;
    weapon->velocity.vy += weapon->accel.vy;
    weapon->velocity.vz += weapon->accel.vz;
    weapon->velocity.vx -= weapon->velocity.vx >> 2;
    weapon->velocity.vy -= weapon->velocity.vy >> 2;
    weapon->velocity.vz -= weapon->velocity.vz >> 2;
    weapon->pos.vx += weapon->velocity.vx;
    weapon->pos.vy += weapon->velocity.vy;
    weapon->pos.vz += weapon->velocity.vz;
    facePtr = track->faces + weapon->section->faceStart;
    do {
        flag = facePtr->flags;
        facePtr++;
        if (flag & 1) {
            facePtr--;
            break;
        }
    } while (1);
    nextFace = track->faces + weapon->section->nextSection.ptr->faceStart;
    do {
        flag = nextFace->flags;
        nextFace++;
        if (flag & 1) {
            nextFace--;
            break;
        }
    } while (1);
    facePoint.vx = track->vertices[facePtr->vertex[0]].vx;
    facePoint.vy = track->vertices[facePtr->vertex[0]].vy;
    facePoint.vz = track->vertices[facePtr->vertex[0]].vz;
    nextPoint.vx = track->vertices[nextFace->vertex[0]].vx;
    nextPoint.vy = track->vertices[nextFace->vertex[0]].vy;
    nextPoint.vz = track->vertices[nextFace->vertex[0]].vz;
    path.vx = nextPoint.vx - facePoint.vx;
    path.vy = nextPoint.vy - facePoint.vy;
    path.vz = nextPoint.vz - facePoint.vz;
    offset.vx = weapon->pos.vx - facePoint.vx;
    offset.vz = weapon->pos.vz - facePoint.vz;
    mag = SquareRoot0(path.vx * path.vx + path.vz * path.vz);
    alpha = (((path.vx * offset.vx + path.vz * offset.vz) / mag) << 16) / mag;
    height = nextPoint.vy - facePoint.vy;
    height = (height * alpha) >> 16;
    if (weapon->pos.vy < facePoint.vy + height - 300) {
        weapon->pos.vy = facePoint.vy + height - 300;
    }
    weapon->count--;
    if (weapon->count == 0) {
        shapes[weapon->shapeNo].used = 0;
    }
    SetSkeletonPosition(shapes[weapon->shapeNo].shapePtr->skeleton, weapon->pos.vx, weapon->pos.vy, weapon->pos.vz);
    SetSkeletonDirectionHPR(shapes[weapon->shapeNo].shapePtr->skeleton, weapon->hdg, weapon->pitch, 0);
    shapes[weapon->shapeNo].shapePtr->skeleton->update = 1;
    AsmDrawObject(shapes[weapon->shapeNo].shapePtr, cameraPtr->camPos, 0); /* Port: roll was never passed; 0 stands for the console's stale a2 */
    for (i = 0; i < 15; i++) {
        ship = ships + i;
        if (!(ship->combatAttr & DESTROYED) && (((i != weapon->owner) && raceType != 3) || (raceType == 3 && (i == ownShip || i == remoteShip) && i != weapon->owner))) {
            distance.vx = (ship->ppivot.vx - weapon->pos.vx) >> 3;
            distance.vy = (ship->ppivot.vy - weapon->pos.vy) >> 3;
            distance.vz = (ship->ppivot.vz - weapon->pos.vz) >> 3;
            if (GetMagnitude(distance) < 100) {
                if (!(ship->attr & SHIELDED)) {
                    if (i != ownShip && i != remoteShip) {
                        FlipShip(ship);
                        ship->speed >>= 5;
                        ship->vroll += 5000;
                        ship->ahdg = (newRand() - 16384) >> 1;
                    } else {
                        ship->vpivot.vx -= (ship->vpivot.vx * 3) >> 2;
                        ship->vpivot.vy -= (ship->vpivot.vy * 3) >> 2;
                        ship->vpivot.vz -= (ship->vpivot.vz * 3) >> 2;
                        ship->vroll += newRand() % 250;
                        ship->ahdg = (newRand() - 16384) >> 1;
                        if (i == ownShip) {
                            SetShake(20);
                        }
                    }
                }
                DamageCollide(2, i);
                weapon->count = -1;
                weapon->target = i;
                shapes[weapon->shapeNo].used = 0;
                PlayDirectionalNote(5, 0, 40, weapon->pos);
                return;
            }
        }
    }
    facePtr = track->faces + weapon->section->faceStart;
    for (i = 0; i < weapon->section->faceCount; i++) {
        facePoint.vx = track->vertices[facePtr->vertex[0]].vx;
        facePoint.vy = track->vertices[facePtr->vertex[0]].vy;
        facePoint.vz = track->vertices[facePtr->vertex[0]].vz;
        if (PlaneLineCollision(facePoint, weapon->pos, facePtr->normal) <= 0 && !(facePtr->flags & 1)) {
            goto expired;
        }
        facePtr++;
    }
    Weapon2Mine(weapon, weapons, shapes);
}


void InitMissile(ShipData *shapes, WeaponShape *slots, WeaponData *w, WeaponData *weapons)
{
    int    i;
    Face  *node;
    u_char flag;

    if (w->owner == ownShip) {
        w->count = MISSILE_FRAMES;
    } else if (w->owner == remoteShip) {
        w->count = MISSILE_FRAMES;
    } else if (w->count < 2) {
        w->count = MISSILE_FRAMES;
    } else {
        w->count = w->count - 1;
        return;
    }
    w->section = shapes[w->owner].nearTrkSect;
    for (i = 0; i < 4; i++) {
        if (slots[i + 21].used == 0) {
            w->shapeNo = i + 21;
            slots[i + 21].used = 1;
            w->target = shapes[w->owner].targetShip;
            node = &track->faces[shapes[w->owner].nearTrkSect->faceStart];
            do {
                flag = node->flags;
                node++;
                if (flag & 1) {
                    node--;
                    break;
                }
            } while (1);
            GetFireVector(&w->accel, shapes[w->owner].unitVecNose, node, shapes[w->owner].ppivot);
            PAL_SPEED_VECTOR(w->accel);
            w->velocity.vx = shapes[w->owner].vpivot.vx >> 6;
            w->velocity.vy = shapes[w->owner].vpivot.vy >> 6;
            w->velocity.vz = shapes[w->owner].vpivot.vz >> 6;
            w->pos.vx = shapes[w->owner].ppivot.vx;
            w->pos.vy = shapes[w->owner].ppivot.vy;
            w->pos.vz = shapes[w->owner].ppivot.vz;
            SetSkeletonDirectionHPR(slots[w->shapeNo].shapePtr->skeleton, shapes[w->owner].hdg, shapes[w->owner].pitch, shapes[w->owner].roll);
            w->hdg = shapes[w->owner].hdg;
            w->pitch = shapes[w->owner].pitch;
            w->update = UpdateMissile;
            shapes[w->owner].targetShip = -1;
            return;
        }
    }
    w->count = 0;
}


void UpdateMinigun(ShipData *ships, WeaponShape *mineData, WeaponData *weapon, WeaponData *objects)
{
    VECTOR   distance;
    VECTOR   unused1;
    VECTOR   unused2;
    VECTOR   vertex;
    VECTOR   zero;
    Face    *face;
    ShipData *ship;
    int      i;
    int      special;

    zero.vx = 0;
    zero.vy = 0;
    zero.vz = 0;
    weapon->pos.vx += weapon->velocity.vx;
    weapon->pos.vy += weapon->velocity.vy;
    weapon->pos.vz += weapon->velocity.vz;
    weapon->section = FindNearestSection(weapon->section, weapon->pos);
    face = &track->faces[weapon->section->faceStart];
    do {
        u_char flag = face->flags;
        face++;
        if (flag & 1) {
            break;
        }
    } while (1);
    i = 0;
    special = 3;
    weapon->count--;
    do {
        ship = &ships[i];
        if (!(ship->combatAttr & 0x8000)) {
            if (i == weapon->owner || raceType == special) {
                if (raceType != special) {
                    goto next;
                }
                if (i != ownShip && i != remoteShip) {
                    goto next;
                }
                if (i == weapon->owner) {
                    goto next;
                }
            }
            distance.vx = (ship->ppivot.vx - weapon->pos.vx) >> 3;
            distance.vy = (ship->ppivot.vy - weapon->pos.vy) >> 3;
            distance.vz = (ship->ppivot.vz - weapon->pos.vz) >> 3;
            if (GetMagnitude(distance) < 100) {
                PlayDirectionalNote(5, 0, 40, weapon->pos);
                if (!(ship->attr & 0x4000)) {
                    DamageCollide(15, i);
                }
                weapon->count = -1;
                weapon->pos.vx -= weapon->velocity.vx;
                weapon->pos.vy -= weapon->velocity.vy;
                weapon->pos.vz -= weapon->velocity.vz;
                AddRandomParticles(particleSlots, 8, &weapon->pos, &weapon->velocity);
                weapon->target = i;
                return;
            }
        }
next:
        i++;
    } while (i < 15);
    face = &track->faces[weapon->section->faceStart];
    for (i = 0; i < weapon->section->faceCount; i++) {
        vertex.vx = track->vertices[face->vertex[0]].vx;
        vertex.vy = track->vertices[face->vertex[0]].vy;
        vertex.vz = track->vertices[face->vertex[0]].vz;
        if (PlaneLineCollision(vertex, weapon->pos, face->normal) <= 0) {
            weapon->count = -2;
            PlayDirectionalNote(6, 0, 40, weapon->pos);
            weapon->pos.vx -= weapon->velocity.vx;
            weapon->pos.vy -= weapon->velocity.vy;
            weapon->pos.vz -= weapon->velocity.vz;
            AddRandomParticles(particleSlots, 8, &weapon->pos, &zero);
            return;
        }
        face++;
    }
    Weapon2Mine(weapon, objects, mineData);
}


void InitMinigun(ShipData *ships, WeaponShape *shapes, WeaponData *weapon, WeaponData *weapons)
{
    ShipData  *ship;
    Face      *face;
    u_char     flag;
    int        i;

    if (weapon->owner == ownShip) {
        weapon->count = MINIGUN_FIRE_INTERVAL;
    } else if (weapon->owner == remoteShip) {
        weapon->count = MINIGUN_FIRE_INTERVAL;
    } else if (weapon->count < 2) {
        weapon->count = MINIGUN_FIRE_INTERVAL;
    } else {
        weapon->count = weapon->count - 1;
        return;
    }
    minigunFrame++;
    if (minigunFrame & 1) {
        weapon->count = 0;
        return;
    }
    if (weapon->owner == ownShip) {
        ship = &ships[weapon->owner];
        ship->vpivot.vx -= ship->unitVecNose.vx >> 3;
        ship->vpivot.vy -= ship->unitVecNose.vy >> 3;
        ship->vpivot.vz -= ship->unitVecNose.vz >> 3;
        PlayNote(12, 192, 15, 15, 400);
        PlayNote(5, 96, 15, 15, 400);
    }
    flashNozzleShip = weapon->owner;
    weapon->section = ships[weapon->owner].nearTrkSect;
    face = &track->faces[ships[weapon->owner].nearTrkSect->faceStart];
    do {
        flag = face->flags;
        face++;
    } while (!(flag & 1));
    weapon->accel.vx = ships[weapon->owner].unitVecNose.vx >> 6;
    weapon->accel.vy = ships[weapon->owner].unitVecNose.vy >> 6;
    weapon->accel.vz = ships[weapon->owner].unitVecNose.vz >> 6;
    weapon->velocity.vx = ships[weapon->owner].vpivot.vx >> 6;
    weapon->velocity.vy = ships[weapon->owner].vpivot.vy >> 6;
    weapon->velocity.vz = ships[weapon->owner].vpivot.vz >> 6;
    weapon->velocity.vy += weapon->accel.vy * 8;
    weapon->velocity.vx += weapon->accel.vx * 8;
    weapon->velocity.vz += weapon->accel.vz * 8;
    weapon->velocity.vy += weapon->accel.vy * 8;
    weapon->pos.vx = ships[weapon->owner].ppivot.vx - (weapon->velocity.vx >> 1);
    weapon->pos.vy = ships[weapon->owner].ppivot.vy - (weapon->velocity.vy >> 1);
    weapon->pos.vz = ships[weapon->owner].ppivot.vz - (weapon->velocity.vz >> 1);
    weapon->hdg = ships[weapon->owner].hdg;
    weapon->pitch = ships[weapon->owner].pitch + 0x800;
    weapon->update = UpdateMinigun;
    ships[weapon->owner].targetShip = -1;
    i = snowCount;
    snowCount = (i + 1) % PARTICLE_COUNT;
    particleSlots[i].pos.vx = weapon->pos.vx;
    particleSlots[i].pos.vy = weapon->pos.vy;
    particleSlots[i].pos.vz = weapon->pos.vz;
    particleSlots[i].vel.vx = weapon->velocity.vx;
    particleSlots[i].vel.vy = weapon->velocity.vy;
    particleSlots[i].vel.vz = weapon->velocity.vz;
    particleSlots[i].life = 15;
    if (!(minigunFrame & 7)) {
        particleSlots[i].type = 2;
    } else {
        particleSlots[i].type = 3;
    }
    particleSlots[i].flags = 0;
    minigunAmmo--;
    if ((short)minigunAmmo <= 0) {
        ships[weapon->owner].weapon = 0;
    }
}


void UpdateMine(ShipData *ships, WeaponShape *shapes, WeaponData *weapon, WeaponData *weapons)
{
    VECTOR    distance;
    ShipData *ship;
    Poly      poly;
    int       i;
    short     r;
    register int headToHead;

    weapon->count--;
    if (weapon->count == 0) {
        shapes[weapon->shapeNo].used = 0;
        return;
    }
    weapon->pos.vx += sar(rsin(weapon->shapeNo * 400 + weapon->count * 40), 9);
    weapon->hdg += 128;
    poly.prim = shapes[weapon->shapeNo].shapePtr->primitives;
    r = (rsin(weapon->count * 140) >> 6) + 230;
    if (r > 255) {
        r = 255;
    }
    for (i = 0; i < 8; i++) {
        if (poly.prim->type == TypeF3) {
            poly.f3->colour.r = r;
            poly.f3->colour.g = 0x40;
            poly.f3->colour.b = 0x40;
            poly.f3 += 1;
        }
    }
    SetSkeletonDirectionHPR(shapes[weapon->shapeNo].shapePtr->skeleton, weapon->hdg, 0, 0);
    SetSkeletonPosition(shapes[weapon->shapeNo].shapePtr->skeleton, weapon->pos.vx, weapon->pos.vy, weapon->pos.vz);
    shapes[weapon->shapeNo].shapePtr->skeleton->update = 1;
    i = 0;
    headToHead = 3;
    AsmDrawObject(shapes[weapon->shapeNo].shapePtr, cameraPtr->camPos, 0); /* Port: roll was never passed; 0 stands for the console's stale a2 */
    ship = ships;
nextShip:
    {
        if (!(ship->combatAttr & DESTROYED) && (((i != weapon->owner) && raceType != headToHead) || (raceType == headToHead && (i == ownShip || i == remoteShip) && i != weapon->owner))) {
            distance.vx = (ship->ppivot.vx - weapon->pos.vx) >> 2;
            distance.vy = (ship->ppivot.vy - weapon->pos.vy) >> 2;
            distance.vz = (ship->ppivot.vz - weapon->pos.vz) >> 2;
            if (GetMagnitude(distance) < 100) {
                if (!(ship->attr & SHIELDED)) {
                    weapon->target = i;
                    weapon->count = -1;
                    if (i != ownShip && i != remoteShip) {
                        ship->speed >>= 3;
                    } else {
                        ship->vpivot.vx -= ship->vpivot.vx >> 3;
                        ship->vpivot.vy -= ship->vpivot.vy >> 3;
                        ship->vpivot.vz -= ship->vpivot.vz >> 3;
                        if (i == ownShip) {
                            SetShake(20);
                        }
                    }
                }
                DamageCollide(3, i);
                {
                    short index = -1;
                    weapon->count = index;
                    index = weapon->shapeNo;
                    shapes[index].used = 0;
                }
                PlayDirectionalNote(5, 0, 40, weapon->pos);
                return;
            }
        }
        i++;
        ship++;
        if (i < 15) {
            goto nextShip;
        }
    }
}


void InitQuake(ShipData *shapes, WeaponShape *unused, WeaponData *w, WeaponData *weapons)
{
    Face *node;
    int   id;
    int   pad[6];

    id = w->owner;
    if (id == ownShip) {
        w->count = QUAKE_FRAMES;
    } else if (id == remoteShip) {
        w->count = QUAKE_FRAMES;
    } else if (w->count < 2) {
        w->count = QUAKE_FRAMES;
    } else {
        w->count = w->count - 1;
        return;
    }
    w->section = shapes[w->owner].nearTrkSect;
    node = &track->faces[shapes[w->owner].nearTrkSect->faceStart];
    while ((node->flags & 1) == 0) {
        node++;
    }
    GetFireVector(&w->accel, shapes[w->owner].unitVecNose, node, shapes[w->owner].ppivot);
    PAL_SPEED_VECTOR(w->accel);
    w->velocity.vx = shapes[w->owner].vpivot.vx >> 6;
    w->velocity.vy = shapes[w->owner].vpivot.vy >> 6;
    w->velocity.vz = shapes[w->owner].vpivot.vz >> 6;
    w->quakeBumpStep = 0;
    w->pos.vx = shapes[w->owner].nearTrkSect->nextSection.ptr->nextSection.ptr->centre.vx;
    w->pos.vy = shapes[w->owner].nearTrkSect->nextSection.ptr->nextSection.ptr->centre.vy;
    w->pos.vz = shapes[w->owner].nearTrkSect->nextSection.ptr->nextSection.ptr->centre.vz;
    w->update = UpdateQuake;
}


void InitPlasma(ShipData *shapes, WeaponShape *unused, WeaponData *w, WeaponData *weapons)
{
    Face *node;
    int   id;

    if (plasmaShotBusy != 0) {
        return;
    }
    plasmaShotBusy = 1;
    InitPlasmaShot();
    id = w->owner;
    if (id == ownShip) {
        w->count = PLASMA_FRAMES;
    } else if (id == remoteShip) {
        w->count = PLASMA_FRAMES;
    } else if (w->count < 2) {
        w->count = PLASMA_FRAMES;
    } else {
        w->count = w->count - 1;
        return;
    }
    w->section = shapes[w->owner].nearTrkSect;
    node = &track->faces[shapes[w->owner].nearTrkSect->faceStart];
    while ((node->flags & 1) == 0) {
        node++;
    }
    GetFireVector(&w->accel, shapes[w->owner].unitVecNose, node, shapes[w->owner].ppivot);
    PAL_SPEED_VECTOR(w->accel);
    w->velocity.vx = shapes[w->owner].vpivot.vx >> 6;
    w->velocity.vy = shapes[w->owner].vpivot.vy >> 6;
    w->velocity.vz = shapes[w->owner].vpivot.vz >> 6;
    w->quakeBumpStep = 0;
    w->pos.vx = shapes[w->owner].ppivot.vx;
    w->pos.vy = shapes[w->owner].ppivot.vy;
    w->pos.vz = shapes[w->owner].ppivot.vz;
    w->update = UpdatePlasma;
}


void InitRocket(ShipData *shapes, WeaponShape *slots, WeaponData *w, WeaponData *weapons)
{
    int    i;
    Face  *node;
    u_char flag;
    int    dx;
    int    pad[4];
    int    dy;
    int    dz;

    if (w->owner == ownShip) {
        w->count = ROCKET_FRAMES;
    } else if (w->owner == remoteShip) {
        w->count = ROCKET_FRAMES;
    } else if (w->count < 2) {
        w->count = ROCKET_FRAMES;
    } else {
        w->count = w->count - 1;
        return;
    }
    w->section = shapes[w->owner].nearTrkSect;
    for (i = 0; i < 6; i++) {
        if (slots[i].used == 0) {
            w->shapeNo = i;
            slots[i].used = 1;
            node = &track->faces[shapes[w->owner].nearTrkSect->faceStart];
            do {
                flag = node->flags;
                node++;
                if (flag & 1) {
                    node--;
                    break;
                }
            } while (1);
            GetFireVector(&w->accel, shapes[w->owner].unitVecNose, node, shapes[w->owner].ppivot);
            PAL_SPEED_VECTOR(w->accel);
            w->quakeBumpStep = 0;
            w->velocity.vx = w->accel.vx << 3;
            w->velocity.vy = w->accel.vy << 3;
            w->velocity.vz = w->accel.vz << 3;
            w->pos.vx = shapes[w->owner].ppivot.vx;
            w->pos.vy = shapes[w->owner].ppivot.vy;
            w->pos.vz = shapes[w->owner].ppivot.vz;
            SetSkeletonDirectionHPR(slots[w->shapeNo].shapePtr->skeleton, shapes[w->owner].hdg, shapes[w->owner].pitch, shapes[w->owner].roll);
            w->update = UpdateRocket;
            return;
        }
    }
    w->count = 0;
}


void InitRocketLeft(ShipData *shapes, WeaponShape *slots, WeaponData *w, WeaponData *weapons)
{
    int    i;
    Face  *node;
    u_char flag;
    int    pad[2];
    VECTOR offset;

    if (w->owner == ownShip) {
        w->count = ROCKET_FRAMES;
    } else if (w->owner == remoteShip) {
        w->count = ROCKET_FRAMES;
    } else if (w->count < 2) {
        w->count = ROCKET_FRAMES;
    } else {
        w->count = w->count - 1;
        return;
    }
    w->section = shapes[w->owner].nearTrkSect;
    for (i = 0; i < 6; i++) {
        if (slots[i].used == 0) {
            w->shapeNo = i;
            slots[i].used = 1;
            node = &track->faces[shapes[w->owner].nearTrkSect->faceStart];
            do {
                flag = node->flags;
                node++;
                if (flag & 1) {
                    node--;
                    break;
                }
            } while (1);
            GetFireVector(&w->accel, shapes[w->owner].unitVecNose, node, shapes[w->owner].ppivot);
            PAL_SPEED_VECTOR(w->accel);
            w->velocity.vx = w->accel.vx << 3;
            w->velocity.vy = w->accel.vy << 3;
            w->velocity.vz = w->accel.vz << 3;
            w->velocity.vx += shapes[w->owner].unitVecWing.vx >> 6;
            w->velocity.vy += shapes[w->owner].unitVecWing.vy >> 6;
            w->velocity.vz += shapes[w->owner].unitVecWing.vz >> 6;
            offset.vx = shapes[w->owner].unitVecWing.vx >> 4;
            offset.vy = shapes[w->owner].unitVecWing.vy >> 4;
            offset.vz = shapes[w->owner].unitVecWing.vz >> 4;
            offset.vx -= shapes[w->owner].unitVecNose.vx >> 4;
            offset.vy -= shapes[w->owner].unitVecNose.vy >> 4;
            offset.vz -= shapes[w->owner].unitVecNose.vz >> 4;
            w->pos.vx = shapes[w->owner].ppivot.vx + offset.vx;
            w->pos.vy = shapes[w->owner].ppivot.vy + offset.vy;
            w->pos.vz = shapes[w->owner].ppivot.vz + offset.vz;
            SetSkeletonDirectionHPR(slots[w->shapeNo].shapePtr->skeleton, shapes[w->owner].hdg, shapes[w->owner].pitch, shapes[w->owner].roll);
            w->update = UpdateRocket;
            return;
        }
    }
    w->count = 0;
}


void InitRocketRight(ShipData *shapes, WeaponShape *slots, WeaponData *w, WeaponData *weapons)
{
    int    i;
    Face  *node;
    u_char flag;
    int    pad[2];
    VECTOR offset;

    if (w->owner == ownShip) {
        w->count = ROCKET_FRAMES;
    } else if (w->owner == remoteShip) {
        w->count = ROCKET_FRAMES;
    } else if (w->count < 2) {
        w->count = ROCKET_FRAMES;
    } else {
        w->count = w->count - 1;
        return;
    }
    w->section = shapes[w->owner].nearTrkSect;
    for (i = 0; i < 6; i++) {
        if (slots[i].used == 0) {
            w->shapeNo = i;
            slots[i].used = 1;
            node = &track->faces[shapes[w->owner].nearTrkSect->faceStart];
            do {
                flag = node->flags;
                node++;
                if (flag & 1) {
                    node--;
                    break;
                }
            } while (1);
            GetFireVector(&w->accel, shapes[w->owner].unitVecNose, node, shapes[w->owner].ppivot);
            PAL_SPEED_VECTOR(w->accel);
            w->velocity.vx = w->accel.vx << 3;
            w->velocity.vy = w->accel.vy << 3;
            w->velocity.vz = w->accel.vz << 3;
            w->velocity.vx -= shapes[w->owner].unitVecWing.vx >> 6;
            w->velocity.vy -= shapes[w->owner].unitVecWing.vy >> 6;
            w->velocity.vz -= shapes[w->owner].unitVecWing.vz >> 6;
            offset.vx = -shapes[w->owner].unitVecWing.vx >> 4;
            offset.vy = -shapes[w->owner].unitVecWing.vy >> 4;
            offset.vz = -shapes[w->owner].unitVecWing.vz >> 4;
            offset.vx -= shapes[w->owner].unitVecNose.vx >> 4;
            offset.vy -= shapes[w->owner].unitVecNose.vy >> 4;
            offset.vz -= shapes[w->owner].unitVecNose.vz >> 4;
            w->pos.vx = shapes[w->owner].ppivot.vx + offset.vx;
            w->pos.vy = shapes[w->owner].ppivot.vy + offset.vy;
            w->pos.vz = shapes[w->owner].ppivot.vz + offset.vz;
            SetSkeletonDirectionHPR(slots[w->shapeNo].shapePtr->skeleton, shapes[w->owner].hdg, shapes[w->owner].pitch, shapes[w->owner].roll);
            w->update = UpdateRocket;
            return;
        }
    }
    w->count = 0;
}


/* Flies an electric bolt towards its target ship, shocking the first ship it touches. */
void UpdateElecBolt(ShipData *ships, WeaponShape *shapes, WeaponData *weapon, WeaponData *weapons)
{
    VECTOR    distance;
    VECTOR    targetVector;
    VECTOR    bestPath;
    VECTOR    facePoint;
    Face     *facePtr;
    ShipData *ship;
    int       i;
    int       mag;
    int       temp;
    int       vhdg;
    int       vpitch;
    int       alpha;
    u_char    flag;

    if (weapon->target != -1) {
        targetVector.vx = (ships[weapon->target].ppivot.vx - weapon->pos.vx) >> 3;
        targetVector.vy = (ships[weapon->target].ppivot.vy - weapon->pos.vy) >> 3;
        targetVector.vz = (ships[weapon->target].ppivot.vz - weapon->pos.vz) >> 3;
        temp = SquareRoot0(targetVector.vx * targetVector.vx + targetVector.vz * targetVector.vz);
        vhdg = -ratan2(targetVector.vx, targetVector.vz) - weapon->hdg;
        vpitch = -ratan2(targetVector.vy, temp) - weapon->pitch;
    } else {
        vhdg = 0;
        vpitch = 0;
    }
    if (vhdg > 2048) {
        vhdg -= 4096;
    } else if (vhdg < -2048) {
        vhdg += 4096;
    }
    if (vpitch <= 2048) {
        goto lowPitch;
    }
    vpitch -= 4096;
    goto steer;
expired:
    weapon->count = -2;
    shapes[weapon->shapeNo].used = 0;
    PlayDirectionalNote(6, 0, 40, weapon->pos);
    return;
lowPitch:
    if (vpitch < -2048) {
        vpitch += 4096;
    }
steer:
    weapon->hdg += vhdg >> 2;
    weapon->pitch += vpitch >> 2;
    weapon->hdg = ang(weapon->hdg);
    weapon->pitch = ang(weapon->pitch);
    weapon->accel.vx = (-rsin(weapon->hdg) * rcos(weapon->pitch)) >> 16;
    weapon->accel.vy = -rsin(weapon->pitch) >> 4;
    weapon->accel.vz = (rcos(weapon->hdg) * rcos(weapon->pitch)) >> 16;
    PAL_SPEED_VECTOR(weapon->accel);
    weapon->velocity.vx += weapon->accel.vx;
    weapon->velocity.vy += weapon->accel.vy;
    weapon->velocity.vz += weapon->accel.vz;
    weapon->velocity.vx -= weapon->velocity.vx >> 2;
    weapon->velocity.vy -= weapon->velocity.vy >> 2;
    weapon->velocity.vz -= weapon->velocity.vz >> 2;
    weapon->pos.vx += weapon->velocity.vx;
    weapon->pos.vy += weapon->velocity.vy;
    weapon->pos.vz += weapon->velocity.vz;
    weapon->section = FindNearestSection(weapon->section, weapon->pos);
    facePtr = track->faces + weapon->section->faceStart;
    do {
        flag = facePtr->flags;
        facePtr++;
        if (flag & 1) {
            facePtr--;
            break;
        }
    } while (1);
    facePoint.vx = track->vertices[facePtr->vertex[0]].vx;
    facePoint.vy = track->vertices[facePtr->vertex[0]].vy;
    facePoint.vz = track->vertices[facePtr->vertex[0]].vz;
    alpha = PlaneLineCollision(facePoint, weapon->pos, facePtr->normal);
    if (alpha < 2000) {
        weapon->pos.vx += ((200 - alpha) * facePtr->normal.vx) >> 13;
        weapon->pos.vy += ((200 - alpha) * facePtr->normal.vy) >> 13;
        weapon->pos.vz += ((200 - alpha) * facePtr->normal.vz) >> 13;
    }
    weapon->count--;
    if (weapon->count == 0) {
        shapes[weapon->shapeNo].used = 0;
    }
    SetSkeletonPosition(shapes[weapon->shapeNo].shapePtr->skeleton, weapon->pos.vx, weapon->pos.vy, weapon->pos.vz);
    SetSkeletonDirectionHPR(shapes[weapon->shapeNo].shapePtr->skeleton, weapon->hdg, weapon->pitch, 0);
    shapes[weapon->shapeNo].shapePtr->skeleton->update = 1;
    AsmDrawObject(shapes[weapon->shapeNo].shapePtr, cameraPtr->camPos, 0); /* Port: roll was never passed; 0 stands for the console's stale a2 */
    for (i = 0; i < 15; i++) {
        ship = ships + i;
        if (!(ship->combatAttr & DESTROYED) && (((i != weapon->owner) && raceType != 3) || (raceType == 3 && (i == ownShip || i == remoteShip) && i != weapon->owner))) {
            distance.vx = (ship->ppivot.vx - weapon->pos.vx) >> 3;
            distance.vy = (ship->ppivot.vy - weapon->pos.vy) >> 3;
            distance.vz = (ship->ppivot.vz - weapon->pos.vz) >> 3;
            if (GetMagnitude(distance) < 100) {
                if (!(ship->attr & SHIELDED)) {
                    ship->attr |= ELECTROED;
                    ship->electroCount = REGION(140, 100);
                }
                DamageCollide(5, i);
                AddRandomParticles(particleSlots, 64, &weapon->pos, &weapon->velocity);
                weapon->count = -1;
                weapon->target = i;
                shapes[weapon->shapeNo].used = 0;
                return;
            }
        }
    }
    facePtr = track->faces + weapon->section->faceStart;
    for (i = 0; i < weapon->section->faceCount; i++) {
        facePoint.vx = track->vertices[facePtr->vertex[0]].vx;
        facePoint.vy = track->vertices[facePtr->vertex[0]].vy;
        facePoint.vz = track->vertices[facePtr->vertex[0]].vz;
        if (PlaneLineCollision(facePoint, weapon->pos, facePtr->normal) <= 0) {
            goto expired;
        }
        facePtr++;
    }
    Weapon2Mine(weapon, weapons, shapes);
}


void InitElecBolt(ShipData *shapes, WeaponShape *slots, WeaponData *w, WeaponData *weapons)
{
    int i;

    if (w->owner == ownShip) {
        w->count = ELECBOLT_FRAMES;
    } else if (w->owner == remoteShip) {
        w->count = ELECBOLT_FRAMES;
    } else if (w->count < 2) {
        w->count = ELECBOLT_FRAMES;
    } else {
        w->count = w->count - 1;
        return;
    }
    w->section = shapes[w->owner].nearTrkSect;
    for (i = 0; i < 3; i++) {
        if (slots[i + 28].used == 0) {
            w->shapeNo = i + 28;
            slots[i + 28].used = 1;
            w->target = shapes[w->owner].targetShip;
            w->accel.vx = shapes[w->owner].unitVecNose.vx >> 8;
            w->accel.vy = shapes[w->owner].unitVecNose.vy >> 8;
            w->accel.vz = shapes[w->owner].unitVecNose.vz >> 8;
            PAL_SPEED_VECTOR(w->accel);
            w->velocity.vx = shapes[w->owner].vpivot.vx >> 6;
            w->velocity.vy = shapes[w->owner].vpivot.vy >> 6;
            w->velocity.vz = shapes[w->owner].vpivot.vz >> 6;
            w->pos.vx = shapes[w->owner].ppivot.vx;
            w->pos.vy = shapes[w->owner].ppivot.vy;
            w->pos.vz = shapes[w->owner].ppivot.vz;
            SetSkeletonDirectionHPR(slots[w->shapeNo].shapePtr->skeleton, shapes[w->owner].hdg, shapes[w->owner].pitch, shapes[w->owner].roll);
            w->hdg = shapes[w->owner].hdg;
            w->pitch = shapes[w->owner].pitch;
            w->update = UpdateElecBolt;
            shapes[w->owner].targetShip = -1;
            return;
        }
    }
    w->count = 0;
}


void InitMine(ShipData *ships, WeaponShape *weaponShapes, WeaponData *weapon, WeaponData *weapons)
{
    int i;

    if (weapon->count == 1) {
        weapon->section = ships[weapon->owner].nearTrkSect;
        for (i = 0; i < 15; i++) {
            if (weaponShapes[6 + i].used == 0) {
                weapon->shapeNo = 6 + i;
                weaponShapes[6 + i].used = 1;
                weapon->pos.vx = ships[weapon->owner].ppivot.vx;
                weapon->pos.vy = ships[weapon->owner].ppivot.vy;
                weapon->pos.vz = ships[weapon->owner].ppivot.vz;
                weapon->hdg = 0;
                if (weapon->owner == ownShip) {
                    PlayNote(11, 200, 15, 15, 256);
                }
                weapon->update = UpdateMine;
                weapon->count = REGION(450, 350);
                return;
            }
        }
        weapon->count = 0;
    } else if (weapon->count > 0) {
        weapon->count--;
    }
}


void InitSmartBomb(ShipData *ships, WeaponShape *weaponShapes, WeaponData *weapon, WeaponData *weapons)
{
    int i;

    weapon->section = ships[weapon->owner].nearTrkSect;
    for (i = 0; i < 15; i++) {
        if (weaponShapes[6 + i].used == 0) {
            weapon->shapeNo = 6 + i;
            weaponShapes[6 + i].used = 1;
            weapon->pos.vx = ships[weapon->owner].ppivot.vx;
            weapon->pos.vy = ships[weapon->owner].ppivot.vy;
            weapon->pos.vz = ships[weapon->owner].ppivot.vz;
            weapon->velocity.vx = ships[weapon->owner].vpivot.vx >> 5;
            weapon->velocity.vy = ships[weapon->owner].vpivot.vy >> 5;
            weapon->velocity.vz = ships[weapon->owner].vpivot.vz >> 5;
            weapon->hdg = 0;
            if (weapon->owner == ownShip) {
                PlayNote(11, 200, 15, 15, 256);
            }
            weapon->update = UpdateSmartBomb;
            weapon->count = PAL_TIME(24);
            return;
        }
    }
    weapon->count = 0;
}


/* Sets off every mine that is in the way of a bump-stopped weapon; the ship's own mine goes too. */
void KillMines(WeaponData *ship, WeaponData *bumps, WeaponShape *bars)
{
    int i;

    AddExplode(ship->pos.vx, ship->pos.vy, ship->pos.vz, -1);
    for (i = 0; i < 30; i++) {
        if (bumps[i].type == 3 && bumps[i].count > 0) {
            bumps[i].count = 0;
            ship->count = 0;
            AddExplode(bumps[i].pos.vx, bumps[i].pos.vy, bumps[i].pos.vz, -1);
            bars[bumps[i].shapeNo].used = 0;
            bars[ship->shapeNo].used = 0;
            PlayDirectionalNote(6, 0, 0x28, ship->pos);
        }
    }
}


void UpdateSmartBomb(ShipData *ships, WeaponShape *shapes, WeaponData *weapon, WeaponData *weapons)
{
    u8 padding[16];
    int mask;
    TrackCamera *camera;
    int vx;
    int vy;
    int newVy;
    int vz;
    int explodeX;
    int explodeY;
    int explodeX2;
    int explodeY2;
    int shipNo;
    ShipData *ship;
    u16 pitch;
    u16 hdg;
    u16 count;
    int rnd;
    Skeleton *cameraValue;
    Object *shapeObj;

    weapon->count--;
    count = weapon->count;
    if ((count << 0x10) == 0) {
        fadeAmount = 0xFF;
        PlayNote(5, 0xFF, 0xF, 0xF, 0x100);
        KillMines(weapon, weapons, shapes);
        shipNo = 0;
        ship = ships;
shiploop:
        {
            mask = 1 << shipNo;
            if (((proximity & mask) && (shipNo != weapon->owner)) && (!(ship->combatAttr & DESTROYED))) {
                DamageCollide(0xE, shipNo);
                if (!(ship->attr & SHIELDED)) {
                    if ((shipNo != ownShip) && (shipNo != remoteShip)) {
                        ship->speed = (u16)(((u16)ship->speed) >> 1);
                        ship->vroll = (s16)(ship->vroll + 0x1388);
                        ship->ahdg = (s16)(((s32)(newRand() - 0x4000)) >> 1);
                    } else {
                        vx = ship->vpivot.vx;
                        vy = ship->vpivot.vy;
                        vx -= (vx * 3) >> 1;
                        newVy = vy - ((vy * 3) >> 1);
                        ship->vpivot.vx = vx;
                        vz = ship->vpivot.vz;
                        ship->vpivot.vy = newVy;
                        ship->vpivot.vz = vz - ((vz * 3) >> 1);
                        rnd = newRand();
                        pitch = ship->vroll + (rnd % 250);
                        ship->vroll = pitch;
                        ship->ahdg = (s16)(((s32)(newRand() - 0x4000)) >> 3);
                        if (shipNo == ownShip) {
                            SetShake(0x14);
                        }
                    }
                    explodeX = (ship->ppivot.vx + (newRand() & 0x3FF)) - 0x200;
                    explodeY = ship->ppivot.vy - (newRand() & 0x1FF);
                    AddExplode(explodeX, explodeY, (ship->ppivot.vz + (newRand() & 0x3FF)) - 0x200, -1);
                    explodeX2 = (ship->ppivot.vx + (newRand() & 0x3FF)) - 0x200;
                    explodeY2 = ship->ppivot.vy - (newRand() & 0x1FF);
                    AddExplode(explodeX2, explodeY2, (ship->ppivot.vz + (newRand() & 0x3FF)) - 0x200, -1);
                    PlayDirectionalNote(5, 0, 0x28, ship->ppivot);
                }
            }
            shipNo += 1;
            ship++;
        }
        if (shipNo < 0xF) goto shiploop;
        weapon->count = 0;
        shapes[weapon->shapeNo].used = 0;
        return;
    }
    weapon->pos.vy -= 0xA;
    weapon->velocity.vx -= weapon->velocity.vx >> 2;
    weapon->velocity.vy -= weapon->velocity.vy >> 2;
    weapon->velocity.vz -= weapon->velocity.vz >> 2;
    weapon->pos.vx += weapon->velocity.vx;
    weapon->pos.vy += weapon->velocity.vy;
    weapon->pos.vz += weapon->velocity.vz;
    hdg = weapon->hdg + 0x100;
    weapon->hdg = hdg;
    SetSkeletonDirectionHPR(shapes[weapon->shapeNo].shapePtr->skeleton, (s16)hdg, 0, 0);
    SetSkeletonPosition(shapes[weapon->shapeNo].shapePtr->skeleton, weapon->pos.vx, weapon->pos.vy, weapon->pos.vz);
    shapes[weapon->shapeNo].shapePtr->skeleton->update = 1;
    camera = cameraPtr;
    cameraValue = camera->camPos;
    shapeObj = shapes[weapon->shapeNo].shapePtr;
    AsmDrawObject(shapeObj, cameraValue, 0); /* Port: roll was never passed; 0 stands for the console's stale a2 */
}


void InitShield(ShipData *ships, WeaponShape *slots, WeaponData *w, WeaponData *weapons)
{
    int i;

    shieldGreen[w->owner] = 0;
    shieldRed[w->owner] = 0;
    w->section = ships[w->owner].nearTrkSect;
    for (i = 0; i < 3; i++) {
        if (slots[i + 25].used == 0) {
            w->shapeNo = i + 25;
            slots[i + 25].used = 1;
            w->pos.vx = ships[w->owner].ppivot.vx;
            w->pos.vy = ships[w->owner].ppivot.vy;
            w->pos.vz = ships[w->owner].ppivot.vz;
            SetSkeletonDirectionHPR(slots[w->shapeNo].shapePtr->skeleton, ships[w->owner].hdg, ships[w->owner].pitch, ships[w->owner].roll);
            w->update = UpdateShield;
            ships[w->owner].attr |= SHIELDED;
            return;
        }
    }
    w->count = 0;
}


void UpdateShield(ShipData *ships, WeaponShape *shapes, WeaponData *weapon, WeaponData *weapons)
{
    VECTOR  distance;
    Face   *facePtr;
    int     i;
    int     k;
    int     j;
    int     mag;
    VECTOR  facePoint;
    Poly    poly;
    short   primitiveCount;
    short  *coords;
    short   col0;
    short   col1;
    short   col2;
    short   col3;
    int     shieldID;
    short   fade;
    DVECTOR screen0;
    DVECTOR screen1;
    DVECTOR screen2;
    DVECTOR screen3;

    if (shieldGreen[weapon->owner] > 0) {
        shieldGreen[weapon->owner] -= 10;
    }
    if (shieldRed[weapon->owner] > 0) {
        shieldRed[weapon->owner] -= 10;
    }
    if (shieldGreen[weapon->owner] < 0) {
        shieldGreen[weapon->owner] = 0;
    }
    if (shieldRed[weapon->owner] < 0) {
        shieldRed[weapon->owner] = 0;
    }
    if (ships[weapon->owner].attr & VIEW_IN) {
        weapon->pos.vx = ships[weapon->owner].ppivot.vx + (ships[weapon->owner].unitVecNose.vx >> 6);
        weapon->pos.vy = ships[weapon->owner].ppivot.vy + (ships[weapon->owner].unitVecNose.vy >> 6);
        weapon->pos.vz = ships[weapon->owner].ppivot.vz + (ships[weapon->owner].unitVecNose.vz >> 6);
    } else {
        weapon->pos.vx = ships[weapon->owner].ppivot.vx;
        weapon->pos.vy = ships[weapon->owner].ppivot.vy;
        weapon->pos.vz = ships[weapon->owner].ppivot.vz;
    }
    weapon->count--;
    fade = 0;
    if (weapon->owner == ownShip) {
        if (ships[weapon->owner].attr & VIEW_IN) {
            SfxEffectLRPan(ambientVoice1, ((rsin(weapon->count * 32) * 14) >> 12) + 15);
            SfxEffectFBPan(ambientVoice1, ((rcos(weapon->count * 64) * 14) >> 12) + 15);
            SetVolume(ambientVoice1, 255);
        } else {
            SetVolume(ambientVoice1, 245);
            SfxEffectLRPan(ambientVoice1, 15);
            SfxEffectFBPan(ambientVoice1, 30);
        }
    }
    if (weapon->count < 20) {
        fade = (20 - weapon->count) << 1;
    }
    if (weapon->count == 0 || (ships[weapon->owner].attr & RESCUE)) {
        weapon->count = 0;
        fadeRed = 0;
        fadeBlue = 0;
        fadeGreen = 0;
        shapes[weapon->shapeNo].used = 0;
        ships[weapon->owner].attr &= ~SHIELDED;
        if (weapon->owner == ownShip) {
            SetVolume(ambientVoice1, 0);
        }
    }
    if (!(ships[weapon->owner].attr & VIEW_IN) || weapon->owner != ownShip) {
        SetSkeletonPosition(shapes[weapon->shapeNo].shapePtr->skeleton, weapon->pos.vx, weapon->pos.vy, weapon->pos.vz);
        SetSkeletonDirectionHPR(shapes[weapon->shapeNo].shapePtr->skeleton, ships[weapon->owner].hdg, ships[weapon->owner].pitch, ships[weapon->owner].roll);
        poly.prim = shapes[weapon->shapeNo].shapePtr->primitives;
        primitiveCount = shapes[weapon->shapeNo].shapePtr->primitiveCount;
        for (k = 0; k < primitiveCount; k++) {
            switch (poly.prim->type) {
            case TypeG3:
                coords = poly.g3->coords;
                col0 = (rsin(weapon->count * coords[0]) >> 5) + 128;
                col1 = (rsin(weapon->count * coords[1]) >> 5) + 128;
                col2 = (rsin(weapon->count * coords[2]) >> 5) + 128;
                if (col0 > 255) col0 = 255;
                if (col1 > 255) col1 = 255;
                if (col2 > 255) col2 = 255;
                col0 = col0 - fade;
                col1 = col1 - fade;
                col2 = col2 - fade;
                if (col0 < 0) col0 = 0;
                if (col1 < 0) col1 = 0;
                if (col2 < 0) col2 = 0;
                poly.g3->colour[0].b = (u_char)col0 >> 1;
                poly.g3->colour[1].b = (u_char)col1 >> 1;
                poly.g3->colour[2].b = (u_char)col2 >> 1;
                poly.g3->colour[0].g = shieldGreen[weapon->owner];
                poly.g3->colour[1].g = shieldGreen[weapon->owner];
                poly.g3->colour[2].g = shieldGreen[weapon->owner];
                poly.g3->colour[0].r = shieldRed[weapon->owner];
                poly.g3->colour[1].r = shieldRed[weapon->owner];
                poly.g3->colour[2].r = shieldRed[weapon->owner];
                poly.g3 += 1;
                break;
            case TypeG4:
                coords = poly.g4->coords;
                col0 = (rsin(weapon->count * coords[0]) >> 5) + 128;
                col1 = (rsin(weapon->count * coords[1]) >> 5) + 128;
                col2 = (rsin(weapon->count * coords[2]) >> 5) + 128;
                col3 = (rsin(weapon->count * coords[3]) >> 5) + 128;
                if (col0 > 255) col0 = 255;
                if (col1 > 255) col1 = 255;
                if (col2 > 255) col2 = 255;
                if (col3 > 255) col2 = 255;
                col0 = col0 - fade;
                col1 = col1 - fade;
                col2 = col2 - fade;
                col3 = col3 - fade;
                if (col0 < 0) col0 = 0;
                if (col1 < 0) col1 = 0;
                if (col2 < 0) col2 = 0;
                if (col3 < 0) col3 = 0;
                poly.g4->colour[0].b = (u_char)col0 >> 1;
                poly.g4->colour[1].b = (u_char)col1 >> 1;
                poly.g4->colour[2].b = (u_char)col2 >> 1;
                poly.g4->colour[3].b = (u_char)col3 >> 1;
                poly.g4->colour[0].g = shieldGreen[weapon->owner];
                poly.g4->colour[1].g = shieldGreen[weapon->owner];
                poly.g4->colour[2].g = shieldGreen[weapon->owner];
                poly.g4->colour[3].g = shieldGreen[weapon->owner];
                poly.g4->colour[0].r = shieldRed[weapon->owner];
                poly.g4->colour[1].r = shieldRed[weapon->owner];
                poly.g4->colour[2].r = shieldRed[weapon->owner];
                poly.g4->colour[3].r = shieldRed[weapon->owner];
                poly.g4 += 1;
                break;
            }
        }
        shapes[weapon->shapeNo].shapePtr->skeleton->update = 1;
        shieldID = weapon->shapeNo - 25;
        TransformTransparentObject(shapes[weapon->shapeNo].shapePtr, cameraPtr->camPos, shieldID);
    } else {
        col0 = (rsin(weapon->count * 128) >> 7) + 64;
        col0 = col0 - fade;
        if (col0 < 0) col0 = 0;
        fadeRed = shieldRed[weapon->owner];
        fadeGreen = shieldGreen[weapon->owner];
        fadeBlue = col0;
        fadeActive = 1;
    }
}


void CycleWeaponGrids(ShipData *ships, struct WeaponGridZone *grids, int noGrids)
{
    struct WeaponGridZone *grid;
    struct WeaponGridZone *current;
    ShipData *ship;
    Face *face;
    int i;
    int k;
    int white;
    SVECTOR colour;

    if (cheatMinigun) {
        ships[ownShip].weapon = 15;
    }
    if (noGrids > 0) {
        white = 255;
        k = 0;
        do {
            grid = grids + k;
            face = track->faces + grid->section->faceStart;
            do {
                u8 flag = face->flags;
                face++;
                if (flag & 1) {
                    break;
                }
            } while (1);
            face--;
            for (i = 0; i < 12; i++) {
                current = grids - (-k);
                ship = ships + i;
                if (!(ship->combatAttr & DESTROYED)) {
                    if (((face->flags & 2) && (ship->attr & LSIDE)) && (current->section == ship->nearTrkSect) && (current->count >= 0)) {
                        current->count = -10;
                        if ((i == ownShip) || (i == remoteShip)) {
                            if (!ship->weapon) {
                                if (i == ownShip) {
                                    PlayNote(14, 192, 15, 15, 256);
                                }
                                GetNewWeapon(ships, i);
                            }
                        } else {
                            GetNewDroneWeapon(ships, i);
                        }
                    }
                    face++;
                    if (((face->flags & 8) && (!(ship->attr & LSIDE))) && (current->section == ship->nearTrkSect) && (current->count >= 0)) {
                        current->count = -10;
                        if ((i == ownShip) || (i == remoteShip)) {
                            if (!ship->weapon) {
                                if (i == ownShip) {
                                    PlayNote(14, 192, 15, 15, 256);
                                }
                                GetNewWeapon(ships, i);
                            }
                        } else {
                            GetNewDroneWeapon(ships, i);
                        }
                    }
                    face--;
                }
            }

            if (grid->count >= 0) {
                colour.vx = (rsin(grid->angle) >> 5) + 128;
                colour.vy = (rcos(grid->angle) >> 5) + 128;
                colour.vz = (rsin(-grid->angle) >> 5) + 128;
                if (colour.vx > 255) {
                    colour.vx = white;
                }
                if (colour.vy > 255) {
                    colour.vy = white;
                }
                if (colour.vz > 255) {
                    colour.vz = white;
                }
                grid->r = colour.vx;
                grid->g = colour.vy;
                grid->b = colour.vz;
            } else {
                grid->r = 128;
                grid->g = 128;
                grid->b = 128;
            }
            if (grid->count < 0) {
                grid->count++;
            }
            grid->angle += 32;
            if (grid->attr & 2) {
                face->red = grid->r;
                face->green = grid->g;
                face->blue = grid->b;
            }
            face++;
            if (grid->attr & 8) {
                face->red = grid->r;
                face->green = grid->g;
                face->blue = grid->b;
            }
            k++;
        } while (grids + k < grids + noGrids);
    }
}


/* Port: NormalClip takes each screen point packed in one word (x in the low half, y in the high half),
   as the console passed a DVECTOR by value. */
static int PackScreenPoint(DVECTOR point)
{
    return (int)((u_short)point.vx | ((u32)(u_short)point.vy << 16));
}


void TransformTransparentObject(Object *object, void *camera, int shieldID)
{
    Poly      poly;
    G4       *nextPoly;
    short     i, count, vertexCount, coord0, coord1, coord2, coord3;
    void     *vertices;
    int       mode, average;
    DVECTOR  *screen;
    u_short  *depth, *flags;
    ShadedTri  *tri;
    ShadedQuad *quad;

    SetSkeletonTransform(object->skeleton);
    poly.prim = object->primitives;
    count = object->primitiveCount;
    mode = 0;
    vertices = object->vertices;
    vertexCount = object->vertexCount;
    primsDrawn += count;
    if (vertexCount <= 128) {
        /* Port: the console's scratchpad RAM at 0x1F800000 */
        screen = (DVECTOR *) PsxScratchpad;
        depth = (u_short *) (PsxScratchpad + 0x200);
        flags = (u_short *) (PsxScratchpad + 0x300);
    } else {
        screen = StaticScreen;
        depth = StaticDepth;
        flags = StaticFlags;
    }
    RotTransPersN(vertices, screen, depth, flags, vertexCount);
    i = 0;
    if (count <= 0) {
        return;
    }
    nextPoly = poly.g4 + 1;
    do {
        switch (poly.prim->type) {
        case 5:
            coord0 = ((G3 *) (nextPoly - 1))->coords[0];
            coord1 = ((G3 *) (nextPoly - 1))->coords[1];
            coord2 = ((G3 *) (nextPoly - 1))->coords[2];
            if (!((flags[coord0] | flags[coord1] | flags[coord2]) & 0x460)) {
                if (NormalClip(PackScreenPoint(screen[coord0]), PackScreenPoint(screen[coord1]), PackScreenPoint(screen[coord2])) > 0) {
                    tri = (ShadedTri *) nextPrim.ptr;
                    SetPolyG3((POLY_G3 *) tri);
                    SetSemiTrans(tri, 1);
                    tri->colour0.r = ((G3 *) (nextPoly - 1))->colour[0].r;
                    tri->colour0.g = ((G3 *) (nextPoly - 1))->colour[0].g;
                    tri->colour0.b = ((G3 *) (nextPoly - 1))->colour[0].b;
                    tri->colour1 = ((G3 *) (nextPoly - 1))->colour[1];
                    tri->colour2 = ((G3 *) (nextPoly - 1))->colour[2];
                    tri->xy0 = screen[coord0];
                    tri->xy1 = screen[coord1];
                    tri->xy2 = screen[coord2];
                    average = AverageZ3(depth[coord0], depth[coord1], depth[coord2]);
                    AddPrim(OT[CurrentScreen] + average, tri);
                    nextPrim.ptr += sizeof(ShadedTri);
                    AddPrim(OT[CurrentScreen] + average, &shieldParams[mode + shieldID * 64][CurrentScreen]);
                    mode++;
                    nextPrim.ptr += sizeof(DR_MODE);
                }
            }
            poly.g3++;
            nextPoly = (G4 *) ((char *) nextPoly + sizeof(G3));
            break;
        case 7:
            coord0 = nextPoly[-1].coords[0];
            coord1 = nextPoly[-1].coords[1];
            coord2 = nextPoly[-1].coords[2];
            coord3 = nextPoly[-1].coords[3];
            if (!((flags[coord0] | flags[coord1] | flags[coord2] | flags[coord3]) & 0x460)) {
                if (NormalClip(PackScreenPoint(screen[coord0]), PackScreenPoint(screen[coord1]), PackScreenPoint(screen[coord2])) > 0) {
                    quad = (ShadedQuad *) nextPrim.ptr;
                    SetPolyG4((POLY_G4 *) quad);
                    SetSemiTrans(quad, 1);
                    quad->colour0.r = nextPoly[-1].colour[0].r;
                    quad->colour0.g = nextPoly[-1].colour[0].g;
                    quad->colour0.b = nextPoly[-1].colour[0].b;
                    quad->colour1 = nextPoly[-1].colour[1];
                    quad->colour2 = nextPoly[-1].colour[2];
                    quad->colour3 = nextPoly[-1].colour[3];
                    quad->xy0 = screen[coord0];
                    quad->xy1 = screen[coord1];
                    quad->xy2 = screen[coord2];
                    quad->xy3 = screen[coord3];
                    average = AverageZ4(depth[coord0], depth[coord1], depth[coord2], depth[coord3]);
                    AddPrim(OT[CurrentScreen] + average, quad);
                    nextPrim.ptr += sizeof(ShadedQuad);
                    AddPrim(OT[CurrentScreen] + average, &shieldParams[mode + shieldID * 64][CurrentScreen]);
                    mode++;
                    nextPrim.ptr += sizeof(DR_MODE);
                }
            }
            {
                G4 *current = nextPoly;
                nextPoly = current + 1;
                poly.g4 = current;
            }
            break;
        }
    } while (++i < count);
}


void GetFireVector(VECTOR *result, VECTOR target, Face *facePtr, VECTOR shipPos)
{
    int    alpha;
    int    origAlpha;
    VECTOR facePoint;

    facePoint.vx = track->vertices[facePtr->vertex[0]].vx;
    facePoint.vy = track->vertices[facePtr->vertex[0]].vy;
    facePoint.vz = track->vertices[facePtr->vertex[0]].vz;
    target.vx = shipPos.vx + (target.vx >> 6);
    target.vy = shipPos.vy + (target.vy >> 6);
    target.vz = shipPos.vz + (target.vz >> 6);
    alpha = PlaneLineCollision(facePoint, target, facePtr->normal);
    origAlpha = PlaneLineCollision(facePoint, shipPos, facePtr->normal);
    alpha = (alpha * 31) >> 5;
    alpha -= origAlpha;
    result->vx = target.vx - ((alpha * facePtr->normal.vx) >> 12);
    result->vy = target.vy - ((alpha * facePtr->normal.vy) >> 12);
    result->vz = target.vz - ((alpha * facePtr->normal.vz) >> 12);
    result->vx -= shipPos.vx;
    result->vy -= shipPos.vy;
    result->vz -= shipPos.vz;
}


void Weapon2Mine(WeaponData *weapon, WeaponData *weaponIndex, WeaponShape *weaponShapes)
{
    int     i;
    VECTOR  distance;
    int     mag;

    for (i = 0; i < 30; i++) {
        if ((weaponIndex[i].type == 3) && (weaponIndex[i].count > 0)) {
            distance.vx = (weaponIndex[i].pos.vx - weapon->pos.vx) >> 2;
            distance.vy = (weaponIndex[i].pos.vy - weapon->pos.vy) >> 2;
            distance.vz = (weaponIndex[i].pos.vz - weapon->pos.vz) >> 2;
            mag = GetMagnitude(distance);
            if (mag < 30) {
                weaponIndex->count = 0;
                weaponShapes[weaponIndex->shapeNo].used = 0;
                weapon->count = -2;
                weaponShapes[weapon->shapeNo].used = 0;
                PlayDirectionalNote(6, 0, 40, weapon->pos);
                return;
            }
        }
    }
}


/* Gives a ship a new weapon, chosen from the table row for its race position. */
void GetNewWeapon(ShipData *ships, int shipNo)
{
    ShipData *ship;
    short     weapon;
    int       position;
    int       group;
    short     chance;
    int       roll;

    ship = ships + shipNo;
    if (shipNo == ownShip) {
        position = playerPosition;
    } else if (shipNo == remoteShip) {
        position = remoteRacePos;
    }
    if (shipNo >= 12) {
        shipIndex[shipNo].weapon = 0;
        return;
    }
    if ((tempGameData->weaponsOn != 0 || raceType == 4) && ship->weapon == 0) {
        if (shipDamage[shipNo] >= 75 && (roll = randWep(), (roll / 2) * 2 == roll - 1)) {
            weapon = 6;
        } else {
            if ((short)position == 1) {
                group = 0;
            } else if ((u_short)(position - 2) < 3) {
                group = 1;
            } else if ((u_short)(position - 5) < 4) {
                group = 2;
            } else if ((u_short)(position - 9) < 3) {
                group = 3;
            } else if ((short)position >= 12) {
                group = 4;
            }
            position = (short)group;
            group = 2;
        retry:
                chance = randWep() % 100;
                if (chance < weaponChance[position][0]) {
                    weapon = 2;
                } else if (chance < weaponChance[position][1]) {
                    weapon = 1;
                } else if (chance < weaponChance[position][2]) {
                    weapon = 5;
                } else if (chance < weaponChance[position][3]) {
                    weapon = 2;
                    if (gameType == group) {
                        weapon = 13;
                    }
                } else if (chance < weaponChance[position][4]) {
                    weapon = 3;
                } else if (chance < weaponChance[position][5]) {
                    weapon = 5;
                    if (gameType == group) {
                        weapon = 14;
                    }
                } else if (chance < weaponChance[position][6]) {
                    weapon = 12;
                } else if (chance < weaponChance[position][7]) {
                    weapon = 7;
                } else if (chance < weaponChance[position][8]) {
                    weapon = 9;
                } else if (chance < weaponChance[position][9]) {
                    weapon = 2;
                    if (gameData->actTrackNum != 1 && gameData->actTrackNum != 7) {
                        weapon = 8;
                    }
                }
            if (weapon == ship->lastWeapon) {
                goto retry;
            }
        }
        ship->weapon = weapon;
        ship->lastWeapon = weapon;
    }
}


void GetNewDroneWeapon(ShipData *ships, int shipNo)
{
    short     weapon;
    short     chance;
    int       group;
    int       selected;
    short     ahead;
    ShipData *ship;

    ahead = 0;
    ship = ships + shipNo;
    if (tempGameData->weaponsOn != 0) {
        if (gameType == 2) {
            ahead = shipIndex[ownShip].distance >= ship->distance;
        }
        selected = ahead;
        group = selected * 6;
again:
        chance = randWep() % 100;
        if (chance < weaponDrone[group]) {
            weapon = 2;
        } else if (chance < weaponDrone[group + 1]) {
            weapon = 1;
        } else if (chance < weaponDrone[group + 2]) {
            weapon = 5;
        } else if (chance < weaponDrone[group + 3]) {
            weapon = 3;
        } else if (chance < weaponDrone[group + 4]) {
            if (gameData->actTrackNum != 1 && gameData->actTrackNum != 7) {
                weapon = 8;
            } else if (ship->autopilotCount != 0) {
                goto haveWeapon;
            } else {
noWeapon:
                weapon = 1234;
                goto done;
            }
        } else if (chance < weaponDrone[group + 5]) {
            goto noWeapon;
        }
haveWeapon:
        if (weapon == ship->lastWeapon) {
            goto again;
        }
done:
        if (weapon != 1234) {
            ship->weapon = weapon;
            ship->lastWeapon = weapon;
        }
    }
}


void ClearWeaponTable(void)
{
    int           i;
    WeaponData   *weapon;
    TrackSection *s;

    for (i = 0; i < 30; i++) {
        weapon = &weaponIndex[i];
        weapon->type = 0;
        weapon->count = 0;
        weaponBars[i].used = 0;
    }
    s = track->sections;
    for (i = 0; i < track->sectionCount; i++) {
        BumpFlatten(s);
        s++;
    }
}


void UpdateRocket(ShipData *ships, WeaponShape *weaponShapes, WeaponData *weapon, WeaponData *weaponIndex)
{
    VECTOR distance;
    VECTOR facePoint;
    VECTOR nextPoint;
    VECTOR path;
    VECTOR offset;
    VECTOR wingPoint;
    VECTOR wingNext;
    char workspace[16];
    Face *facePtr;
    Face *nextFace;
    int i;
    int mag, alpha, height;
    ShipData *ship;
    unsigned char flag;

    weapon->section = FindNearestSection(weapon->section, weapon->pos);
    facePtr = track->faces + weapon->section->faceStart;
    nextFace = track->faces + weapon->section->nextSection.ptr->faceStart;
    while (!(facePtr->flags & 1)) facePtr++;
    do {
        flag = nextFace->flags;
        nextFace++;
        if (flag & 1) {
            nextFace--;
            break;
        }
    } while (1);
    weapon->velocity.vx += weapon->accel.vx;
    weapon->velocity.vy += weapon->accel.vy;
    weapon->velocity.vz += weapon->accel.vz;
    weapon->velocity.vx -= weapon->velocity.vx >> 5;
    weapon->velocity.vy -= weapon->velocity.vy >> 5;
    weapon->velocity.vz -= weapon->velocity.vz >> 5;
    weapon->pos.vx += weapon->velocity.vx;
    weapon->pos.vz += weapon->velocity.vz;
    facePoint.vx = track->vertices[facePtr->vertex[0]].vx;
    facePoint.vy = track->vertices[facePtr->vertex[0]].vy;
    facePoint.vz = track->vertices[facePtr->vertex[0]].vz;
    nextPoint.vx = track->vertices[nextFace->vertex[0]].vx;
    nextPoint.vy = track->vertices[nextFace->vertex[0]].vy;
    nextPoint.vz = track->vertices[nextFace->vertex[0]].vz;
    path.vx = nextPoint.vx - facePoint.vx;
    path.vy = nextPoint.vy - facePoint.vy;
    path.vz = nextPoint.vz - facePoint.vz;
    offset.vx = weapon->pos.vx - facePoint.vx;
    offset.vz = weapon->pos.vz - facePoint.vz;
    mag = SquareRoot0(path.vx * path.vx + path.vz * path.vz);
    alpha = (((path.vx * offset.vx + path.vz * offset.vz) / mag) << 16) / mag;
    height = nextPoint.vy - facePoint.vy;
    height = (height * alpha) >> 16;
    weapon->pos.vy = facePoint.vy + height - 300;
    ratan2(path.vy, SquareRoot0(path.vx * path.vx + path.vz * path.vz));
    wingPoint.vx = facePoint.vx;
    wingPoint.vy = facePoint.vy;
    wingPoint.vz = facePoint.vz;
    facePtr++;
    wingNext.vx = track->vertices[facePtr->vertex[0]].vx;
    wingNext.vy = track->vertices[facePtr->vertex[0]].vy;
    wingNext.vz = track->vertices[facePtr->vertex[0]].vz;
    path.vx = wingNext.vx - wingPoint.vx;
    path.vz = wingNext.vz - wingPoint.vz;
    offset.vx = weapon->pos.vx - wingPoint.vx;
    offset.vz = weapon->pos.vz - wingPoint.vz;
    mag = SquareRoot0(path.vx * path.vx + path.vz * path.vz);
    alpha = (((path.vx * offset.vx + path.vz * offset.vz) / mag) << 16) / mag;
    height = wingNext.vy - wingPoint.vy;
    alpha = (height * alpha) >> 16;
    height = alpha - (height >> 1);
    weapon->pos.vy += height;
    weapon->pitch = 0;
    weapon->count--;
    if (weapon->count == 0) weaponShapes[weapon->shapeNo].used = 0;
    SetSkeletonPosition(weaponShapes[weapon->shapeNo].shapePtr->skeleton, weapon->pos.vx, weapon->pos.vy, weapon->pos.vz);
    weaponShapes[weapon->shapeNo].shapePtr->skeleton->update = 1;
    AsmDrawObject(weaponShapes[weapon->shapeNo].shapePtr, cameraPtr->camPos, 0); /* Port: roll was never passed; 0 stands for the console's stale a2 */
    for (i = 0; i < 15; i++) {
        ship = ships + i;
        if (!(ship->combatAttr & DESTROYED) && (((i != weapon->owner) && raceType != 3) || (raceType == 3 && (i == ownShip || i == remoteShip) && i != weapon->owner))) {
            distance.vx = (ship->ppivot.vx - weapon->pos.vx) >> 2;
            distance.vy = (ship->ppivot.vy - weapon->pos.vy) >> 2;
            distance.vz = (ship->ppivot.vz - weapon->pos.vz) >> 2;
            if (GetMagnitude(distance) < 100) {
                if (!(ship->attr & SHIELDED)) {
                    if (i != ownShip && i != remoteShip) {
                        FlipShip(ship);
                        ship->speed >>= 5;
                        ship->vroll += newRand() % 250;
                        ship->ahdg = (newRand() - 16384) >> 1;
                    } else {
                        ship->vpivot.vx -= (ship->vpivot.vx * 3) >> 2;
                        ship->vpivot.vy -= (ship->vpivot.vy * 3) >> 2;
                        ship->vpivot.vz -= (ship->vpivot.vz * 3) >> 2;
                        ship->vroll += newRand() % 250;
                        ship->ahdg = (newRand() - 16384) >> 1;
                        if (i == ownShip) SetShake(20);
                    }
                }
                DamageCollide(1, i);
                weapon->target = i;
                weapon->count = -1;
                weaponShapes[weapon->shapeNo].used = 0;
                if (i != ownShip) PlayDirectionalNote(5, 0, 40, weapon->pos);
                else PlayNote(5, 255, 15, 15, 256);
                return;
            }
        }
    }
    facePtr = track->faces + weapon->section->faceStart;
    if (!(weapon->section->flags & 1)) {
        for (i = 0; i < weapon->section->faceCount; i++) {
            facePoint.vx = track->vertices[facePtr->vertex[0]].vx;
            facePoint.vy = track->vertices[facePtr->vertex[0]].vy;
            facePoint.vz = track->vertices[facePtr->vertex[0]].vz;
            if (PlaneLineCollision(facePoint, weapon->pos, facePtr->normal) <= 0 && !(facePtr->flags & 1)) {
                weapon->count = -2;
                weaponShapes[weapon->shapeNo].used = 0;
                PlayDirectionalNote(6, 0, 40, weapon->pos);
                return;
            }
            facePtr++;
        }
    }
    Weapon2Mine(weapon, weaponIndex, weaponShapes);
}
#define CHARGE plasmacharge[CurrentScreen]

/* Moves a plasma shot along the track surface. It damages the first ship it touches and
   stops at a wall. While the owner is charging, the count is above 200 and a charge bar is drawn. */
void UpdatePlasma(ShipData *shipIndex, WeaponShape *weaponShapes, WeaponData *weapon, WeaponData *weaponIndex)
{
    VECTOR distance, facePoint, nextPoint, path, offset, wingPoint, wingNext;
    Face *facePtr, *nextFace;
    int i;
    int mag, alpha, height;
    ShipData *ship;
    unsigned char flag;
    char workspace[16];

    weapon->count--;
    if (weapon->count > PLASMA_CHARGE_FRAMES && weapon->owner == ownShip) {
        SetPolyG4(&CHARGE);
        CHARGE.r0 = 255;
        CHARGE.g0 = 255;
        CHARGE.b0 = 255;
        CHARGE.r1 = 255;
        CHARGE.g1 = 255;
        CHARGE.b1 = 255;
        CHARGE.r2 = 0;
        CHARGE.g2 = 0;
        CHARGE.b2 = 0;
        CHARGE.r3 = 0;
        CHARGE.g3 = 0;
        CHARGE.b3 = 0;
        CHARGE.x0 = 140;
        CHARGE.x1 = 200 - 3 * (weapon->count - PLASMA_CHARGE_FRAMES);
        CHARGE.x2 = 140;
        CHARGE.x3 = 200 - 3 * (weapon->count - PLASMA_CHARGE_FRAMES);
        CHARGE.y0 = 70;
        CHARGE.y1 = 70;
        CHARGE.y2 = 74;
        CHARGE.y3 = 74;
        AddPrim(OT[CurrentScreen], &CHARGE);
        goto done;
    }
    goto compute;
shipNote:
    PlayDirectionalNote(5, 0, 40, weapon->pos);
    goto done;
wallNote:
    weapon->count = -2;
    plasmaShotBusy = 0;
    weaponShapes[weapon->shapeNo].used = 0;
    PlayDirectionalNote(6, 0, 40, weapon->pos);
    goto done;
compute:
    if (weapon->count == PLASMA_CHARGE_FRAMES) {
        weapon->section = shipIndex[weapon->owner].nearTrkSect;
        facePtr = track->faces + shipIndex[weapon->owner].nearTrkSect->faceStart;
        do {
            flag = facePtr->flags;
            facePtr++;
            if (flag & 1) {
                facePtr--;
                break;
            }
        } while (1);
        GetFireVector(&weapon->accel, shipIndex[weapon->owner].unitVecNose, facePtr, shipIndex[weapon->owner].ppivot);
        PAL_SPEED_VECTOR(weapon->accel);
        weapon->velocity.vx = shipIndex[weapon->owner].vpivot.vx >> 6;
        weapon->velocity.vy = shipIndex[weapon->owner].vpivot.vy >> 6;
        weapon->velocity.vz = shipIndex[weapon->owner].vpivot.vz >> 6;
        weapon->quakeBumpStep = 0;
        weapon->pos.vx = shipIndex[weapon->owner].ppivot.vx;
        weapon->pos.vy = shipIndex[weapon->owner].ppivot.vy;
        weapon->pos.vz = shipIndex[weapon->owner].ppivot.vz;
    }
    weapon->section = FindNearestSection(weapon->section, weapon->pos);
    facePtr = track->faces + weapon->section->faceStart;
    nextFace = track->faces + weapon->section->nextSection.ptr->faceStart;
    while (!(facePtr->flags & 1)) facePtr++;
    do {
        flag = nextFace->flags;
        nextFace++;
        if (flag & 1) {
            nextFace--;
            break;
        }
    } while (1);
    weapon->velocity.vx += weapon->accel.vx;
    weapon->velocity.vy += weapon->accel.vy;
    weapon->velocity.vz += weapon->accel.vz;
    weapon->velocity.vx -= weapon->velocity.vx >> 5;
    weapon->velocity.vy -= weapon->velocity.vy >> 5;
    weapon->velocity.vz -= weapon->velocity.vz >> 5;
    weapon->pos.vx += weapon->velocity.vx;
    weapon->pos.vz += weapon->velocity.vz;
    facePoint.vx = track->vertices[facePtr->vertex[0]].vx;
    facePoint.vy = track->vertices[facePtr->vertex[0]].vy;
    facePoint.vz = track->vertices[facePtr->vertex[0]].vz;
    nextPoint.vx = track->vertices[nextFace->vertex[0]].vx;
    nextPoint.vy = track->vertices[nextFace->vertex[0]].vy;
    nextPoint.vz = track->vertices[nextFace->vertex[0]].vz;
    path.vx = nextPoint.vx - facePoint.vx;
    path.vy = nextPoint.vy - facePoint.vy;
    path.vz = nextPoint.vz - facePoint.vz;
    offset.vx = weapon->pos.vx - facePoint.vx;
    offset.vz = weapon->pos.vz - facePoint.vz;
    mag = SquareRoot0(path.vx * path.vx + path.vz * path.vz);
    alpha = (((path.vx * offset.vx + path.vz * offset.vz) / mag) << 16) / mag;
    height = nextPoint.vy - facePoint.vy;
    height = (height * alpha) >> 16;
    weapon->pos.vy = facePoint.vy + height - 300;
    ratan2(path.vy, SquareRoot0(path.vx * path.vx + path.vz * path.vz));
    wingPoint.vx = facePoint.vx;
    wingPoint.vy = facePoint.vy;
    wingPoint.vz = facePoint.vz;
    facePtr++;
    wingNext.vx = track->vertices[facePtr->vertex[0]].vx;
    wingNext.vy = track->vertices[facePtr->vertex[0]].vy;
    wingNext.vz = track->vertices[facePtr->vertex[0]].vz;
    path.vx = wingNext.vx - wingPoint.vx;
    path.vz = wingNext.vz - wingPoint.vz;
    offset.vx = weapon->pos.vx - wingPoint.vx;
    offset.vz = weapon->pos.vz - wingPoint.vz;
    mag = SquareRoot0(path.vx * path.vx + path.vz * path.vz);
    alpha = (((path.vx * offset.vx + path.vz * offset.vz) / mag) << 16) / mag;
    height = wingNext.vy - wingPoint.vy;
    alpha = (height * alpha) >> 16;
    height = alpha - (height >> 1);
    weapon->pos.vy += height;
    weapon->pitch = 0;
    NewPlasmaSegment(&weapon->pos);
    if (weapon->count == 0) plasmaShotBusy = 0;
    for (i = 0; i < 15; i++) {
        ship = shipIndex + i;
        if (!(ship->combatAttr & DESTROYED) && (((i != weapon->owner) && raceType != 3) || (raceType == 3 && (i == ownShip || i == remoteShip) && i != weapon->owner))) {
            distance.vx = (ship->ppivot.vx - weapon->pos.vx) >> 2;
            distance.vy = (ship->ppivot.vy - weapon->pos.vy) >> 2;
            distance.vz = (ship->ppivot.vz - weapon->pos.vz) >> 2;
            if (GetMagnitude(distance) < 100) {
                if (!(ship->attr & SHIELDED)) {
                    if (i != ownShip) {
                        ship->speed >>= 5;
                        ship->vroll += newRand() % 250;
                        ship->ahdg = (newRand() - 16384) >> 1;
                    } else {
                        ship->vpivot.vx -= (ship->vpivot.vx * 3) >> 2;
                        ship->vpivot.vy -= (ship->vpivot.vy * 3) >> 2;
                        ship->vpivot.vz -= (ship->vpivot.vz * 3) >> 2;
                        ship->vroll += newRand() % 250;
                        ship->ahdg = (newRand() - 16384) >> 1;
                        if (i == ownShip) SetShake(20);
                    }
                }
                DamageCollide(13, i);
                plasmaShotBusy = 0;
                weapon->target = i;
                weapon->count = -1;
                weaponShapes[weapon->shapeNo].used = 0;
                if (i != ownShip) goto shipNote;
                PlayNote(5, 255, 15, 15, 256);
                goto done;
            }
        }
    }
    facePtr = track->faces + weapon->section->faceStart;
    if (!(weapon->section->flags & 1)) {
        for (i = 0; i < weapon->section->faceCount; facePtr++, i++) {
            facePoint.vx = track->vertices[facePtr->vertex[0]].vx;
            facePoint.vy = track->vertices[facePtr->vertex[0]].vy;
            facePoint.vz = track->vertices[facePtr->vertex[0]].vz;
            if (PlaneLineCollision(facePoint, weapon->pos, facePtr->normal) <= 0 && !(facePtr->flags & 1)) goto wallNote;
        }
    }
    goto done;
done:;
}


/* Applies a hit of the given type to a ship. The class and the track scale the amount, and the
   player's camera switches to the destroyed view. A type missing from the damage table leaves
   the amount undefined. */
void DamageCollide(int damageType, int shipNo)
{
    s32 scaledDamage;
    s32 shipAttributes;
    signed char classIndex;
    u_char trackIndex;
    ShipData *playerShip;

    if ((linkSendFlag == 0) && (attractDelayFrames <= 0)) {
        if (shipNo == ownShip && damageType != 0x80 && damageType != 0x81 && damageType != 0x82) {
            PortPadRumble(4); /* Port: a hit the picture does not shake by taps the pad; 0x80 is the pit lane's
                                repair, which is no hit at all, and a wall's (0x81) and another ship's (0x82)
                                shake it by speed (collide.c's WallHitRumble and ShipHitRumble) */
        }
        switch (damageType) {
        case 0xC:
            scaledDamage = 0x800;
            break;
        case 0xD:
            scaledDamage = 0x3E700;
            break;
        case 0xE:
        case 0x1:
            scaledDamage = 0x1400;
            break;
        case 0x2:
            scaledDamage = 0xA00;
            break;
        case 0xF:
            scaledDamage = 0xF00;
            break;
        case 0x3:
        case 0x5:
            scaledDamage = 0x500;
            break;
        case 0x80:
            scaledDamage = -0x100;
            break;
        case 0x82:
            scaledDamage = 0x40;
            break;
        case 0x81:
            scaledDamage = 0x80;
            break;
        }
        if (damageType != 0x80) {
            if (shipNo < 3) {
                scaledDamage = (s32) (scaledDamage * 3) >> 1;
            }
            if ((u32) (shipNo - 3) < 3U) {
                scaledDamage += scaledDamage >> 2;
            }
            if ((u32) (shipNo - 6) < 3U) {
                scaledDamage = (s32) (scaledDamage * 3) >> 2;
            }
            if ((u32) (shipNo - 0xC) < 3U) {
                scaledDamage >>= 1;
            }
        }
        scaledDamage = (scaledDamage * 3) >> 1;
        if (damageType == 0x80) {
            classIndex = gameData->shipClass;
            if (classIndex == 0) {
                scaledDamage = -0xA0;
            }
            if (classIndex == 1) {
                scaledDamage = -0xC8;
            }
            if (classIndex == 2) {
                scaledDamage = -0xF0;
            }
            if (classIndex == 3) {
                scaledDamage = -0x118;
            }
            trackIndex = gameData->actTrackNum;
            switch (trackIndex) {
            case 1:
                scaledDamage = (scaledDamage * 0x1D) / 31;
                break;
            case 2:
                scaledDamage = (scaledDamage * 0x1D) / 39;
                break;
            case 6:
                scaledDamage = (scaledDamage * 0x1D) / 23;
                break;
            case 8:
                scaledDamage = (scaledDamage * 0x1D) / 23;
                break;
            case 13:
                scaledDamage = (scaledDamage * 0x1D) / 20;
                break;
            case 17:
                scaledDamage = (scaledDamage * 0x1D) / 15;
                break;
            case 7:
            case 20:
                scaledDamage = (scaledDamage * 0x1D) / 18;
                break;
            }
            if (shipNo != ownShip && shipNo != remoteShip) {
                scaledDamage = (s32) (scaledDamage * 3) >> 2;
            }
        }
        if (shipNo == ownShip) {
            if (damageType != 0x80 && (raceType != 1 || shipNo != ghostShip)) {
                perfectLap = 0;
            }
        }
        if ((shipIndex[shipNo].attr & SHIELDED) && (scaledDamage > 0)) {
            scaledDamage = 0;
        }
        if (PortOriginalTrack() >= 0 && !PortOriginalTrackIs64() && scaledDamage > 0) {
            scaledDamage = 0; /* Port: on the original Wipeout's tracks the shields stay full (it had none); Wipeout 64's
                                 drain as XL's */
        }
        fracdamage[shipNo] += scaledDamage;
        if (fracdamage[shipNo] < 0) fracdamage[shipNo] = 0;
        shipDamage[shipNo] = fracdamage[shipNo] >> 8;
        if (cheatDamage != 0) shipDamage[ownShip] = 0;
        if (shipDamage[shipNo] >= 101) {
            if (shipTargetIndex[shipNo] == -1) {
                shipDamage[shipNo] = 100;
                if (shipNo == ownShip || shipNo == remoteShip) {
                    if (raceContext != 1) {
                        shipTargetIndex[shipNo] = 100;
                        if (shipNo == ownShip) {
                            playerShip = &shipIndex[shipNo];
                            shipAttributes = playerShip->attr;
                            CameraUpdate = UpdateCameraAttractDestroyed;
                            playerShip->attr = shipAttributes & ~4;
                        }
                    }
                } else {
                    shipTargetIndex[shipNo] = 15;
                }
            }
        }
        if (damageType != 128 && damageType != 129) {
            shieldGreen[shipNo] = 255;
            shieldRed[shipNo] = 255;
        }
        if (damageType == 129) shieldRed[shipNo] = 255;
        if (olddam < 60 && shipDamage[ownShip] >= 60 && shipNo == ownShip) {
            PlayNote(54, 510, 15, 15, 256);
        }
        olddam = shipDamage[ownShip];
    }
}


void CheckPitShips(void)
{
    int   i;
    int   j;
    Face *facePtr;

    for (i = 0; i < 15; i++) {
        if (!(shipIndex[i].combatAttr & DESTROYED)) {
            facePtr = track->faces + shipIndex[i].nearTrkSect->faceStart;
            if (facePtr->flags & 0x40) {
                DamageCollide(128, i);
                if (i == ownShip) {
                    SetVolume(ambientVoice2, 192);
                }
                AddPitParticles(particleSlots, 10, &shipIndex[i].ppivot, 8);
            } else if ((facePtr + 1)->flags & 0x40) {
                DamageCollide(128, i);
                for (j = 0; j < 8; j++) {
                    AddElectric(&shipIndex[i].ppivot, 2);
                }
                if (i == ownShip) {
                    SetVolume(ambientVoice2, 192);
                }
                AddPitParticles(particleSlots, 10, &shipIndex[i].ppivot, 3);
            } else if (i == ownShip) {
                SetVolume(ambientVoice2, 0);
            }
        } else if (i == ownShip) {
            SetVolume(ambientVoice2, 0);
        }
    }
}


/* Draws the full-screen colour fade: screen flashes (smart bomb, shield) and the cheat fade. */
void UpdateFadeScreen(void)
{
    u_char red;
    u_char green;
    u_char blue;

    SetTile(&fadeTile[CurrentScreen]);
    SetSemiTrans(&fadeTile[CurrentScreen], 1);
    SetDrawMode(&fadedrm[CurrentScreen], 1, 1, GetTPage(0, 1, 0, 0) & 0xFFFF, 0);
    if (fadeRed + fadeAmount < 256) {
        red = fadeRed + fadeAmount;
    } else {
        red = 255;
    }
    fadeTile[CurrentScreen].r0 = red;
    if (fadeGreen + fadeAmount + cheatfade < 256) {
        green = cheatfade + (fadeGreen + fadeAmount);
    } else {
        green = 255;
    }
    fadeTile[CurrentScreen].g0 = green;
    if (fadeBlue + fadeAmount < 256) {
        blue = fadeBlue + fadeAmount;
    } else {
        blue = 255;
    }
    fadeTile[CurrentScreen].b0 = blue;
    fadeTile[CurrentScreen].w = 320;
    fadeTile[CurrentScreen].x0 = 0;
    fadeTile[CurrentScreen].y0 = 0;
    fadeTile[CurrentScreen].h = REGION(240, 255);
    AddPrim(OT[CurrentScreen], &fadeTile[CurrentScreen]);
    if (fadeAmount > 0) {
        fadeAmount -= 8;
    }
    if (fadeAmount < 0) {
        fadeAmount = 0;
    }
    if (cheatfade > 0) {
        cheatfade -= 10;
    }
    if (cheatfade < 0) {
        cheatfade = 0;
    }
}


void FlipShip(ShipData *playerShip)
{
    SVector normal;
    int     normalX;
    int     normalY;
    int     normalZ;
    Face   *facePtr;

    facePtr = track->faces + playerShip->nearTrkSect->faceStart;
    if (!(facePtr->flags & 1)) {
        facePtr++;
        while (!(facePtr->flags & 1)) {
            facePtr++;
        }
    }
    facePtr++;
    normalX = facePtr->normal.vx;
    normal.vx = normalX;
    normalY = facePtr->normal.vy;
    normal.vy = normalY;
    normalZ = facePtr->normal.vz;
    normal.vz = normalZ;
    playerShip->vpivot.vx += normalX * 2;
    playerShip->vpivot.vy += normal.vy * 2;
    playerShip->vpivot.vz += normal.vz * 2;
}


void GetCheatWeapon(ShipData *ships, int i)
{
    ShipData *s = &ships[i];
    short     w;

    if (i >= 12) {
        shipIndex[i].weapon = 0;
        return;
    }
    if (tempGameData->weaponsOn == 0) return;
    if (s->weapon != 0) return;
    do {
        switch (newRand() & 7) {
        case 2: w = 5; break;
        case 3: w = 13; break;
        case 4: w = 14; break;
        case 1: case 5: w = 1; break;
        case 6: w = 12; break;
        case 0: default: w = 2; break;
        }
        if (w == 13 && plasmaShotBusy) w = s->lastWeapon;
    } while (w == s->lastWeapon);
    s->weapon = w;
    s->lastWeapon = w;
}


/* Advances the weapon random seed and returns it as a 15-bit value. */
int randWep(void)
{
    int result;

    result = (seedWep >> 1) ^ (seedWep * 8);
    seedWep = result;
    return result & 0x7FFF;
}
