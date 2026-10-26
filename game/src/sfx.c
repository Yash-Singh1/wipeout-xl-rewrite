#include "functions.h"
#include "sfx.h"

/* $gp-relative globals are tentative definitions; the rest are extern. */
extern u_char sfxInitialized;
extern u_char reverbReserved;
extern u_char reverbActive;
extern u_char reverbDepth;
extern u_char callbackPending;
extern u_char sfxEnabled;
extern u_char sfxGain;
extern u_char driverMode;
extern u_char nextVoice;
extern u_char cdVolume;
extern u_char globalVolume;
extern void (*sfxCallback)(int sound);
extern int    totalSampleMem;
extern SfxEffect effectList[SFX_EFFECTS];
extern SfxVoice  voiceList[SFX_VOICES];
extern SfxSound  playList[SFX_PLAYS];
extern char      pbSpuMallocRec[];
extern int      pwVolumeStore[SFX_VOICES][2];
extern u_char    pbStereoLookup[];
extern u_char    pbRearLookup[];
extern u_char    pbQPanLookup[][4];
extern char      msgSfxTransEffectMalloc[];
extern char      msgSfxTransEffectNoIds[];
extern char      msgSfxTransEffectRateMismatch[];

void SfxInit(u_char mode)
{
    SpuCommonAttr attr;
    int           i;

    SpuInit();
    SpuInitMalloc(SFX_EFFECTS, pbSpuMallocRec);
    attr.mvol.left = 0x3FC0;
    attr.mvol.right = 0x3FC0;
    attr.cd.volume.left = 0x3FC0;
    attr.cd.volume.right = 0x3FC0;
    attr.mask = 0;
    attr.cd.mix = 1;
    attr.cd.reverb = 0;
    SpuSetCommonAttr(&attr);
    SpuSetTransferMode(0);
    for (i = 0; i < SFX_EFFECTS; i++) {
        effectList[i].address = 0;
        effectList[i].size = 0;
        effectList[i].addressRight = 0;
        effectList[i].sizeRight = 0;
    }
    for (i = 0; i < SFX_VOICES; i++) {
        voiceList[i].flags = 0;
        voiceList[i].count = 0;
        playList[i].state = 0;
        playList[i].voice = -1;
        playList[i].voiceRight = -1;
    }
    callbackPending = 0;
    sfxInitialized = 1;
    driverMode = mode;
    reverbReserved = 0;
    sfxEnabled = 0;
    nextVoice = 0;
}


void SfxKillAllVoices(void)
{
    int i;

    nextVoice = 0;
    for (i = 0; i < SFX_VOICES; i++) {
        SpuSetKey(0, 1 << i);
        voiceList[i].flags = 0;
        voiceList[i].count = 0;
        playList[i].state = 0;
        playList[i].voice = -1;
        playList[i].voiceRight = -1;
    }
}


void SfxInitReverb(void)
{
    SpuReverbAttr attr;

    if (sfxInitialized && !reverbReserved) {
        SpuSetReverb(1);
        SpuSetReverbVoice(1, 0xFFFFFF);
        attr.mask = 1;
        attr.mode = 4;
        SpuSetReverbModeParam(&attr);
        if (SpuReserveReverbWorkArea(1) == 1) {
            reverbReserved = 1;
            reverbActive = 0;
            reverbDepth = 64;
            SpuClearReverbWorkArea(4);
            SpuSetReverb(1);
        }
    }
}


void SfxUnInitReverb(void)
{
    if (sfxInitialized && reverbReserved) {
        SpuSetReverb(0);
        SpuClearReverbWorkArea(4);
        reverbReserved = 0;
        reverbActive = 0;
    }
}


void SfxReverbOn(void)
{
    SpuReverbAttr attr;

    if (sfxInitialized && reverbReserved) {
        attr.mask = 6;
        attr.depth.left = reverbDepth << 8;
        attr.depth.right = -(reverbDepth << 8);
        SpuSetReverbModeParam(&attr);
        reverbActive = 1;
    }
}


