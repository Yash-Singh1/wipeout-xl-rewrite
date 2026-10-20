#include "functions.h"
#include "include_asm_g8.h"

/* $gp-relative globals are tentative definitions; the rest are extern. */
int serialEvent;
int serialReadFd;
int serialWriteFd;

extern char sioDevice[];

Texture *linkSignTexture;
int      combError;
int      combErrorSeen;

extern WADFILE *textureWad;
extern s32 remoteShip;
extern s16 CurrentScreen;


int _comb_control(int op, int reg, int value);

/* $gp-relative globals are tentative definitions; the rest are extern. */
int           serialResult;
u_char        serialReadSeq;
u_int         shakeCount;
int           writeFlag;
u_char        serialWriteSeq;
extern short   menuRecvMode;
extern short   menuRecvKey;
extern u_short menuSendKey;
extern u_char  menuSendFlags;
extern u_char  menuRecvFlags;
extern int     raceType;
extern int     negconCentre;
extern int     remoteStartGame;
extern u_char  gameType;
void memcpy_80089544(void *dst, void *src, int len);

/* Link-up sign polygons, one per screen. */
extern POLY_FT4 linkSign[2];

#define SHAKE_LIMIT (u_int)(writeFlag + 0x7A120)

void InitLinkUpSign(void)
{
    POLY_FT4 *poly;
    Texture  *texture;
    int       i;

    linkSignTexture = WADLoadTexture(textureWad, 1);
    for (i = 0; i < 2; i++) {
        poly = &linkSign[i];
        SetPolyFT4(poly);
        linkSign[i].r0 = 0x80;
        linkSign[i].g0 = 0x80;
        linkSign[i].b0 = 0x80;
        texture = linkSignTexture;
        linkSign[i].x0 = 0x80;
        linkSign[i].y0 = 0x40;
        linkSign[i].x1 = 0xC0;
        linkSign[i].y1 = 0x40;
        linkSign[i].x2 = 0x80;
        linkSign[i].y2 = 0x80;
        linkSign[i].x3 = 0xC0;
        linkSign[i].y3 = 0x80;
        poly->u0 = texture->u0;
        poly->v0 = linkSignTexture->v0;
        poly->u1 = linkSignTexture->u1;
        poly->v1 = linkSignTexture->v1;
        poly->u2 = linkSignTexture->u2;
        poly->v2 = linkSignTexture->v2;
        poly->u3 = linkSignTexture->u3;
        poly->v3 = linkSignTexture->v3;
        poly->tpage = GetTPage(0, 1, linkSignTexture->textureX, linkSignTexture->textureY);
        SetSemiTrans(poly, 1);
        linkSign[i].clut = linkSignTexture->cba;
    }
}


extern Skeleton **cameraPtr;
extern int *OT[];

