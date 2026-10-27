#include "functions.h"
#include "tim.h"
#include "ships.h"
#include "portorig.h" /* Port: the original Wipeout's and Wipeout 64's tracks */

#define Clut4Bit 0
#define Clut8Bit 1
#define CLUT4 0x00
#define CLUT8 0x01

/* $gp-relative globals are tentative definitions; the rest are extern. */
extern int shipClutCount;

extern Texture       *textureTable[];
extern unsigned short ghostClutX;
extern unsigned short ghostClutY;
extern GhostClut      ghostCluts[];
extern TIMlist        timList;
extern TIMlist        wadTimList;
extern short          seqTextureCount;
extern char           errorString[];
extern char           fmtTrackNumPadded[];
extern char           fmtTrackNum[];
extern int            skyPos[];
extern Object        *sharedPrimTable[][40];


short InitScene(Object **objects, short *objectCount, Skeleton *camera, ConfigData *config)
{
    short  sceneTextures;
    short  skyTextures;
    short  objectIndex;
    short  sharedIndex;
    short  sky;
    Object *object;

    sceneTextures = TextureTableCount;
    getPath(scenePath, config->actTrackNum, "\\scene.cmp");
    LoadCmpFiles(LoadCompressedTextureSequence(scenePath));
    skyTextures = TextureTableCount;
    getPath(scenePath, config->actTrackNum, "\\sky.cmp");
    LoadCmpFiles(LoadCompressedTextureSequence(scenePath));
    for (objectIndex = 0; objectIndex < 128; objectIndex++) {
        for (sharedIndex = 0; sharedIndex < 40; sharedIndex++) {
            sharedPrimTable[objectIndex][sharedIndex] = 0;
        }
    }
    getPath(scenePath, config->actTrackNum, "\\scene.prm");
    objects[(*objectCount)++] = LoadPrm(scenePath, sceneTextures);
    for (objectIndex = 0; objectIndex < *objectCount; objectIndex++) {
        object = objects[objectIndex];
        while (object) {
            object->skeleton->super = camera;
            object = object->next;
        }
    }
    sky = *objectCount;
    getPath(scenePath, config->actTrackNum, "\\sky.prm");
    objects[(*objectCount)++] = LoadPrm(scenePath, skyTextures);
    object = objects[sky];
    object->skeleton->super = camera->super;
    SetSkeletonPosition(objects[sky]->skeleton, 0, skyPos[config->trackNum], 0);
    if (config->actTrackNum == 20) {
        SetSkeletonDirection(objects[sky]->skeleton, 0, 100, 0);
    }
    SceneRadiusCheck(objects[0]);
    return sky;
}


short WADInitScene(Object **objects, u_short *objectCount, Skeleton *camera, ConfigData *config, WADFILE *wad)
{
    u_short sceneTextures;
    u_short skyTextures;
    u_short sky;
    short   objectIndex;
    short   sharedIndex;
    Object *object;

    sceneTextures = TextureTableCount;
    LoadCmpFiles(WADLoadCompressedTextureSequence(wad));
    skyTextures = TextureTableCount;
    LoadCmpFiles(WADLoadCompressedTextureSequence(wad));
    for (objectIndex = 0; objectIndex < 128; objectIndex++) {
        for (sharedIndex = 0; sharedIndex < 40; sharedIndex++) {
            sharedPrimTable[objectIndex][sharedIndex] = 0;
        }
    }
    object = WADLoadPrm(wad, (short)sceneTextures);
    objects[(short)(*objectCount)++] = object;
    objectIndex = 0;
    if ((short)*objectCount > 0) {
        do {
            object = objects[objectIndex];
            while (object) {
                object->skeleton->super = camera;
                object = object->next;
            }
            objectIndex++;
        } while (objectIndex < (short)*objectCount);
    }
    sky = *objectCount;
    object = WADLoadPrm(wad, (short)skyTextures);
    objects[(short)(*objectCount)++] = object;
    object = objects[(short)sky];
    object->skeleton->super = camera->super;
    SetSkeletonPosition(objects[(short)sky]->skeleton, 0, skyPos[config->trackNum], 0);
    PortOriginalPlaceSky(objects[(short)sky]); /* Port: Wipeout 64's sky (portorig.c) */
    SceneRadiusCheck(objects[0]);
    return (short)sky;
}



void getPath(char *trackPath, unsigned char trackNo, char *name)
{
    /* Port: the console's buffer had two bytes, but "%02d" writes three (digits and terminator)
       and a track number can have three digits; sized for the longest result. */
    char trackNumText[4];

    __builtin_memcpy(trackPath, "c:\\wipeout2\\track", 18);
    if (trackNo <= 9) {
        sprintf(trackNumText, fmtTrackNumPadded, trackNo);
    } else {
        sprintf(trackNumText, fmtTrackNum, trackNo);
    }
    strcat(trackPath, trackNumText);
    strcat(trackPath, name);
}


