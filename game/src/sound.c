#include "functions.h"
#include "include_asm_g8.h"

/* $gp-relative globals are tentative definitions; the rest are extern. */
int           remoteSamp1;
int           remoteSamp2;
short         scrapeVol;
int           reverblev;
signed char   undercover;
int           enginesamp;
int           turbulancesamp;
char         *sampleBuffer;
WADFILE      *sampleWad;
unsigned char sfxMode;
unsigned char controlledSoundOn;
int           controlledSfxId;
int           scrapesamp;
int           ambientVoice1;
int           ambientVoice2;
int           crowdsamp2;
int           crowdsamp;
signed char   cdSetMode;
CameraUpdateFunc oldcameraupdate;

extern char         msgCrowdPrefix[];
extern int          cdMusicCount;
extern int          ownShip;
extern ShipData    *shipIndex;
extern TrackCamera *cameraPtr;
extern CameraUpdateFunc CameraUpdate;
extern int          sampleHandle[];
extern int          oldcam[3];
extern int          totalSampleMem;
extern HeapBlock   *reserveHeap;
extern CdlLOC       cdTrackLocs[];
extern char        sampleNameTwo[];
extern char        sampleNameOne[];
extern char        sampleNameGo[];


void LoadSample(int sampleId, char *name)
{
    char path[64];
    int  handle;

    __builtin_memcpy(path, "c:\\wipeout2\\sound\\", 19);
    strcat(path, name);
    sampleBuffer = DAlloc(reserveHeap, 0xC800);
    readnextfile(sampleWad, sampleBuffer);
    handle = SfxTransEffect(sampleBuffer, 0);
    sampleHandle[sampleId] = handle;
    if (handle < 0) {
        printf("WARNING: during load sample: %s\n", name);
        printf("WARNING: sample not loaded correctly\n");
    }
    DFree(reserveHeap, sampleBuffer);
}


void InitSpuSound(void)
{
    int i;

    totalSampleMem = 0;
    SfxInit(sfxMode);
    SfxOn();
    SfxInitReverb();
    SfxReverbOn();
    SfxSetReverbDepth(0);
    sampleWad = openwadfilenvc((int)"c:\\wipeout2\\sound\\samples.wad");
    LoadSample(0, "shipship.vag");
    for (i = 1; i < 0x80; i++) {
        sampleHandle[i] = sampleHandle[0];
    }
    LoadSample(5, "weapnhit.vag");
}


void InitSpuSamples(void)
{
    LoadSample(1, "electro.vag");
    LoadSample(2, "engines.vag");
    LoadSample(6, "wepnwall.vag");
    LoadSample(7, "hitwall.vag");
    LoadSample(8, "menumove.vag");
    LoadSample(9, "menusel.vag");
    LoadSample(0xA, "recharge.vag");
    LoadSample(0xB, "mine.vag");
    LoadSample(0xC, "missile.vag");
    LoadSample(0xE, "pickup.vag");
    LoadSample(0xF, "shield2.vag");
    LoadSample(0x10, "spautopl.vag");
    LoadSample(0x11, "droid.vag");
    LoadSample(0x12, "turblnce.vag");
    LoadSample(0x13, "crowd.vag");
    LoadSample(0x14, "spmines.vag");
    LoadSample(0x15, "spmissl.vag");
    LoadSample(0x16, "sprocket.vag");
    LoadSample(0x18, "spelectr.vag");
    LoadSample(0x1A, "three.vag");
    LoadSample(0x1B, sampleNameTwo);
    LoadSample(0x1C, sampleNameOne);
    LoadSample(0x1D, sampleNameGo);
    LoadSample(0x22, "newturbo.vag");
    LoadSample(0x23, "thunder.vag");
    LoadSample(0x24, "shpstall.vag");
    LoadSample(0x25, "locked.vag");
    LoadSample(0x26, "quake.vag");
    LoadSample(0x27, "plasma.vag");
    LoadSample(0x2D, "chkpoint.vag");
    LoadSample(0x2E, "dtchwepn.vag");
    LoadSample(0x2F, "epak.vag");
    LoadSample(0x30, "warning.vag");
    LoadSample(0x31, "menuslid.vag");
    LoadSample(0x32, "scrapelp.vag");
    LoadSample(0x33, "eliminat.vag");
    LoadSample(0x36, "shldlow.vag");
    closewadfilenvc(sampleWad);
}


