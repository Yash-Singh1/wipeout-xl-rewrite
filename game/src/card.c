#include "functions.h"
#include "card.h"
#include "trackasm.h"
#include "ttf.h"

/* $gp-relative globals are tentative definitions; the rest are extern. */
extern int           cardSwEvent0;
extern int           cardSwEvent1;
extern int           cardSwEvent2;
extern int           cardSwEvent3;
extern int           cardHwEvent0;
extern int           cardHwEvent1;
extern int           cardHwEvent2;
extern int           cardHwEvent3;
extern Texture      *cardIconInfo;
extern unsigned char TrackHiCount;
extern unsigned char cardDirLoaded1;
extern unsigned char cardDirChanged0;
extern unsigned char cardDirChanged1;
extern CardDevice    cardPrefix0;
extern CardDevice    cardPrefix1;
extern int           trackNameScroll;
extern u_char        cdMusicIndex;
extern int           cdMusicTrack;
extern int           cdMusicCount;
extern u_short       keyDefineThrustPad[];
extern u_short       keyDefineFire[];
extern u_short       keyDefineView[];
extern u_short       keyDefineDitch[];
extern u_short       keyDefineRBrake[];
extern u_short       keyDefineLBrake[];
extern u_char        trackMedals[];
extern Label6        labelEmpty[];
extern char          cardPattern0[];
extern char          cardPattern1[];
extern Label6        labelGame[];
extern Label8        labelGameEurope[];
extern Label5        labelUsed[];

void LoadCardTexture(void)
{
    heap = reserveHeap;
    cardIconInfo = LoadTexture("c:\\wipeout2\\textures\\lucky.tim", 1);
    heap = mainheapcopy;
}


void InitCardEvents(void)
{
    cardSwEvent0 = OpenEvent(0xF4000001, 4, 0x2000, 0);
    cardSwEvent1 = OpenEvent(0xF4000001, 0x8000, 0x2000, 0);
    cardSwEvent2 = OpenEvent(0xF4000001, 0x100, 0x2000, 0);
    cardSwEvent3 = OpenEvent(0xF4000001, 0x2000, 0x2000, 0);
    cardHwEvent0 = OpenEvent(0xF0000011, 4, 0x2000, 0);
    cardHwEvent1 = OpenEvent(0xF0000011, 0x8000, 0x2000, 0);
    cardHwEvent2 = OpenEvent(0xF0000011, 0x100, 0x2000, 0);
    cardHwEvent3 = OpenEvent(0xF0000011, 0x2000, 0x2000, 0);
    EnableEvent(cardSwEvent0);
    EnableEvent(cardSwEvent1);
    EnableEvent(cardSwEvent2);
    EnableEvent(cardSwEvent3);
    EnableEvent(cardHwEvent0);
    EnableEvent(cardHwEvent1);
    EnableEvent(cardHwEvent2);
    EnableEvent(cardHwEvent3);
}


void CloseCardEvents(void)
{
    CloseEvent(cardSwEvent0);
    CloseEvent(cardSwEvent1);
    CloseEvent(cardSwEvent2);
    CloseEvent(cardSwEvent3);
    CloseEvent(cardHwEvent0);
    CloseEvent(cardHwEvent1);
    CloseEvent(cardHwEvent2);
    CloseEvent(cardHwEvent3);
}


int GetCardInfo(int slot)
{
    if (slot == 0) {
        _card_info(0);
    } else {
        _card_info(0x10);
    }
    return CheckCardEvents();
}
/* Waits for a software card event and returns which one fired. */
int CheckCardEvents(void)
{
    for (;;) {
        if (TestEvent(cardSwEvent0) == 1) return 0;
        if (TestEvent(cardSwEvent1) == 1) return 1;
        if (TestEvent(cardSwEvent2) == 1) return 2;
        if (TestEvent(cardSwEvent3) == 1) return 3;
    }
}


