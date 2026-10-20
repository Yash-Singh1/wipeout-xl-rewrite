#include "raceinit.h"
#include "include_asm_g8.h"
int     attractStep;
u_int   randSeed;
u_char  cdMusicIndex;
extern int             ownShip;
extern int             cdMusicCount;
extern int             cdMusicTrack;
extern int             trackNameScroll;
extern int             attractModeFlag;
extern int             attractDelayFrames;
extern u_char          noCheckPoints;
extern int             roadSectionCount;
extern int             weaponGridSections;
extern TrackData      *track;
extern ConfigData     *gameData;
extern char            sufCheckPoints[];
extern char            fmtCheckPointClass[];
extern char            extChk[];
extern char            fmtTwoDigits[];
extern char            errorString[];

/* Walks the sections under the player's ship: a checkpoint the player passes adds its time
   bonus, and the bonus counter flashes "EXTENDED TIME" while it runs down. */

extern int             linkRaceFlag;
extern RaceLine        raceLines[];
extern u_char          cdMusicIndex;


void InitTrackTextures(ConfigData *fileId)
{
    char          path[256];
    char          filename[8];
    int           sourceOffset;
    int           fileSize;
    short         sectionNumber;
    TrackSection *section;
    u_char       *buffer;
    Face         *facePtr;
    short         faceNumber;

    sourceOffset = 0;
    __builtin_memcpy(path, "c:\\wipeout2\\track", 18);
    sprintf(filename, fmtTwoDigits, fileId->actTrackNum);
    strcat(path, filename);
    strcat(path, "\\track.tex");
    fileSize = FileLength(path);
    if (fileSize > 0) {
        buffer = DAlloc(heap, fileSize + 0x20);
        if (buffer == 0) {
            sprintf(errorString, "initrace.c : InitTrackTextures(): Fail allocate memory\n");
            Error(errorString, 2);
        }
        if (LoadFile(path, buffer) != fileSize) {
            sprintf(errorString, "initrace.c:InitTrackTextures(): Failed to load file \n");
            Error(errorString, 2);
        }
        section = track->sections;
        for (sectionNumber = 0; sectionNumber < track->sectionCount; sectionNumber++) {
            facePtr = track->faces + section->faceStart;
            for (faceNumber = 0; faceNumber < section->faceCount; faceNumber++) {
                facePtr->texture = buffer[sourceOffset++];
                facePtr->flags = buffer[sourceOffset++];
                facePtr++;
            }
            section = NextIndexSection(section);
        }
        DFree(heap, buffer);
    }
}


void InitCheckPoints(CheckPoints *checkpoints, ConfigData *config)
{
    char   path[256];
    char   suffix[8];
    int   fileSize;
    char  *fileBuffer;
    short  checkpoint;

    __builtin_memcpy(path, "c:\\wipeout2\\track", 18);
    sprintf(suffix, fmtTwoDigits, config->actTrackNum);
    strcat(path, suffix);
    strcat(path, sufCheckPoints);
    sprintf(suffix, fmtCheckPointClass, config->shipClass);
    strcat(path, suffix);
    strcat(path, extChk);
    noCheckPoints = 0;
    fileSize = FileLength(path);
    if (fileSize <= 0) {
        noCheckPoints = 1;
        return;
    }
    fileBuffer = DAlloc(heap, fileSize + 32);
    if (!fileBuffer) {
        sprintf(errorString, "initrace.c : InitCheckPoints (): Failed to allocate memory for file \n");
        Error(errorString, 2);
    }
    if (LoadFile(path, fileBuffer) != fileSize) {
        sprintf(errorString, "initrace.c:InitCheckPoints(): Failed to load file \n");
        Error(errorString, 2);
    }
    __builtin_memcpy(checkpoints, fileBuffer, 36);
    for (checkpoint = 0; checkpoint < 6; checkpoint++) {
        checkpoints[checkpoint].reached = -1;
    }
    DFree(heap, fileBuffer);
}