void DrawLinkSign(ShipData *ships)
{
    SVECTOR    svec;
    VECTOR     world;
    short      sxy[2];
    VECTOR     cam;
    int       rtp[2];
    POLY_FT4  *poly;
    int       sx, sy;
    int        size, depth, half, screen, right, bottom, page;

    if (ships[remoteShip].attr & 0x200) {
        cam.vx = (*cameraPtr)->relative.t[0];
        cam.vy = (*cameraPtr)->relative.t[1];
        cam.vz = (*cameraPtr)->relative.t[2];
        (*cameraPtr)->super->update = 1;
        SetSkeletonTransform((*cameraPtr)->super);
        world.vx = ships[remoteShip].ppivot.vx + cam.vx;
        world.vy = ships[remoteShip].ppivot.vy + cam.vy - 500;
        world.vz = ships[remoteShip].ppivot.vz + cam.vz;
        if (world.vx < 0 || (world.vx <= 0x7FFF && world.vz < 0) || world.vz <= 0x7FFF) {
            svec.vx = world.vx;
            svec.vy = world.vy;
            svec.vz = world.vz;
            depth = RotTransPers(&svec, (int *)sxy, &rtp[0], &rtp[1]);
            sx = sxy[0];
            sy = sxy[1];
            if (!(rtp[1] & 0x80460000)) {
                size = 0x6400 / depth;
                if ((short)size > 0x78) {
                    size = 0x78;
                }
                half = (short)size >> 1;
                sx -= half;
                depth -= 0x100;
                sy -= half;
                if (depth < 0) {
                    depth = 0;
                }
                screen = CurrentScreen;
                linkSign[screen].r0 = 0x80;
                right = size + sx;
                bottom = size + sy;
                linkSign[screen].x0 = sx;
                linkSign[screen].y0 = sy;
                linkSign[screen].x1 = right;
                linkSign[screen].y1 = sy;
                linkSign[screen].x2 = sx;
                linkSign[screen].y2 = bottom;
                linkSign[screen].x3 = right;
                linkSign[screen].y3 = bottom;
                linkSign[CurrentScreen].g0 = 0x80;
                linkSign[CurrentScreen].b0 = 0x80;
                linkSign[CurrentScreen].u0 = linkSignTexture->u0;
                linkSign[CurrentScreen].v0 = linkSignTexture->v0;
                linkSign[CurrentScreen].u1 = linkSignTexture->u1;
                linkSign[CurrentScreen].v1 = linkSignTexture->v1;
                linkSign[CurrentScreen].u2 = linkSignTexture->u2;
                linkSign[CurrentScreen].v2 = linkSignTexture->v2;
                linkSign[CurrentScreen].u3 = linkSignTexture->u3;
                linkSign[CurrentScreen].v3 = linkSignTexture->v3;
                page = GetTPage(0, 1, linkSignTexture->textureX, linkSignTexture->textureY);
                poly = &linkSign[CurrentScreen];
                poly->tpage = page;
                linkSign[CurrentScreen].clut = linkSignTexture->cba;
                AddPrim(OT[CurrentScreen] + (depth >> 1), poly);
            }
        }
    }
}


void _error(void)
{
    _comb_control(2, 0, 0);
    combError = 1;
    combErrorSeen = 1;
}

void _init_cont(void)
{
    AddCOMB();
    serialEvent = OpenEvent(0xF000000B, 0x8000, 0x1000, _error);
    serialWriteFd = open(sioDevice, 2);
    serialReadFd = open(sioDevice, 1);
    EnableEvent(serialEvent);
    _comb_control(1, 3, 0x1FA400);
    _comb_control(1, 1, 0);
}

void doSerialMenu(ConfigData *config, u_char *selection)
{
    u_char send[16];
    u_char recv[16];
    int    result;
    int    spare;
    u_char value;

    GsGetVcount();
    send[0] = selection[0x15];
    send[1] = (signed char)raceType;
    send[2] = selection[0x14];
    send[3] = selection[0x12];
    send[4] = (signed char)menuSendKey;
    send[5] = config->weaponsOn;
    send[6] = config->trackNum;
    send[7] = config->actTrackNum;
    send[8] = (signed char)negconCentre;
    send[9] = menuSendFlags;
    result = doSerialTransfer(send, recv, 0xA);
    serialResult = result;
    if (result == 0) {
        menuRecvMode = recv[0];
        menuRecvKey = recv[4];
        remoteStartGame = recv[8];
        menuRecvFlags = recv[9];
        if (gameType == 1) {
            raceType = recv[1];
            value = recv[2];
            selection[0x14] = value;
            config->shipClass = value;
            spare = recv[3];
            selection[0x12] = spare;
            config->actTrackNum = spare;
            config->weaponsOn = recv[5];
            config->trackNum = recv[6];
            config->actTrackNum = recv[7];
        }
    } else {
        killLink();
        raceType = 0;
    }
}


