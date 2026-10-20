#include "cdrom.h"

#include <math.h>
#include <string.h>

#define SAMPLES_PER_SECOND 44100.0
#define ACK_SAMPLES 66            /* about 1.5 ms: the first response of a command */
#define MIN_SEEK_SAMPLES 176      /* 4 ms: staying on the same track */

uint8_t CdToBcd(unsigned value)
{
    return (uint8_t)((value / 10) * 16 + value % 10);
}

unsigned CdFromBcd(uint8_t value)
{
    return (value >> 4) * 10 + (value & 15);
}

int32_t CdMsfToLba(const uint8_t msf[3])
{
    return (int32_t)((CdFromBcd(msf[0]) * 60 + CdFromBcd(msf[1])) * 75 + CdFromBcd(msf[2])) - 150;
}

void CdLbaToMsf(int32_t lba, uint8_t msf[3])
{
    int32_t absolute = lba + 150;

    if (absolute < 0) {
        absolute = 0;
    }
    msf[0] = CdToBcd((unsigned)absolute / 4500);
    msf[1] = CdToBcd((unsigned)absolute / 75 % 60);
    msf[2] = CdToBcd((unsigned)absolute % 75);
}

void CdDriveInit(CdDrive *cd, const Disc *disc, double speed)
{
    memset(cd, 0, sizeof(*cd));
    cd->disc = disc;
    cd->speed = speed >= 0.25 ? speed : 1.0;
    cd->motor = true;
    cd->mix[0] = 0x80;
    cd->mix[2] = 0x80;
}

static void push_event(CdDrive *cd, CdEventKind kind)
{
    if (cd->eventCount == (int)(sizeof(cd->events) / sizeof(cd->events[0]))) {
        memmove(cd->events, cd->events + 1, sizeof(cd->events) - sizeof(cd->events[0]));
        cd->eventCount--;
    }
    cd->events[cd->eventCount++] = kind;
}

bool CdDrivePopEvent(CdDrive *cd, CdEventKind *kind)
{
    if (!cd->eventCount) {
        return false;
    }
    *kind = cd->events[0];
    memmove(cd->events, cd->events + 1, (size_t)(--cd->eventCount) * sizeof(cd->events[0]));
    return true;
}

bool CdDriveReady(const CdDrive *cd)
{
    return cd->now >= cd->ackAt;
}

bool CdDriveIdle(const CdDrive *cd)
{
    return cd->now >= cd->ackAt && !cd->completeAt;
}

static uint8_t status(const CdDrive *cd)
{
    uint8_t s = cd->motor ? CDS_MOTOR : 0;

    switch (cd->state) {
    case CD_READING: s |= CDS_READING; break;
    case CD_SEEKING: s |= CDS_SEEKING; break;
    case CD_PLAYING: s |= CDS_PLAYING; break;
    default: break;
    }
    return s;
}

/* Samples a sector takes to read at the current mode and drive speed. */
static double sector_period(const CdDrive *cd)
{
    double rate = (cd->mode & CDM_DOUBLE_SPEED) ? 150.0 : 75.0;

    return SAMPLES_PER_SECOND / (rate * cd->speed);
}

static uint64_t seek_samples(const CdDrive *cd, int32_t target)
{
    double distance = fabs((double)target - (double)cd->pos);
    double ms;

    if (distance < 4) {
        return MIN_SEEK_SAMPLES;
    }
    /* about 25 ms for a short jump up to roughly 0.6 s across the disc */
    ms = 25.0 + 575.0 * sqrt(fmin(1.0, distance / 200000.0));
    if (!cd->motor) {
        ms += 600.0;
    }
    return (uint64_t)(ms / 1000.0 / cd->speed * SAMPLES_PER_SECOND);
}

static void begin_seek(CdDrive *cd, int32_t target, CdState after)
{
    cd->seekTarget = target;
    cd->seekDone = cd->now + seek_samples(cd, target);
    cd->afterSeek = after;
    cd->state = CD_SEEKING;
    cd->motor = true;
    cd->sector = NULL;
}

