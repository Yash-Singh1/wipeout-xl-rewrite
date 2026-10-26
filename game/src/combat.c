#include "functions.h"
#include "portlink.h" /* Port: two players on one machine */

/* $gp-relative globals are tentative definitions; the rest are extern. */
extern int serialEvent;
extern int serialReadFd;
extern int serialWriteFd;

extern char sioDevice[];

extern Texture *linkSignTexture;
extern int      combError;
extern int      combErrorSeen;

extern s32 remoteShip;
extern s16 CurrentScreen;

/* $gp-relative globals are tentative definitions; the rest are extern. */
extern int           serialResult;
extern u_char        serialReadSeq;
extern u_int         shakeCount;
extern int           writeFlag;
extern u_char        serialWriteSeq;
extern short   menuRecvMode;
extern short   menuRecvKey;
extern u_char  menuRecvFlags;
extern int     raceType;
extern int     negconCentre;
extern int     remoteStartGame;

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


void DrawLinkSign(ShipData *ships)
{
    SVECTOR    relativePos;
    VECTOR     translatedPos;
    short      screenPos[2];
    VECTOR     cameraOffset;
    int        projection[2];
    POLY_FT4  *poly;
    int        sx, sy;
    int        size, depth, halfSize, screen, right, bottom, texturePage;

    if (ships[remoteShip].attr & VISIBLE) {
        /* Port: cameraPtr is a TrackCamera *; the console code read its first field, camPos, through a Skeleton ** */
        cameraOffset.vx = cameraPtr->camPos->relative.t[0];
        cameraOffset.vy = cameraPtr->camPos->relative.t[1];
        cameraOffset.vz = cameraPtr->camPos->relative.t[2];
        cameraPtr->camPos->super->update = 1;
        SetSkeletonTransform(cameraPtr->camPos->super);
        translatedPos.vx = ships[remoteShip].ppivot.vx + cameraOffset.vx;
        translatedPos.vy = ships[remoteShip].ppivot.vy + cameraOffset.vy - 500;
        translatedPos.vz = ships[remoteShip].ppivot.vz + cameraOffset.vz;
        if (translatedPos.vx < 0 || (translatedPos.vx <= 0x7FFF && translatedPos.vz < 0) ||
            translatedPos.vz <= 0x7FFF) {
            relativePos.vx = translatedPos.vx;
            relativePos.vy = translatedPos.vy;
            relativePos.vz = translatedPos.vz;
            depth = RotTransPers(&relativePos, (int *)screenPos, &projection[0], &projection[1]);
            sx = screenPos[0];
            sy = screenPos[1];
            if (!(projection[1] & 0x80460000)) {
                size = 0x6400 / depth;
                if ((short)size > 0x78) {
                    size = 0x78;
                }
                halfSize = (short)size >> 1;
                sx -= halfSize;
                depth -= 0x100;
                sy -= halfSize;
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
                texturePage = GetTPage(0, 1, linkSignTexture->textureX, linkSignTexture->textureY);
                poly = &linkSign[CurrentScreen];
                poly->tpage = texturePage;
                linkSign[CurrentScreen].clut = linkSignTexture->cba;
                AddPrim(OT[CurrentScreen] + (depth >> 1), poly);
                PortSceneSprite(poly); /* Port: over the other ship, also when it is near (the HUD's entry) */
            }
        }
    }
}


