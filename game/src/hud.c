#include "hud.h"
#include "hiscores.h"
#include "ayrace.h"
#include "include_asm_g8.h"

/* Font textures are loaded at consecutive texture-table slots; the last one is the dimmed colour. */
extern short   hudTexIndex16;
extern short   hudTexIndex12;
extern short   hudTexIndex8;
extern short   hudTexIndexAlt;

extern u_char  trackMedals[];
extern int     savedLapTimes[];
extern int     shipDamage[];
extern int     envmapr;
extern int     envmapg;
extern int     envmapb;
extern short   sixTracks;
extern short   eightTracks;
extern short   rivalFinishPos;
extern u_char  raceEndState;
extern int     ownShip;
extern int     raceType;
extern int     cardNum;
extern int     quadIndex;
extern POLY_FT4 quadPool[];
/* Port: deleteTextures, endTextures are parts of textureTable; the uses read them directly. */
extern FontGlyph fontGlyphs16[];
extern FontGlyph fontGlyphs12[];
extern FontGlyph fontGlyphs8[];
extern u_short glyphWidths16[];
extern u_short glyphWidths12[];
extern u_short glyphWidths8[];
extern short   CurrentScreen;
extern Texture *textureTable[];
extern char    drFontsFile[];
/* Leading part of every challenge texture path. */
typedef struct PathSeed {
    char text[22];
} PathSeed; /* Port: the PlayStation compiler aligned this typedef to 4 (size stays 22); clang needs the size to be a multiple. */

extern PathSeed texturePathSeed[];
extern char    extCmp[];
extern char    msgLap[];
extern char    msgBadLuck[];
extern char    msgOutOfTime[];
extern char    msgShipDestroyed[];
extern char    msgRaceStats[];
extern char    msgCongratulations[];
extern char    msgBeatPlayerTwo[];
extern char    msgBeatPlayerOne[];
extern char    msgNoMedal[];
extern char    msgRacePosition[];
extern char    msgEliminations[];
extern char    msgYouBeatPlayerTwo[];
extern char    msgYouBeatPlayerOne[];
extern char    msgRaceTime[];
extern char    msgContinue[];
extern char    sufGold[];
extern char    sufSilver[];
extern char    sufBronze[];
extern char    msgAwarded[];


/* Textures for the four speedo elements, one per element, loaded by InitSpeedo. */
Texture *speedoFrameTex;
Texture *speedoSpeedTex;
Texture *speedoShieldTex;
Texture *speedoMarkerTex;

int raceInfoFrame;
char classNameVector[7] __attribute__((aligned(4)));
char classNameVenom[6] __attribute__((aligned(4)));
char classNameRapier[7] __attribute__((aligned(4)));
char classNamePhantom[8] __attribute__((aligned(4)));
/* Globals the module owns; they live in the small-data area, so they are tentative definitions. */
int     bonusFlashFrame;
int     bonusFrames;

extern short           linkDropped;
extern int             checkTimer;
extern u_char          noCheckPoints;
extern TrackData      *track;
extern ConfigData     *gameData;
extern char            msgExtendedTime[];


void InitText(void)
{
    void     *timPtr;
    int       i;

    hudTexIndex16 = TextureTableCount;
    timPtr = LoadCompressedTextureSequence(drFontsFile);
    LoadCmpFiles(timPtr);
    hudTexIndex12 = hudTexIndex16 + 1;
    hudTexIndex8 = hudTexIndex16 + 2;
    hudTexIndexAlt = hudTexIndex16 + 4;
    for (i = 0; i < 600; i++) {
        SetPolyFT4(&quadPool[i]);
        (&quadPool[i])->tpage = GetTPage(0, 0, 576, 256);
        quadPool[i].code = 0x2D;
        quadPool[i].r0 = 255;
        quadPool[i].g0 = 255;
        quadPool[i].b0 = 255;
    }
}


void InitSpeedo(Speedo *speedo)
{
    int screen;

    speedoFrameTex = WADLoadTexture(textureWad, 1);
    speedoSpeedTex = WADLoadTexture(textureWad, 1);
    speedoShieldTex = WADLoadTexture(textureWad, 1);
    speedoMarkerTex = WADLoadTexture(textureWad, 1);
    screen = 0;
    do {
        SetPolyFT4(&speedo->frame[screen]);
        speedo->frame[screen].r0 = 0xFF;
        speedo->frame[screen].g0 = 0xFF;
        speedo->frame[screen].b0 = 0xFF;
        speedo->frame[screen].y0 = 0xA3;
        speedo->frame[screen].y1 = 0xA3;
        speedo->frame[screen].x0 = 0xC3;
        speedo->frame[screen].x1 = 0x132;
        speedo->frame[screen].x2 = 0xC3;
        speedo->frame[screen].y2 = 0xE2;
        speedo->frame[screen].x3 = 0x132;
        speedo->frame[screen].y3 = 0xE2;
        speedo->frame[screen].u0 = (u_char) speedoFrameTex->u0;
        speedo->frame[screen].v0 = (u_char) speedoFrameTex->v0;
        speedo->frame[screen].u1 = (u_char) speedoFrameTex->u1;
        speedo->frame[screen].v1 = (u_char) speedoFrameTex->v1;
        speedo->frame[screen].u2 = (u_char) speedoFrameTex->u2;
        speedo->frame[screen].v2 = (u_char) speedoFrameTex->v2;
        speedo->frame[screen].u3 = (u_char) speedoFrameTex->u3;
        speedo->frame[screen].v3 = (u_char) speedoFrameTex->v3;
        speedo->frame[screen].tpage = GetTPage(0, 0, speedoFrameTex->textureX, speedoFrameTex->textureY);
        speedo->frame[screen].clut = (u_short) speedoFrameTex->cba;
        SetPolyFT4(&speedo->speed[screen]);
        speedo->speed[screen].r0 = 0xFF;
        speedo->speed[screen].g0 = 0xFF;
        speedo->speed[screen].b0 = 0xFF;
        speedo->speed[screen].x0 = 0xCE;
        speedo->speed[screen].y0 = 0xAD;
        speedo->speed[screen].x1 = 0x12E;
        speedo->speed[screen].y1 = 0xAD;
        speedo->speed[screen].x2 = 0xCE;
        speedo->speed[screen].y2 = 0xC3;
        speedo->speed[screen].x3 = 0x12E;
        speedo->speed[screen].y3 = 0xC3;
        speedo->speed[screen].u0 = (u_char) speedoSpeedTex->u0;
        speedo->speed[screen].v0 = (u_char) speedoSpeedTex->v0;
        speedo->speed[screen].u1 = (u_char) speedoSpeedTex->u1;
        speedo->speed[screen].v1 = (u_char) speedoSpeedTex->v1;
        speedo->speed[screen].u2 = (u_char) speedoSpeedTex->u2;
        speedo->speed[screen].v2 = (u_char) (speedoSpeedTex->v2 + 1);
        speedo->speed[screen].u3 = (u_char) speedoSpeedTex->u3;
        speedo->speed[screen].v3 = (u_char) (speedoSpeedTex->v3 + 1);
        speedo->speed[screen].tpage = GetTPage(1, 0, speedoSpeedTex->textureX, speedoSpeedTex->textureY);
        speedo->speed[screen].clut = (u_short) speedoSpeedTex->cba;
        SetPolyFT4(&speedo->shield[screen]);
        speedo->shield[screen].r0 = 0xFF;
        speedo->shield[screen].g0 = 0xFF;
        speedo->shield[screen].b0 = 0xFF;
        speedo->shield[screen].x0 = 0xC6;
        speedo->shield[screen].y0 = 0xC9;
        speedo->shield[screen].x1 = 0x126;
        speedo->shield[screen].y1 = 0xC9;
        speedo->shield[screen].x2 = 0xC6;
        speedo->shield[screen].y2 = 0xDF;
        speedo->shield[screen].x3 = 0x126;
        speedo->shield[screen].y3 = 0xDF;
        speedo->shield[screen].u0 = (u_char) speedoShieldTex->u0;
        speedo->shield[screen].v0 = (u_char) speedoShieldTex->v0;
        speedo->shield[screen].u1 = (u_char) speedoShieldTex->u1;
        speedo->shield[screen].v1 = (u_char) speedoShieldTex->v1;
        speedo->shield[screen].u2 = (u_char) speedoShieldTex->u2;
        speedo->shield[screen].v2 = (u_char) (speedoShieldTex->v2 + 1);
        speedo->shield[screen].u3 = (u_char) speedoShieldTex->u3;
        speedo->shield[screen].v3 = (u_char) (speedoShieldTex->v3 + 1);
        speedo->shield[screen].tpage = GetTPage(1, 1, speedoShieldTex->textureX, speedoShieldTex->textureY);
        SetSemiTrans(&speedo->shield[screen], 1);
        speedo->shield[screen].clut = (u_short) speedoShieldTex->cba;
        SetPolyFT4(&speedo->marker[screen]);
        speedo->marker[screen].r0 = 0xFF;
        speedo->marker[screen].g0 = 0xFF;
        speedo->marker[screen].b0 = 0xFF;
        speedo->marker[screen].x0 = 0xCE;
        speedo->marker[screen].y0 = 0xAD;
        speedo->marker[screen].x1 = 0x12E;
        speedo->marker[screen].y1 = 0xAD;
        speedo->marker[screen].x2 = 0xCE;
        speedo->marker[screen].y2 = 0xC3;
        speedo->marker[screen].x3 = 0x12E;
        speedo->marker[screen].y3 = 0xC3;
        speedo->marker[screen].u0 = (u_char) speedoMarkerTex->u0;
        speedo->marker[screen].v0 = (u_char) speedoMarkerTex->v0;
        speedo->marker[screen].u1 = (u_char) speedoMarkerTex->u1;
        speedo->marker[screen].v1 = (u_char) speedoMarkerTex->v1;
        speedo->marker[screen].u2 = (u_char) speedoMarkerTex->u2;
        speedo->marker[screen].v2 = (u_char) (speedoMarkerTex->v2 + 1);
        speedo->marker[screen].u3 = (u_char) speedoMarkerTex->u3;
        speedo->marker[screen].v3 = (u_char) (speedoMarkerTex->v3 + 1);
        speedo->marker[screen].tpage = GetTPage(0, 1, speedoMarkerTex->textureX, speedoMarkerTex->textureY);
        SetSemiTrans(&speedo->marker[screen], 1);
        speedo->marker[screen].clut = (u_short) speedoMarkerTex->cba;
        screen++;
    } while (screen < 2);
}