/* Paints the red and green tiles of the in-game menu rows and the footer bars. */
void InitInGameMenu(struct InGameMenu *menu)
{
    short    group;
    short    slot;
    POLY_G4 *tile;
    int      row;
    u_short  texturePage;

    group = 0;
    do {
        slot = 0;
        do {
            tile = &menu->red[group][slot];
            SetPolyG4(tile);
            menu->red[group][slot].r0 = 0x3F;
            menu->red[group][slot].g0 = 0;
            menu->red[group][slot].b0 = 0;
            menu->red[group][slot].r1 = 0x7F;
            menu->red[group][slot].g1 = 0;
            menu->red[group][slot].b1 = 0;
            menu->red[group][slot].r2 = 0xBF;
            menu->red[group][slot].g2 = 0;
            menu->red[group][slot].b2 = 0;
            menu->red[group][slot].r3 = 0xFF;
            menu->red[group][slot].g3 = 0;
            menu->red[group][slot].b3 = 0;
            tile = &menu->green[group][slot];
            SetPolyG4(tile);
            menu->green[group][slot].r0 = 0;
            menu->green[group][slot].g0 = 0x3F;
            menu->green[group][slot].b0 = 0;
            menu->green[group][slot].r1 = 0;
            menu->green[group][slot].g1 = 0x7F;
            menu->green[group][slot].b1 = 0;
            menu->green[group][slot].r2 = 0;
            menu->green[group][slot].g2 = 0xBF;
            menu->green[group][slot].b2 = 0;
            menu->green[group][slot].r3 = 0;
            menu->green[group][slot].g3 = 0xFF;
            menu->green[group][slot].b3 = 0;
            slot++;
        } while (slot < 2);
        group++;
        slot = 0;
    } while (group < 0x14);
    group = 0;
    do {
        row = group << 16;
        row >>= 16;
        tile = &menu->footer[row];
        SetPolyG4(tile);
        menu->footer[row].r0 = 0x20;
        menu->footer[row].g0 = 0x20;
        menu->footer[row].b0 = 0x20;
        menu->footer[row].r1 = 0x40;
        menu->footer[row].g1 = 0x40;
        menu->footer[row].b1 = 0x40;
        menu->footer[row].r2 = 0x40;
        menu->footer[row].g2 = 0x40;
        menu->footer[row].b2 = 0x40;
        menu->footer[row].r3 = 0x20;
        menu->footer[row].g3 = 0x20;
        menu->footer[row].b3 = 0x20;
        SetSemiTrans(tile, 1);
        texturePage = GetTPage(2, 2, 0, 0);
        SetDrawMode((DR_MODE *) menu->textureWindow[row], 1, 1, texturePage, 0);
        group++;
    } while (group < 2);
}


/* Port: the parameters were typed int (they hold the weapon grid table, the grid count and the
   special grid table that SetTrackAttributes takes); the call below no longer goes through a
   cast function pointer type. */
void InitWeaponGrids(WeaponGrid *weaponGrids, TrackData *context, int *noWeaponGrids, short *specialCount, SpecialGrid *specialGrid)
{
    TrackSection *section;
    TrackSection *original;
    Face         *face;
    int           number;
    short         more;
    short         branch;

    section = context->sections;
    more = 1;
    branch = 1;
    number = 0;
    roadSectionCount = 0;
    do {
        section->secNo = number;
        if (gameData->actTrackNum == 7 && (u_short) (number - 70) < 17) {
            face = context->faces;
            face += section->faceStart;
            do {
                u_char flag = face->flags;
                face++;
                if (flag & 1) break;
            } while (1);
            face--;
            face->flags |= 0x40;
            face++;
            face->flags |= 0x40;
        }
        *noWeaponGrids = SetTrackAttributes(section, number, weaponGrids, *noWeaponGrids, specialCount, specialGrid);
        if (section->junction.index != -1 && (section->junction.ptr->flags & 16)) {
            original = section;
            section = section->junction.ptr;
            while (branch) {
                number++;
                section->secNo = number;
                if (gameData->actTrackNum == 7 && (u_short) (number - 70) < 17) {
                    face = context->faces;
                    face += section->faceStart;
                    do {
                        u_char flag = face->flags;
                        face++;
                        if (flag & 1) break;
                    } while (1);
                    face--;
                    face->flags |= 0x40;
                    face++;
                    face->flags |= 0x40;
                }
                *noWeaponGrids = SetTrackAttributes(section, number, weaponGrids, *noWeaponGrids, specialCount, specialGrid);
                section = section->nextSection.ptr;
                if (section->junction.index != -1) branch = 0;
            }
            section = original->nextSection.ptr;
            number++;
            section->secNo = number;
            branch = 1;
            if (gameData->actTrackNum == 7 && (u_short) (number - 70) < 17) {
                face = context->faces;
                face += section->faceStart;
                do {
                    u_char flag = face->flags;
                    face++;
                    if (flag & 1) break;
                } while (1);
                face--;
                face->flags |= 0x40;
                face++;
                face->flags |= 0x40;
            }
            *noWeaponGrids = SetTrackAttributes(section, number, weaponGrids, *noWeaponGrids, specialCount, specialGrid);
            roadSectionCount++;
            if (section == context->sections) more = 0;
        }
        section = section->nextSection.ptr;
        roadSectionCount++;
        number++;
        if (section == context->sections) more = 0;
    } while (more);
    weaponGridSections = number;
}