int CheckHWCardEvents(void)
{
    while (1) {
        if (TestEvent(cardHwEvent0) == 1) return 0;
        if (TestEvent(cardHwEvent1) == 1) return 1;
        if (TestEvent(cardHwEvent2) == 1) return 2;
        if (TestEvent(cardHwEvent3) == 1) return 3;
    }
}


void ClearEventHW(void)
{
    TestEvent(cardHwEvent0);
    TestEvent(cardHwEvent1);
    TestEvent(cardHwEvent2);
    TestEvent(cardHwEvent3);
}


void ClearEventSW(void)
{
    TestEvent(cardSwEvent0);
    TestEvent(cardSwEvent1);
    TestEvent(cardSwEvent2);
    TestEvent(cardSwEvent3);
}


int CardContentStatus(void)
{
    return CheckCardEvents();
}


void LoadCard(int slot)
{
    ClearEventSW();
    if (slot == 0) {
        _card_load(0);
    } else {
        _card_load(0x10);
    }
}


/* Writes an all-0xFF sector to every sector of the card on this port. */
int UnFormat(int slot)
{
    char buffer[128];
    char *p;
    int i;
    u_char fill;

    fill = 0xFF;
    i = 0x7F;
    p = &buffer[0x7F];
    do {
        *p = fill;
        i--;
        p--;
    } while (i >= 0);
    for (i = 0; i < 0x400; i++) {
        ClearEventHW();
        _new_card();
        _card_write(slot, i, buffer);
        if (CheckHWCardEvents() != 0) {
            return 0;
        }
    }
    return 1;
}


int QuickUnFormat(int slot)
{
    u_char sector[128];

    sector[1] = 0xFF;
    sector[0] = 0xFF;
    ClearEventHW();
    _new_card();
    _card_write(slot, 0, &sector[0]);
    return CheckHWCardEvents() == 0;
}


u_char SaveCardFile(char *name, int slot, ConfigData *settings)
{
    CardHeader header;
    char   fileName[0x20];

    GetHeader(header.bytes, name);
    MakeFileName(name, fileName, slot);
    if (CreateFile(fileName) & 0xFF) {
        return (WriteFile(header.bytes, fileName, settings) & 0xFF) != 0;
    }
    return 0;
}


int RenameFile1(char *card, char *source, int type, void *output)
{
    char    header1[32];
    char    header2[32];
    CardHeader data;

    MakeFileName(source, header1, type);
    MakeFileName(card, header2, type);
    GetHeader(data.bytes, header2);
    if (rename(header1, header2) != 0) {
        return (WriteFile(data.bytes, header2, output) & 0xFF) != 0;
    }
    return 0;
}


int RenameFile(char *name, int slot, ConfigData *settings)
{
    CardHeader header;
    char fileName[32];

    GetHeader(header.bytes, name);
    MakeFileName(name, fileName, slot);
    return (WriteFile(header.bytes, fileName, settings) & 0xFF) != 0;
}


/* Builds the card file header (CARD_HEADER_SIZE bytes): "SC" magic, the Japanese title, the file name and the
   icon image. */