typedef struct SpeedDividers { int value[4]; } SpeedDividers;
extern SpeedDividers speedDividerTable;
extern SpeedDividers speedDividerTableAlt;
int energyBlinkFrame;

/* Port: was ((Speedo *)(CurrentScreen * sizeof(POLY_FT4) + (int)speedo)); the same quad of the current screen. */
#define SPEEDO_QUAD(part) speedo->part[CurrentScreen]

void fill_speedo(Speedo *speedo, ShipData *ships, ConfigData *game, int player)
{
    SpeedDividers dividers;
    SpeedDividers unused;
    int speed;
    int top, bottom;
    int inset = 11;
    short secondInset;
    int energyTop;
    int thrustTop, thrustBottom;

    dividers = speedDividerTable;
    unused = speedDividerTableAlt;
    if (CameraUpdate == UpdateCameraAttractInternal) speed = ships[ownShip].speed * 7;
    else speed = ships[ownShip].speed;
    top = speed / dividers.value[game->shipClass];
    bottom = top - inset;
    if (top >= 96) {
        bottom = 96;
        top = speedoSpeedTex->textureW * 2;
    }
    if (bottom < 0) bottom = 0;
    SPEEDO_QUAD(speed).x0 = 206;
    SPEEDO_QUAD(speed).y0 = 173;
    SPEEDO_QUAD(speed).x1 = 206 + top;
    SPEEDO_QUAD(speed).y1 = 173;
    SPEEDO_QUAD(speed).x2 = 206;
    SPEEDO_QUAD(speed).y2 = 195;
    SPEEDO_QUAD(speed).x3 = 206 + bottom;
    SPEEDO_QUAD(speed).y3 = 195;
    SPEEDO_QUAD(speed).u0 = (u_short)speedoSpeedTex->u0;
    SPEEDO_QUAD(speed).v0 = (u_short)speedoSpeedTex->v0;
    SPEEDO_QUAD(speed).u1 = (u_char)speedoSpeedTex->u0 + top;
    SPEEDO_QUAD(speed).v1 = (u_short)speedoSpeedTex->v1;
    SPEEDO_QUAD(speed).u2 = (u_short)speedoSpeedTex->u2;
    SPEEDO_QUAD(speed).v2 = (u_char)speedoSpeedTex->v2 + 1;
    SPEEDO_QUAD(speed).u3 = (u_char)speedoSpeedTex->u0 + bottom;
    SPEEDO_QUAD(speed).v3 = (u_char)speedoSpeedTex->v3 + 1;
    secondInset = inset;
    energyTop = 100 - shipDamage[ownShip];
    energyTop = energyTop * (speedoShieldTex->textureW * 2 - secondInset) / 100;
    energyTop += secondInset;
    bottom = energyTop - secondInset;
    if (bottom <= 0) bottom = 0;
    if (energyTop <= 0) energyTop = 0;
    SPEEDO_QUAD(shield).x0 = 198;
    SPEEDO_QUAD(shield).y0 = 201;
    SPEEDO_QUAD(shield).x1 = 198 + bottom;
    SPEEDO_QUAD(shield).y1 = 201;
    SPEEDO_QUAD(shield).x2 = 198;
    SPEEDO_QUAD(shield).y2 = 223;
    SPEEDO_QUAD(shield).x3 = 198 + energyTop;
    SPEEDO_QUAD(shield).y3 = 223;
    SPEEDO_QUAD(shield).u0 = (u_short)speedoShieldTex->u0;
    SPEEDO_QUAD(shield).v0 = (u_short)speedoShieldTex->v0;
    SPEEDO_QUAD(shield).u1 = (u_char)speedoShieldTex->u0 + bottom;
    SPEEDO_QUAD(shield).v1 = (u_short)speedoShieldTex->v1;
    SPEEDO_QUAD(shield).u2 = (u_short)speedoShieldTex->u2;
    SPEEDO_QUAD(shield).v2 = (u_char)speedoShieldTex->v2 + 1;
    SPEEDO_QUAD(shield).u3 = (u_char)speedoShieldTex->u0 + energyTop;
    SPEEDO_QUAD(shield).v3 = (u_char)speedoShieldTex->v3 + 1;
    speedo->shield[CurrentScreen].r0 = 128;
    speedo->shield[CurrentScreen].g0 = 128;
    speedo->shield[CurrentScreen].b0 = 128;
    thrustTop = ships[ownShip].thrust / (short)(ships[ownShip].maxThrust / 104);
    if (thrustTop >= 96) {
        thrustTop = speedoMarkerTex->textureW * 4;
    }
    thrustBottom = thrustTop - secondInset + 1;
    if (thrustBottom < 0) thrustBottom = 0;
    SPEEDO_QUAD(marker).x0 = 206;
    SPEEDO_QUAD(marker).y0 = 173;
    SPEEDO_QUAD(marker).x1 = 206 + thrustTop;
    SPEEDO_QUAD(marker).y1 = 173;
    SPEEDO_QUAD(marker).x2 = 206;
    SPEEDO_QUAD(marker).y2 = 195;
    SPEEDO_QUAD(marker).x3 = 206 + thrustBottom;
    SPEEDO_QUAD(marker).y3 = 195;
    SPEEDO_QUAD(marker).u0 = (u_short)speedoMarkerTex->u0;
    SPEEDO_QUAD(marker).v0 = (u_short)speedoMarkerTex->v0;
    SPEEDO_QUAD(marker).u1 = (u_char)speedoMarkerTex->u0 + thrustTop;
    SPEEDO_QUAD(marker).v1 = (u_short)speedoMarkerTex->v1;
    SPEEDO_QUAD(marker).u2 = (u_short)speedoMarkerTex->u2;
    SPEEDO_QUAD(marker).v2 = (u_char)speedoMarkerTex->v2 + 1;
    SPEEDO_QUAD(marker).u3 = (u_char)speedoMarkerTex->u0 + thrustBottom;
    SPEEDO_QUAD(marker).v3 = (u_char)speedoMarkerTex->v3 + 1;
    AddPrim(OT[CurrentScreen], &speedo->speed[CurrentScreen]);
    if (player != -1) AddPrim(OT[CurrentScreen], &speedo->shield[CurrentScreen]);
    else if (100 - shipDamage[ownShip] >= 41) AddPrim(OT[CurrentScreen], &speedo->shield[CurrentScreen]);
    else if (energyBlinkFrame % 8 < 5) AddPrim(OT[CurrentScreen], &speedo->shield[CurrentScreen]);
    AddPrim(OT[CurrentScreen], &speedo->marker[CurrentScreen]);
    energyBlinkFrame++;
}