void SfxReverbOff(void)
{
    SpuReverbAttr attr;

    if (sfxInitialized && reverbReserved) {
        attr.mask = 6;
        attr.depth.left = 0;
        attr.depth.right = 0;
        SpuSetReverbModeParam(&attr);
        reverbActive = 0;
    }
}


void SfxSetReverbDepth(u_char depth)
{
    SpuReverbAttr attr;

    if (sfxInitialized && reverbReserved) {
        if (depth >= 128) depth = 127;
        reverbDepth = depth;
        if (reverbActive) {
            attr.mask = 6;
            attr.depth.left = depth << 8;
            attr.depth.right = -(depth << 8);
            SpuSetReverbModeParam(&attr);
        }
    }
}


void SfxSetVolume(u_char gain)
{
    if (sfxInitialized) sfxGain = gain;
}


void SfxSetCdVolume(u_char volume)
{
    u_char mix[4];

    mix[0] = volume;
    mix[2] = volume;
    mix[1] = 0;
    mix[3] = 0;
    CdMix(mix);
}


void SfxSetCallBack(void (*callback)(int sound))
{
    if (sfxInitialized) {
        sfxCallback = callback;
        callbackPending = 1;
    }
}


void SfxSetMode(u_char mode)
{
    if (sfxInitialized) driverMode = mode;
}


void SfxUnInit(void)
{
    if (sfxInitialized) {
        if (reverbReserved) SfxUnInitReverb();
        SpuSetKey(0, 0xFFFFFF);
        SpuQuit();
        sfxInitialized = 0;
    }
}


void SfxOn(void)
{
    if (sfxInitialized) sfxEnabled = 1;
}


void SfxOff(void)
{
    int i;

    if (sfxInitialized) {
        SpuSetKey(0, 0xFFFFFF);
        for (i = 0; i < SFX_VOICES; i++) {
            voiceList[i].flags = 0;
            voiceList[i].count = 0;
            playList[i].state = 0;
            playList[i].voice = -1;
            playList[i].voiceRight = -1;
        }
        sfxEnabled = 0;
    }
}


void SfxPause(void)
{
    SpuVoiceAttr attr;
    int voice;

    for (voice = 0; voice < SFX_VOICES; voice++) {
        attr.mask = 3;
        attr.voice = 1 << voice;
        SpuGetVoiceAttr(&attr);
        pwVolumeStore[voice][0] = attr.volume.left;
        pwVolumeStore[voice][1] = attr.volume.right;
        attr.voice = 1 << voice;
        attr.mask = 3;
        attr.volume.left = 0;
        attr.volume.right = 0;
        voiceList[voice].left = 0;
        voiceList[voice].right = 0;
        SpuSetVoiceAttr(&attr);
    }
}


void SfxContinue(void)
{
    SpuVoiceAttr attr;
    int *leftVolume;
    int *rightVolume;
    int *volumeBase;
    int  rightValue;
    int  leftValue;
    int   voice;
    int   voiceOffset;

    voice = 0;
    voiceOffset = 0;
    volumeBase = pwVolumeStore[0];
    rightVolume = volumeBase + 1;
    leftVolume = volumeBase;
    do {
        leftValue = voice;
        leftValue = voice;
        leftValue = voice;
        leftValue = voice;
        leftValue = voice;
        leftValue = voice;
        attr.voice = 1 << voice;
        attr.mask = 3;
        leftValue = *leftVolume;
        attr.volume.left = (s16) leftValue;
        rightValue = *rightVolume;
        leftVolume += 2;
        attr.volume.right = (s16) rightValue;
        voiceList[voice].left = (s16) leftValue;
        voiceList[voice].right = attr.volume.right;
        rightVolume += 2;
        SpuSetVoiceAttr(&attr);
        voice += 1;
        voiceOffset += 0x1C;
    } while (voice < 0x18);
}


