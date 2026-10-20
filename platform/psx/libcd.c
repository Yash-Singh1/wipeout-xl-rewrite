/* libcd: the PsyQ CD-ROM library over the drive model (cdrom.c) and the disc image.

   Commands go to the drive; CdControlB and CdSync wait for the drive's responses and
   CdRead / CdReadSync move sectors into the game's buffer once the drive has read them
   (after the drive's own seek and read time). While the game waits, video fields pass
   (PsxIoWait), so the game's vertical-blank work, such as a loading animation, goes on. */
#include <string.h>

#include "internal.h"

typedef struct { uint8_t minute, second, sector, track; } Loc;

typedef struct {
    Loc pos;
    uint32_t size;
    char name[16];
} File;

enum { SYNC_COMPLETE = 2, SYNC_DISK_ERROR = 5, SYNC_NO_INTR = 0 };

static struct {
    bool pending;       /* a CdRead whose sectors are not yet in the buffer */
    uint8_t *dest;
    int sectors;
    int32_t start;
    uint32_t mode;
    bool error;         /* the last command was rejected */
} rd;

static int cdDebug;

int CdSync(int mode, uint8_t *result);

static void wait_ready(void)
{
    for (;;) {
        bool ready;
        uint64_t ack;

        PsxLock();
        ready = CdDriveReady(&g_io.cd);
        ack = g_io.cd.ackAt;
        PsxUnlock();
        if (ready) {
            return;
        }
        PsxIoWait(ack);
    }
}

static uint8_t current_status(void)
{
    uint8_t s;

    PsxLock();
    s = (uint8_t)((g_io.cd.motor ? CDS_MOTOR : 0) |
                  (g_io.cd.state == CD_READING ? CDS_READING : 0) |
                  (g_io.cd.state == CD_SEEKING ? CDS_SEEKING : 0) |
                  (g_io.cd.state == CD_PLAYING ? CDS_PLAYING : 0));
    PsxUnlock();
    return s;
}

/* Copies the sectors of a finished read into the game's buffer. */
void PsxCdService(void)
{
    uint8_t *dest;
    int sectors;
    int32_t start;
    uint32_t mode;
    bool finished;

    PsxLock();
    finished = rd.pending && !g_io.cd.job.active && g_io.cd.job.finished;
    dest = rd.dest;
    sectors = rd.sectors;
    start = rd.start;
    mode = rd.mode;
    PsxUnlock();
    if (!finished) {
        return;
    }
    if (mode & CDM_SIZE_2340) {
        int i;

        for (i = 0; i < sectors; i++) {
            const uint8_t *raw = DiscRawSector(g_io.disc, start + i);

            if (raw) {
                memcpy(dest + (size_t)i * 2340, raw + 12, 2340);
            } else {
                memset(dest + (size_t)i * 2340, 0, 2340);
            }
        }
    } else if (!DiscReadData(g_io.disc, start, sectors, dest)) {
        int i;

        for (i = 0; i < sectors; i++) {
            if (!DiscReadData(g_io.disc, start + i, 1, dest + (size_t)i * 2048)) {
                memset(dest + (size_t)i * 2048, 0, 2048);
            }
        }
    }
    PsxLock();
    rd.pending = false;
    g_io.cd.job.finished = false;
    PsxUnlock();
}

int CdInit(void)
{
    uint8_t response[16];

    wait_ready();
    PsxLock();
    memset(&rd, 0, sizeof(rd));
    memset(&g_io.cd.job, 0, sizeof(g_io.cd.job));
    CdDriveCommand(&g_io.cd, CDC_INIT, NULL, 0, response);
    PsxUnlock();
    CdSync(0, NULL);
    return 1;
}

int CdSetDebug(int level)
{
    int previous = cdDebug;

    cdDebug = level;
    return previous;
}

static int param_count(int command, bool hasParam)
{
    switch (command) {
    case CDC_SETLOC: return 3;
    case CDC_SETMODE: return 1;
    case CDC_SETFILTER: return 2;
    case CDC_GETTD: return 1;
    case CDC_PLAY: return hasParam ? 1 : 0;
    default: return 0;
    }
}

static int issue(int com, const uint8_t *param, uint8_t *result, bool wait)
{
    uint8_t response[16];
    int length, count;
    uint8_t setloc[3];
    int command = com & 0xFF;
    bool locParam = param && (command == CDC_SEEKL || command == CDC_SEEKP || command == CDC_READN ||
                              command == CDC_READS);

    wait_ready();
    PsxLock();
    if (locParam) {
        /* a position given with a seek or read command is set first, as libcd does */
        memcpy(setloc, param, 3);
        CdDriveCommand(&g_io.cd, CDC_SETLOC, setloc, 3, response);
        count = 0;
    } else {
        count = param_count(command, param != NULL);
    }
    length = CdDriveCommand(&g_io.cd, command, param, count, response);
    PsxUnlock();
    rd.error = length < 0;
    if (length < 0) {
        length = 2;
    }
    if (result) {
        memcpy(result, response, (size_t)length);
    }
    if (wait && !rd.error) {
        CdSync(0, NULL);
    }
    return rd.error ? 0 : 1;
}

