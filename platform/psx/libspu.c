/* libspu: the PsyQ sound library over the SPU model (spu.c). Each call does what libspu
   does to the SPU's registers and sound RAM; the register sequences were compared with
   the ones the retail game produced on a console emulator.

   Attribute structs use the layout of game/include/psyq.h (32-bit long). */
#include <string.h>

#include "internal.h"

typedef struct { int16_t left, right; } Volume;

typedef struct {
    uint32_t voice;
    uint32_t mask;
    Volume volume, volmode, volumex;
    uint16_t pitch, note, sample_note;
    int16_t envx;
    uint32_t addr, loop_addr;
    int32_t a_mode, s_mode, r_mode;
    uint16_t ar, dr, sr, rr, sl, adsr1, adsr2;
} VoiceAttr;

typedef struct {
    Volume volume;
    int32_t reverb;
    int32_t mix;
} ExtAttr;

typedef struct {
    uint32_t mask;
    Volume mvol, mvolmode, mvolx;
    ExtAttr cd, ext;
} CommonAttr;

typedef struct {
    uint32_t mask;
    int32_t mode;
    Volume depth;
    int32_t delay, feedback;
} ReverbAttr;

/* SpuVoiceAttr.mask bits */
#define VOICE_VOLL 0x1
#define VOICE_VOLR 0x2
#define VOICE_VOLMODEL 0x4
#define VOICE_VOLMODER 0x8
#define VOICE_PITCH 0x10
#define VOICE_WDSA 0x80
#define VOICE_ADSR_AMODE 0x100
#define VOICE_ADSR_SMODE 0x200
#define VOICE_ADSR_RMODE 0x400
#define VOICE_ADSR_AR 0x800
#define VOICE_ADSR_DR 0x1000
#define VOICE_ADSR_SR 0x2000
#define VOICE_ADSR_RR 0x4000
#define VOICE_ADSR_SL 0x8000
#define VOICE_LSAX 0x10000
#define VOICE_ADSR_ADSR1 0x20000
#define VOICE_ADSR_ADSR2 0x40000

/* SpuCommonAttr.mask bits */
#define COMMON_MVOLL 0x1
#define COMMON_MVOLR 0x2
#define COMMON_MVOLMODEL 0x4
#define COMMON_MVOLMODER 0x8
#define COMMON_CDVOLL 0x40
#define COMMON_CDVOLR 0x80
#define COMMON_CDREV 0x100
#define COMMON_CDMIX 0x200

/* SpuReverbAttr.mask bits */
#define REVERB_MODE 0x1
#define REVERB_DEPTHL 0x2
#define REVERB_DEPTHR 0x4
#define REVERB_MODE_CLEAR_WA 0x100

#define ALLOC_MAX 160
#define SPU_MALLOC_BASE 0x1010u

typedef struct { uint32_t address, size; } Allocation;

static Allocation allocation[ALLOC_MAX];
static unsigned allocationLimit;
static bool reverbReserved;
static int reverbMode;      /* the preset in force: 0 off, 4 studio C */

/* Studio C ("Studio Large"): the reverb unit's 32 parameters. */
static const uint16_t studioC[32] = {
    0x00E3, 0x00A9, 0x6F60, 0x4FA8, 0xBCE0, 0x4510, 0xBEF0, 0xA680, 0x5680, 0x52C0, 0x0DFB,
    0x0B58, 0x0D09, 0x0A3C, 0x0BD9, 0x0973, 0x0B59, 0x08DA, 0x08D9, 0x05E9, 0x07EC, 0x04B0,
    0x06EF, 0x03D2, 0x05EA, 0x031D, 0x031C, 0x0238, 0x0154, 0x00AA, 0x8000, 0x8000};
#define STUDIO_C_BASE 0x79020u /* sound RAM end minus the 0x6FE0-byte work area */
#define NO_REVERB_BASE 0x7FFF0u

static uint32_t reverb_base(int mode)
{
    return mode == 4 ? STUDIO_C_BASE : NO_REVERB_BASE;
}

static void wr(uint32_t offset, uint16_t value)
{
    SpuWriteRegister(g_io.spu, offset, value);
}

static uint16_t rd(uint32_t offset)
{
    return SpuReadRegister(g_io.spu, offset);
}

/* ---- initialisation ---------------------------------------------------------------------- */