/* Port: the messages are data, so they are printed through "%s" (printf of a non-literal format). */
int SfxTransEffect(void *left, void *right)
{
    VagHeader *vag;
    int        size;
    int        address;
    int        sample;
    int        rate;

    if (!sfxInitialized) return -1;
    vag = left;
    size = Big2Little(vag->size);
    totalSampleMem += size;
    address = SpuMalloc(size);
    if (address == -1) goto noMemory;
    for (sample = 0; effectList[sample].size; ) {
        if (++sample == SFX_EFFECTS) goto noSlot;
    }
    rate = Big2Little(vag->rate);
    effectList[sample].address = address;
    effectList[sample].size = size;
    effectList[sample].rate = (rate << 12) / 44100;
    SpuSetTransferStartAddr(address);
    SpuWrite((char *)(vag + 1), size);
    SpuIsTransferCompleted(1);
    if (!right) return sample;
    vag = right;
    size = Big2Little(vag->size);
    address = SpuMalloc(size);
    if (address == -1) {
noMemory:
        printf("%s", msgSfxTransEffectMalloc);
        return -2;
    }
    if (Big2Little(vag->rate) != rate) {
        printf("%s", msgSfxTransEffectRateMismatch);
        return -4;
    }
    goto transferRight;
noSlot:
    printf("%s", msgSfxTransEffectNoIds);
    return -3;
transferRight:
    effectList[sample].addressRight = address;
    effectList[sample].sizeRight = size;
    SpuSetTransferStartAddr(address);
    SpuWrite((char *)(vag + 1), size);
    SpuIsTransferCompleted(1);
    return sample;
}


int SfxFreeEffect(int sample)
{
    if (!sfxInitialized) return -1;
    if (effectList[sample].address == 0) return -1;
    if (effectList[sample].addressRight) {
        SpuFree(effectList[sample].addressRight);
        effectList[sample].sizeRight = 0;
        effectList[sample].rate = 0;
    }
    SpuFree(effectList[sample].address);
    effectList[sample].size = 0;
    effectList[sample].rate = 0;
    return 1;
}


void SfxDoSfx(void)
{
    int keyOn;
    int i;
    int status;
    int sound;
    int chain;

    if (sfxInitialized) {
        keyOn = 0;
        for (i = 0; i < SFX_VOICES; i++) {
            if (voiceList[i].count == 1) keyOn |= 1 << i;
            if ((voiceList[i].flags & VOICE_USED) && voiceList[i].count == 0) {
                status = SpuGetKeyStatus(1 << i);
                if (status == 0 || status == 3) {
                    sound = voiceList[i].play;
                    if (callbackPending && (playList[sound].state & SOUND_ID)) {
                        sfxCallback(sound);
                    }
                    voiceList[i].flags = 0;
                    if (!(playList[sound].state & SOUND_ID)) {
                        playList[sound].state = 0;
                    } else if (playList[sound].state & SOUND_CHAINED) {
                        chain = playList[sound].chain;
                        voiceList[playList[chain].voice].flags &= ~VOICE_CHAINED;
                        if (playList[chain].voiceRight >= 0) {
                            voiceList[playList[chain].voiceRight].flags &= ~VOICE_CHAINED;
                        }
                    }
                }
            }
            if (voiceList[i].count != 0 && !(voiceList[i].flags & VOICE_CHAINED)) {
                voiceList[i].count--;
            }
        }
        if (keyOn > 0) SpuSetKey(1, keyOn);
    }
}


void SfxPlay(int sample, u_short volume, int lrPan, int fbPan, int pitch, int flags, short requestedVoice)
{
    SpuVoiceAttr attr;
    int level;
    int sound;
    int voice;
    int rate;
    int mix;
    int product;

    level = (globalVolume * volume) >> 8;
    if (level >= 256) level = 255;
    volume = level;
    if (!sfxInitialized) return;
    if (!sfxEnabled) return;
    sound = GetFreePlayId();
    if (sound == -1) return;
    mix = volume * sfxGain / 255;
    rate = effectList[sample].rate;
    playList[sound].flags = flags;
    playList[sound].lrPan = lrPan;
    playList[sound].fbPan = fbPan;
    playList[sound].pitch = pitch;
    playList[sound].rate = rate;
    playList[sound].state = SOUND_USED;
    playList[sound].volume = mix;
    if (requestedVoice < 21) voice = GetFreeVoice();
    else voice = requestedVoice;
    if (voice == -1) return;
    product = pitch * rate;
    playList[sound].voice = voice;
    voiceList[voice].flags = VOICE_USED;
    voiceList[voice].count = 1;
    voiceList[voice].pitch = (short)(product / 256);
    voiceList[voice].play = sound;
    attr.voice = 1U << voice;
    attr.mask = 0x90;
    attr.addr = effectList[sample].address;
    attr.pitch = voiceList[voice].pitch;
    SpuSetVoiceAttr(&attr);
    playList[sound].voiceRight = -1;
    SetEffectVolume(sound);
}