void LoadCmpFiles(TIMlist *timPtr)
{
    short    i;
    Texture *texture;

    for (i = 0; i < seqTextureCount; i++) {
        texture = NewLoadTexture((Tim *)(timPtr->memBase + timPtr->memOffset[i]), 1, 0);
        if (texture) {
            textureTable[(short)TextureTableCount++] = texture;
        }
    }
    DFree(reserveHeap, timPtr->memBase);
}


void GhostLoadCmpFiles(TIMlist *timPtr)
{
    short    i;
    Texture *texture;

    for (i = 0; i < seqTextureCount; i++) {
        texture = NewLoadTexture((Tim *)(timPtr->memBase + timPtr->memOffset[i]), 1, 1);
        if (texture) {
            textureTable[(short)TextureTableCount++] = texture;
        }
    }
    DFree(reserveHeap, timPtr->memBase);
}


/* Uploads one TIM of a sequence and returns its texture record. A 4-bit TIM with no
   position is placed in the next free CLUT and texture cell; scene 1 also uploads a
   ghost copy of its CLUT. */
Texture *NewLoadTexture(struct Tim *tim, int context, int scene)
{
    TimClut4 *tc4;
    TimClut8 *tc8;
    Texture  *texture;
    RECT      imageRect;
    char      textureScratch[16];
    u16       newClutX, newClutY, newTexX, newTexY;
    u16       ghostX, ghostY;
    short     highBit;
    int       i;
    int       texX, texY;
    int       ghostIndex;
    int       pageX, pageY, pageX4, pageX2;
    int       tsbContext;
    int       sceneId;

    tsbContext = context;
    sceneId = scene;
    if ((tim->flags & 7) == Clut4Bit) {
        texture = DAlloc(heap, sizeof(Texture));
        texture->type = CLUT4;
        tc4 = (TimClut4 *)tim;

        if (*(int *)&tc4->textureX == 0) {
            newClutX = gClutX;
            newClutY = gClutY;
            newTexX = gTextureX;
            newTexY = gTextureY;
            tc4->clutW = 16;
            tc4->clutH = 1;
            tc4->clutX = newClutX;
            gClutX = newClutX + 16;
            tc4->clutY = newClutY;
            tc4->textureX = newTexX;
            tc4->textureY = newTexY;
            if ((short)gClutX >= 384) {
                gClutX = 320;
                gClutY = newClutY + 1;
            }
            gTextureY = newTexY + 32;
            if ((short)gTextureY >= 256) {
                gTextureX = newTexX + 8;
                gTextureY = 0;
            }
        }

        texture->clutX = tc4->clutX;
        texture->clutY = tc4->clutY;
        texture->clutW = tc4->clutW;
        texture->clutH = tc4->clutH;

        texture->textureX = tc4->textureX;
        texture->textureY = tc4->textureY;
        texture->textureW = tc4->textureW;
        texture->textureH = tc4->textureH;

        texX = tc4->textureX;
        texY = (short)tc4->textureY;
        pageX = texX - (texX & 0xFFC0);
        pageY = texY - (texY & 0x100);
        pageX4 = pageX * 4;
        texture->u0 = pageX4;
        texture->v0 = pageY;
        texture->u1 = (short)((((u16)tc4->textureW + pageX) * 4) - 1);
        texture->v1 = pageY;
        texture->u2 = pageX4;
        texture->v2 = (short)(((u16)tc4->textureH + pageY) - 1);
        texture->u3 = (short)((((u16)tc4->textureW + pageX) * 4) - 1);
        texture->v3 = ((u16)tc4->textureH + pageY) - 1;

        texture->tsb = (tsbContext << 5) | ((texture->textureX >> 6) + ((texture->textureY >> 4) & 16));
        texture->tsb |= (texture->textureY & 0x200) << 2; /* Port: the second half of video memory (an original Wipeout track's textures) */
        texture->cba = (texture->clutY << 6) | (texture->clutX >> 4);

        imageRect.x = tc4->clutX;
        imageRect.y = tc4->clutY;
        imageRect.w = tc4->clutW;
        imageRect.h = tc4->clutH;
        LoadImage(&imageRect, (u_long *)tc4->clut);
        DrawSync(0);

        if ((short)sceneId == 1) {
            highBit = 0x8000;
            for (i = 0; i < 16; i++) {
                tc4->clut[i] |= highBit;
            }
            ghostIndex = shipClutCount;
            ghostX = ghostClutX;
            ghostY = ghostClutY;
            ghostCluts[ghostIndex].clutX = ghostX;
            ghostClutX = ghostX + 16;
            ghostCluts[ghostIndex].clutY = ghostY;
            if ((short)ghostClutX >= 384) {
                ghostClutX = 320;
                ghostClutY = ghostY + 1;
            }
            imageRect.x = ghostCluts[ghostIndex].clutX;
            imageRect.y = ghostCluts[ghostIndex].clutY;
            LoadImage(&imageRect, (u_long *)tc4->clut);
            DrawSync(0);
            shipClutCount += 1;
        }

        imageRect.x = tc4->textureX;
        imageRect.y = tc4->textureY;
        imageRect.w = tc4->textureW;
        imageRect.h = tc4->textureH;
        LoadImage(&imageRect, (u_long *)(tc4 + 1));
        DrawSync(0);
    } else if ((tim->flags & 7) == Clut8Bit) {
        tc8 = (TimClut8 *)tim;
        texture = DAlloc(heap, sizeof(Texture));
        texture->type = CLUT8;

        if (*(int *)&tc8->textureX == 0) {
            Error("tim.c:LoadTexture(): Cannot auto place 8 bit textures", Fatal);
        }

        texture->clutX = tc8->clutX;
        texture->clutY = tc8->clutY;
        texture->clutW = tc8->clutW;
        texture->clutH = tc8->clutH;

        texture->textureX = tc8->textureX;
        texture->textureY = tc8->textureY;
        texture->textureW = tc8->textureW;
        texture->textureH = tc8->textureH;

        texX = tc8->textureX;
        texY = (short)tc8->textureY;
        pageX = texX - (texX & 0xFFC0);
        pageY = texY - (texY & 0x100);
        pageX2 = pageX * 2;
        texture->u0 = pageX2;
        texture->v0 = pageY;
        texture->u1 = (short)((((u16)tc8->textureW + pageX) * 2) - 1);
        texture->v1 = pageY;
        texture->u2 = pageX2;
        texture->v2 = (short)(((u16)tc8->textureH + pageY) - 1);
        texture->u3 = (short)((((u16)tc8->textureW + pageX) * 2) - 1);
        texture->v3 = ((u16)tc8->textureH + pageY) - 1;

        texture->tsb = (context << 5) | 128 | ((texture->textureX >> 6) + ((texture->textureY >> 4) & 16));
        texture->tsb |= (texture->textureY & 0x200) << 2; /* Port: (as for 4-bit textures) */
        texture->cba = (texture->clutY << 6) | (texture->clutX >> 4);

        imageRect.x = tc8->clutX;
        imageRect.y = tc8->clutY;
        imageRect.w = tc8->clutW;
        imageRect.h = tc8->clutH;
        LoadImage(&imageRect, (u_long *)tc8->clut);
        DrawSync(0);

        imageRect.x = tc8->textureX;
        imageRect.y = tc8->textureY;
        imageRect.w = tc8->textureW;
        imageRect.h = tc8->textureH;
        LoadImage(&imageRect, (u_long *)(tc8 + 1));
        DrawSync(0);
    } else {
        texture = 0;
    }
    return texture;
}