void StartControlledSound(void)
{
    if (controlledSoundOn == 0) {
        controlledSoundOn = 1;
        SfxKillAllVoices();
        enginesamp = SfxPlayId(sampleHandle[2], 0, 0xF, 0xF, 0xFF, 2);
        remoteSamp1 = SfxPlayId(sampleHandle[2], 0, 0xF, 0xF, 0xFF, 2);
        remoteSamp2 = SfxPlayId(sampleHandle[2], 0, 0xF, 0xF, 0x100, 2);
        ambientVoice1 = SfxPlayId(sampleHandle[15], 0, 0xF, 0xF, 0x99, 2);
        ambientVoice2 = SfxPlayId(sampleHandle[10], 0, 0xF, 0xF, 0xFF, 2);
        turbulancesamp = SfxPlayId(sampleHandle[18], 0, 0xF, 0xF, 0x280, 2);
        crowdsamp = SfxPlayId(sampleHandle[19], 0, 0xF, 0xF, 0x100, 2);
        crowdsamp2 = SfxPlayId(sampleHandle[19], 0, 0xF, 0xF, 0x100, 2);
        controlledSfxId = SfxPlayId(sampleHandle[17], 0, 0xF, 0xF, 0x100, 2);
        scrapesamp = SfxPlayId(sampleHandle[50], 0, 0xF, 0xF, 0xC0, 2);
        oldcam[0] = 0;
        oldcam[1] = 0;
        oldcam[2] = 0;
    }
}


void FindVagInVab(void)
{
}


void UpdateNotes(int unused)
{
    SfxDoSfx();
}


void KillNote(void)
{
}


void KillNotes(void)
{
}


void KillFadeNotes(void)
{
}


void PlayNote(int sampleId, short volume, short lrPan, short fbPan, int pitch)
{
    int voice;

    voice = 0;
    if (sampleId == 0x33) {
        voice = 0x17;
    }
    if (sampleId == 0x36) {
        voice = 0x16;
    }
    if (sampleId == 0x10) {
        voice = 0x15;
    }
    SfxPlay(sampleHandle[sampleId], volume & 0xFFFF, lrPan & 0xFF, fbPan & 0xFF, pitch, 2, voice);
}


void PlayControlledNote(int sampleId, int volume, int lrPan, int fbPan, int pitch)
{
    SfxPlayId(sampleHandle[(short)sampleId], volume & 0xFF, lrPan & 0xFF, fbPan & 0xFF, pitch, 2);
}


int KillControlledNote(short voice)
{
    SfxStop(voice);
    return -1;
}
void PlayDirectionalNote(short sampleId, short pitch, short duration, VECTOR pos)
{
    VECTOR distance;
    int    magnitude;
    int    leftVol;
    int    rightVol;
    int    sum;
    int    volume;
    int    pan;
    short  angle;
    short  relAngle;

    distance.vx = (pos.vx + cameraPtr->camPos->relative.t[0]) >> 1;
    distance.vy = (pos.vy + cameraPtr->camPos->relative.t[1]) >> 1;
    distance.vz = (pos.vz + cameraPtr->camPos->relative.t[2]) >> 1;
    magnitude = GetMagnitude(distance);
    leftVol = 0x4000 - magnitude;
    if (leftVol < 0) {
        leftVol = 0;
    }
    rightVol = 0x4000 - magnitude;
    if (rightVol < 0) {
        rightVol = 0;
    }
    angle = ang((short)(cameraPtr->hdg - ratan2(distance.vx, distance.vz)));
    relAngle = angle;
    if (angle >= 0x401) {
        relAngle = 0x800 - angle;
    }
    if (relAngle < -0x400) {
        relAngle = -0x800 - relAngle;
    }
    leftVol += (leftVol * relAngle) >> 11;
    rightVol -= (rightVol * relAngle) >> 11;
    leftVol = (leftVol << 13) >> 18;
    rightVol = (rightVol << 13) >> 18;
    sum = leftVol + rightVol;
    volume = sum >> 1;
    if (volume >= 0x100) {
        volume = 0xFF;
    }
    pan = 0xF;
    if ((leftVol > 0) && (rightVol > 0)) {
        leftVol = (leftVol * 0x1E0000) / sum;
        pan = leftVol >> 16;
        pan = pan * 2 - 15;
        if (pan < 0) {
            pan = 0;
        }
        if (pan >= 31) {
            pan = 30;
        }
    }
    pan = 30 - pan;
    SfxPlay(sampleHandle[sampleId], volume & 0xFFFF, pan & 0xFF, 0xF, pitch + 0x100, 2, 0);
}