void SpuInit(void)
{
    static const uint8_t silent[16] = {7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7};
    unsigned i;

    PsxLock();
    wr(SPU_CONTROL, 0);
    wr(SPU_TRANSFER_CONTROL, 4);
    wr(SPU_MAIN_VOL_L, 0);
    wr(SPU_MAIN_VOL_R, 0);
    wr(SPU_REVERB_VOL_L, 0);
    wr(SPU_REVERB_VOL_R, 0);
    wr(SPU_KEY_OFF, 0xFFFF);
    wr(SPU_KEY_OFF + 2, 0xFF);
    wr(SPU_REVERB_ON, 0);
    wr(SPU_REVERB_ON + 2, 0);
    wr(SPU_PITCH_MOD, 0);
    wr(SPU_PITCH_MOD + 2, 0);
    wr(SPU_NOISE_MODE, 0);
    wr(SPU_NOISE_MODE + 2, 0);
    wr(SPU_CD_VOL_L, 0);
    wr(SPU_CD_VOL_R, 0);
    wr(SPU_EXT_VOL_L, 0);
    wr(SPU_EXT_VOL_R, 0);
    /* a silent looping block at 0x1000 parks the voices */
    wr(SPU_TRANSFER_ADDRESS, 0x200);
    SpuTransfer(g_io.spu, silent, sizeof(silent));
    wr(SPU_CONTROL, 0x10);
    wr(SPU_CONTROL, 0);
    for (i = 0; i < SPU_VOICES; i++) {
        wr(i * 16 + 0, 0);
        wr(i * 16 + 2, 0);
        wr(i * 16 + 4, 0x3FFF);
        wr(i * 16 + 6, 0x200);
        wr(i * 16 + 8, 0);
        wr(i * 16 + 10, 0);
    }
    /* every voice plays the silent block for a moment, then is released (the console
       takes 51 samples between the two commands) */
    wr(SPU_KEY_ON, 0xFFFF);
    wr(SPU_KEY_ON + 2, 0xFF);
    PsxAudioRenderLocked(NULL, 51);
    wr(SPU_KEY_OFF, 0xFFFF);
    wr(SPU_KEY_OFF + 2, 0xFF);
    PsxAudioRenderLocked(NULL, 50);
    wr(SPU_CONTROL, SPU_CTRL_ENABLE | SPU_CTRL_UNMUTE);
    wr(SPU_REVERB_BASE, (uint16_t)(NO_REVERB_BASE / 8));
    memset(allocation, 0, sizeof(allocation));
    allocationLimit = 0;
    reverbReserved = false;
    reverbMode = 0;
    PsxUnlock();
}

/* The SPU keeps running unless told otherwise; quitting and starting again only matter to
   the library's own bookkeeping, which this implementation does not need. */
void SpuQuit(void) {}
void SpuStart(void) {}

/* ---- sound RAM ----------------------------------------------------------------------------- */

int SpuInitMalloc(int num, void *records)
{
    (void)records;
    if (num <= 0 || num > ALLOC_MAX) {
        return 0;
    }
    PsxLock();
    memset(allocation, 0, sizeof(allocation));
    allocationLimit = (unsigned)num;
    PsxUnlock();
    return num;
}

uint32_t SpuMalloc(int size)
{
    uint32_t rounded, address = SPU_MALLOC_BASE, end;
    unsigned slot = ALLOC_MAX, i;
    bool conflict;

    if (size <= 0 || size > (int)SPU_RAM_SIZE) {
        return (uint32_t)-1;
    }
    rounded = ((uint32_t)size + 7) & ~7u;
    PsxLock();
    for (i = 0; i < allocationLimit; i++) {
        if (!allocation[i].size) {
            slot = i;
            break;
        }
    }
    if (slot == ALLOC_MAX) {
        PsxUnlock();
        return (uint32_t)-1;
    }
    /* the lowest gap that fits */
    do {
        conflict = false;
        for (i = 0; i < allocationLimit; i++) {
            const Allocation *a = &allocation[i];

            if (a->size && address < a->address + a->size && address + rounded > a->address) {
                address = a->address + a->size;
                conflict = true;
                break;
            }
        }
    } while (conflict);
    end = reverbReserved ? reverb_base(reverbMode) : SPU_RAM_SIZE;
    if (address + rounded > end) {
        PsxUnlock();
        return (uint32_t)-1;
    }
    allocation[slot].address = address;
    allocation[slot].size = rounded;
    PsxUnlock();
    return address;
}

void SpuFree(uint32_t address)
{
    unsigned i;

    PsxLock();
    for (i = 0; i < allocationLimit; i++) {
        if (allocation[i].size && allocation[i].address == address) {
            allocation[i].size = 0;
        }
    }
    PsxUnlock();
}