int SfxPlayId(int sample, u_char volume, u_char lrPan, u_char fbPan, int pitch, int flags)
{
    SpuVoiceAttr attr;
    int          level;
    int          sound;
    int          voice;
    int          rate;
    int          mix;

    level = (u_int)(globalVolume * volume) >> 8;
    if (!sfxInitialized) return 0;
    if (!sfxEnabled) return 0;
    sound = GetFreePlayId();
    if (sound == -1) return -1;
    mix = level * sfxGain / 255;
    rate = effectList[sample].rate;
    playList[sound].state = SOUND_USED | SOUND_ID;
    playList[sound].flags = flags;
    playList[sound].lrPan = lrPan;
    playList[sound].fbPan = fbPan;
    playList[sound].pitch = pitch;
    playList[sound].rate = rate;
    playList[sound].volume = mix;
    voice = GetFreeVoice();
    if (voice == -1) return -1;
    playList[sound].voice = voice;
    voiceList[voice].flags = VOICE_USED;
    voiceList[voice].count = 1;
    voiceList[voice].pitch = (short)(pitch * rate / 256);
    voiceList[voice].play = sound;
    attr.voice = 1 << voice;
    attr.mask = 0x90;
    attr.addr = effectList[sample].address;
    attr.pitch = voiceList[voice].pitch;
    SpuSetVoiceAttr(&attr);
    playList[sound].voiceRight = -1;
    SetEffectVolume(sound);
    return sound;
}


int SfxChainId(int parent, int sample, int volume, int lrPan, u_char fbPan, int pitch, int flags)
{
    SpuVoiceAttr attr;
    int          sound;
    int          voice;
    int          rate;
    int          mix;
    int          voicePitch;

    /* Port: the console left $v0 as it was after each test: the zero of !sfxInitialized, and the
       sfxEnabled flag just found set.  The enabled test is the original's own (inverted: the function
       does nothing while sound is on; no caller exists). */
    if (!sfxInitialized) return 0;
    if (sfxEnabled) return sfxEnabled;
    sound = GetFreePlayId();
    if (sound == -1) return -1;
    mix = (volume & 255) * sfxGain / 255;
    rate = effectList[sample].rate;
    playList[sound].state = SOUND_USED | SOUND_ID;
    playList[sound].lrPan = lrPan;
    playList[sound].fbPan = fbPan;
    playList[sound].flags = flags;
    playList[sound].rate = rate;
    playList[sound].pitch = pitch;
    playList[sound].volume = mix;
    playList[parent].chain = sound;
    playList[parent].state |= SOUND_CHAINED;
    voice = GetFreeVoice();
    if (voice == -1) return -1;
    playList[sound].voice = voice;
    voiceList[voice].flags = VOICE_USED | VOICE_CHAINED;
    voiceList[voice].count = 2;
    voicePitch = (short)(pitch * rate / 256);
    voiceList[voice].pitch = voicePitch;
    voiceList[voice].play = sound;
    attr.voice = 1 << voice;
    attr.mask = SFX_ATTR_PITCH | SFX_ATTR_ADDRESS;
    attr.addr = effectList[sample].address;
    attr.pitch = voiceList[voice].pitch;
    SpuSetVoiceAttr(&attr);
    if (driverMode == SFX_QSOUND && effectList[sample].addressRight) {
        voice = GetFreeVoice();
        if (voice != -1) {
            playList[sound].voiceRight = voice;
            voiceList[voice].flags = VOICE_USED;
            voiceList[voice].count = 1;
            voiceList[voice].pitch = voicePitch;
            voiceList[voice].play = sound;
            attr.voice = 1 << voice;
            attr.mask = SFX_ATTR_PITCH | SFX_ATTR_ADDRESS;
            attr.addr = effectList[sample].addressRight;
            attr.pitch = voiceList[voice].pitch;
            SpuSetVoiceAttr(&attr);
        }
    } else if (driverMode == SFX_DOLBY && (flags & SFX_REAR)) {
        voiceList[voice].count = 3;
        voice = GetFreeVoice();
        if (voice != -1) {
            playList[sound].voiceRight = voice;
            voiceList[voice].flags = VOICE_USED | VOICE_CHAINED;
            voiceList[voice].count = 2;
            voiceList[voice].pitch = (short)(pitch * rate / 256);
            voiceList[voice].play = sound;
            attr.voice = 1 << voice;
            attr.mask = SFX_ATTR_PITCH | SFX_ATTR_ADDRESS;
            attr.addr = effectList[sample].address;
            attr.pitch = voiceList[voice].pitch;
            SpuSetVoiceAttr(&attr);
        }
    } else {
        playList[sound].voiceRight = -1;
    }
    SetEffectVolume(sound);
    return sound;
}