void SetPitch(short voice, int pitch)
{
    SfxEffectPitch(voice, pitch);
}


void SetVolume(short voice, int volume)
{
    SfxEffectVol(voice, volume & 0xFF);
}


void AdjustDirectionalNote(short voice, short unused1, short unused2, VECTOR pos)
{
    VECTOR distance;
    int    magnitude;
    int    leftVol;
    int    rightVol;
    int    volume;
    int    pan;
    int    reduced;
    short  angle;
    short  relAngle;

    distance.vx = (pos.vx + cameraPtr->camPos->relative.t[0]) >> 1;
    distance.vy = (pos.vy + cameraPtr->camPos->relative.t[1]) >> 1;
    distance.vz = (pos.vz + cameraPtr->camPos->relative.t[2]) >> 1;
    magnitude = GetMagnitude(distance);
    leftVol = 0x2FFF - magnitude;
    if (leftVol < 0) {
        leftVol = 0;
    }
    rightVol = 0x2FFF - magnitude;
    if (rightVol < 0) {
        rightVol = 0;
    }
    angle = ang((short)(cameraPtr->hdg - ratan2(distance.vx, distance.vz)));
    relAngle = angle;
    if (angle >= 0x401) {
        relAngle = 0x800 - angle;
    }
    if (relAngle < -0x400) {
        relAngle = -0x800 - relAngle;
    }
    leftVol += (leftVol * relAngle) >> 11;
    rightVol -= (rightVol * relAngle) >> 11;
    leftVol = (leftVol << 13) >> 18;
    rightVol = (rightVol << 13) >> 18;
    volume = (leftVol + rightVol) >> 1;
    if (volume >= 0x100) {
        volume = 0xFF;
    }
    pan = 15;
    if ((leftVol > 0) && (rightVol > 0)) {
        if (leftVol >= 256) {
            leftVol = 255;
        }
        if (rightVol >= 256) {
            rightVol = 255;
        }
        pan = (leftVol << 8) / (leftVol + rightVol);
        pan = ((pan << 5) >> 8) - 1;
        if (pan < 0) {
            pan = 0;
        }
        if (pan >= 31) {
            pan = 30;
        }
    }
    if ((u_int)(pan - 16) < 9) {
        pan = 25;
    }
    if ((u_int)(pan - 6) < 9) {
        pan = 5;
    }
    pan = 30 - pan;
    SfxEffectLRPan(voice, pan & 255);
    if (shipIndex[ownShip].attr & VIEW_IN) {
        reduced = volume >> 1;
    } else {
        reduced = volume >> 2;
    }
    SetVolume(voice, (short)(volume - reduced));
}