void AddTime(int time, int x, int y, int size, int colour, unsigned char flags)
{
    int          j;
    int          length = 0;
    int          digits[20];
    Texture     *texture;
    int          mins = 0, secs = 0, tenths = 0;
    FontGlyph   *font, *glyph;
    POLY_FT4    *poly;
    u_short      width;
    int          charNum, clut, minBottom, secBottom;

    switch (size) {
    case 16: font = fontGlyphs16; texture = textureTable[hudTexIndex16]; break;
    case 12: font = fontGlyphs12; texture = textureTable[hudTexIndex12]; break;
    case 8:  font = fontGlyphs8;  texture = textureTable[hudTexIndex8];  break;
    }
    if (colour == 1) clut = texture->cba;
    else clut = textureTable[hudTexIndexAlt]->cba;
    tenths = (time / 6) % 10;
    time = time / 60;
    secs = time % 60;
    mins = time / 60;
    if (flags & 1) {
        digits[0] = mins % 10;
    } else {
        digits[0] = mins % 10;
        if (mins > 9) {
            mins = mins / 10;
            digits[1] = mins % 10;
        } else {
            digits[1] = 0;
        }
        length = 1;
    }
    if (!(flags & 2)) {
        for (j = length; j >= 0; j--) {
            minBottom = y + size;
            charNum = digits[j] + 26;
            width = font[charNum].width;
            glyph = &font[charNum];
            poly = &quadPool[quadIndex];
            poly->x0 = x;
            poly->y0 = y;
            poly->y1 = y;
            poly->x2 = x;
            poly->y2 = minBottom;
            poly->y3 = minBottom;
            poly->x1 = x + width;
            poly->x3 = x + width;
            poly->u0 = glyph->vramX;
            poly->v0 = glyph->vramY;
            poly->u1 = (u_char) glyph->vramX + width;
            poly->v1 = glyph->vramY;
            poly->u2 = glyph->vramX;
            poly->v2 = (u_char) glyph->vramY + size;
            poly->u3 = (u_char) glyph->vramX + width;
            poly->v3 = (u_char) glyph->vramY + size;
            poly->clut = clut;
            poly->tpage = GetTPage(0, 0, 576, 256);
            AddPrim(OT[CurrentScreen], poly);
            x += width;
            quadIndex++;
        }
    }
    if (!(flags & 2)) {
        width = font[36].width;
        glyph = &font[36];
        poly = &quadPool[quadIndex];
        poly->x0 = x;
        poly->y0 = y;
        poly->x1 = x + width;
        poly->y1 = y;
        poly->x2 = x;
        poly->y2 = y + size;
        poly->x3 = x + width;
        poly->y3 = y + size;
        poly->u0 = glyph->vramX;
        poly->v0 = glyph->vramY;
        poly->u1 = (u_char) glyph->vramX + width;
        poly->v1 = glyph->vramY;
        poly->u2 = glyph->vramX;
        poly->v2 = (u_char) glyph->vramY + size;
        poly->u3 = (u_char) glyph->vramX + width;
        poly->v3 = (u_char) glyph->vramY + size;
        poly->clut = clut;
        poly->tpage = GetTPage(0, 0, 576, 256);
        AddPrim(OT[CurrentScreen], poly);
        x += width;
        quadIndex++;
    }
    digits[0] = secs % 10;
    if (secs > 9) {
        secs = secs / 10;
        digits[1] = secs % 10;
    } else {
        digits[1] = 0;
    }
    for (j = 1; j >= 0; j--) {
        secBottom = y + size;
        charNum = digits[j] + 26;
        width = font[charNum].width;
        glyph = &font[charNum];
        poly = &quadPool[quadIndex];
        poly->x0 = x;
        poly->y0 = y;
        poly->y1 = y;
        poly->x2 = x;
        poly->y2 = secBottom;
        poly->y3 = secBottom;
        poly->x1 = x + width;
        poly->x3 = x + width;
        poly->u0 = glyph->vramX;
        poly->v0 = glyph->vramY;
        poly->u1 = (u_char) glyph->vramX + width;
        poly->v1 = glyph->vramY;
        poly->u2 = glyph->vramX;
        poly->v2 = (u_char) glyph->vramY + size;
        poly->u3 = (u_char) glyph->vramX + width;
        poly->v3 = (u_char) glyph->vramY + size;
        poly->clut = clut;
        poly->tpage = GetTPage(0, 0, 576, 256);
        AddPrim(OT[CurrentScreen], poly);
        x += width;
        quadIndex++;
    }
    width = font[37].width;
    glyph = &font[37];
    poly = &quadPool[quadIndex];
    poly->x0 = x;
    poly->y0 = y;
    poly->y1 = y;
    poly->x2 = x;
    poly->y2 = y + size;
    poly->y3 = y + size;
    poly->x1 = x + width;
    poly->x3 = x + width;
    poly->u0 = glyph->vramX;
    poly->v0 = glyph->vramY;
    poly->u1 = (u_char) glyph->vramX + width;
    poly->v1 = glyph->vramY;
    poly->u2 = glyph->vramX;
    poly->v2 = (u_char) glyph->vramY + size;
    poly->u3 = (u_char) glyph->vramX + width;
    poly->v3 = (u_char) glyph->vramY + size;
    poly->clut = clut;
    poly->tpage = GetTPage(0, 0, 576, 256);
    AddPrim(OT[CurrentScreen], poly);
    x += width;
    quadIndex++;
    font = fontGlyphs8;
    if (size != 8) {
        size = 8;
        y += 8;
    }
    width = font[tenths + 26].width;
    glyph = &font[tenths + 26];
    poly = &quadPool[quadIndex];
    poly->x0 = x;
    poly->y0 = y;
    poly->y1 = y;
    poly->x2 = x;
    poly->y2 = y + size;
    poly->y3 = y + size;
    poly->x1 = x + width;
    poly->x3 = x + width;
    poly->u0 = glyph->vramX;
    poly->v0 = glyph->vramY;
    poly->u1 = (u_char) glyph->vramX + width;
    poly->v1 = glyph->vramY;
    poly->u2 = glyph->vramX;
    poly->v2 = (u_char) glyph->vramY + size;
    poly->u3 = (u_char) glyph->vramX + width;
    poly->v3 = (u_char) glyph->vramY + size;
    poly->clut = clut;
    poly->tpage = GetTPage(0, 0, 576, 256);
    AddPrim(OT[CurrentScreen], poly);
    x += width;
    quadIndex++;
}


void AddText(unsigned char *textPointer, int x, int y, int size, int colour)
{
    unsigned char *text = textPointer;
    unsigned char  letter;
    Texture       *charTexture;
    FontGlyph     *charSet;
    u_short        width;
    int           clut;
    short          glyphIndex;
    short          bottom;
    POLY_FT4      *quad;

    switch (size) {
    case 16: charSet = fontGlyphs16; charTexture = textureTable[hudTexIndex16]; break;
    case 12: charSet = fontGlyphs12; charTexture = textureTable[hudTexIndex12]; break;
    case 8:  charSet = fontGlyphs8;  charTexture = textureTable[hudTexIndex8];  break;
    }
    if (colour == 1) clut = charTexture->cba;
    else clut = textureTable[hudTexIndexAlt]->cba;
    bottom = y + size;
    while (1) {
        letter = *text++;
        if (letter != 32) {
            if (letter == 0) return;
            glyphIndex = letter - 'A';
            width = charSet[glyphIndex].width;
            quad = &quadPool[quadIndex];
            quad->x0 = x; quad->y0 = y;
            quad->x1 = x + width; quad->y1 = y;
            quad->x2 = x; quad->y2 = bottom;
            quad->x3 = x + width; quad->y3 = bottom;
            quad->u0 = charSet[glyphIndex].vramX;
            quad->v0 = charSet[glyphIndex].vramY;
            quad->u1 = charSet[glyphIndex].vramX + width;
            quad->v1 = charSet[glyphIndex].vramY;
            quad->u2 = charSet[glyphIndex].vramX;
            quad->v2 = charSet[glyphIndex].vramY + size;
            quad->u3 = charSet[glyphIndex].vramX + width;
            quad->v3 = charSet[glyphIndex].vramY + size;
            quad->clut = clut;
            quad->tpage = GetTPage(0, 0, 576, 256);
            AddPrim(OT[CurrentScreen], quad);
            quadIndex++;
            x += width;
        } else {
            x += 8;
        }
    }
}