int CdControl(uint8_t command, const uint8_t *param, uint8_t *result)
{
    return issue(command, param, result, false);
}

int CdControlF(uint8_t command, const uint8_t *param)
{
    return issue(command, param, NULL, false);
}

int CdControlB(uint8_t command, const uint8_t *param, uint8_t *result)
{
    return issue(command, param, result, true);
}

int CdSync(int mode, uint8_t *result)
{
    bool idle;

    PsxIoService();
    for (;;) {
        uint64_t deadline;

        PsxLock();
        idle = CdDriveIdle(&g_io.cd);
        deadline = g_io.cd.completeAt > g_io.cd.ackAt ? g_io.cd.completeAt : g_io.cd.ackAt;
        PsxUnlock();
        if (idle || mode) {
            break;
        }
        PsxIoWait(deadline);
        PsxIoService();
    }
    if (result) {
        result[0] = current_status();
    }
    if (!idle) {
        return SYNC_NO_INTR;
    }
    return rd.error ? SYNC_DISK_ERROR : SYNC_COMPLETE;
}

int CdReady(int mode, uint8_t *result)
{
    (void)mode;
    if (result) {
        result[0] = current_status();
    }
    return 1; /* CdlDataReady */
}

int CdRead(int sectors, uint32_t *buffer, int mode)
{
    uint8_t response[16];

    wait_ready();
    PsxLock();
    if (g_io.cd.mode != (uint8_t)mode) {
        uint8_t m = (uint8_t)mode;

        CdDriveCommand(&g_io.cd, CDC_SETMODE, &m, 1, response);
    }
    rd.pending = sectors > 0;
    rd.dest = (uint8_t *)buffer;
    rd.sectors = sectors;
    rd.start = CdDriveReadTarget(&g_io.cd);
    rd.mode = (uint32_t)mode;
    CdDriveStartRead(&g_io.cd, sectors, mode);
    PsxUnlock();
    return 1;
}

/* mode 0 waits for the read to finish and returns 0; mode 1 returns the number of sectors
   still to come, or 0 when the read is done. A read that a command cut short returns -1. */
int CdReadSync(int mode, uint8_t *result)
{
    for (;;) {
        int remaining = 0;

        PsxIoService();
        bool aborted = false;

        PsxLock();
        if (rd.pending && !g_io.cd.job.active && !g_io.cd.job.finished) {
            /* a command (Pause, Play, Init) cut the read short */
            rd.pending = false;
            aborted = true;
        } else if (rd.pending) {
            remaining = g_io.cd.job.total - g_io.cd.job.arrived;
            if (remaining < 1) {
                remaining = 1;
            }
        }
        PsxUnlock();
        if (aborted) {
            if (result) {
                result[0] = current_status() | CDS_ERROR;
            }
            return -1;
        }
        if (!remaining) {
            if (result) {
                result[0] = current_status();
            }
            return 0;
        }
        PsxIoWait(0);
        if (mode) {
            if (result) {
                result[0] = current_status();
            }
            return remaining;
        }
    }
}

/* ---- table of contents and positions ---------------------------------------------------------- */

int CdGetToc(Loc *toc)
{
    uint8_t table[100][4];
    int count;

    PsxLock();
    count = CdDriveToc(&g_io.cd, table);
    PsxUnlock();
    memcpy(toc, table, (size_t)(count + 1) * 4);
    return count;
}

int CdGetToc2(int mode, void *toc)
{
    (void)mode;
    return CdGetToc(toc);
}

int CdMix(const uint8_t *volume)
{
    PsxLock();
    memcpy(g_io.cd.mix, volume, 4);
    PsxUnlock();
    return 1;
}

Loc *CdIntToPos(int sector, Loc *loc)
{
    uint8_t msf[3];

    CdLbaToMsf(sector, msf);
    loc->minute = msf[0];
    loc->second = msf[1];
    loc->sector = msf[2];
    return loc;
}

int CdPosToInt(const Loc *loc)
{
    uint8_t msf[3] = {loc->minute, loc->second, loc->sector};

    return CdMsfToLba(msf);
}

File *CdSearchFile(File *file, const char *name)
{
    int32_t lba;
    uint32_t size;
    const char *base;

    if (!DiscFindFile(g_io.disc, name, &lba, &size)) {
        return NULL;
    }
    memset(file, 0, sizeof(*file));
    CdIntToPos(lba, &file->pos);
    file->size = size;
    base = strrchr(name, '\\');
    base = base ? base + 1 : name;
    strncpy(file->name, base, sizeof(file->name) - 1);
    return file;
}