static void finish_seek(CdDrive *cd)
{
    cd->pos = cd->seekTarget;
    cd->state = cd->afterSeek;
    cd->phase = 0;
    cd->sector = NULL;
    cd->nextSector = (double)cd->now + sector_period(cd);
}

static void stop_activity(CdDrive *cd)
{
    cd->state = CD_IDLE;
    cd->sector = NULL;
    cd->phase = 0;
    if (cd->job.active) {
        cd->job.active = false;
    }
}

static int error_response(CdDrive *cd, int code, uint8_t *response)
{
    response[0] = status(cd) | CDS_ERROR;
    response[1] = (uint8_t)code;
    cd->completeAt = 0;
    push_event(cd, CDE_ERROR);
    return -1;
}

static bool valid_bcd(uint8_t b)
{
    return (b & 15) <= 9 && (b >> 4) <= 9;
}

int CdDriveToc(const CdDrive *cd, uint8_t table[100][4])
{
    int count = DiscTrackCount(cd->disc), i;

    memset(table, 0, 100 * 4);
    for (i = 0; i <= count && i < 100; i++) {
        int32_t lba = i == 0 ? DiscLeadOut(cd->disc) : DiscTrackInfo(cd->disc, i - 1)->index1;
        int32_t absolute = lba + 150;

        table[i][0] = CdToBcd((unsigned)absolute / 4500);
        table[i][1] = CdToBcd((unsigned)absolute / 75 % 60);
        table[i][2] = 0; /* the drive reports whole seconds */
        table[i][3] = 0;
    }
    return count;
}

static int getlocp(const CdDrive *cd, uint8_t *response)
{
    const DiscTrack *track = DiscTrackAt(cd->disc, cd->pos);
    int32_t relative;
    uint8_t absolute[3];

    memset(response, 0, 8);
    if (!track) {
        track = DiscTrackInfo(cd->disc, DiscTrackCount(cd->disc) - 1);
    }
    response[0] = CdToBcd((unsigned)track->number);
    response[1] = cd->pos < track->index1 ? 0 : 1;
    relative = cd->pos - track->index1;
    if (relative < 0) {
        relative = -relative - 1; /* the pregap counts down, its last sector reading 00:00:00 */
    }
    response[2] = CdToBcd((unsigned)relative / 4500);
    response[3] = CdToBcd((unsigned)relative / 75 % 60);
    response[4] = CdToBcd((unsigned)relative % 75);
    CdLbaToMsf(cd->pos, absolute);
    response[5] = absolute[0];
    response[6] = absolute[1];
    response[7] = absolute[2];
    return 8;
}