/* ---- transfers ------------------------------------------------------------------------------ */

uint32_t SpuSetTransferStartAddr(uint32_t address)
{
    if (address > SPU_RAM_SIZE - 8) {
        return 0;
    }
    address = (address + 7) & ~7u;
    PsxLock();
    wr(SPU_TRANSFER_ADDRESS, (uint16_t)(address / 8));
    PsxUnlock();
    return address;
}

int SpuSetTransferMode(int mode)
{
    return mode == 1 ? 1 : 0;
}

uint32_t SpuWrite(const void *data, uint32_t size)
{
    if (size > 0x7F000) {
        size = 0x7F000;
    }
    PsxLock();
    SpuTransfer(g_io.spu, data, size);
    PsxUnlock();
    return size;
}

/* Transfers happen at once in this implementation. */
int SpuIsTransferCompleted(int wait)
{
    (void)wait;
    return 1;
}

/* ---- voices ------------------------------------------------------------------------------------ */

/* A volume as the voice register holds it: a plain value is a 15-bit signed volume (negative
   values invert the phase); a sweep mode (1-7) selects the register's sweep encoding. */
static uint16_t volume_register(int16_t volume, int mode)
{
    if (mode >= 1 && mode <= 7) {
        unsigned rate = volume < 0 || volume > 127 ? 127 : (unsigned)volume;

        return (uint16_t)(0x8000u + (unsigned)(mode - 1) * 0x1000u + rate);
    }
    return (uint16_t)volume & 0x7FFF;
}

static unsigned limited(unsigned value, unsigned maximum)
{
    return value < maximum ? value : maximum;
}

void SpuSetVoiceAttr(const VoiceAttr *a)
{
    uint32_t mask = a->mask ? a->mask : 0x7FFFF;
    unsigned i;

    PsxLock();
    for (i = 0; i < SPU_VOICES; i++) {
        unsigned base = i * 16;
        uint16_t low, high;

        if (!((a->voice >> i) & 1)) {
            continue;
        }
        if (mask & VOICE_PITCH) {
            wr(base + 4, a->pitch);
        }
        if (mask & VOICE_WDSA) {
            wr(base + 6, (uint16_t)((a->addr + 7) / 8));
        }
        low = rd(base + 8);
        high = rd(base + 10);
        if (mask & (VOICE_ADSR_AMODE | VOICE_ADSR_AR)) {
            low = (uint16_t)((low & 0xFF) | (limited(a->ar, 127) << 8));
            low = (uint16_t)((low & 0x7FFF) | (a->a_mode == 5 ? 0x8000 : 0));
        }
        if (mask & VOICE_ADSR_DR) {
            low = (uint16_t)((low & 0xFF0F) | (limited(a->dr, 15) << 4));
        }
        if (mask & VOICE_ADSR_SL) {
            low = (uint16_t)((low & 0xFFF0) | limited(a->sl, 15));
        }
        if (mask & (VOICE_ADSR_SMODE | VOICE_ADSR_SR)) {
            unsigned mode = a->s_mode == 1 ? 0 : a->s_mode == 5 ? 2 : a->s_mode == 7 ? 3 : 1;

            high = (uint16_t)((high & 63) | (limited(a->sr, 127) << 6) | (mode << 14));
        }
        if (mask & (VOICE_ADSR_RMODE | VOICE_ADSR_RR)) {
            high = (uint16_t)((high & 0xFFC0) | limited(a->rr, 31) | (a->r_mode == 7 ? 32 : 0));
        }
        if (mask & VOICE_ADSR_ADSR1) {
            low = a->adsr1;
        }
        if (mask & VOICE_ADSR_ADSR2) {
            high = a->adsr2;
        }
        if (mask & (VOICE_ADSR_AMODE | VOICE_ADSR_AR | VOICE_ADSR_DR | VOICE_ADSR_SL | VOICE_ADSR_ADSR1)) {
            wr(base + 8, low);
        }
        if (mask & (VOICE_ADSR_SMODE | VOICE_ADSR_SR | VOICE_ADSR_RMODE | VOICE_ADSR_RR | VOICE_ADSR_ADSR2)) {
            wr(base + 10, high);
        }
        if (mask & VOICE_LSAX) {
            wr(base + 14, (uint16_t)((a->loop_addr + 7) / 8));
        }
        if (mask & (VOICE_VOLL | VOICE_VOLMODEL)) {
            wr(base, volume_register(a->volume.left, (mask & VOICE_VOLMODEL) ? a->volmode.left : 0));
        }
        if (mask & (VOICE_VOLR | VOICE_VOLMODER)) {
            wr(base + 2, volume_register(a->volume.right, (mask & VOICE_VOLMODER) ? a->volmode.right : 0));
        }
    }
    PsxUnlock();
}