void AddNumber(int number, int x, int y, int size, int colour)
{
    int          digits[12];
    int          digitIndex, digitCount;
    int          glyphIndex;
    u_short       width;
    Texture      *charTexture;
    FontGlyph    *charSet;
    POLY_FT4     *quad;
    POLY_FT4     *quads;
    u_short       clut;
    int          quadOffset;
    u_short       texturePage;

    switch (size) {
    case 16: charSet = fontGlyphs16; charTexture = textureTable[hudTexIndex16]; break;
    case 12: charSet = fontGlyphs12; charTexture = textureTable[hudTexIndex12]; break;
    case 8:  charSet = fontGlyphs8;  charTexture = textureTable[hudTexIndex8];  break;
    }
    for (digitCount = 0; digitCount < 5; digitCount++) {
        digits[digitCount] = number % 10;
        number /= 10;
        if (number == 0) break;
    }
    for (digitIndex = digitCount; digitIndex >= 0; digitIndex--) {
        quads = quadPool;
        glyphIndex = digits[digitIndex] + 26;
        width = charSet[glyphIndex].width;
        quad = (POLY_FT4 *)((char *)quads + quadIndex * 40);
        quad->x0 = x; quad->y0 = y;
        quad->x1 = width + x; quad->y1 = y;
        quad->x2 = x; quad->y2 = y + size;
        quad->x3 = width + x; quad->y3 = y + size;
        quads[quadIndex].u0 = charSet[glyphIndex].vramX;
        quads[quadIndex].v0 = charSet[glyphIndex].vramY;
        quads[quadIndex].u1 = charSet[glyphIndex].vramX + width;
        quads[quadIndex].v1 = charSet[glyphIndex].vramY;
        quads[quadIndex].u2 = charSet[glyphIndex].vramX;
        quads[quadIndex].v2 = charSet[glyphIndex].vramY + size;
        quads[quadIndex].u3 = charSet[glyphIndex].vramX + width;
        quads[quadIndex].v3 = charSet[glyphIndex].vramY + size;
        texturePage = GetTPage(0, 0, 576, 256);
        quadOffset = quadIndex * 40;
        quads[quadIndex].tpage = texturePage;
        if (colour == 1) clut = charTexture->cba;
        else clut = textureTable[hudTexIndexAlt]->cba;
        /* Port: stored through quadClutField, a symbol inside quadPool. */
        ((POLY_FT4 *)((char *)quads + quadOffset))->clut = clut;
        AddPrim(OT[CurrentScreen], (POLY_FT4 *)((char *)quads + quadIndex * 40));
        quadIndex++;
        x += width;
    }
}


void AddJustifyNumber(int number, int xInput, int y, int size, int colour)
{
    int          digits[12];
    int          zeroLocal = 0;
    int          digitIndex, digitCount;
    int          glyphIndex;
    u_short       width = 0;
    int          x = xInput;
    Texture      *charTexture;
    FontGlyph    *charSet;
    POLY_FT4     *quad;
    POLY_FT4     *quads;
    u_short       clut;
    int          quadOffset;
    u_short       texturePage;

    switch (size) {
    case 16: charSet = fontGlyphs16; charTexture = textureTable[hudTexIndex16]; break;
    case 12: charSet = fontGlyphs12; charTexture = textureTable[hudTexIndex12]; break;
    case 8:  charSet = fontGlyphs8;  charTexture = textureTable[hudTexIndex8];  break;
    }
    for (digitCount = 0; digitCount < 5; digitCount++) {
        digits[digitCount] = number % 10;
        number /= 10;
        if (number == 0) break;
    }
    for (digitIndex = digitCount; digitIndex + zeroLocal >= 0; digitIndex--) {
        glyphIndex = digits[digitIndex] + 26;
        width = charSet[glyphIndex].width;
        x -= width;
    }
    for (digitIndex = digitCount; digitIndex + zeroLocal >= 0; digitIndex--) {
        quads = quadPool;
        glyphIndex = digits[digitIndex] + 26;
        width = charSet[glyphIndex].width;
        quad = (POLY_FT4 *)((char *)quads + quadIndex * 40);
        quad->x0 = x; quad->y0 = y;
        quad->x1 = width + x; quad->y1 = y;
        quad->x2 = x; quad->y2 = y + size;
        quad->x3 = width + x; quad->y3 = y + size;
        quads[quadIndex].u0 = charSet[glyphIndex].vramX;
        quads[quadIndex].v0 = charSet[glyphIndex].vramY;
        quads[quadIndex].u1 = charSet[glyphIndex].vramX + width;
        quads[quadIndex].v1 = charSet[glyphIndex].vramY;
        quads[quadIndex].u2 = charSet[glyphIndex].vramX;
        quads[quadIndex].v2 = charSet[glyphIndex].vramY + size;
        quads[quadIndex].u3 = charSet[glyphIndex].vramX + width;
        quads[quadIndex].v3 = charSet[glyphIndex].vramY + size;
        texturePage = GetTPage(0, 0, 576, 256);
        quadOffset = quadIndex * 40;
        quads[quadIndex].tpage = texturePage;
        if (colour == 1) clut = charTexture->cba;
        else clut = textureTable[hudTexIndexAlt]->cba;
        /* Port: stored through quadClutField, a symbol inside quadPool. */
        ((POLY_FT4 *)((char *)quads + quadOffset))->clut = clut;
        AddPrim(OT[CurrentScreen], (POLY_FT4 *)((char *)quads + quadIndex * 40));
        quadIndex++;
        x += width;
    }
}


void AddDelEnd(unsigned char isDelete, int x, int y, int height, int colour)
{
    short      displayBounds[4];
    Texture   *deleteTexture = textureTable[3 + hudTexIndexAlt];
    Texture   *endTexture = textureTable[4 + hudTexIndexAlt];
    POLY_FT4  *quads = quadPool;
    POLY_FT4  *quad = (POLY_FT4 *)((char *)quads + quadIndex * 40);
    int       right = x + 24;
    int       bottom = y + height;
    u_short    texturePage, clut;
    int       quadOffset;

    quad->y0 = y; quad->y1 = y;
    quad->x0 = x; quad->x1 = right; quad->x2 = x;
    quad->y2 = bottom; quad->x3 = right; quad->y3 = bottom;
    if (isDelete) {
        quad->u0 = deleteTexture->u0;
        quads[quadIndex].v0 = deleteTexture->v0;
        quads[quadIndex].u1 = deleteTexture->u1;
        quads[quadIndex].v1 = deleteTexture->v1;
        quads[quadIndex].u2 = deleteTexture->u2;
        quads[quadIndex].v2 = deleteTexture->v2 + 1;
        quads[quadIndex].u3 = deleteTexture->u3;
        quads[quadIndex].v3 = deleteTexture->v3 + 1;
    } else {
        quad->u0 = endTexture->u0;
        quads[quadIndex].v0 = endTexture->v0;
        quads[quadIndex].u1 = endTexture->u1;
        quads[quadIndex].v1 = endTexture->v1;
        quads[quadIndex].u2 = endTexture->u2;
        quads[quadIndex].v2 = endTexture->v2 + 1;
        quads[quadIndex].u3 = endTexture->u3;
        quads[quadIndex].v3 = endTexture->v3 + 1;
    }
    texturePage = GetTPage(0, 0, 576, 256);
    quadOffset = quadIndex * 40;
    /* Port: stored through quadTPageField, a symbol inside quadPool. */
    ((POLY_FT4 *)((char *)quads + quadOffset))->tpage = texturePage;
    if (colour == 1) clut = deleteTexture->cba;
    else clut = textureTable[hudTexIndexAlt]->cba;
    /* Port: stored through quadClutField, a symbol inside quadPool. */
    ((POLY_FT4 *)((char *)quads + quadOffset))->clut = clut;
    AddPrim(OT[CurrentScreen], (POLY_FT4 *)((char *)quadPool + quadIndex * 40));
    quadIndex++;
}


void SetCharTextures(unsigned char size)
{
    short     currentLetter, searchLetter, glyphIndex, previousGlyph, row, vramY, vramX;
    u_short  *widthTable;

    switch (size) {
    case 8:  widthTable = glyphWidths8;  break;
    case 12: widthTable = glyphWidths12; break;
    case 16: widthTable = glyphWidths16; break;
    }
    for (glyphIndex = 0; glyphIndex < 40; glyphIndex++) {
        currentLetter = widthTable[glyphIndex];
        vramY = 0;
        row = currentLetter / 100;
        vramY += row * size;
        vramX = 0;
        for (previousGlyph = 0; previousGlyph < glyphIndex; previousGlyph++) {
            searchLetter = widthTable[previousGlyph];
            if ((short) (searchLetter / 100) == row) vramX += searchLetter % 100;
        }
        switch (size) {
        case 8:
            fontGlyphs8[glyphIndex].vramX = vramX;
            fontGlyphs8[glyphIndex].vramY = vramY + 100;
            fontGlyphs8[glyphIndex].width = currentLetter % 100;
            break;
        case 12:
            fontGlyphs12[glyphIndex].vramX = vramX;
            fontGlyphs12[glyphIndex].vramY = vramY + 64;
            fontGlyphs12[glyphIndex].width = currentLetter % 100;
            break;
        case 16:
            fontGlyphs16[glyphIndex].vramX = vramX;
            fontGlyphs16[glyphIndex].vramY = vramY;
            fontGlyphs16[glyphIndex].width = currentLetter % 100;
            break;
        }
    }
}