void SfxStop(u_int sound)
{
    int   voice;
    u_int mask;

    if (!sfxInitialized || sound == -1) {
        printf("warning from qsfx: not initialised\n");
        return;
    }
    if (sound >= SFX_PLAYS) {
        printf("warning from qsfx: playid out of range\n");
        return;
    }
    /* Port: a sound that never played has voice -1 and `mask` was not set: the console indexed voiceList[-1] (the memory in
       front of the table) and passed whatever the register held to SpuSetKey.  Here no voice is stopped in that case. */
    mask = 0;
    voice = playList[sound].voice;
    if (voice >= 0 && voiceList[voice].play == sound) {
        mask = 1 << voice;
        voiceList[voice].flags = 0;
    }
    SpuSetKey(0, mask);
    playList[sound].state = 0;
}


void SfxEffectLRPan(u_int sound, u_char pan)
{
    if (sfxInitialized && sound != -1 && sound < SFX_PLAYS) {
        if (playList[sound].state & SOUND_ID) {
            playList[sound].lrPan = pan;
            SetEffectVolume(sound);
        }
    }
}


void SfxEffectFBPan(u_int sound, u_char pan)
{
    if (sfxInitialized && sound != -1 && sound < SFX_PLAYS) {
        if (playList[sound].state & SOUND_ID) {
            playList[sound].fbPan = pan;
            SetEffectVolume(sound);
        }
    }
}


void SfxEffectVol(u_int sound, u_char volume)
{
    int level;

    level = (u_int)(globalVolume * volume) >> 8;
    if (sfxInitialized && sound != -1 && sound < SFX_PLAYS) {
        if (playList[sound].state & SOUND_ID) {
            playList[sound].volume = level * sfxGain / 255;
            SetEffectVolume(sound);
        }
    }
}


void SfxEffectPitch(u_int sound, int pitch)
{
    SpuVoiceAttr attr;
    int          voice;
    int          rate;
    signed char  right;

    if (sfxInitialized && sound != -1 && sound < SFX_PLAYS) {
        if (playList[sound].state & SOUND_ID) {
            voice = playList[sound].voice;
            playList[sound].pitch = pitch;
            rate = playList[sound].rate;
            if (voiceList[voice].play == sound) {
                attr.voice = 1 << voice;
                attr.mask = 0x10;
                attr.pitch = pitch * rate / 256;
                voiceList[voice].pitch = attr.pitch;
                SpuSetVoiceAttr(&attr);
            }
            right = playList[sound].voiceRight;
            if (right != 0) {
                voice = right;
                /* Port: a sound with one voice has voiceRight == -1 (not 0), so this test passes and the console read
                   voiceList[-1], the memory in front of the table.  There is no such voice; the table is not extended. */
                if (voice >= 0 && voiceList[voice].play == sound) {
                    attr.voice = 1 << voice;
                    attr.mask = 0x10;
                    attr.pitch = pitch * rate / 256;
                    voiceList[voice].pitch = attr.pitch;
                    SpuSetVoiceAttr(&attr);
                }
            }
        }
    }
}