int CdDriveCommand(CdDrive *cd, int command, const uint8_t *param, int paramCount, uint8_t *response)
{
    int length = 1;
    bool wasActive = cd->state == CD_READING || cd->state == CD_PLAYING;

    cd->completeAt = 0; /* a new command replaces the pending second response */
    cd->ackAt = cd->now + ACK_SAMPLES;
    switch (command) {
    case CDC_NOP:
    case CDC_FORWARD:
    case CDC_BACKWARD:
    case CDC_SETFILTER:
        break;
    case CDC_SETLOC: {
        int32_t lba;

        if (paramCount < 3 || !valid_bcd(param[0]) || !valid_bcd(param[1]) || !valid_bcd(param[2]) ||
            CdFromBcd(param[1]) >= 60 || CdFromBcd(param[2]) >= 75) {
            return error_response(cd, 0x10, response);
        }
        lba = CdMsfToLba(param);
        cd->setloc = lba;
        cd->setlocPending = true;
        break;
    }
    case CDC_PLAY: {
        int32_t target = CdDriveReadTarget(cd);

        if (paramCount >= 1 && param[0]) {
            unsigned number = CdFromBcd(param[0]);

            if (number >= 1 && number <= (unsigned)DiscTrackCount(cd->disc)) {
                target = DiscTrackInfo(cd->disc, (int)number - 1)->index1;
            }
        }
        cd->setlocPending = false;
        if (target < 0 || target >= DiscLeadOut(cd->disc)) {
            return error_response(cd, 0x10, response);
        }
        stop_activity(cd);
        begin_seek(cd, target, CD_PLAYING);
        break;
    }
    case CDC_READN:
    case CDC_READS: {
        int32_t target = CdDriveReadTarget(cd);

        cd->setlocPending = false;
        if (target < 0 || target >= DiscLeadOut(cd->disc)) {
            return error_response(cd, 0x10, response);
        }
        cd->job.active = false;
        begin_seek(cd, target, CD_READING);
        break;
    }
    case CDC_SEEKL:
    case CDC_SEEKP: {
        int32_t target = CdDriveReadTarget(cd);

        cd->setlocPending = false;
        if (target < 0 || target >= DiscLeadOut(cd->disc)) {
            return error_response(cd, 0x10, response);
        }
        stop_activity(cd);
        begin_seek(cd, target, CD_IDLE);
        cd->completeAt = cd->seekDone > cd->now ? cd->seekDone : cd->now + 1;
        break;
    }
    case CDC_STANDBY:
        stop_activity(cd);
        cd->completeAt = cd->now + (cd->motor ? 66 : (uint64_t)(0.5 * SAMPLES_PER_SECOND));
        cd->motor = true;
        break;
    case CDC_STOP:
        stop_activity(cd);
        cd->motor = false;
        cd->completeAt = cd->now + (uint64_t)(0.25 * SAMPLES_PER_SECOND);
        break;
    case CDC_PAUSE:
        stop_activity(cd);
        cd->completeAt = cd->now + (wasActive ? (uint64_t)(0.04 * SAMPLES_PER_SECOND) : 88);
        break;
    case CDC_INIT:
    case CDC_RESET:
        stop_activity(cd);
        cd->mode = 0;
        cd->muted = false;
        cd->motor = true;
        cd->setlocPending = false;
        cd->completeAt = cd->now + (uint64_t)(0.06 * SAMPLES_PER_SECOND);
        break;
    case CDC_MUTE:
        cd->muted = true;
        break;
    case CDC_DEMUTE:
        cd->muted = false;
        break;
    case CDC_SETMODE:
        if (paramCount < 1) {
            return error_response(cd, 0x20, response);
        }
        cd->mode = param[0];
        break;
    case CDC_GETPARAM:
        response[1] = cd->mode;
        response[2] = 0;
        response[3] = 0;
        response[4] = 0;
        length = 5;
        break;
    case CDC_GETLOCL: {
        uint8_t absolute[3];

        CdLbaToMsf(cd->pos, absolute);
        memcpy(response, absolute, 3);
        response[3] = 2; /* mode 2 sectors */
        memset(response + 4, 0, 4);
        return 8;
    }
    case CDC_GETLOCP:
        return getlocp(cd, response);
    case CDC_GETTN:
        response[1] = 0x01;
        response[2] = CdToBcd((unsigned)DiscTrackCount(cd->disc));
        length = 3;
        break;
    case CDC_GETTD: {
        unsigned number;
        uint8_t table[100][4];
        int count;

        if (paramCount < 1 || !valid_bcd(param[0])) {
            return error_response(cd, 0x10, response);
        }
        number = CdFromBcd(param[0]);
        count = CdDriveToc(cd, table);
        if (number > (unsigned)count) {
            return error_response(cd, 0x10, response);
        }
        response[1] = table[number][0];
        response[2] = table[number][1];
        length = 3;
        break;
    }
    case CDC_GETID:
        stop_activity(cd);
        cd->completeAt = cd->now + (uint64_t)(0.03 * SAMPLES_PER_SECOND);
        break;
    case CDC_READTOC:
        cd->completeAt = cd->now + (uint64_t)(0.4 * SAMPLES_PER_SECOND / cd->speed);
        break;
    default:
        return error_response(cd, 0x40, response);
    }
    response[0] = status(cd);
    return length;
}