void CentreText(unsigned char *text, int unusedX, int yInput, int sizeInput, int colour)
{
    short         textWidth = 0;
    int          y = yInput;
    int          size = sizeInput;
    int          x;
    unsigned char *scan, letter;
    u_short       width;
    Texture      *charTexture;
    FontGlyph    *charSet;
    POLY_FT4     *quad;
    POLY_FT4     *quads;
    u_short       clut;
    int          quadOffset;
    u_short       texturePage;
    short         glyphIndex;

    switch (size) {
    case 16: charSet = fontGlyphs16; charTexture = textureTable[hudTexIndex16]; break;
    case 12: charSet = fontGlyphs12; charTexture = textureTable[hudTexIndex12]; break;
    case 8:  charSet = fontGlyphs8;  charTexture = textureTable[hudTexIndex8];  break;
    }
    scan = text;
    letter = *scan;
    while (letter) {
        glyphIndex = letter - 'A';
        if (letter != 32) {
            textWidth += charSet[glyphIndex].width;
        } else {
            textWidth += 8;
        }
        scan++;
        letter = *scan;
    }
    x = 160 - ((short) textWidth >> 1);
    quads = quadPool;
    while (1) {
        letter = *text++;
        if (letter != 32) {
            if (letter == 0) return;
            glyphIndex = letter - 'A';
            width = charSet[glyphIndex].width;
            quad = (POLY_FT4 *)((char *)quads + quadIndex * 40);
            quad->x0 = x; quad->y0 = y;
            quad->x1 = width + x; quad->y1 = y;
            quad->x2 = x; quad->y2 = y + size;
            quad->x3 = width + x; quad->y3 = y + size;
            quads[quadIndex].u0 = charSet[glyphIndex].vramX;
            quads[quadIndex].v0 = charSet[glyphIndex].vramY;
            quads[quadIndex].u1 = charSet[glyphIndex].vramX + width;
            quads[quadIndex].v1 = charSet[glyphIndex].vramY;
            quads[quadIndex].u2 = charSet[glyphIndex].vramX;
            quads[quadIndex].v2 = charSet[glyphIndex].vramY + size;
            quads[quadIndex].u3 = charSet[glyphIndex].vramX + width;
            quads[quadIndex].v3 = charSet[glyphIndex].vramY + size;
            texturePage = GetTPage(0, 0, 576, 256);
            quadOffset = quadIndex * 40;
            quads[quadIndex].tpage = texturePage;
            if (colour == 1) clut = charTexture->cba;
            else clut = textureTable[hudTexIndexAlt]->cba;
            /* Port: stored through quadClutField, a symbol inside quadPool. */
            ((POLY_FT4 *)((char *)quads + quadOffset))->clut = clut;
            AddPrim(OT[CurrentScreen], (POLY_FT4 *)((char *)quads + quadIndex * 40));
            quadIndex++;
            x = width + x;
        } else {
            x += 8;
        }
    }
}


void CentreText2(unsigned char *text, int x, int y, int size, int colour)
{
    short         textWidth = 0;
    unsigned char *scan, letter;
    u_short       width;
    Texture      *charTexture;
    FontGlyph    *charSet;
    POLY_FT4     *quad;
    POLY_FT4     *quads;
    u_short       clut;
    int          quadOffset;
    u_short       texturePage;
    short         glyphIndex;

    switch (size) {
    case 16: charSet = fontGlyphs16; charTexture = textureTable[hudTexIndex16]; break;
    case 12: charSet = fontGlyphs12; charTexture = textureTable[hudTexIndex12]; break;
    case 8:  charSet = fontGlyphs8;  charTexture = textureTable[hudTexIndex8];  break;
    }
    scan = text;
    letter = *scan;
    while (letter) {
        glyphIndex = letter - 'A';
        if (letter != 32) {
            textWidth += charSet[glyphIndex].width;
        } else {
            textWidth += 8;
        }
        scan++;
        letter = *scan;
    }
    x -= ((short) textWidth >> 1);
    quads = quadPool;
    while (1) {
        letter = *text++;
        if (letter != 32) {
            if (letter == 0) return;
            glyphIndex = letter - 'A';
            width = charSet[glyphIndex].width;
            quad = (POLY_FT4 *)((char *)quads + quadIndex * 40);
            quad->x0 = x; quad->y0 = y;
            quad->x1 = width + x; quad->y1 = y;
            quad->x2 = x; quad->y2 = y + size;
            quad->x3 = width + x; quad->y3 = y + size;
            quads[quadIndex].u0 = charSet[glyphIndex].vramX;
            quads[quadIndex].v0 = charSet[glyphIndex].vramY;
            quads[quadIndex].u1 = charSet[glyphIndex].vramX + width;
            quads[quadIndex].v1 = charSet[glyphIndex].vramY;
            quads[quadIndex].u2 = charSet[glyphIndex].vramX;
            quads[quadIndex].v2 = charSet[glyphIndex].vramY + size;
            quads[quadIndex].u3 = charSet[glyphIndex].vramX + width;
            quads[quadIndex].v3 = charSet[glyphIndex].vramY + size;
            texturePage = GetTPage(0, 0, 576, 256);
            quadOffset = quadIndex * 40;
            quads[quadIndex].tpage = texturePage;
            if (colour == 1) clut = charTexture->cba;
            else clut = textureTable[hudTexIndexAlt]->cba;
            /* Port: stored through quadClutField, a symbol inside quadPool. */
            ((POLY_FT4 *)((char *)quads + quadOffset))->clut = clut;
            AddPrim(OT[CurrentScreen], (POLY_FT4 *)((char *)quads + quadIndex * 40));
            quadIndex++;
            x = width + x;
        } else {
            x += 8;
        }
    }
}


typedef struct { short y[4]; } __attribute__((packed)) HudTextRows;

extern HudTextRows classRowY[];
extern RaceLine raceLines[];
extern int ghostShip;
extern int remoteShip;
extern int cdMusicTrack;
extern int attractDelayFrames;
extern u_char perfectLapCount;
extern u_char linkRaceEnd;
extern u_char cdMusicIndex;
extern u_char rearLockCycle;
extern u_char cdMusicTable[];
extern char trackNameText[];
extern char msgLapTimes[];
extern char msgAutopilot[];
extern char msgDisengaging[];
extern char msgRearLock[];
extern char msgPerfectLap[];
extern char fmtCdTrack[];
extern char msgWrongWay[];
extern char fmtNumber[];
extern char msgP[];
extern char msgOf[];
extern char msgCheck[];
extern char msgPos[];
int trackNameScroll;
int trackNameOffset;
int hudFrame;

