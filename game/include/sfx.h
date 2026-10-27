#ifndef SFX_H
#define SFX_H

#define SFX_WEAPON_LAUNCH      12
#define SFX_TURBO              34
#define SFX_START_STALL        36
#define SFX_MISSILE_LOCK       37
#define SFX_QUAKE              38
#define SFX_PLASMA             39
#define SFX_WEAPON_DITCH       46
#define SFX_ENERGY_PICKUP      47
#define SFX_LOW_ENERGY         54

#define SFX_EFFECTS 128
#define SFX_VOICES  24
#define SFX_PLAYS   24

#define SFX_QSOUND 3
#define SFX_DOLBY  4
#define SFX_REAR   4
#define SFX_ATTR_PITCH   0x10
#define SFX_ATTR_ADDRESS 0x80

/* Header of a .VAG sample file; the multi-byte fields are big-endian. */
typedef struct VagHeader {
    char  magic[4];
    int  version;
    int  reserved8;
    int  size;             /* bytes of sample data after the header */
    int  rate;             /* sampling frequency */
    char  reserved14[12];
    char  name[16];
} VagHeader;

/* A sample uploaded to sound RAM. */
typedef struct SfxEffect {
    int address;           /* 0 when the slot is free */
    int size;
    int addressRight;      /* second channel of a stereo sample */
    int sizeRight;
    int rate;              /* playback rate relative to 44.1 kHz, 4.12 fixed point */
} SfxEffect;

/* State of one hardware voice. */
typedef struct SfxVoice {
    int flags;             /* VOICE_* */
    int count;             /* frames until the voice is keyed on (1) or released */
    int left;
    int right;
    int pitch;
    int play;              /* the play that owns the voice */
    int pad18;
} SfxVoice;

#define VOICE_USED    1
#define VOICE_CHAINED 4

/* A sound being played, on one voice or a stereo pair. */
typedef struct SfxSound {
    int   rate;
    int   state;           /* SOUND_* */
    int   flags;           /* caller's flags: 2 centred mono, 4 rear speakers */
    int   pitch;
    int   chain;           /* the sound started after this one by SfxChainId */
    signed char voice;
    signed char voiceRight; /* -1 when not used */
    u_char volume;
    u_char lrPan;
    u_char fbPan;
    char   pad[3];
} SfxSound;

#define SOUND_USED    1
#define SOUND_ID      2     /* has an id the caller can still control */
#define SOUND_CHAINED 4

#endif