void SetEffectVolume(int sound)
{
    switch (driverMode) {
    case 1:
        SetMonoVolume(sound);
        return;
    case 2:
        SetStereoVolume(sound);
        return;
    case 3:
        SetQSoundVolume(sound);
        return;
    case 4:
        SetDolbyVolume(sound);
        return;
    }
}


void SetMonoVolume(int sound)
{
    SpuVoiceAttr attr;
    int          voice;
    int          volume;

    voice = playList[sound].voice;
    volume = playList[sound].volume << 6;
    if (voiceList[voice].play == sound) {
        attr.voice = 1 << voice;
        attr.mask = 3;
        attr.volume.left = volume;
        attr.volume.right = volume;
        voiceList[voice].left = volume;
        voiceList[voice].right = volume;
        SpuSetVoiceAttr(&attr);
    }
}


void SetStereoVolume(int sound)
{
    SpuVoiceAttr attr;
    int          voice;
    int          volume;
    int          pan;
    int          left;
    int          right;

    volume = playList[sound].volume << 6;
    pan = playList[sound].lrPan;
    if (playList[sound].flags & 2) {
        left = volume;
        right = -volume;
    } else {
        left = (volume * pbStereoLookup[pan]) >> 8;
        right = (volume * pbStereoLookup[30 - pan]) >> 8;
    }
    voice = playList[sound].voice;
    if (voiceList[voice].play == sound) {
        attr.voice = 1 << voice;
        attr.mask = 3;
        attr.volume.left = left;
        attr.volume.right = right;
        voiceList[voice].left = left;
        voiceList[voice].right = right;
        SpuSetVoiceAttr(&attr);
    }
}


void SetQSoundVolume(int sound)
{
    SpuVoiceAttr attr;
    int gain;
    int volume;
    int pan;
    int frontLeft;
    int frontRight;
    int rearLeft;
    int rearRight;
    int voice;

    gain = playList[sound].volume;
    pan = playList[sound].lrPan;
    volume = gain << 6;
    if (playList[sound].voiceRight != -1) {
        frontLeft = (pbQPanLookup[pan][0] * volume) >> 8;
        frontRight = (pbQPanLookup[pan][1] * volume) >> 8;
        rearLeft = (pbQPanLookup[pan][2] * volume) >> 8;
        rearRight = (pbQPanLookup[pan][3] * volume) >> 8;
    } else {
        frontLeft = (volume * pbStereoLookup[pan]) >> 8;
        /* Port: this was panGainFR[-pan], where panGainFR was the name of pbStereoLookup[30]. */
        frontRight = (volume * pbStereoLookup[30 - pan]) >> 8;
        rearLeft = 0;
        rearRight = 0;
    }
    if (playList[sound].flags & 2) {
        frontLeft = volume;
        frontRight = -volume;
        rearLeft = 0;
        rearRight = 0;
    }
    voice = playList[sound].voice;
    if (voiceList[voice].play == sound) {
        attr.voice = 1 << voice;
        attr.mask = 3;
        attr.volume.left = frontLeft;
        attr.volume.right = frontRight;
        voiceList[voice].left = frontLeft;
        voiceList[voice].right = frontRight;
        SpuSetVoiceAttr(&attr);
    }
    voice = playList[sound].voiceRight;
    if (voice != -1) {
        if (voiceList[voice].play == sound) {
            attr.voice = 1 << voice;
            attr.mask = 3;
            attr.volume.left = rearLeft;
            attr.volume.right = rearRight;
            voiceList[voice].left = rearLeft;
            voiceList[voice].right = rearRight;
            SpuSetVoiceAttr(&attr);
        }
    }
}