void UpdateRaceHud(ShipData *shipIndex, Speedo *speedo, int choice, ConfigData *gameData, HiScoreData *scoreData)
{
    struct { int reserved[4]; HudTextRows rows; char text[8]; } locals;
    short i, sectionDifference, lap, position;
    int racingCount, speed;
    short autopilot;
    char *trackName, *artist;

    locals.rows = classRowY[0];
    racingCount = 0;
    for (i = 0; i < 15; i++) {
        if (!(shipIndex[i].combatAttr & 0x8000) && shipIndex[i].lap < classLaps[gameData->shipClass] + 2) {
            sectionDifference = shipIndex[i].trackDistance - raceLines[shipIndex[i].nearTrkSect->secNo].distance;
            if (sectionDifference > (numTrackSections >> 1)) {
                if (shipIndex[i].reverseLap == 0) {
                    shipIndex[i].lap++;
                    if (i == ownShip && shipIndex[i].lap > 1 && shipIndex[i].lap <= classLaps[gameData->shipClass] + 1)
                        CheckForHiScore(gameData, scoreData, shipIndex, i, 1);
                } else shipIndex[i].reverseLap--;
                shipIndex[i].combatAttr |= 0x4000;
                if ((i == ownShip || i == ghostShip) && raceType == 1 && i < 12) shipIndex[i].weapon = 9;
                if (i == ownShip) {
                    if (perfectLap) perfectLapCount = 120;
                    perfectLap = 1;
                }
            } else if (sectionDifference < -(numTrackSections >> 1)) {
                if (i == ownShip) perfectLap = 0;
                shipIndex[i].reverseLap++;
            }
            lap = shipIndex[i].lap - shipIndex[i].reverseLap;
            if (lap < 0) lap = 0;
            shipIndex[i].distance = lap * numTrackSections + raceLines[shipIndex[i].nearTrkSect->secNo].distance;
            shipIndex[i].trackDistance = raceLines[shipIndex[i].nearTrkSect->secNo].distance;
        }
    }
    position = CalcPosition(shipIndex);
    playerPosition = position;
    if ((unsigned)(raceType - 2) < 2) {
        if (shipIndex[remoteShip].lap > 0 && choice == -1 && shipIndex[remoteShip].lap < classLaps[gameData->shipClass] + 1)
            shipIndex[remoteShip].lapTimes[shipIndex[remoteShip].lap - 1] += 2;
    }
    if (shipIndex[ownShip].combatAttr & 0x4000) {
        lap = shipIndex[ownShip].lap;
        if (lap == 0) lap = 1;
        if (choice == -1 && shipIndex[ownShip].lap < classLaps[gameData->shipClass] + 1)
            shipIndex[ownShip].lapTimes[lap - 1] += 2;
        if (!(shipIndex[ownShip].attr & 64)) goto finishShips;
        if (shipIndex[ownShip].lap < classLaps[gameData->shipClass] + 1)
            AddTime(shipIndex[ownShip].lapTimes[lap - 1], 16, 201, 16, 1, 1);
        else AddTime(shipIndex[ownShip].lapTimes[classLaps[gameData->shipClass] - 1], 16, 201, 16, 1, 1);
    }
    if (shipIndex[ownShip].attr & 64) {
        if (shipIndex[ownShip].combatAttr & 0x4000) {
            Print(msgLapTimes, 22, locals.rows.y[gameData->shipClass] - 8, 8);
            for (i = 0; i < classLaps[gameData->shipClass] - 1; i++) {
                sprintf(locals.text, fmtNumber, i + 1);
                Print(locals.text, 16, locals.rows.y[gameData->shipClass] + i * 10 + 4, 8);
            }
            for (i = 1; i < shipIndex[ownShip].lap; i++) {
                if (i < classLaps[gameData->shipClass])
                    AddTime(shipIndex[ownShip].lapTimes[i - 1], 24, locals.rows.y[gameData->shipClass] + i * 10 - 8, 8, 0, 1);
            }
        }
        autopilot = shipIndex[ownShip].autopilotCount;
        if (autopilot) {
            AddText(msgAutopilot, 120, 108, 8, 0);
            if (autopilot < 30) AddText(msgDisengaging, 110, 120, 8, 0);
            else AddNumber((short)(autopilot / 30), 160, 128, 12, 1);
            speed = shipIndex[ownShip].moveDist * 34;
        } else speed = shipIndex[ownShip].speed;
        if (gameType != 2 && linkRaceEnd != 2) {
            AddText(msgP, 260, 84, 8, 0);
            if (gameType == 0) AddNumber(2, 274, 84, 8, 0);
            else AddNumber(1, 274, 84, 8, 0);
            AddNumber(remoteRacePos, 290, 84, 8, 1);
        }
        AddText(msgLap, 255, 50, 8, 0);
        if (shipIndex[ownShip].lap > classLaps[gameData->shipClass])
            AddJustifyNumber(classLaps[gameData->shipClass], 260, 60, 16, 1);
        else AddJustifyNumber(shipIndex[ownShip].lap, 260, 60, 16, 1);
        AddText(msgOf, 260, 68, 8, 0);
        AddNumber(classLaps[gameData->shipClass], 282, 60, 16, 1);
        if (shipIndex[ownShip].rearTarget != -1 && rearLockCycle < 13 && shipIndex[ownShip].weapon == 2)
            /* Port: the call passed a fourth argument (the size, 8) that PrintCX never reads; dropped. */
            PrintCX(msgRearLock, 168, 68);
        if (perfectLapCount) {
            CentreText(msgPerfectLap, 120, 160, 8, 1);
            perfectLapCount--;
        }
        if (trackNameScroll >= -479) {
            trackNameOffset = trackNameScroll;
            if ((unsigned)(trackNameScroll + 199) < 199) trackNameOffset = 0;
            if (trackNameOffset < -199) trackNameOffset += 200;
            if (trackNameOffset < 0) trackNameOffset = -trackNameOffset;
            if (pauseState.selection == -1) trackNameScroll -= 6;
            if (gameData->cdTrack == 1) cdMusicTrack = cdMusicTable[cdMusicIndex];
            else cdMusicTrack = gameData->cdTrack;
            trackName = GetCdTrackName(cdMusicTrack - 1);
            artist = (char *)GetCdTrackArtist(cdMusicTrack - 1);
            sprintf(trackNameText, fmtCdTrack, cdMusicTrack - 1, trackName, artist);
            /* Port: the call passed a fifth argument (1) that Print never reads; dropped. */
            Print(trackNameText, 16, (trackNameOffset >> 2) + 217, 8);
        }
        AddText(msgCheck, 20, 20, 8, 0);
        for (i = 0; i < 15; i++)
            if (!(shipIndex[i].combatAttr & 0x8000)) racingCount++;
        AddText(msgPos, 255, 20, 8, 0);
        AddJustifyNumber(position, 260, 30, 16, 1);
        AddText(msgOf, 260, 38, 8, 0);
        AddNumber((short)racingCount, 282, 30, 16, 1);
        if ((shipIndex[ownShip].attr & 64) && shipIndex[ownShip].lap >= 0) {
            if (checkTimer <= 0) {
                if (hudFrame % 30 < 21) AddTime(checkTimer, 20, 30, 16, 0, 1);
            } else AddTime(checkTimer, 20, 30, 16, 1, 1);
        }
        /* Port: junction.ptr == (TrackSection *)-1 tested the 32-bit slot; compare the slot as the index (-1 = no junction). */
        if (choice == -1 && autopilot == 0 && shipDamage[ownShip] < 100 && !(shipIndex[ownShip].attr & 8) && shipIndex[ownShip].nearTrkSect->junction.index == -1 && shipIndex[ownShip].nearTrkSect->nextSection.ptr->junction.index == -1)
            CentreText(msgWrongWay, 50, 110, 8, 0);
        AddJustifyNumber(speed / 95, 240, 171, 16, 1);
    }
finishShips:
    for (i = 0; i < 15; i++) {
        if (i != ownShip && shipIndex[i].lap > classLaps[gameData->shipClass] && (shipIndex[i].attr & 64)) {
            shipIndex[i].attr &= ~64;
            if (i == remoteShip) {
                shipIndex[i].update = UpdateDroneShip;
                shipIndex[i].updateCount = 0;
                shipIndex[i].attr = 0;
                shipIndex[i].autopilotCount = 0;
                /* Port: the call passed a third argument (the ship) that humanToDroneSpeed never reads; dropped. */
                humanToDroneSpeed(shipIndex, i);
            }
        }
    }
    if (shipIndex[ownShip].lap > classLaps[gameData->shipClass] && choice == -1) {
        if (!(shipIndex[ownShip].attr & 64)) goto done;
        shipIndex[ownShip].update = UpdateDroneShip;
        shipIndex[ownShip].updateCount = 0;
        shipIndex[ownShip].attr = 0;
        shipIndex[ownShip].autopilotCount = 0;
        /* Port: the call passed a third argument (the ship) that humanToDroneSpeed never reads; dropped. */
        humanToDroneSpeed(shipIndex, ownShip);
        CameraUpdate = UpdateCameraAttractMode;
        copyInTempLapTimes(shipIndex);
        gameData->raceInfo = 1;
        linkSendFlag = 1;
        attractDelayFrames = 1;
        raceEndState = 1;
        finishPosition = playerPosition;
        AssignMedals(gameData);
    }
    if (shipIndex[ownShip].attr & 64) {
        if (shipIndex[ownShip].attr & 1024) shipIndex[ownShip].speed = 0;
        AddPrim(OT[CurrentScreen], &speedo->frame[CurrentScreen]);
        fill_speedo(speedo, shipIndex, gameData, choice);
    }
done:
    hudFrame++;
}