void AdjustDopplerEngine(int shipNo, int channel, int unused)
{
    VECTOR    distance;
    VECTOR    previous;
    VECTOR    future;
    VECTOR    nextdistance;
    ShipData *ship;
    ShipData *futureShip;
    int       leftVol;
    int       rightVol;
    int       engine;
    int       pan;
    short     volume;
    int       mix;
    short     outVolume;
    int       angleResult;
    int       rawAngle;
    short     angle;
    short     pitch;
    short     sample;

    volume = 0;
    ship = (ShipData *)(shipNo * sizeof(ShipData) + (int)shipIndex);
    pitch = (u_short)ship->speed >> 8;
    if (oldcameraupdate != CameraUpdate) {
        oldcam[0] = cameraPtr->camPos->relative.t[0];
        oldcam[1] = cameraPtr->camPos->relative.t[1];
        oldcam[2] = cameraPtr->camPos->relative.t[2];
        oldcameraupdate = CameraUpdate;
        return;
    }
    previous.vx = ship->ppivot.vx;
    previous.vy = ship->ppivot.vy;
    previous.vz = ship->ppivot.vz;
    distance.vx = (previous.vx + oldcam[0]) >> 2;
    distance.vy = (previous.vy + oldcam[1]) >> 2;
    distance.vz = (previous.vz + oldcam[2]) >> 2;
    engine = GetMagnitude(distance);

    futureShip = (ShipData *)(shipNo * sizeof(ShipData) + (int)shipIndex);
    future.vx = futureShip->ppivot.vx + (futureShip->vpivot.vx >> 6);
    future.vy = futureShip->ppivot.vy + (futureShip->vpivot.vy >> 6);
    future.vz = futureShip->ppivot.vz + (futureShip->vpivot.vz >> 6);
    nextdistance.vx = (future.vx + cameraPtr->camPos->relative.t[0]) >> 2;
    nextdistance.vy = (future.vy + cameraPtr->camPos->relative.t[1]) >> 2;
    nextdistance.vz = (future.vz + cameraPtr->camPos->relative.t[2]) >> 2;
    pitch -= (GetMagnitude(nextdistance) - engine) >> 2;

    leftVol = 3000 - engine;
    if (leftVol < 0) {
        leftVol = 0;
    }
    rightVol = 3000 - engine;
    if (rightVol < 0) {
        rightVol = 0;
    }
    angleResult = ratan2(distance.vx, distance.vz);
    rawAngle = ang((short)((u_short)cameraPtr->hdg - angleResult));
    angle = rawAngle;
    if ((short)rawAngle > 1024) {
        angle = 2048 - rawAngle;
    }
    if ((short)angle < -1024) {
        angle = -2048 - angle;
    }
    leftVol += (leftVol * (short)angle) >> 11;
    rightVol -= (rightVol * (short)angle) >> 11;
    leftVol = (short)(leftVol >> 3) >> 2;
    rightVol = (short)(rightVol >> 3) >> 2;

    pan = 15;
    if (leftVol > 0 && rightVol > 0) {
        if (leftVol > 255) {
            leftVol = 255;
        }
        if (rightVol > 255) {
            rightVol = 255;
        }
        pan = (leftVol << 8) / (leftVol + rightVol);
        pan = ((pan << 5) >> 8) - 1;
        if (pan < 0) {
            pan = 0;
        }
        if (pan > 30) {
            pan = 30;
        }
    }
    mix = (u_int)(leftVol + rightVol) >> 1;
    pan = 30 - pan;
    SfxEffectLRPan(channel, (u_char)pan);

    mix &= 255;
    volume += mix;
    volume += volume << 1;
    if (volume >= 256) {
        volume = 255;
    }
    outVolume = volume - (volume >> 2);
    sample = channel;
    SetPitch(sample, pitch + 225);
    SetVolume(sample, outVolume);
}


void AdjustRemoteEngineSound(int ship1, int ship2, int extra2, int extra3)
{
    if (ship1 >= 0) {
        AdjustDopplerEngine(ship1, remoteSamp1, extra2);
    } else {
        SetVolume((short)remoteSamp1, 0);
    }
    if (ship2 >= 0) {
        AdjustDopplerEngine(ship2, remoteSamp2, extra3);
    } else {
        SetVolume((short)remoteSamp2, 0);
    }
    oldcam[0] = cameraPtr->camPos->relative.t[0];
    oldcam[1] = cameraPtr->camPos->relative.t[1];
    oldcam[2] = cameraPtr->camPos->relative.t[2];
}


