/* The user's disc image: a .cue sheet with raw 2352-byte-sector .bin files (one per
   track, or one for the whole disc), or WAV music tracks (16-bit stereo PCM at 44100 Hz).
   Sector addresses here are LBAs: 0 is the first
   sector of track 1 (the console's 00:02:00); add 150 for the absolute minute:second:frame
   a drive reports. */
#ifndef PSX_DISC_H
#define PSX_DISC_H

#include <stdbool.h>
#include <stdint.h>

#define DISC_MAX_TRACKS 99
#define DISC_RAW_SECTOR 2352
#define DISC_SECTOR_FRAMES 588 /* stereo sample frames in a CD-DA sector */

typedef struct DiscTrack {
    int number;
    bool audio;
    int data_offset;   /* bytes before the 2048-byte user data in a raw sector */
    int32_t index0;    /* start of the pregap (INDEX 00; equals index1 when there is none) */
    int32_t index1;    /* start of the track proper (INDEX 01) */
    int32_t end;       /* first sector after the track (the next track's index0) */
} DiscTrack;

typedef struct Disc Disc;

/* Opens a .cue sheet and maps its files. Returns NULL (and a message in error) on failure. */
Disc *DiscOpen(const char *cuePath, char *error, int errorSize);
void DiscClose(Disc *disc);
int DiscTrackCount(const Disc *disc);
const DiscTrack *DiscTrackInfo(const Disc *disc, int index);
int32_t DiscLeadOut(const Disc *disc);
/* The 2352 raw bytes of a sector, or NULL outside the image. */
const uint8_t *DiscRawSector(const Disc *disc, int32_t lba);
/* Hints that these sectors are about to be read, so the audio thread does not wait for the disk. */
void DiscPrefetch(const Disc *disc, int32_t lba, int sectors);
/* The track containing lba (by INDEX 00), or NULL. */
const DiscTrack *DiscTrackAt(const Disc *disc, int32_t lba);

/* ISO 9660 lookup in track 1, for CdSearchFile and the tests. path uses '\' or '/'
   separators and may end in ";1". */
bool DiscFindFile(const Disc *disc, const char *path, int32_t *lba, uint32_t *size);
/* Copies the 2048-byte user data of count sectors starting at lba; false if any is missing. */
bool DiscReadData(const Disc *disc, int32_t lba, int count, void *out);

#endif