short CalcPosition(ShipData *ships)
{
    VECTOR  playerOffset, shipOffset, direction;
    int    playerDistance, shipDistance, playerAngle, shipAngle;
    int    playerProgress;
    int    shipIndex;
    short   position = 1;

    playerProgress = ships[ownShip].distance;
    playerOffset.vx = ships[ownShip].ppivot.vx - ships[ownShip].nearTrkSect->centre.vx;
    playerOffset.vy = ships[ownShip].ppivot.vy - ships[ownShip].nearTrkSect->centre.vy;
    playerOffset.vz = ships[ownShip].ppivot.vz - ships[ownShip].nearTrkSect->centre.vz;
    direction.vx = ships[ownShip].nearTrkSect->nextSection.ptr->centre.vx - ships[ownShip].nearTrkSect->centre.vx;
    direction.vy = ships[ownShip].nearTrkSect->nextSection.ptr->centre.vy - ships[ownShip].nearTrkSect->centre.vy;
    direction.vz = ships[ownShip].nearTrkSect->nextSection.ptr->centre.vz - ships[ownShip].nearTrkSect->centre.vz;
    for (shipIndex = 0; shipIndex < 15; shipIndex++) {
        if (shipIndex != ownShip && !(ships[shipIndex].combatAttr & 0x8000)) {
            if (playerProgress == ships[shipIndex].distance) {
                shipOffset.vx = ships[shipIndex].ppivot.vx - ships[shipIndex].nearTrkSect->centre.vx;
                shipOffset.vy = ships[shipIndex].ppivot.vy - ships[shipIndex].nearTrkSect->centre.vy;
                shipOffset.vz = ships[shipIndex].ppivot.vz - ships[shipIndex].nearTrkSect->centre.vz;
                playerAngle = GetAng(playerOffset, direction);
                shipAngle = GetAng(shipOffset, direction);
                playerDistance = (rcos(playerAngle >> 3) * GetMagnitude(playerOffset)) >> 12;
                shipDistance = (rcos(shipAngle >> 3) * GetMagnitude(shipOffset)) >> 12;
                if (shipDistance >= playerDistance) position++;
            } else if (playerProgress < ships[shipIndex].distance) {
                position++;
            }
        }
    }
    return position;
}


void AssignMedals(ConfigData *config)
{
    short goldCount = 0;
    short trackIndex;

    if (raceType == 0 || raceType == 4) {
        if ((unsigned int) ((u_short) finishPosition - 1) < 3U) {
            switch (finishPosition) {
            case 1:
                if (trackMedals[config->trackNum] == 0 || (short) finishPosition < trackMedals[config->trackNum]) trackMedals[config->trackNum] = 1;
                envmapr = 255; envmapg = 128; envmapb = 0;
                break;
            case 2:
                if (trackMedals[config->trackNum] == 0 || (short) finishPosition < trackMedals[config->trackNum]) trackMedals[config->trackNum] = 2;
                envmapr = 224; envmapg = 224; envmapb = 224;
                break;
            case 3:
                if (trackMedals[config->trackNum] == 0 || (short) finishPosition < trackMedals[config->trackNum]) trackMedals[config->trackNum] = 3;
                envmapr = 192; envmapg = 32; envmapb = 0;
                break;
            }
        }
        for (trackIndex = 0; trackIndex < 8; trackIndex++) {
            if (trackMedals[trackIndex] == 1) goldCount++;
        }
        if (goldCount == 6 && config->class1Unlocked == 0) {
            config->venomComplete = 1;
            sixTracks = 1;
        }
        if (goldCount == 8) {
            config->rapierComplete = 1;
            config->venomComplete = 0;
            eightTracks = 1;
        }
    }
}


void copyInTempLapTimes(ShipData *ships)
{
    int count;
    int value;

    count = 0;
    while (count < 5) {
        value = ships[ownShip].lapTimes[count];
        savedLapTimes[count] = (value / 6) * 6;
        count++;
    }
}


void EndChallenge(ConfigData *config)
{
    if (raceType == 4) {
        if (config->venomComplete == 1) {
            config->venomChallengeDone = 1;
            config->class1Race = 0;
            raceType = 0x63;
        } else if (config->rapierComplete != 0) {
            config->rapierChallengeDone = 1;
            config->class2Race = 0;
            raceType = 0x63;
        }
    }
}


void InitScreenTex(Texture **textures, char *challengeName)
{
    char     filename[256];
    u_short  textureIndex;

    *(PathSeed *) filename = *texturePathSeed;
    strcat(filename, challengeName);
    strcat(filename, extCmp);
    textureIndex = TextureTableCount;
    LoadCmpFiles(LoadCompressedTextureSequence(filename));
    textures[0] = textureTable[(short) textureIndex];
    textures[1] = textureTable[(short) (textureIndex + 1)];
}


void BackPoly(POLY_F4 *selectBox, DR_MODE *selectTrans, short x, short y, short w, short h)
{
    SetPolyF4(&selectBox[CurrentScreen]);
    selectBox[CurrentScreen].r0 = 6;
    selectBox[CurrentScreen].g0 = 6;
    selectBox[CurrentScreen].b0 = 6;
    selectBox[CurrentScreen].x0 = x;
    selectBox[CurrentScreen].y0 = y;
    selectBox[CurrentScreen].x1 = x + w;
    selectBox[CurrentScreen].y1 = y;
    selectBox[CurrentScreen].x2 = x;
    selectBox[CurrentScreen].y2 = y + h;
    selectBox[CurrentScreen].x3 = x + w;
    selectBox[CurrentScreen].y3 = y + h;
    SetSemiTrans(&selectBox[CurrentScreen], 1);
    SetDrawMode(&selectTrans[CurrentScreen], 1, 1, GetTPage(0, 0, 0, 0), 0);
    AddPrim(OT2[CurrentScreen], &selectBox[CurrentScreen]);
    AddPrim(OT2[CurrentScreen], &selectTrans[CurrentScreen]);
}


/* Race-end and lap-time readout for the race that has just finished. */
void DisplayRaceInfo(ConfigData *state, HiScoreData *context, ShipData *scene)
{
    union { int alignment; u_char bytes[40]; } textBuffer;
    short racingLeft;
    short nextLap;
    short lapNext;
    short nextIndex;
    short lapIndex;
    short i;
    short eliminated;
    int   rowY;
    int   rowY2;
    int   lapTime;
    int   racing;
    int   maxShips;
    int   total;
    int   sumBase, lapValue, shipNum, initialShip;
    signed char shipClass;
    u_char endState;
    u_char laps;

    total = 0;
    racing = 0;
    maxShips = 12;
    if (state->shipClass == 3) maxShips = 15;
    i = 0;
    if (ownShip >= 12) maxShips = 15;
    do {
        if (!(scene[i].combatAttr & DESTROYED)) racing += 1;
        i++;
    } while (i < 15);
    racingLeft = maxShips - racing;
    eliminated = racingLeft;
    if ((unsigned int) (racingLeft & 0xFFFF) >= 0x10U) eliminated = 0;

    if (raceEndState == 2) {
        if (scene[ownShip].combatAttr & 0x2D) {
            CentreText(msgBadLuck, 0xF, 0x18, 0xC, 0);
            CentreText(msgOutOfTime, 0xF, 0x3C, 8, 0);
        } else if (shipTargetIndex[ownShip] < 8) {
            CentreText(msgBadLuck, 0xF, 0x18, 0xC, 0);
            CentreText(msgShipDestroyed, 0x2D, 0x3C, 8, 0);
        }
        initialShip = ownShip;
        context->totalTime = 0;
        if (scene[initialShip].lap >= 2) {
            AddText(msgRaceStats, 0xF, 0x64, 8, 0);
            i = 0;
            if ((scene[ownShip].lap - 1) > 0) {
                do {
                    rowY = (i * 0xC) + 0x73;
                    AddText(msgLap, 0x19, rowY, 8, 0);
                    AddNumber(i + 1, 0x41, rowY, 8, 0);
                    AddTime(savedLapTimes[i], 0x55, rowY, 8, 1, 1);
                    sumBase = context->totalTime;
                    lapValue = savedLapTimes[i];
                    shipNum = ownShip;
                    context->totalTime = sumBase + lapValue;
                    nextIndex = i + 1;
                    i = nextIndex;
                } while (nextIndex < (scene[shipNum].lap - 1));
            }
        }
    } else {
        if (raceType != 1) {
            if (raceType != 3) {
                if (finishPosition < 4) {
                    CentreText(msgCongratulations, 0xF, 0x18, 0xC, 0);
                    if (gameType == 2) {
                        shipClass = state->shipClass;
                        switch (shipClass) {
                        case 0: __builtin_memcpy(textBuffer.bytes, classNameVector, 7); break;
                        case 1: __builtin_memcpy(textBuffer.bytes, classNameVenom, 6); break;
                        case 2: __builtin_memcpy(textBuffer.bytes, classNameRapier, 7); break;
                        case 3: __builtin_memcpy(textBuffer.bytes, classNamePhantom, 8); break;
                        }
                        switch (finishPosition) {
                        case 1: strcat(textBuffer.bytes, sufGold); break;
                        case 2: strcat(textBuffer.bytes, sufSilver); break;
                        case 3: strcat(textBuffer.bytes, sufBronze); break;
                        }
                        strcat(textBuffer.bytes, msgAwarded);
                        CentreText(textBuffer.bytes, 0xF, 0x30, 8, 1);
                    }
                } else {
                    CentreText(msgBadLuck, 0xF, 0x18, 0xC, 0);
                    if (gameType != 2 && finishPosition < rivalFinishPos) {
                        if (gameType == 0) {
                            CentreText(msgBeatPlayerTwo, 0xF, 0x30, 8, 1);
                        } else {
                            CentreText(msgBeatPlayerOne, 0xF, 0x30, 8, 1);
                        }
                    } else {
                        CentreText(msgNoMedal, 0xF, 0x30, 8, 1);
                    }
                }
                AddText(msgRacePosition, 0xF, 0x4A, 8, 0);
                AddNumber(finishPosition, 0x9B, 0x4A, 8, 1);
                if (eliminated != 0) {
                    AddText(msgEliminations, 0xF, 0xC4, 8, 0);
                    AddNumber(eliminated, 0x90, 0xC4, 8, 1);
                }
            }
        }
        if (raceType == 3) {
            if (finishPosition < rivalFinishPos && (endState = raceEndState) == 1) {
                CentreText(msgCongratulations, 0xF, 0x18, 0xC, 0);
                if (gameType == 0) {
                    CentreText(msgYouBeatPlayerTwo, 0xF, 0x30, 8, endState);
                } else {
                    CentreText(msgYouBeatPlayerOne, 0xF, 0x30, 8, endState);
                }
            } else {
                CentreText(msgBadLuck, 0xF, 0x18, 0xC, 0);
            }
        }
        AddText(msgRaceStats, 0xF, 0x64, 8, 0);
        context->totalTime = 0;
        i = 0;
        if (classLaps[state->shipClass] != 0) {
            do {
                rowY2 = (i * 0xC) + 0x73;
                AddText(msgLap, 0x19, rowY2, 8, 0);
                AddNumber(i + 1, 0x41, rowY2, 8, 0);
                AddTime(savedLapTimes[i], 0x55, rowY2, 8, 1, 1);
                context->totalTime = (int) (context->totalTime + savedLapTimes[i]);
                nextLap = i + 1;
                i = nextLap;
            } while (nextLap < (int) classLaps[state->shipClass]);
        }
        if ((raceType == 0 || raceType == 4) && finishPosition < 4) {
            DrawTeamShips(0x46, 0, 0xB4, cardNum, 0, teamShip[0]);
        }
        AddText(msgRaceTime, 0xF, 0xB4, 8, 0);
        if (raceType == 0 || raceType == 4 || raceType == 1) {
            AddTime(context->totalTime, 0x80, 0xB4, 8, 1, 1);
        } else {
            laps = classLaps[state->shipClass];
            lapIndex = 0;
            if (laps != 0) {
                do {
                    lapTime = savedLapTimes[lapIndex];
                    lapNext = lapIndex + 1;
                    lapIndex = lapNext;
                    total += lapTime;
                } while (lapNext < (int) laps);
            }
            AddTime(total, 0x80, 0xB4, 8, 1, 1);
        }
    }
    DoButton(0x7F, 0xD2, 1, msgContinue);
    raceInfoFrame++;
}
/* Walks the sections under the player's ship: a checkpoint the player passes adds its time
   bonus, and the bonus counter flashes "EXTENDED TIME" while it runs down. */