void InitAttractMode(int mode, ShipData *ships)
{
    int       attributes;
    int       nextAttract;
    int       quotient;
    ShipData *ship;

    if (mode == 1) {
        ship = &ships[ownShip];
        attributes = ship->attr;
        nextAttract = attractStep + 1;
        quotient = nextAttract / 3;
        CameraUpdate = UpdateCameraAttractMode;
        ship->update = UpdateDroneShipStart;
        attractStep = nextAttract;
        attributes &= ~4;
        attributes &= ~0x40;
        ship->attr = attributes;
        if (nextAttract == (quotient * 3)) {
            attractDelayFrames = 0xCE4;
            attractModeFlag = mode;
        } else {
            attractDelayFrames = 0x384;
        }
        if (attractModeFlag == 1) {
            InitCredits();
        }
    }
}


int myrand(void)
{
    u_int temp;

    temp = (randSeed >> 1) ^ (randSeed * 8);
    randSeed = temp;
    return temp & 0xFFFF;
}


void InitShufflePlay(void)
{
    char workspace[16];
    int  trackIndex;
    int  trackCount;
    int  randomValue;
    int  clearIndex;
    int  assigned;
    int  selectedTrack;

    randSeed = GsGetVcount() ^ 0x12345678;
    trackCount = cdMusicCount - 1;
    clearIndex = 0;
    if (trackCount > 0) {
        do {
            cdMusicTable[clearIndex] = 0;
            clearIndex += 1;
        } while (clearIndex < trackCount);
    }
    clearIndex = 0;
    assigned = trackCount;
    if (assigned > 0) {
        do {
            assigned = 0;
            selectedTrack = clearIndex + 2;
            while (assigned == 0) {
                randomValue = myrand();
                trackIndex = randomValue % trackCount;
                if (cdMusicTable[trackIndex] == 0) {
                    cdMusicTable[trackIndex] = selectedTrack;
                    assigned = 1;
                }
            }
            clearIndex += 1;
        } while (clearIndex < trackCount);
    }
    cdMusicIndex = 0;
}


void InitCDAudio(ConfigData *menu, unsigned char *unused)
{
    int status;
    int next;
    int count;
    int muted;

    status = menu->cdTrack;
    muted = 0;
    if (status == 1) {
        next = cdMusicIndex + 1;
        count = cdMusicCount - 1;
        cdMusicIndex = next % count;
        trackNameScroll = 0x1E0;
        status = cdMusicTable[cdMusicIndex];
        cdMusicTrack = status;
        CdMusicPlay(status, muted);
    } else {
        CdMusicPlay(status, muted);
    }
}
void GetJoyPadPacketOnVsync(int flags)
{
    if (!(flags & 0xFF) && (gameType == 1)) {
        VSync(0);
    }
}


void InitRaceData(RaceData *race)
{
    race->selection = -1;
    race->showOptions = 1;
    race->stage = 2;
    race->musicPaused = 0;
    race->confirmed = 2;
    race->confirmChoice = 0;
    race->command = -1;
    race->frameCount = 0;
    race->count = 0;
    race->pad14 = 0;
    race->active = 0;
    linkSendFlag = 0;
    linkRaceFlag = 0;
}


void InitRaceResetData(struct ResetData *reset, ShipData *ships, ConfigData *config, WeaponData *weapons, WeaponShape *weaponShapes, RescueData *rescue, Object **shipShapes, CheckPoints *checkPoints)
{
    reset->ships = ships;
    reset->config = config;
    reset->weapons = weapons;
    reset->weaponShapes = weaponShapes;
    reset->rescue = rescue;
    reset->shipShapes = shipShapes;
    reset->checkPoints = checkPoints;
}