void GetHeader(u_char *buffer, u_char *name)
{
    RECT  rect;
    short i, j;

    for (j = 0; j < 256; j++) buffer[j] = 0;
    buffer[0] = 0x53;
    buffer[1] = 0x43;
    buffer[2] = 0x11;
    buffer[3] = 1;
    if (portEuropeCard) {
        /* the title "ＷＯ２０９７　" in Shift-JIS */
        buffer[4] = 0x82;
        buffer[5] = 0x76;
        buffer[6] = 0x82;
        buffer[7] = 0x6e;
        buffer[8] = 0x82;
        buffer[9] = 0x51;
        buffer[10] = 0x82;
        buffer[11] = 0x4f;
        buffer[12] = 0x82;
        buffer[13] = 0x58;
        buffer[14] = 0x82;
        buffer[15] = 0x56;
        buffer[16] = 0x81;
        buffer[17] = 0x40;
    } else {
        /* "ＷＯＸＬ　" */
        buffer[4] = 0x82;
        buffer[5] = 0x76;
        buffer[6] = 0x82;
        buffer[7] = 0x6e;
        buffer[8] = 0x82;
        buffer[9] = 0x77;
        buffer[10] = 0x82;
        buffer[11] = 0x6b;
        buffer[12] = 0x81;
        buffer[13] = 0x40;
    }
    for (i = 0; (u_int)i < strlen(name); i++) {
        buffer[2 * i + CARD_NAME_OFFSET] = 0x82;
        buffer[2 * i + CARD_NAME_OFFSET + 1] = name[i] + 0x1f;
    }
    buffer[2 * i + CARD_NAME_OFFSET + 2] = 0x81;
    buffer[2 * i + CARD_NAME_OFFSET + 3] = 0x40;
    rect.x = cardIconInfo->clutX;
    rect.y = cardIconInfo->clutY;
    rect.w = cardIconInfo->clutW;
    rect.h = cardIconInfo->clutH;
    StoreImage(&rect, (u_long *)(buffer + 96));
    DrawSync(0);
    rect.x = cardIconInfo->textureX;
    rect.y = cardIconInfo->textureY;
    rect.w = 4;
    rect.h = cardIconInfo->textureH;
    StoreImage(&rect, (u_long *)(buffer + 128));
    DrawSync(0);
}


/* Builds the card file name for a slot ("bu00" or "bu10" prefix) followed by the given name. */
void MakeFileName(char *suffix, void *header, int slot)
{
    if (slot == 0) {
        __builtin_memcpy(header, portEuropeCard ? "bu00:BESLES-00327" : "bu00:BASCUS-94351", 18);
    } else {
        __builtin_memcpy(header, portEuropeCard ? "bu10:BESLES-00327" : "bu10:BASCUS-94351", 18);
    }
    strcat(header, suffix);
}


int CreateFile(char *fileName)
{
    int result;

    result = open(fileName, 0x10200);
    if (result < 0) {
        close(result);
        return 0;
    }
    close(result);
    return 1;
}


/* The 256-byte settings block of a save file: volumes, screen position, the key configuration, the options,
   progress, name, cheats and medals. (Port: built apart from WriteFile, for the autosave's comparison too.) */
void BuildSaveBlock(u_long *block, ConfigData *settings)
{
    short     i, j;
    u_short   musicVolume, effectVolume, otherMusicVolume, otherEffectVolume, screenX, screenY;
    u_short  *keys;
    u_char   *out;

    keys = (u_short *)&block[3];
    musicVolume = (u_short)settings->musicVolume;
    effectVolume = (u_short)settings->effectVolume;
    otherMusicVolume = (u_short)settings->otherMusicVolume;
    otherEffectVolume = (u_short)settings->otherEffectVolume;
    screenX = (u_short)settings->screenX;
    screenY = (u_short)settings->screenY;
    i = 0;
    ((u_short *)block)[0] = musicVolume;
    ((u_short *)block)[1] = effectVolume;
    ((u_short *)block)[2] = otherMusicVolume;
    ((u_short *)block)[3] = otherEffectVolume;
    ((u_short *)block)[4] = screenX;
    ((u_short *)block)[5] = screenY;
    for (; i < 2; i++) {
        for (j = 0; j < 9; j++) {
            *keys = savedKeyConfig.pad[i][j];
            keys++;
        }
        for (j = 0; j < 6; j++) {
            *keys = savedKeyConfig.other[i][j];
            keys++;
        }
    }
    out = (u_char *)keys;
    *out = settings->cdTrack;           out++;
    *out = settings->soundMode;         out++;
    *out = settings->negconMin;         out++;
    *out = settings->negconCentre;      out++;
    *out = settings->negconType;        out++;
    *out = settings->screenOffsetState; out++;
    *out = settings->weaponsOn;         out++;
    *out = settings->moveScreen;        out++;
    *out = settings->internalView;      out++;
    *out = settings->venomComplete;     out++;
    *out = settings->rapierComplete;    out++;
    *out = settings->venomChallengeDone;       out++;
    *out = settings->rapierChallengeDone;       out++;
    *out = settings->teamCheatActive; out++;
    *out = settings->class1Unlocked;    out++;
    *out = settings->class2Unlocked;    out++;
    *out = settings->trackCheatActive; out++;
    *out = settings->phantomTrackCheatActive; out++;
    for (i = 0; i < 9; i++) {
        *out = settings->name[i];
        out++;
    }
    for (i = 0; i < 15; i++) {
        *out = ((u_char *)settings + i)[0x45];
        out++;
    }
    for (i = 0; i < 13; i++) {
        *out = settings->entered.digits[i];
        out++;
    }
    for (i = 0; i < 8; i++) {
        *out = trackMedals[i];
        out++;
    }
}