void AlterCheckPoint(CheckPoints *checkpoints, ShipData *shipIndex, int targetShip, ConfigData *gameData)
{
    short     i;
    int       seconds;
    int       minutes;
    int       frames;
    u_char    flags;
    int       target;
    int       pitch;
    int       bonus;
    Face     *facePtr;
    u_short   combatAttr;
    ShipData *player;

    facePtr = track->faces + shipIndex[ownShip].nearTrkSect->faceStart;
    for (;;) {
        flags = facePtr->flags;
        facePtr++;
        if (flags & TRACK_BASE) {
            facePtr--;
            break;
        }
    }
    if ((targetShip == -1) && (linkDropped == 0) && (shipIndex[ownShip].lap < classLaps[gameData->shipClass] + 1)) {
        player = &shipIndex[ownShip];
        if (player->lap >= 0) {
            if (checkTimer > 0) {
                frames = checkTimer - 2;
                seconds = frames / 60;
                checkTimer = frames;
                minutes = seconds / 60;
                if (((seconds - minutes * 60) < 11) && ((frames / 6) == seconds * 10) && (minutes == 0) &&
                    ((target = shipTargetIndex[ownShip]), (pitch = 10 - seconds), target == targetShip)) {
                    PlayNote(45, 320, 15, 15, pitch * 8 + 260);
                }
                if (cheatTime != 0) {
                    checkTimer = 35999;
                }
            } else if (noCheckPoints == 0) {
                combatAttr = player->combatAttr;
                if (!(combatAttr & DESTROYED)) {
                    player->combatAttr = combatAttr | 8;
                    if (player->autopilotCount > 0) {
                        player->autopilotCount = 0;
                        player->update = UpdatePlayerShip;
                        player->vhdg = 0;
                    }
                }
            }
            goto checkpointPass;
        }
    } else {
checkpointPass:
        if (shipIndex[ownShip].lap > 0) {
            if (facePtr->flags & CHECKPOINT) {
                for (i = 0; i < 6; i++) {
                    if (checkpoints[i].section == shipIndex[ownShip].nearTrkSect->secNo) {
                        if (checkpoints[i].reached < shipIndex[ownShip].lap &&
                            ((shipIndex[ownShip].attr & DIRECTION) ||
                             shipIndex[ownShip].nearTrkSect->junction.index != -1 ||
                             shipIndex[ownShip].nearTrkSect->nextSection.ptr->junction.index != -1)) {
                            bonus = checkpoints[i].time * 60;
                            bonusFrames = 60;
                            checkTimer += bonus;
                            shipIndex[ownShip].combatAttr &= ~8;
                            checkpoints[i].reached = shipIndex[ownShip].lap;
                            PlayNote(45, 320, 15, 15, 256);
                        }
                        break;
                    }
                }
            }
            if ((shipIndex[ownShip].attr & RACING) && (shipIndex[ownShip].lap > 0) && (bonusFrames > 0)) {
                if ((bonusFlashFrame % 30) < 21) {
                    CentreText(msgExtendedTime, 230, 75, 8, 0);
                }
                bonusFrames -= 1;
            }
            bonusFlashFrame += 1;
        }
    }
}


extern short continueAnimFrame;
extern POLY_FT4 continuePolys[3][2];

#define CONTINUE_QUAD(i) (&continuePolys[i][CurrentScreen])

void DisplayContinues(Texture *texture, int x, int y)
{
    int width = 0;
    short i = 0;
    int originY = y;
    int spacing = 10;
    short scale;
    int size, w, h, left, right, top, bottom;
    short lx, ty;
    POLY_FT4 *poly;

    if (continuesLeft) {
        do {
            width += (u_short)texture->textureW * 4;
            i++;
        } while (i < continuesLeft);
    }
    width = x - ((short)(width + spacing * (continuesLeft - 1)) >> 1);
    for (i = 0; i < continuesLeft; i++) {
        SetPolyFT4(CONTINUE_QUAD(i));
        continuePolys[i][CurrentScreen].r0 = 128;
        continuePolys[i][CurrentScreen].g0 = 128;
        continuePolys[i][CurrentScreen].b0 = 128;
        if (i == continuesLeft - 1) {
            scale = 32 - continueAnimFrame;
        } else {
            scale = 32;
        }
        size = texture->textureW * 4;
        w = size * scale / 32;
        h = texture->textureH * scale;
        if (h < 0) {
            h += 31;
        }
        left = width - (short)w / 2;
        lx = left;
        right = left + w;
        /* Port: was address arithmetic on (int)continuePolys; the same quad. */
        poly = CONTINUE_QUAD(i);
        top = originY - (short)(h >> 5) / 2;
        ty = top;
        bottom = top + (h >> 5);
        poly->x0 = lx;
        poly->y0 = ty;
        poly->x1 = right;
        poly->y1 = ty;
        poly->x2 = lx;
        poly->y2 = bottom;
        poly->x3 = right;
        poly->y3 = bottom;
        poly->u0 = texture->u0;
        CONTINUE_QUAD(i)->v0 = texture->v0;
        CONTINUE_QUAD(i)->u1 = texture->u1;
        CONTINUE_QUAD(i)->v1 = texture->v1;
        CONTINUE_QUAD(i)->u2 = texture->u2;
        CONTINUE_QUAD(i)->v2 = texture->v2 + 1;
        CONTINUE_QUAD(i)->u3 = texture->u3;
        CONTINUE_QUAD(i)->v3 = texture->v3 + 1;
        CONTINUE_QUAD(i)->tpage = GetTPage(0, 0, texture->textureX, texture->textureY);
        continuePolys[i][CurrentScreen].clut = texture->cba;
        AddPrim(OT[CurrentScreen], CONTINUE_QUAD(i));
        width += spacing + (u_short)texture->textureW * 4;
    }
}