/* Reads the first voice selected in attr->voice. */
void SpuGetVoiceAttr(VoiceAttr *a)
{
    unsigned index = 0;
    const SpuVoice *v;
    uint16_t l, r;

    while (index < SPU_VOICES && !((a->voice >> index) & 1)) {
        index++;
    }
    if (index == SPU_VOICES) {
        return;
    }
    PsxLock();
    v = &g_io.spu->voice[index];
    l = v->reg[0];
    r = v->reg[1];
    a->volume.left = (int16_t)((l & 0x8000) ? (l & 0xFFF) : (l & 0x3FFF) - (l & 0x4000));
    a->volume.right = (int16_t)((r & 0x8000) ? (r & 0xFFF) : (r & 0x3FFF) - (r & 0x4000));
    a->volmode.left = (int16_t)((l & 0x8000) ? ((l >> 12) & 7) + 1 : 0);
    a->volmode.right = (int16_t)((r & 0x8000) ? ((r >> 12) & 7) + 1 : 0);
    a->volumex.left = v->volume[0].level;
    a->volumex.right = v->volume[1].level;
    a->pitch = v->reg[2];
    a->addr = (uint32_t)v->reg[3] * 8;
    a->loop_addr = (uint32_t)v->reg[7] * 8;
    a->envx = (int16_t)v->reg[6];
    a->ar = (v->reg[4] >> 8) & 127;
    a->dr = (v->reg[4] >> 4) & 15;
    a->sl = v->reg[4] & 15;
    a->sr = (v->reg[5] >> 6) & 127;
    a->rr = v->reg[5] & 31;
    a->adsr1 = v->reg[4];
    a->adsr2 = v->reg[5];
    a->a_mode = (v->reg[4] & 0x8000) ? 5 : 1;
    a->s_mode = ((v->reg[5] >> 14) & 3) * 2 + 1;
    a->r_mode = (v->reg[5] & 32) ? 7 : 3;
    PsxUnlock();
}

/* Key on (1) or off (0) for the voices in the bit mask. */
void SpuSetKey(int on, uint32_t voices)
{
    unsigned reg = on == 1 ? SPU_KEY_ON : SPU_KEY_OFF;

    PsxLock();
    wr(reg, (uint16_t)voices);
    wr(reg + 2, (uint16_t)((voices >> 16) & 255));
    PsxUnlock();
}

/* The library's idea of a voice: the last key command given and whether the envelope is
   still above zero. 0 off, 1 on, 2 keyed off but still sounding, 3 keyed on but silent
   (a sample that ran to its end). */
int SpuGetKeyStatus(uint32_t voices)
{
    unsigned i;
    int result = -1;

    PsxLock();
    for (i = 0; i < SPU_VOICES; i++) {
        if ((voices >> i) & 1) {
            bool envelope = g_io.spu->voice[i].reg[6] != 0;
            bool keyed = (g_io.spu->key_latch >> i) & 1;

            result = keyed ? (envelope ? 1 : 3) : (envelope ? 2 : 0);
            break;
        }
    }
    PsxUnlock();
    return result;
}

/* ---- master, CD and reverb ----------------------------------------------------------------------- */

/* A zero mask means "everything", as in libspu. The fields the game leaves uninitialised on
   its stack when it does that (the sweep modes, the external input) are not applied: volumes
   are set as fixed volumes and the external input is left off. */