/* Writes a save file: the header, the 1536-byte block and the 256-byte settings block. */
int WriteFile(void *header, char *filename, ConfigData *settings)
{
    u_long    block[64];
    int       fd;

    fd = open(filename, 2);
    if (fd >= 0 && write(fd, header, CARD_HEADER_SIZE) == CARD_HEADER_SIZE && write(fd, hiScoreTable, 0x600) == 0x600) {
        BuildSaveBlock(block, settings);
        if (write(fd, block, 0x100) == 0x100) {
            close(fd);
            return 1;
        }
    }
    close(fd);
    return 0;
}


/* Port: writes a save file anew, keeping its own header (title and icon): the autosave's, which may run where
   the card's icon is not in video memory (GetHeader needs it). */
int RewriteCardFile(char *name, int slot, ConfigData *settings)
{
    char   fileName[0x20];
    u_char header[0x200];
    int    fd;

    MakeFileName(name, fileName, slot);
    fd = open(fileName, 1);
    if (fd < 0 || read(fd, header, CARD_HEADER_SIZE) != CARD_HEADER_SIZE) {
        close(fd);
        return 0;
    }
    close(fd);
    return WriteFile(header, fileName, settings);
}


/* LoadCardFiles of WipEout 2097: its card browser predates multi-block saves, so a file larger than one block is
   listed block by block, each copying the icon of the first. */
