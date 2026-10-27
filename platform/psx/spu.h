/* The PlayStation sound processor, as hardware: 512 KiB of sound RAM, 24 voices
   (ADPCM decoding, Gaussian interpolation, ADSR envelopes, volume sweeps, noise,
   pitch modulation), the reverb unit, the capture buffers, and the mixer that
   produces one 44.1 kHz stereo sample at a time. No PsyQ library is involved
   here (see libspu.c); register offsets are relative to 0x1F801C00.

   The algorithms follow the nocash psx-spx description of the SPU. The mixer
   was checked sample by sample against a console emulator's recording of the
   retail game (voices, reverb and master mix), see tools/iotest. */
#ifndef PSX_SPU_H
#define PSX_SPU_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define SPU_RAM_SIZE 0x80000u
#define SPU_VOICES 24
#define SPU_SAMPLE_RATE 44100

/* Register offsets (relative to 0x1F801C00). */
enum {
    SPU_MAIN_VOL_L = 0x180,
    SPU_MAIN_VOL_R = 0x182,
    SPU_REVERB_VOL_L = 0x184,
    SPU_REVERB_VOL_R = 0x186,
    SPU_KEY_ON = 0x188,   /* 32 bits: voices 0-23 */
    SPU_KEY_OFF = 0x18C,
    SPU_PITCH_MOD = 0x190,
    SPU_NOISE_MODE = 0x194,
    SPU_REVERB_ON = 0x198,
    SPU_ENDX = 0x19C,
    SPU_REVERB_BASE = 0x1A2,
    SPU_IRQ_ADDRESS = 0x1A4,
    SPU_TRANSFER_ADDRESS = 0x1A6,
    SPU_TRANSFER_FIFO = 0x1A8,
    SPU_CONTROL = 0x1AA,
    SPU_TRANSFER_CONTROL = 0x1AC,
    SPU_STATUS = 0x1AE,
    SPU_CD_VOL_L = 0x1B0,
    SPU_CD_VOL_R = 0x1B2,
    SPU_EXT_VOL_L = 0x1B4,
    SPU_EXT_VOL_R = 0x1B6,
    SPU_REVERB_REGS = 0x1C0   /* 32 halfwords: dAPF1 ... vRIN */
};

/* SPUCNT bits */
#define SPU_CTRL_ENABLE 0x8000
#define SPU_CTRL_UNMUTE 0x4000
#define SPU_CTRL_REVERB_ENABLE 0x0080
#define SPU_CTRL_CD_REVERB 0x0004
#define SPU_CTRL_CD_ENABLE 0x0001

typedef enum { ADSR_OFF, ADSR_ATTACK, ADSR_DECAY, ADSR_SUSTAIN, ADSR_RELEASE } SpuPhase;

/* A volume envelope: the counter/step machinery shared by ADSR and sweeps. */
typedef struct SpuEnvelope {
    uint32_t counter;
    uint32_t increment;
    int32_t step;
    uint8_t rate;
    bool decrease;
    bool exponential;
    bool negative;
} SpuEnvelope;

typedef struct SpuSweep {
    int16_t level;
    SpuEnvelope envelope;
    bool active;
} SpuSweep;

typedef struct SpuVoice {
    uint16_t reg[8];    /* volL, volR, pitch, start, adsr1, adsr2, current ADSR level, repeat */
    uint32_t address;   /* current ADPCM block */
    uint32_t counter;   /* 12-bit fractional sample position */
    int16_t history[2]; /* ADPCM predictor history */
    int16_t decoded[31];/* 3 carried samples + 28 of the current block */
    uint8_t flags;
    bool has_block;
    bool ignore_loop;   /* the repeat address was set by software after key-on */
    bool first_block;
    SpuPhase phase;
    SpuEnvelope envelope;
    SpuSweep volume[2];
    int32_t last;       /* sample after the ADSR level, before the voice volume */
} SpuVoice;

typedef void (*SpuTraceFn)(void *user, uint64_t sample, uint32_t offset, uint16_t value);

typedef struct Spu {
    uint8_t ram[SPU_RAM_SIZE];
    uint16_t reg[0x140];
    SpuVoice voice[SPU_VOICES];
    SpuSweep main[2];
    uint32_t key_on, key_off;
    uint32_t key_latch;        /* voices whose last key command was "on" */
    uint32_t endx;
    uint32_t transfer_address;
    uint32_t reverb_address;
    uint8_t reverb_position;
    int16_t reverb_input[2][64];
    int16_t reverb_output[2][32];
    int32_t noise_timer;
    uint16_t noise_level;
    uint64_t samples;          /* output samples produced since reset */
    /* Per-sample stems before the main volume: dry voices 0-23 (after voice volume),
       24 = reverb return, 25 = CD input after its volume. */
    int16_t stems[26][2];
    /* The port's headphone surround (surround.c): when split_rear is set, the voices whose two volumes have opposite
       signs (a game's Dolby Surround encoding of the sounds behind the player) are left out of the output and mixed
       here instead, in phase, after the main volume. The reverb still hears them as encoded. */
    bool split_rear;
    int16_t rear[2];
    SpuTraceFn trace;
    void *trace_user;
} Spu;

void SpuReset(Spu *spu);
void SpuWriteRegister(Spu *spu, uint32_t offset, uint16_t value);
uint16_t SpuReadRegister(const Spu *spu, uint32_t offset);
void SpuWriteRam(Spu *spu, uint32_t address, const void *data, size_t size);
void SpuTransfer(Spu *spu, const void *data, size_t size);
/* One output sample. cd is the CD-DA input (stereo, may be NULL). */
void SpuSample(Spu *spu, const int16_t *cd, int16_t out[2]);

/* Exposed for tests. */
void SpuDecodeBlock(const uint8_t block[16], int16_t history[2], int16_t out[28]);
int32_t SpuGaussian(const int16_t samples[4], unsigned fraction);
void SpuEnvelopeInit(SpuEnvelope *e, unsigned rate, unsigned mask, bool decrease, bool exponential,
                     bool negative);
/* Advances the envelope one sample; returns false when it can change no further. */
bool SpuEnvelopeTick(SpuEnvelope *e, int16_t *level);

#endif