void AdjustEngineSound(ShipData *ship, TrackCamera *camera)
{
    char workspace[64];
    s16 turbVolume;
    s32 pan;
    s16 reducedVolume;
    s32 pitchValue;
    s32 inView;
    s32 brakeSum;
    s32 turbProduct;
    u16 speed;
    s16 pitch;
    u32 speedLevel;
    u32 brakeLevel;
    s16 volume;
    s16 brake;

    if (scrapeVol > 0) {
        scrapeVol -= 0xA;
    }
    SetVolume((short)scrapesamp, scrapeVol);
    if (shipIndex[ownShip].combatAttr & DESTROYED) {
        SetVolume((s16)enginesamp, 0);
        SetVolume((s16)turbulancesamp, 0);
        return;
    }
    if (camera->section->snow == 1) {
        if (reverblev < 0x64) {
            reverblev += 0xA;
        }
        SfxSetReverbDepth(reverblev);
        undercover = 1;
    } else {
        if (reverblev > 0) {
            reverblev -= 0xA;
        }
        SfxSetReverbDepth(reverblev);
        undercover = 0;
    }
    speed = ship->speed;
    speedLevel = speed >> 0xA;
    volume = speedLevel;
    if ((s32) speedLevel >= 0x21) {
        volume = 0x20;
    }
    pitch = speed >> 8;
    if (ship->attr & VIEW_IN) {
        SfxEffectLRPan((s32) enginesamp, 0xF);
        SfxEffectFBPan((s32) enginesamp, 0xF);
        SfxEffectFBPan((s32) turbulancesamp, 0xF);
        volume += 0x80;
        if (volume >= 0x100) {
            volume = 0xFF;
        }
        pitchValue = pitch + 0xFF;
        reducedVolume = volume - (volume >> 2);
        SetPitch((s16)enginesamp, pitchValue);
        SetVolume((s16)enginesamp, reducedVolume);
    } else {
        SfxEffectFBPan((s32) enginesamp, 0x16);
        SfxEffectFBPan((s32) turbulancesamp, 0x16);
        AdjustDopplerEngine(ownShip, (s32) enginesamp, 0);
    }
    brakeSum = (s32) (ship->r_brake_rot + ship->l_brake_rot) >> 2;
    volume = brakeSum;
    if (brakeSum >= 0x81) {
        volume = 0x80;
    }
    brakeLevel = (u16) ship->speed >> 8;
    brake = brakeLevel;
    if ((s32) brakeLevel >= 0x81) {
        brake = 0x80;
    }
    turbProduct = brake * (s16)volume;
    turbVolume = turbProduct >> 7;
    turbVolume += brake;
    volume = turbVolume;
    if (turbVolume >= 0x100) {
        volume = 0xFF;
    }
    SetVolume((s16)turbulancesamp, (s16) (volume - ((s32) (volume << 0x10) >> 0x12)));
    volume = ship->r_brake_rot >> 4;
    inView = ship->attr & VIEW_IN;
    if (!inView) {
        volume = (u32)(s32)volume >> 1;
    }
    if (volume > 15) {
        volume = 15;
    }
    pan = 15 - volume;
    volume = ship->l_brake_rot >> 4;
    if (!inView) {
        volume = (u32)(s32)volume >> 2;
    }
    if (volume > 15) {
        volume = 15;
    }
    pan += volume;
    SfxEffectLRPan((s32)turbulancesamp, pan & 0xFF);
}


int InitCD(CdlLOC *locs, int unused)
{
    u_char mode;
    int    count;
    int    i;
    CdlLOC *loc;

    CdInit();
    CdSetDebug(0);
    mode = 5;
    CdControlB(0xE, &mode, 0);
    count = CdGetToc(locs);
    i = 1;
    cdMusicCount = count;
    if (i < count) {
        loc = locs + 1;
        do {
            CdIntToPos(CdPosToInt(loc) - 0x4A, loc);
            i += 1;
            loc += 1;
        } while (i < cdMusicCount);
    }
    return cdMusicCount;
}


int InitCrowdSound(Object **prm, short prmCount, Object **destination, int count)
{
    int     i;
    Object *obj;

    for (i = 0; i < prmCount; i++) {
        obj = prm[i];
        while (obj) {
            if (strncmp(obj->name, msgCrowdPrefix, 4) == 0) {
                destination[count] = obj;
                count++;
            }
            obj = obj->next;
        }
    }
    return count;
}