static void LoadCardFilesEurope(CardSlot *cards, int slot)
{
    DIRENTRY       directory;
    CardFilename   filename;
    struct {
        u_char  u, v;
        short   x, y;
    } uv;
    CardEntry     *source;
    int            blocks;
    u_char         i, j;
    DIRENTRY      *result;
    CardEntry     *dest;
    int            count = 0;
    CardEntry     *entry;
    int            firstBlock;

    if (slot == 0) {
        uv.v = 222;
        uv.x = 576;
        uv.u = 0;
        uv.y = 192;
    } else {
        uv.v = 238;
        uv.x = 576;
        uv.u = 0;
        uv.y = 207;
    }
    for (i = 0; i < 15; i++) {
        entry = &cards[slot].entry[i];
        entry->name[0] = 0;
        entry->used = 0;
        entry->filename[0] = 0;
        *(Label6 *)&entry->label = labelEmpty[0];
        entry->type = 1;
    }
    if (slot == 0) {
        result = firstfile(cardPattern0, &directory);
        cardDirChanged0 = 0;
    }
    if (slot == 1) {
        result = firstfile(cardPattern1, &directory);
        cardDirChanged1 = 0;
    }
    if (result != 0) {
        do {
            if (slot == 0) {
                __builtin_memcpy(filename.bytes, &cardPrefix0, 6);
            } else {
                __builtin_memcpy(filename.bytes, &cardPrefix1, 6);
            }
            strcat(filename.bytes, directory.name);
            if (strncmp(directory.name, CARD_GAME_CODE, 12) == 0) {
                i = 0;
                j = 12;
                do {
                    cards[slot].entry[count].name[i] = directory.name[j];
                    i++;
                    j++;
                } while (i < 8);
                cards[slot].entry[count].name[8] = 0;
                cards[slot].entry[count].used = 1;
                strcpy(cards[slot].entry[count].filename, directory.name);
                cards[slot].entry[count].type = 4;
                cards[slot].entry[count].label = labelGameEurope[0];
                strcat((char *)&cards[slot].entry[count].label, cards[slot].entry[count].name);
                count++;
            } else {
                blocks = directory.size / 8192;
                LoadSaveIcons(filename.bytes, slot, &uv.u, &uv.v, &uv.x, &uv.y, count);
                firstBlock = count;
                for (i = 0; i < blocks; i++) {
                    cards[slot].entry[count].name[0] = 0;
                    cards[slot].entry[count].used = 1;
                    strcpy(cards[slot].entry[count].filename, directory.name);
                    cards[slot].entry[count].type = 2;
                    *(Label5 *)&cards[slot].entry[count].label = labelUsed[0];
                    if (i != 0) {
                        dest = &cards[slot].entry[count];
                        source = &cards[slot].entry[firstBlock];
                        dest->icon[0] = source->icon[0];
                        dest->icon[1] = source->icon[1];
                        dest->icon[2] = source->icon[2];
                        dest->icon[3] = source->icon[3];
                        dest->icon[4] = source->icon[4];
                        dest->icon[5] = source->icon[5];
                        dest->icon[6] = source->icon[6];
                        dest->icon[7] = source->icon[7];
                        dest->x = source->x;
                        dest->y = source->y;
                    }
                    count++;
                }
            }
        } while (nextfile(&directory) != 0);
        printf("load card complete\n");
        if (slot == 0) TrackHiCount = 1;
        if (slot == 1) cardDirLoaded1 = 1;
    }
}