TIMlist *LoadCompressedTextureSequence(char *fileName)
{
    short  i;
    int   *fileBuffer;
    int   fileSize;
    int   textureSize;
    int   totalSize;

    GsClearVcount();
    fileSize = FileLength(fileName);
    if (fileSize <= 0) {
        sprintf(errorString, "Tim decompress: file %s not found\n", fileName);
        Error(errorString, Fatal);
    }
    fileBuffer = DAlloc(reserveHeap, fileSize + 32);
    if (!fileBuffer) {
        Error("Decompress Load: Failed to allocate memory for track vertices", Fatal);
    }
    if (LoadFile(fileName, (char *)fileBuffer) != fileSize) {
        sprintf(errorString, "Decompress(): Failed to load file %s\n", fileName);
        Error(errorString, Fatal);
    }
    seqTextureCount = fileBuffer[0];
    totalSize = 0;
    for (i = 0; i < seqTextureCount; i++) {
        timList.memOffset[i] = totalSize;
        textureSize = fileBuffer[i + 1];
        totalSize += textureSize;
    }
    timList.memBase = DAlloc(reserveHeap, totalSize);
    /* Port: the console added memBase to each offset here (an int held the address); the
       offsets stay offsets and LoadCmpFiles/GhostLoadCmpFiles add memBase when they use them. */
    GsGetVcount();
    GsClearVcount();
    ExpandData((unsigned char *)(fileBuffer + 1 + seqTextureCount), (unsigned char *)timList.memBase);
    GsGetVcount();
    DFree(reserveHeap, (char *)fileBuffer);
    return &timList;
}


TIMlist *WADLoadCompressedTextureSequence(WADFILE *wad)
{
    int *buffer;
    int  length;
    short i;

    GsClearVcount();
    buffer = DAlloc(reserveHeap, wad->sizes[wad->current] + 32);
    if (!buffer) {
        Error("Decompress Load: Failed to allocate memory for track vertices", Fatal);
    }
    readnextfile(wad, (char *)buffer);
    seqTextureCount = buffer[0];
    length = 0;
    for (i = 0; i < seqTextureCount; i++) {
        wadTimList.memOffset[i] = length;
        length += buffer[i + 1];
    }
    wadTimList.memBase = DAlloc(reserveHeap, length + 32);
    /* Port: the offsets stay offsets (see LoadCompressedTextureSequence). */
    GsGetVcount();
    GsClearVcount();
    ExpandData((u_char *)buffer + ((seqTextureCount * 4) + 4), wadTimList.memBase);
    GsGetVcount();
    DFree(reserveHeap, (char *)buffer);
    return &wadTimList;
}