/* Builds the race line table: each section of the track gets a distance along the lap and
   its own point; the branch of a junction is measured against the main road. */
void InitRaceLine(TrackData *track, ConfigData *game)
{
    short         sections[632];
    short         active = 1;
    short         start[24] = {0, 0x14, 0xBC, 0, 0, 0, 0x80, 0x57C, 0x18, 0, 0, 0, 0, 0x40, 0, 0, 0, 0x30, 0, 0, 0x58, 0, 0, 0};
    TrackSection *section, *junction, *saved;
    short        *list;
    RaceLine     *line;
    Face         *face;
    short         more;
    int           distance, branchDistance, mainDistance;
    int           branchPosition, mainPosition;
    int           branchStep, mainStep, minDistance;
    int           i, count;
    int          workspace[2];
    short         offset, value;
    u_char        flags;

    section = track->sections;
    list = sections;
    distance = 0;
    line = raceLines;
    while (active) {
        line[section->secNo].distance = distance;
        line[section->secNo].x = 0;
        line[section->secNo].y = 0;
        line[section->secNo].z = 0;
        *list++ = section->secNo;
        if (section->junction.index != -1 && (section->junction.ptr->flags & 16)) {
            saved = section;
            section = saved->junction.ptr;
            branchDistance = 0;
            branchPosition = distance;
            mainPosition = distance;
            mainDistance = 0;
            more = 1;
            do {
                section = section->nextSection.ptr;
                branchDistance++;
                if (section->junction.index != -1) more = 0;
            } while (more);
            section = saved->nextSection.ptr;
            section = section->nextSection.ptr;
            more = 1;
            mainDistance++;
            do {
                section = section->nextSection.ptr;
                mainDistance++;
                if (section->junction.index != -1) more = 0;
            } while (more);
            mainDistance += 2;
            if (mainDistance >= branchDistance) {
                minDistance = branchDistance;
                mainStep = (minDistance * 4) / mainDistance;
                branchStep = 4;
            } else {
                minDistance = mainDistance;
                mainStep = 4;
                branchStep = (minDistance * 4) / branchDistance;
            }
            section = saved->junction.ptr;
            for (i = 0; i < branchDistance; i++) {
                line[section->secNo].distance = branchPosition;
                line[section->secNo].x = 0;
                line[section->secNo].y = 0;
                line[section->secNo].z = 0;
                section = section->nextSection.ptr;
                branchPosition += branchStep;
            }
            section = saved;
            for (i = 0; i < mainDistance; i++) {
                line[section->secNo].distance = mainPosition;
                line[section->secNo].x = 0;
                line[section->secNo].y = 0;
                line[section->secNo].z = 0;
                mainPosition += mainStep;
                *list++ = section->secNo;
                section = section->nextSection.ptr;
            }
            distance += minDistance * 4;
            line[section->secNo].distance = distance;
            line[section->secNo].x = 0;
            line[section->secNo].y = 0;
            line[section->secNo].z = 0;
            *list++ = section->secNo;
            if (section == track->sections) active = 0;
        }
        section = section->nextSection.ptr;
        distance += 4;
        if (section == track->sections) active = 0;
    }
    *list = -1;
    numTrackSections = distance;
    section = track->sections;
    i = 0;
    if (weaponGridSections > 0) {
        count = weaponGridSections;
        do {
            face = track->faces + section->faceStart;
            do {
                flags = face->flags;
                face++;
                if (flags & 1) {
                    face--;
                    break;
                }
            } while (1);
            raceLines[section->secNo].x = track->vertices[face->vertex[0]].vx;
            raceLines[section->secNo].y = track->vertices[face->vertex[0]].vy;
            raceLines[section->secNo].z = track->vertices[face->vertex[0]].vz;
            i++;
            section++;
        } while (i < count);
    }
    offset = start[game->actTrackNum];
    for (i = 0; i < weaponGridSections; i++) {
        value = raceLines[i].distance - offset;
        if (value >= numTrackSections) value -= numTrackSections;
        if (value < 0) value += numTrackSections;
        raceLines[i].distance = value;
    }
}