/* Fills one slot's browser entries from the directory of the card, then copies the icons. */
void LoadCardFiles(CardSlot *cards, int slot)
{
    DIRENTRY       directory;
    CardFilename   filename;
    struct {
        u_char  u, v;
        short   x, y;
    } uv;
    DIRENTRY      *result;
    u_char         i, j;
    int            count = 0;
    int            blocks;
    CardEntry     *entry;
    CardEntry     *source;
    CardEntry     *dest;

    if (portEuropeCard) {
        LoadCardFilesEurope(cards, slot);
        return;
    }
    if (slot == 0) {
        uv.v = 222;
        uv.x = 576;
        uv.u = 0;
        uv.y = 192;
    } else {
        uv.v = 238;
        uv.x = 576;
        uv.u = 0;
        uv.y = 207;
    }
    for (i = 0; i < 15; i++) {
        entry = &cards[slot].entry[i];
        entry->name[0] = 0;
        entry->used = 0;
        entry->filename[0] = 0;
        *(Label6 *)&entry->label = labelEmpty[0];
        entry->type = 1;
        entry->first = 0;
        entry->multi = 0;
    }
    if (slot == 0) {
        result = firstfile(cardPattern0, &directory);
        cardDirChanged0 = 0;
    }
    if (slot == 1) {
        result = firstfile(cardPattern1, &directory);
        cardDirChanged1 = 0;
    }
    if (result == &directory) {
        do {
            if (slot == 0) {
                __builtin_memcpy(filename.bytes, &cardPrefix0, 6);
            } else {
                __builtin_memcpy(filename.bytes, &cardPrefix1, 6);
            }
            strcat(filename.bytes, directory.name);
            if (strncmp(directory.name, CARD_GAME_CODE, 12) == 0) {
                i = 0;
                j = 12;
                do {
                    cards[slot].entry[count].name[i] = directory.name[j];
                    i++;
                    j++;
                } while (i < 8);
                cards[slot].entry[count].name[8] = 0;
                cards[slot].entry[count].used = 1;
                strcpy(cards[slot].entry[count].filename, directory.name);
                cards[slot].entry[count].type = 4;
                *(Label6 *)&cards[slot].entry[count].label = labelGame[0];
                strcat((char *)&cards[slot].entry[count].label, cards[slot].entry[count].name);
                count++;
            } else {
                blocks = directory.size / 8192;
                for (i = 0; i < blocks; i++) {
                    cards[slot].entry[count].name[0] = 0;
                    cards[slot].entry[count].used = 1;
                    strcpy(cards[slot].entry[count].filename, directory.name);
                    cards[slot].entry[count].type = 2;
                    *(Label5 *)&cards[slot].entry[count].label = labelUsed[0];
                    cards[slot].entry[count].first = 1;
                    if (blocks > 1) cards[slot].entry[count].multi = 1;
                    if (i != 0) cards[slot].entry[count].first = 0;
                    count++;
                }
            }
            printf("Filename = %s \n", directory.name);
        } while (nextfile(&directory) == &directory);
        for (i = 0; i < 15; i++) {
            if (cards[slot].entry[i].type == 2) {
                if (slot == 0) {
                    __builtin_memcpy(filename.bytes, &cardPrefix0, 6);
                } else {
                    __builtin_memcpy(filename.bytes, &cardPrefix1, 6);
                }
                strcat(filename.bytes, cards[slot].entry[i].filename);
                if (cards[slot].entry[i].first == 1) {
                    LoadSaveIcons(filename.bytes, slot, &uv.u, &uv.v, &uv.x, &uv.y, i);
                }
                for (j = 0; j < 15; j++) {
                    source = &cards[slot].entry[i];
                    dest = &cards[slot].entry[j];
                    if (strncmp(source->filename, dest->filename, 20) == 0 && dest->first == 0) {
                        dest->icon[0] = source->icon[0];
                        dest->icon[1] = source->icon[1];
                        dest->icon[2] = source->icon[2];
                        dest->icon[3] = source->icon[3];
                        dest->icon[4] = source->icon[4];
                        dest->icon[5] = source->icon[5];
                        dest->icon[6] = source->icon[6];
                        dest->icon[7] = source->icon[7];
                        dest->x = source->x;
                        dest->y = source->y;
                    }
                }
            }
        }
        printf("load card complete\n");
        if (slot == 0) TrackHiCount = 1;
        if (slot == 1) cardDirLoaded1 = 1;
    }
}