int _error(void)
{
    _comb_control(2, 0, 0);
    combError = 1;
    combErrorSeen = 1;
    return 0; /* Port: OpenEvent's handler type returns int; the console's handler returned nothing and the value was ignored */
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

void doSerialMenu(ConfigData *config, MenuSelection *selection)
{
    u_char send[16];
    u_char recv[16];
    int    result;
    int    spare;
    u_char value;

    GsGetVcount();
    /* Port: selection was addressed by byte offset (0x15, 0x14 and 0x12 are ship, shipClass and actTrackNum). */
    send[0] = selection->ship;
    send[1] = (signed char)raceType;
    send[2] = selection->shipClass;
    send[3] = selection->actTrackNum;
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
            selection->shipClass = value;
            config->shipClass = value;
            spare = recv[3];
            selection->actTrackNum = spare;
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


/* Per-frame packet exchanged with the other console during a linked race: ship state and race results. */
#define LINK_RACE_PACKET_BYTES 66

typedef struct LinkPacket {
    int    posX, posY, posZ;
    int    bestLap;
    int    bestRace;
    int    attractShip;
    int    seedWep;
    u_short hdg, pitch, roll;
    /* The sender's controller record, field for field: ControlInput.head, .buttons, .pressed,
       .pad6 (viewed as one halfword), .thrust, .steering. */
    u_short head;
    u_short buttons;
    u_short pressed;
    u_short pad6;
    u_char  thrust;
    u_char  steering;
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
extern u_char raceEndState;
extern s16 rivalFinishPos;
extern u_char remoteRaceDone;
extern s32 ownShip;
extern s32 remoteBestLap;
extern s32 myBestLap;
extern s32 remoteBestRace;
extern s32 linkRaceFlag;
extern u_char linkRaceDone;
extern u_char linkRaceEnd;
extern u_char linkSyncB;
extern s32 myBestRace;

void doSerialGame(ConfigData *config, ShipData *ships)
{
    LinkPacket send;
    LinkPacket recv;
    short i;
    int result;
    ShipData *ship;
    u_char *targetCursor;

    /* Values copied into the outgoing packet. */
    int bestRace, attract, seed;
    u_short pressed, pad6;
    u_char thrust, steering, state;
    int posX, posY, posZ;
    u_short hdg, pitch, roll;

    /* Values read back from the received packet. */
    int recvX, recvY, recvZ;
    int bestLapIn, bestRaceIn;
    u_short hdgIn, pitchIn, rollIn, headIn, buttonsIn, pressedIn, pad6In;
    u_char thrustIn, steeringIn, stateIn, positionIn, finishIn, sendFlagIn, completeIn, endIn, doneIn;
    int targetIndex;

    attract = attractShip;
    seed = seedWep;
    bestRace = myBestRace;
    pressed = playerControl[0].pressed;
    pad6 = *(u_short *)playerControl[0].pad6;
    thrust = playerControl[0].thrust;
    steering = playerControl[0].steering;
    posX = ships[ownShip].ppivot.vx;
    posY = ships[ownShip].ppivot.vy;
    posZ = ships[ownShip].ppivot.vz;
    hdg = ships[ownShip].hdg;
    pitch = ships[ownShip].pitch;
    roll = ships[ownShip].roll;
    send.bestLap = myBestLap;
    send.bestRace = bestRace;
    send.attractShip = attract;
    send.seedWep = seed;
    send.pressed = pressed;
    send.pad6 = pad6;
    send.thrust = thrust;
    send.head = playerControl[0].head;
    send.buttons = playerControl[0].buttons;
    send.steering = steering;
    send.posX = posX;
    send.posY = posY;
    send.posZ = posZ;
    send.hdg = hdg;
    send.pitch = pitch;
    send.roll = roll;
    state = linkMyState;
    send.state = state;
    targetCursor = send.targets;
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
        *targetCursor++ = target;
    } while (i < 15);
    result = doSerialTransfer((u_char *)&send, (u_char *)&recv, LINK_RACE_PACKET_BYTES);
    serialResult = result;
    if (result == 0) {
        ship = &ships[remoteShip];
        recvX = recv.posX;
        recvY = recv.posY;
        recvZ = recv.posZ;
        bestLapIn = recv.bestLap;
        bestRaceIn = recv.bestRace;
        ship->ppivot.vx = recvX;
        ship->ppivot.vy = recvY;
        ship->ppivot.vz = recvZ;
        remoteBestLap = bestLapIn;
        remoteBestRace = bestRaceIn;
        if (gameType == 1) {
            attractShip = recv.attractShip;
            seedWep = recv.seedWep;
        }
        targetCursor = recv.targets;
        hdgIn = recv.hdg;
        pitchIn = recv.pitch;
        rollIn = recv.roll;
        headIn = recv.head;
        buttonsIn = recv.buttons;
        pressedIn = recv.pressed;
        pad6In = recv.pad6;
        thrustIn = recv.thrust;
        steeringIn = recv.steering;
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
        remoteControl[0].head = headIn;
        remoteControl[0].buttons = buttonsIn;
        remoteControl[0].pressed = pressedIn;
        *(u_short *)remoteControl[0].pad6 = pad6In;
        remoteControl[0].thrust = thrustIn;
        remoteControl[0].steering = steeringIn;
        linkRemoteState = stateIn;
        remoteRacePos = positionIn;
        rivalFinishPos = finishIn;
        linkRaceFlag = sendFlagIn;
        linkSyncB = completeIn;
        linkRaceEnd = endIn;
        remoteRaceDone = doneIn;
        do {
            int targetByte = *targetCursor++;

            targetIndex = (signed char)targetByte;
            if (targetIndex != -1) {
                shipTargetIndex[i] = targetIndex;
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

    if (PortLinkActive()) {
        return PortLinkTransfer(send, recv, count); /* Port: the link between the two players' games (portlink.c) */
    }
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

    checksum = 0;
    buffer[0] = (type << 4) | (serialWriteSeq & 15);
    memcpy(buffer + 1, data, count); /* Port: the console's memcpy, called by its address */
    p = buffer;
    if (count + 1 > 0) {
        length = count + 1;
        end = p + length;
        do {
            checksum += *p++;
        } while (p < end);
    }
    buffer[count + 1] = checksum;
    write(serialWriteFd, buffer, count + 2);
    serialWriteSeq = (serialWriteSeq + 1) & 15;
    return 0;
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
        end = p + count;
        do {
            sum += *p++;
        } while (p < end);
    }
    if ((sum & 0xFF) == buf[len + 1]) {
        serialReadSeq = buf[0] & 0xF;
        memcpy(data, &buf[1], len);
        sum = 0;
    } else {
        sum = 1;
    }
    return sum;
}