void SetDolbyVolume(int sound)
{
    SpuVoiceAttr attr;
    int          volume;
    int          flags;
    int          pan;
    int          fb;
    int          front;
    int          back;
    int          frontLeft;
    int          frontRight;
    int          backLeft;
    int          backRight;
    int          voice;

    volume = playList[sound].volume;
    flags = playList[sound].flags;
    pan = playList[sound].lrPan;
    back = volume << 6;
    fb = playList[sound].fbPan;
    front = back;
    if (flags & 4) {
        if (fb < 15) front = back * fb / 15;
        else if (fb >= 16) back = back * (31 - fb) / 15;
    }
    frontLeft = (front * pbStereoLookup[pan]) >> 8;
    frontRight = (front * pbStereoLookup[30 - pan]) >> 8;
    backLeft = (back * pbRearLookup[pan]) >> 8;
    backRight = (back * pbRearLookup[30 - pan]) >> 8;
    if ((flags & 6) == 2 && driverMode != 1) {
        frontLeft = backLeft;
        frontRight = -backRight;
    }
    voice = playList[sound].voice;
    if (voiceList[voice].play == sound) {
        attr.voice = 1 << voice;
        attr.mask = 3;
        attr.volume.left = frontLeft;
        attr.volume.right = frontRight;
        voiceList[voice].left = frontLeft;
        voiceList[voice].right = frontRight;
        SpuSetVoiceAttr(&attr);
    }
    if (driverMode == 4 && (flags & 4)) {
        voice = playList[sound].voiceRight;
        frontLeft = backLeft;
        frontRight = -backRight;
        if (voiceList[voice].play == sound) {
            attr.voice = 1 << voice;
            attr.mask = 3;
            attr.volume.left = frontLeft;
            attr.volume.right = frontRight;
            voiceList[voice].left = frontLeft;
            voiceList[voice].right = frontRight;
            SpuSetVoiceAttr(&attr);
        }
    }
}


int Big2Little(u_int value)
{
    return (value >> 24) | ((value >> 8) & 0xFF00) | ((value & 0xFF00) << 8) | (value << 24);
}


int GetFreePlayId(void)
{
    int i;

    i = 0;
    while (playList[i].state & SOUND_USED) {
        if (++i == SFX_PLAYS) return -1;
    }
    return i;
}


int GetFreeVoice(void)
{
    int   voice;
    int   found;
    int   slot;
    short start;

    voice = nextVoice;
    found = 1;
    if (voiceList[voice].flags & VOICE_USED) {
        start = voice;
        do {
            voice = (voice + 1) % 21;
            if (voice == start) {
                found = 0;
                goto searched;
            }
        } while (voiceList[voice].flags & VOICE_USED);
    searched:;
    }
    if (!found) {
        voice = 0;
        nextVoice = (nextVoice + 1) % 21;
        for (slot = 0; voice != 21 && !found; slot++) {
            if (!(playList[voiceList[slot].play].state & SOUND_ID)) {
                found = 1;
                goto chosen;
            } else {
                voice++;
            }
        }
    chosen:
        if (!found) voice = -1;
    }
    nextVoice = (voice + 1) % 21;
    return voice;
}


void SetGlobalVolume(int sound, int cd)
{
    SpuCommonAttr attr;

    globalVolume = ((u_char)sound >> 1) * 3u >> 2;
    cdVolume = cd;
    attr.mvol.left = 0x3FC0;
    attr.mvol.right = 0x3FC0;
    attr.mask = 0;
    attr.cd.volume.left = (u_char)cd << 6;
    attr.cd.volume.right = (u_char)cd << 6;
    attr.cd.mix = 1;
    attr.cd.reverb = 0;
    SpuSetCommonAttr(&attr);
}


void SetSoundVolume(int volume)
{
    SetGlobalVolume(volume & 0xFF, cdVolume);
}


void SetCdVolume(int volume)
{
    SetGlobalVolume(globalVolume, volume & 0xFF);
}