void AdjustCrowdSound(Object **objects, int count)
{
    int     i;
    VECTOR  distance;
    int     chosen[2];
    int     smallest[2];
    VECTOR  pos;
    int     mag;

    smallest[0] = 10000;
    smallest[1] = 10000;
    chosen[0] = -1;
    chosen[1] = -1;
    for (i = 0; i < count; i++) {
        distance.vx = (objects[i]->skeleton->relative.t[0] + cameraPtr->camPos->relative.t[0]) >> 3;
        distance.vy = (objects[i]->skeleton->relative.t[1] + cameraPtr->camPos->relative.t[1]) >> 3;
        distance.vz = (objects[i]->skeleton->relative.t[2] + cameraPtr->camPos->relative.t[2]) >> 3;
        mag = GetMagnitude(distance);
        if (mag < smallest[0]) {
            if (chosen[0] != -1) {
                smallest[1] = smallest[0];
                chosen[1] = chosen[0];
            }
            smallest[0] = mag;
            chosen[0] = i;
        } else if (mag < smallest[1]) {
            smallest[1] = mag;
            chosen[1] = i;
        }
    }
    if (smallest[0] < 8192) {
        pos.vx = objects[chosen[0]]->skeleton->relative.t[0];
        pos.vy = objects[chosen[0]]->skeleton->relative.t[1];
        pos.vz = objects[chosen[0]]->skeleton->relative.t[2];
        AdjustDirectionalNote((short)crowdsamp, 0, -1, pos);
    } else {
        SetVolume((short)crowdsamp, 0);
    }
    if (smallest[1] < 8192) {
        pos.vx = objects[chosen[1]]->skeleton->relative.t[0];
        pos.vy = objects[chosen[1]]->skeleton->relative.t[1];
        pos.vz = objects[chosen[1]]->skeleton->relative.t[2];
        AdjustDirectionalNote((short)crowdsamp2, 0, -1, pos);
    } else {
        SetVolume((short)crowdsamp2, 0);
    }
}
void KillControlledSound(void)
{
    SetVolume((short)enginesamp, 0);
    SetVolume((short)remoteSamp1, 0);
    SetVolume((short)remoteSamp2, 0);
    SetVolume((short)controlledSfxId, 0);
    SetVolume((short)ambientVoice1, 0);
    SetVolume((short)crowdsamp2, 0);
    SetVolume((short)crowdsamp, 0);
    SetVolume((short)ambientVoice2, 0);
    SetVolume((short)turbulancesamp, 0);
    SetVolume((short)scrapesamp, 0);
    SfxDoSfx();
}


void MuteControlledSound(void)
{
    SetVolume((short)enginesamp, 0);
    SetVolume((short)remoteSamp1, 0);
    SetVolume((short)remoteSamp2, 0);
    SetVolume((short)controlledSfxId, 0);
    SetVolume((short)crowdsamp2, 0);
    SetVolume((short)crowdsamp, 0);
    SetVolume((short)ambientVoice2, 0);
    SetVolume((short)turbulancesamp, 0);
    SetVolume((short)scrapesamp, 0);
    SfxDoSfx();
}


void CdMusicPlay(int track, int unused)
{
    cdSetMode = 5;
    CdControlB(0xE, (u_char *)&cdSetMode, 0);
    CdControlB(2, (u_char *)&cdTrackLocs[track], 0);
    CdControlB(3, 0, 0);
}


void CdMusicStop(void)
{
    CdControlB(9, 0, 0);
}


void TerminateSound(void)
{
    if (controlledSoundOn != 0) {
        controlledSoundOn = 0;
        SfxStop(enginesamp);
        SfxStop(remoteSamp1);
        SfxStop(remoteSamp2);
        SfxStop(controlledSfxId);
        SfxStop(ambientVoice1);
        SfxStop(crowdsamp2);
        SfxStop(crowdsamp);
        SfxStop(ambientVoice2);
        SfxStop(turbulancesamp);
        SfxStop(scrapesamp);
        SfxDoSfx();
    }
}