void SpuSetCommonAttr(const CommonAttr *a)
{
    uint32_t mask = a->mask;
    uint16_t control;

    PsxLock();
    if (!mask || (mask & (COMMON_MVOLL | COMMON_MVOLMODEL))) {
        wr(SPU_MAIN_VOL_L, volume_register(a->mvol.left, mask & COMMON_MVOLMODEL ? a->mvolmode.left : 0));
    }
    if (!mask || (mask & (COMMON_MVOLR | COMMON_MVOLMODER))) {
        wr(SPU_MAIN_VOL_R, volume_register(a->mvol.right, mask & COMMON_MVOLMODER ? a->mvolmode.right : 0));
    }
    if (!mask || (mask & COMMON_CDVOLL)) {
        wr(SPU_CD_VOL_L, (uint16_t)a->cd.volume.left);
    }
    if (!mask || (mask & COMMON_CDVOLR)) {
        wr(SPU_CD_VOL_R, (uint16_t)a->cd.volume.right);
    }
    control = rd(SPU_CONTROL);
    if (!mask || (mask & COMMON_CDREV)) {
        control = (uint16_t)((control & ~SPU_CTRL_CD_REVERB) | (a->cd.reverb == 1 ? SPU_CTRL_CD_REVERB : 0));
    }
    if (!mask || (mask & COMMON_CDMIX)) {
        control = (uint16_t)((control & ~SPU_CTRL_CD_ENABLE) | (a->cd.mix == 1 ? SPU_CTRL_CD_ENABLE : 0));
    }
    wr(SPU_CONTROL, control);
    PsxUnlock();
}

int SpuSetReverb(int on)
{
    uint16_t control;
    int state;

    PsxLock();
    control = rd(SPU_CONTROL);
    if (on == 0 || on == 1) {
        wr(SPU_CONTROL, (uint16_t)((control & ~SPU_CTRL_REVERB_ENABLE) | (on ? SPU_CTRL_REVERB_ENABLE : 0)));
    }
    state = (rd(SPU_CONTROL) & SPU_CTRL_REVERB_ENABLE) != 0;
    PsxUnlock();
    return state;
}

uint32_t SpuSetReverbVoice(int on, uint32_t voices)
{
    uint32_t mask;

    PsxLock();
    mask = rd(SPU_REVERB_ON) | (uint32_t)rd(SPU_REVERB_ON + 2) << 16;
    mask = on == 1 ? mask | voices : on == 0 ? mask & ~voices : mask;
    wr(SPU_REVERB_ON, (uint16_t)mask);
    wr(SPU_REVERB_ON + 2, (uint16_t)(mask >> 16));
    PsxUnlock();
    return mask & 0xFFFFFF;
}

int SpuClearReverbWorkArea(int mode)
{
    uint32_t base;

    if (mode != 0 && mode != 4) {
        return -1;
    }
    base = reverb_base(mode);
    PsxLock();
    memset(g_io.spu->ram + base, 0, SPU_RAM_SIZE - base);
    PsxUnlock();
    return 0;
}

/* Only the presets the game uses are known: 0 (off) and 4 (studio C). */
int SpuSetReverbModeParam(const ReverbAttr *a)
{
    uint32_t mask = a->mask ? a->mask : 0x1F;
    int result = 0;

    PsxLock();
    if (mask & REVERB_MODE) {
        int mode = a->mode & ~REVERB_MODE_CLEAR_WA;

        if (mode != 0 && mode != 4) {
            result = -1;
        } else {
            uint32_t base = reverb_base(mode);
            uint16_t control = rd(SPU_CONTROL);
            unsigned i;

            for (i = 0; i < allocationLimit; i++) {
                if (allocation[i].size && allocation[i].address + allocation[i].size > base) {
                    result = -1; /* sample memory overlaps the work area */
                }
            }
            if (!result) {
                reverbMode = mode;
                wr(SPU_CONTROL, (uint16_t)(control & ~SPU_CTRL_REVERB_ENABLE));
                wr(SPU_REVERB_VOL_L, 0);
                wr(SPU_REVERB_VOL_R, 0);
                for (i = 0; i < 32; i++) {
                    wr(SPU_REVERB_REGS + i * 2, mode == 4 ? studioC[i] : 0);
                }
                wr(SPU_REVERB_BASE, (uint16_t)(base / 8));
                wr(SPU_CONTROL, control);
                if (a->mode & REVERB_MODE_CLEAR_WA) {
                    memset(g_io.spu->ram + base, 0, SPU_RAM_SIZE - base);
                }
            }
        }
    }
    if (!result && (mask & REVERB_DEPTHL)) {
        wr(SPU_REVERB_VOL_L, (uint16_t)a->depth.left);
    }
    if (!result && (mask & REVERB_DEPTHR)) {
        wr(SPU_REVERB_VOL_R, (uint16_t)a->depth.right);
    }
    PsxUnlock();
    return result;
}

int SpuReserveReverbWorkArea(int on)
{
    PsxLock();
    if (on == 0 || on == 1) {
        reverbReserved = on == 1;
    }
    on = reverbReserved;
    PsxUnlock();
    return on;
}