/* Reads a save file into the game settings and the key/track tables; returns 1 on success. */
u_char LoadCardData(char *name, int slot, ConfigData *settings)
{
    u_short *keyCursor;
    short   pair;
    short   k;
    u_char  header[0x200];
    char    fileName[0x20];
    u_short data[0x80];
    int     fd;
    u_short setting0, setting1, setting2, setting3, setting4, setting5;
    u_char *bytes;
    int     hi;
    int     mute;
    int     track;

    MakeFileName(name, fileName, slot);
    fd = open(fileName, 1);
    if (fd < 0 || read(fd, header, CARD_HEADER_SIZE) != CARD_HEADER_SIZE || read(fd, hiScoreTable, 0x600) != 0x600 || read(fd, data, 0x100) != 0x100) {
        close(fd);
        return 0;
    }
    ReInitGameProgress();
    keyCursor = &data[6];
    setting0 = data[0];
    setting1 = data[1];
    setting2 = data[2];
    setting3 = data[3];
    setting4 = data[4];
    setting5 = data[5];
    pair = 0;
    settings->musicVolume = setting0;
    settings->effectVolume = setting1;
    settings->otherMusicVolume = setting2;
    settings->otherEffectVolume = setting3;
    settings->screenX = setting4;
    settings->screenY = setting5;
    for (; pair < 2; pair++) {
        for (k = 0; k < 9; k++) {
            savedKeyConfig.pad[pair][k] = *keyCursor;
            keyCursor++;
        }
        for (k = 0; k < 6; k++) {
            savedKeyConfig.other[pair][k] = *keyCursor;
            keyCursor++;
        }
    }
    pair = 0;
    for (; pair < 2; pair++) {
        keyDefineThrustPad[pair] = savedKeyConfig.pad[pair][0];
        keyDefineFire[pair] = savedKeyConfig.pad[pair][1];
        keyDefineView[pair] = savedKeyConfig.pad[pair][2];
        keyDefineDitch[pair] = savedKeyConfig.pad[pair][3];
        keyDefineRBrake[pair] = savedKeyConfig.pad[pair][4];
        keyDefineLBrake[pair] = savedKeyConfig.pad[pair][5];
    }
    bytes = (u_char *)keyCursor;
    settings->cdTrack = *bytes++;
    settings->soundMode = *bytes++;
    settings->negconMin = *bytes++;
    settings->negconCentre = *bytes++;
    settings->negconType = *bytes++;
    settings->screenOffsetState = *bytes++;
    settings->weaponsOn = *bytes++;
    settings->moveScreen = *bytes++;
    settings->internalView = *bytes++;
    settings->venomComplete = *bytes++;
    settings->rapierComplete = *bytes++;
    settings->venomChallengeDone = *bytes++;
    settings->rapierChallengeDone = *bytes++;
    settings->teamCheatActive = *bytes++;
    settings->class1Unlocked = *bytes++;
    settings->class2Unlocked = *bytes++;
    settings->trackCheatActive = *bytes++;
    settings->phantomTrackCheatActive = *bytes++;
    for (pair = 0; pair < 9; pair++) {
        settings->name[pair] = *bytes++;
    }
    for (pair = 0; pair < 15; pair++) {
        ((u_char *)settings + pair)[0x45] = *bytes++;
    }
    for (pair = 0; pair < 13; pair++) {
        settings->entered.digits[pair] = *bytes++;
    }
    for (pair = 0; pair < 8; pair++) {
        trackMedals[pair] = *bytes++;
    }
    close(fd);
    SetCdVolume(((short)settings->musicVolume * 0xFF) / 100);
    SetSoundVolume(((short)settings->effectVolume * 0xFF) / 100);
    mute = 0;
    track = settings->cdTrack;
    if (track == 1) {
        hi = (cdMusicIndex + 1) % (cdMusicCount - 1);
        trackNameScroll = 0x1E0;
        cdMusicIndex = hi;
        track = cdMusicTable[hi & 0xFF];
        cdMusicTrack = track;
        CdMusicPlay(track, mute);
    } else {
        CdMusicPlay(track, mute);
    }
    InitChallenges();
    InitMovies();
    return 1;
}


/* Looks for the first save file on a card (slot 0 then slot 1) and loads its settings. */
void FirstSavedGame(void *data)
{
    char  name[16];
    short count;
    short slot = -1;
    short i = 0;
    int   status;
    short step;

    count = 0;
    for (i = 0; i < 2; i++) {
        status = GetCardInfo(i);
        if (status == 1) goto cardcheck;
        if (status >= 2) goto cardcheck;
        if (status != 0) goto next;
        ClearEventHW();
        if (slot == 0) _card_clear(0); else _card_clear(16);
        CheckHWCardEvents();
cardcheck:
        if (status == 0) {
            LoadCard(i);
            if (CardContentStatus() == 0) slot = i;
        }
next:
        if (slot != -1) {
            LoadFirstFile(&count, slot, name);
            if (count != 0) {
                if (LoadCardInitData(name, slot, data)) {
                    PortSaveSlotSet(slot, name); /* Port: the save in use (portsave.c) */
                }
                return;
            }
        }
    }
}


