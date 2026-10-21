/* The CD-ROM drive: the controller's commands (Setloc, Play, ReadN, Pause, GetlocP...),
   seeking and reading with the drive's timing, and CD-DA playback delivered one
   44.1 kHz stereo sample at a time to the SPU's CD input.

   Time is counted in output samples (44100 a second); the mixer calls CdDriveSample once
   for every SPU sample, so the drive and the SPU can never drift apart. Everything here
   runs under the audio lock; nothing touches game memory (the host copies a finished read
   to the game's buffer, see libcd.c). */
#ifndef PSX_CDROM_H
#define PSX_CDROM_H

#include <stdbool.h>
#include <stdint.h>

#include "disc.h"

/* Command codes (CdlSetloc ... ) */
enum {
    CDC_NOP = 0x01, CDC_SETLOC = 0x02, CDC_PLAY = 0x03, CDC_FORWARD = 0x04, CDC_BACKWARD = 0x05,
    CDC_READN = 0x06, CDC_STANDBY = 0x07, CDC_STOP = 0x08, CDC_PAUSE = 0x09, CDC_INIT = 0x0A,
    CDC_MUTE = 0x0B, CDC_DEMUTE = 0x0C, CDC_SETFILTER = 0x0D, CDC_SETMODE = 0x0E,
    CDC_GETPARAM = 0x0F, CDC_GETLOCL = 0x10, CDC_GETLOCP = 0x11, CDC_GETTN = 0x13,
    CDC_GETTD = 0x14, CDC_SEEKL = 0x15, CDC_SEEKP = 0x16, CDC_GETID = 0x1A, CDC_READS = 0x1B,
    CDC_RESET = 0x1C, CDC_READTOC = 0x1E
};

/* Mode bits (Setmode) */
#define CDM_CDDA 0x01
#define CDM_AUTOPAUSE 0x02
#define CDM_REPORT 0x04
#define CDM_SIZE_2340 0x20
#define CDM_DOUBLE_SPEED 0x80

/* Status byte bits */
#define CDS_ERROR 0x01
#define CDS_MOTOR 0x02
#define CDS_SEEK_ERROR 0x04
#define CDS_ID_ERROR 0x08
#define CDS_SHELL_OPEN 0x10
#define CDS_READING 0x20
#define CDS_SEEKING 0x40
#define CDS_PLAYING 0x80

/* Events the drive raises toward the BIOS event system (class HwCD). */
typedef enum { CDE_COMPLETE = 1, CDE_ERROR = 2, CDE_DATA_END = 3 } CdEventKind;

typedef enum { CD_IDLE, CD_SEEKING, CD_READING, CD_PLAYING } CdState;

typedef struct CdReadJob {
    bool active;     /* sectors still to arrive */
    bool finished;   /* all arrived, not yet collected by the host */
    int32_t start;   /* LBA of the first sector */
    int32_t total;
    int32_t arrived;
    uint32_t mode;   /* the mode in force (sector size) */
} CdReadJob;

typedef struct CdDrive {
    const Disc *disc;
    double speed;            /* >= 1: faster mechanics than the console's drive */
    uint64_t now;            /* samples since power-on */

    CdState state;
    CdState afterSeek;
    bool motor;
    bool muted;
    bool silent;             /* the port's second player: the music is the first player's game's */
    uint8_t mode;
    uint8_t mix[4];          /* L->L, L->R, R->R, R->L; 0x80 is unity */
    int32_t pos;             /* sector under the head */
    bool setlocPending;
    int32_t setloc;
    int32_t seekTarget;
    uint64_t seekDone;
    double nextSector;       /* READING: sample time the next sector arrives */
    unsigned phase;          /* PLAYING: sample within the current sector */
    const uint8_t *sector;   /* PLAYING: raw bytes of the current sector, or NULL for silence */

    uint64_t ackAt;          /* the first response of the last command arrives */
    uint64_t completeAt;     /* a second response (INT2) is pending, 0 if none */

    CdReadJob job;

    CdEventKind events[32];
    int eventCount;
} CdDrive;

void CdDriveInit(CdDrive *cd, const Disc *disc, double speed);
/* One output sample of CD-DA (before the SPU's CD volume), advancing time by one sample. */
void CdDriveSample(CdDrive *cd, int16_t out[2]);

/* True once the previous command's first response has arrived. */
bool CdDriveReady(const CdDrive *cd);
/* True once the previous command is completely finished (including a second response). */
bool CdDriveIdle(const CdDrive *cd);

/* Executes a command. Writes the response bytes (without a status byte for GetlocL/P)
   and returns their number; -1 if the command was rejected (an error event is queued). */
int CdDriveCommand(CdDrive *cd, int command, const uint8_t *param, int paramCount, uint8_t *response);
/* Where a read, seek or play command given now starts. */
int32_t CdDriveReadTarget(const CdDrive *cd);
/* Starts reading `sectors` sectors from the Setloc position (or the head), as CdRead does. */
void CdDriveStartRead(CdDrive *cd, int sectors, int mode);
/* The sample time at which the running read job will have all its sectors, 0 if none. */
uint64_t CdDriveReadEta(const CdDrive *cd);
/* Pops a queued event; false if none. */
bool CdDrivePopEvent(CdDrive *cd, CdEventKind *kind);

/* The table of contents as libcd returns it: [0] lead-out, [1..n] track starts, minutes and
   seconds in BCD (the drive reports seconds only), frames 0. Returns the track count. */
int CdDriveToc(const CdDrive *cd, uint8_t table[100][4]);

uint8_t CdToBcd(unsigned value);
unsigned CdFromBcd(uint8_t value);
/* Absolute minute/second/frame (BCD) <-> LBA. */
int32_t CdMsfToLba(const uint8_t msf[3]);
void CdLbaToMsf(int32_t lba, uint8_t msf[3]);

#endif