/* Per-frame packet exchanged with the other console during a linked race. */
typedef struct LinkPacket {
    int    posX, posY, posZ;
    int    bestLap;
    int    bestRace;
    int    attractShip;
    int    seedWep;
    u_short hdg, pitch, roll;
    u_short control;
    u_short padHead;
    u_short padButtons;
    u_short padPressed;
    u_char  padAux0;
    u_char  padAux1;
    u_char  state;
    u_char  position;
    u_char  finishPosition;
    u_char  sendFlag;
    u_char  raceComplete;
    u_char  raceEnd;
    u_char  raceDone;
    u_char  targets[15];
} LinkPacket;

extern s32 attractShip;
extern s16 linked;
extern s16 linkDropped;
extern s16 linkTimer;
extern s16 linkMyState;
extern s16 linkRemoteState;
extern s32 seedWep;
extern u8 raceEndState;
extern u8 raceCompleteFlag;
extern s16 rivalFinishPos;
extern u8 finishPosition;
extern u8 remoteRaceDone;
extern s32 ownShip;
extern u8 linkSendFlag;
extern s32 remoteBestLap;
extern s16 remoteRacePos;
extern s32 myBestLap;
extern s32 remoteBestRace;
extern s32 linkRaceFlag;
extern u8 linkRaceDone;
extern u8 linkRaceEnd;
extern u8 playerPosition;
extern u8 linkSyncB;
extern s32 myBestRace;
extern s32 shipTargetIndex[];
extern u16 playerControl;
extern u16 linkPadHead;
extern u16 linkPadButtons;
extern u16 linkPadPressed;
extern u8 linkPadAux0;
extern u8 linkPadAux1;
extern u16 remoteControl;
extern u16 remotePadHead;
extern u16 remotePadButtons;
extern u16 remotePadPressed;
extern u8 remotePadAux0;
extern u8 remotePadAux1;

void doSerialGame(int unused, ShipData *ships)
{
    LinkPacket send;
    LinkPacket recv;
    short i;
    int result;
    ShipData *ship;
    u_char *packet;

    /* values copied out of the globals before the packet is filled */
    int bestRace, attract, seed;
    u_short buttons, pressed;
    u_char aux0, aux1, state;
    int px, py, pz;
    u_short hdg, pitch, roll;

    /* values read back from the received packet */
    int lap, race;
    u_short hdgIn, pitchIn, rollIn, controlIn, padHeadIn, buttonsIn, pressedIn;
    u_char aux0In, aux1In, stateIn, positionIn, finishIn, sendFlagIn, completeIn, endIn, doneIn;
    int value;

    attract = attractShip;
    seed = seedWep;
    bestRace = myBestRace;
    buttons = linkPadButtons;
    pressed = linkPadPressed;
    aux0 = linkPadAux0;
    aux1 = linkPadAux1;
    px = ships[ownShip].ppivot.vx;
    py = ships[ownShip].ppivot.vy;
    pz = ships[ownShip].ppivot.vz;
    hdg = ships[ownShip].hdg;
    pitch = ships[ownShip].pitch;
    roll = ships[ownShip].roll;
    send.bestLap = myBestLap;
    send.bestRace = bestRace;
    send.attractShip = attract;
    send.seedWep = seed;
    send.padButtons = buttons;
    send.padPressed = pressed;
    send.padAux0 = aux0;
    send.control = playerControl;
    send.padHead = linkPadHead;
    send.padAux1 = aux1;
    send.posX = px;
    send.posY = py;
    send.posZ = pz;
    send.hdg = hdg;
    send.pitch = pitch;
    send.roll = roll;
    state = linkMyState;
    send.state = state;
    packet = send.targets;
    i = 0;
    send.position = playerPosition;
    send.finishPosition = finishPosition;
    send.sendFlag = linkSendFlag;
    send.raceComplete = raceCompleteFlag;
    send.raceEnd = raceEndState;
    send.raceDone = linkRaceDone;
    do {
        u_char target = (u_char)shipTargetIndex[i];

        i++;
        *packet++ = target;
    } while (i < 15);
    result = doSerialTransfer((u_char *)&send, (u_char *)&recv, 0x42);
    serialResult = result;
    if (result == 0) {
        ship = &ships[remoteShip];
        px = recv.posX;
        py = recv.posY;
        pz = recv.posZ;
        lap = recv.bestLap;
        race = recv.bestRace;
        ship->ppivot.vx = px;
        ship->ppivot.vy = py;
        ship->ppivot.vz = pz;
        remoteBestLap = lap;
        remoteBestRace = race;
        if (gameType == 1) {
            attractShip = recv.attractShip;
            seedWep = recv.seedWep;
        }
        packet = recv.targets;
        hdgIn = recv.hdg;
        pitchIn = recv.pitch;
        rollIn = recv.roll;
        controlIn = recv.control;
        padHeadIn = recv.padHead;
        buttonsIn = recv.padButtons;
        pressedIn = recv.padPressed;
        aux0In = recv.padAux0;
        aux1In = recv.padAux1;
        stateIn = recv.state;
        positionIn = recv.position;
        finishIn = recv.finishPosition;
        sendFlagIn = recv.sendFlag;
        completeIn = recv.raceComplete;
        endIn = recv.raceEnd;
        doneIn = recv.raceDone;
        i = 0;
        ship->hdg = hdgIn;
        ship->pitch = pitchIn;
        ship->roll = rollIn;
        remoteControl = controlIn;
        remotePadHead = padHeadIn;
        remotePadButtons = buttonsIn;
        remotePadPressed = pressedIn;
        remotePadAux0 = aux0In;
        remotePadAux1 = aux1In;
        linkRemoteState = stateIn;
        remoteRacePos = positionIn;
        rivalFinishPos = finishIn;
        linkRaceFlag = sendFlagIn;
        linkSyncB = completeIn;
        linkRaceEnd = endIn;
        remoteRaceDone = doneIn;
        do {
            int raw = *packet++;

            value = (signed char)raw;
            if (value != -1) {
                shipTargetIndex[i] = value;
            }
            i++;
        } while (i < 15);
        if (linkRaceDone != 0) {
            remoteRaceDone = 1;
        }
        if (remoteRaceDone != 0) {
            linkRaceDone = 1;
        }
        if ((s16)linkMyState == 3) {
            linkRemoteState = 3;
        }
        if (linkRemoteState == 3) {
            linkMyState = 3;
        }
        if ((s16)linkMyState == 4) {
            linkRemoteState = 4;
        }
        if (linkRemoteState == 4) {
            linkMyState = 4;
        }
    } else {
        killLink();
        linkDropped = 1;
        linked = 0;
        linkTimer = 0x96;
    }
}