void LoadFirstFile(u_short *count, int slot, char *name)
{
    DIRENTRY record;
    char    *path;
    u_char   i;
    u_char   j;

    *count = 0;
    *name = 0;
    if ((slot << 16) == 0) {
        path = cardPattern0;
    } else {
        path = cardPattern1;
    }
    if (firstfile(path, &record) != &record) {
        *count = 0;
        return;
    }
    *count = 0;
    for (;;) {
        if (strncmp(record.name, CARD_GAME_CODE, 12) == 0) {
            i = 0;
            j = 12;
            do {
                name[i] = record.name[j];
                i++;
                j++;
            } while ((i & 0xFF) < 8);
            name[8] = 0;
            *count += 1;
            return;
        }
        if (nextfile(&record) == 0) {
            return;
        }
    }
}


int LoadCardInitData(char *name, short port, ConfigData *settings)
{
    CardFileBuffer buffer;
    int            fd;
    short          i, j;
    u_short        a, b, c, d, e, f;
    u_short       *cursor;
    u_char        *bytes;

    MakeFileName(name, buffer.name, port);
    fd = open(buffer.name, 1);
    if (fd < 0 || read(fd, buffer.header, CARD_HEADER_SIZE) != CARD_HEADER_SIZE || read(fd, hiScoreTable, 1536) != 1536 || read(fd, buffer.data, 256) != 256) {
        close(fd);
        return 0;
    }
    cursor = buffer.data + 6;
    a = buffer.data[0];
    b = buffer.data[1];
    c = buffer.data[2];
    d = buffer.data[3];
    e = buffer.data[4];
    f = buffer.data[5];
    settings->musicVolume = a;
    settings->effectVolume = b;
    settings->otherMusicVolume = c;
    settings->otherEffectVolume = d;
    settings->screenX = e;
    settings->screenY = f;
    for (i = 0; i < 2; i++) {
        for (j = 0; j < 9; j++) {
            savedKeyConfig.pad[i][j] = *cursor++;
        }
        for (j = 0; j < 6; j++) {
            savedKeyConfig.other[i][j] = *cursor++;
        }
    }
    for (i = 0; i < 2; i++) {
        keyDefineThrustPad[i] = savedKeyConfig.pad[i][0];
        keyDefineFire[i] = savedKeyConfig.pad[i][1];
        keyDefineView[i] = savedKeyConfig.pad[i][2];
        keyDefineDitch[i] = savedKeyConfig.pad[i][3];
        keyDefineRBrake[i] = savedKeyConfig.pad[i][4];
        keyDefineLBrake[i] = savedKeyConfig.pad[i][5];
    }
    bytes = (u_char *)cursor;
    settings->cdTrack = *bytes++;
    settings->soundMode = *bytes++;
    settings->negconMin = *bytes++;
    settings->negconCentre = *bytes++;
    settings->negconType = *bytes++;
    settings->screenOffsetState = *bytes++;
    settings->weaponsOn = *bytes++;
    settings->moveScreen = *bytes++;
    settings->internalView = *bytes++;
    settings->venomComplete = *bytes++;
    settings->rapierComplete = *bytes++;
    settings->venomChallengeDone = *bytes++;
    settings->rapierChallengeDone = *bytes++;
    settings->teamCheatActive = *bytes++;
    settings->class1Unlocked = *bytes++;
    settings->class2Unlocked = *bytes++;
    settings->trackCheatActive = *bytes++;
    settings->phantomTrackCheatActive = *bytes++;
    for (i = 0; i < 9; i++) settings->name[i] = *bytes++;
    for (i = 0; i < 15; i++) ((u_char *)settings + i)[0x45] = *bytes++;
    for (i = 0; i < 13; i++) settings->entered.digits[i] = *bytes++;
    for (i = 0; i < 8; i++) trackMedals[i] = *bytes++;
    close(fd);
    SetCdVolume(settings->musicVolume * 255 / 100);
    SetSoundVolume(settings->effectVolume * 255 / 100);
    InitMovies();
    return 1;
}