/* Where a read command given now starts: the Setloc position, else wherever the head is going
   or is. */
int32_t CdDriveReadTarget(const CdDrive *cd)
{
    if (cd->setlocPending) {
        return cd->setloc;
    }
    return cd->state == CD_SEEKING ? cd->seekTarget : cd->pos;
}

void CdDriveStartRead(CdDrive *cd, int sectors, int mode)
{
    int32_t target = CdDriveReadTarget(cd);

    cd->completeAt = 0;
    cd->ackAt = cd->now + ACK_SAMPLES;
    cd->setlocPending = false;
    cd->mode = (uint8_t)mode;
    memset(&cd->job, 0, sizeof(cd->job));
    cd->job.active = true;
    cd->job.start = target;
    cd->job.total = sectors;
    cd->job.mode = (uint32_t)mode;
    if (sectors <= 0) {
        cd->job.active = false;
        cd->job.finished = true;
        return;
    }
    begin_seek(cd, target, CD_READING);
}

static void arrive_sector(CdDrive *cd)
{
    cd->nextSector += sector_period(cd);
    if (cd->pos >= DiscLeadOut(cd->disc)) {
        stop_activity(cd);
        push_event(cd, CDE_DATA_END);
        return;
    }
    cd->pos++;
    if (cd->job.active && ++cd->job.arrived >= cd->job.total) {
        cd->job.active = false;
        cd->job.finished = true;
        cd->state = CD_IDLE; /* libcd pauses the drive when the last sector is in */
    }
}

static int16_t saturate(int32_t value)
{
    return (int16_t)(value > 32767 ? 32767 : value < -32768 ? -32768 : value);
}

static void play_sample(CdDrive *cd, int16_t out[2])
{
    if (cd->phase == 0) {
        const DiscTrack *track;

        if (cd->pos >= DiscLeadOut(cd->disc)) {
            stop_activity(cd);
            push_event(cd, CDE_DATA_END);
            return;
        }
        track = DiscTrackAt(cd->disc, cd->pos);
        cd->sector = track && track->audio ? DiscRawSector(cd->disc, cd->pos) : NULL;
    }
    if (cd->sector && !cd->muted) {
        const uint8_t *p = cd->sector + cd->phase * 4;
        int32_t l = (int16_t)(p[0] | p[1] << 8);
        int32_t r = (int16_t)(p[2] | p[3] << 8);

        out[0] = saturate((l * cd->mix[0] + r * cd->mix[3]) >> 7);
        out[1] = saturate((l * cd->mix[1] + r * cd->mix[2]) >> 7);
    }
    if (++cd->phase == DISC_SECTOR_FRAMES) {
        cd->phase = 0;
        cd->pos++;
        if ((cd->mode & CDM_AUTOPAUSE) && cd->pos < DiscLeadOut(cd->disc)) {
            const DiscTrack *before = DiscTrackAt(cd->disc, cd->pos - 1);
            const DiscTrack *after = DiscTrackAt(cd->disc, cd->pos);

            if (before && after && before != after) {
                stop_activity(cd);
                push_event(cd, CDE_DATA_END);
            }
        }
    }
}

void CdDriveSample(CdDrive *cd, int16_t out[2])
{
    out[0] = 0;
    out[1] = 0;
    switch (cd->state) {
    case CD_SEEKING:
        if (cd->now >= cd->seekDone) {
            finish_seek(cd);
        }
        break;
    case CD_READING:
        while (cd->state == CD_READING && (double)cd->now >= cd->nextSector) {
            arrive_sector(cd);
        }
        break;
    case CD_PLAYING:
        play_sample(cd, out);
        break;
    case CD_IDLE:
        break;
    }
    if (cd->completeAt && cd->now >= cd->completeAt) {
        cd->completeAt = 0;
        push_event(cd, CDE_COMPLETE);
    }
    cd->now++;
}