int doSerialTransfer(u_char *send, u_char *recv, int count)
{
    int   delay;

    if (gameType == 0) {
        EnterCriticalSection();
        _comb_control(1, 1, 3);
        shakeCount = 0;
        while (!(_comb_control(0, 0, 0) & 0x100)) {
            shakeCount += 1;
            if (shakeCount >= SHAKE_LIMIT) {
                ExitCriticalSection();
                return 1;
            }
        }
        _comb_control(1, 1, 0);
        shakeCount = 0;
        while (_comb_control(0, 0, 0) & 0x100) {
            shakeCount += 1;
            if (shakeCount >= SHAKE_LIMIT) {
                ExitCriticalSection();
                return 1;
            }
        }
        _comb_control(1, 1, 3);
        shakeCount = 0;
        while (!(_comb_control(0, 0, 0) & 0x100)) {
            shakeCount += 1;
            if (shakeCount >= SHAKE_LIMIT) {
                ExitCriticalSection();
                return 1;
            }
        }
        delay = 0xC7;
        do {
            delay -= 1;
        } while (delay >= 0);
        sendSerialPacket(1, send, count);
        shakeCount = 0;
        while (_comb_control(0, 0, 0) & 0x100) {
            shakeCount += 1;
            if (shakeCount >= SHAKE_LIMIT) {
                ExitCriticalSection();
                return 1;
            }
        }
        _comb_control(1, 1, 0);
        shakeCount = 0;
        while (!(_comb_control(0, 0, 0) & 0x80)) {
            shakeCount += 1;
            if (shakeCount >= SHAKE_LIMIT) {
                ExitCriticalSection();
                return 1;
            }
        }
        _comb_control(1, 1, 3);
        receiveSerialPacket(2, recv, count);
        _comb_control(1, 1, 0);
        shakeCount = 0;
        while (_comb_control(0, 0, 0) & 0x100) {
            shakeCount += 1;
            if (shakeCount >= SHAKE_LIMIT) {
                ExitCriticalSection();
                return 1;
            }
        }
        goto done;
    }
    EnterCriticalSection();
    shakeCount = 0;
    while (!(_comb_control(0, 0, 0) & 0x80)) {
        shakeCount += 1;
        if (shakeCount >= SHAKE_LIMIT) {
            ExitCriticalSection();
            return 1;
        }
    }
    _comb_control(1, 1, 3);
    shakeCount = 0;
    while (_comb_control(0, 0, 0) & 0x80) {
        shakeCount += 1;
        if (shakeCount >= SHAKE_LIMIT) {
            ExitCriticalSection();
            return 1;
        }
    }
    _comb_control(1, 1, 0);
    shakeCount = 0;
    while (!(_comb_control(0, 0, 0) & 0x80)) {
        shakeCount += 1;
        if (shakeCount >= SHAKE_LIMIT) {
            ExitCriticalSection();
            return 1;
        }
    }
    _comb_control(1, 1, 3);
    receiveSerialPacket(2, recv, count);
    _comb_control(1, 1, 0);
    shakeCount = 0;
    while (_comb_control(0, 0, 0) & 0x100) {
        shakeCount += 1;
        if (shakeCount >= SHAKE_LIMIT) {
            ExitCriticalSection();
            return 1;
        }
    }
    _comb_control(1, 1, 3);
    shakeCount = 0;
    while (!(_comb_control(0, 0, 0) & 0x100)) {
        shakeCount += 1;
        if (shakeCount >= SHAKE_LIMIT) {
            ExitCriticalSection();
            return 1;
        }
    }
    delay = 0xC7;
    do {
        delay -= 1;
    } while (delay >= 0);
    sendSerialPacket(1, send, count);
    shakeCount = 0;
    while (_comb_control(0, 0, 0) & 0x100) {
        shakeCount += 1;
        if (shakeCount >= SHAKE_LIMIT) {
            ExitCriticalSection();
            return 1;
        }
    }
    _comb_control(1, 1, 0);
done:
    ExitCriticalSection();
    GsGetVcount();
    return 0;
}


int sendSerialPacket(int type, u_char *data, int count)
{
    u_char buffer[72];
    u_char *p;
    u_char *end;
    int    checksum;
    int    length;
    int    result;

    checksum = 0;
    buffer[0] = (type << 4) | (serialWriteSeq & 15);
    memcpy_80089544(buffer + 1, data, count);
    p = buffer;
    if (count + 1 > 0) {
        length = count + 1;
        end = (u_char *)(length + (int)p);
        do {
            checksum += *p++;
        } while ((int)p < (int)end);
    }
    buffer[count + 1] = checksum;
    write(serialWriteFd, buffer, count + 2);
    result = 0;
    if (count < 0) {
        serialWriteSeq = (serialWriteSeq + 1) & 15;
        return result;
    }
    serialWriteSeq = (serialWriteSeq + 1) & 15;
    return result;
}


int receiveSerialPacket(int unused, u_char *data, int len)
{
    u_char buf[72];
    int    sum;
    u_char *p;
    int    count;
    u_char *end;

    sum = 0;
    read(serialReadFd, buf, len + 2);
    p = buf;
    count = len + 1;
    if (count > 0) {
        end = (u_char *)(count + (int)p);
        do {
            sum += *p++;
        } while ((int)p < (int)end);
    }
    if ((sum & 0xFF) == buf[len + 1]) {
        serialReadSeq = buf[0] & 0xF;
        memcpy_80089544(data, &buf[1], len);
        sum = 0;
    } else {
        sum = 1;
    }
    return sum;
}
