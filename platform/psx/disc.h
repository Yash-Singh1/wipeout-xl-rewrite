/* The user's disc image: a .cue sheet with raw 2352-byte-sector .bin files (one per track, or one for the whole
   disc), 2048-byte-sector data tracks (an .iso) or WAV music tracks (16-bit stereo PCM at 44100 Hz); an .iso by
   itself, as a disc of its data track alone; a CloneCD .ccd with its .img; an Alcohol 120% .mds with its .mdf.
   Whatever the image holds, the sectors are given raw (a 2048-byte track's made up with its sync and header).
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
    int sector_size;   /* bytes a sector takes in the image: 2352 raw, or 2048 (user data alone) */
    int32_t index0;    /* start of the pregap (INDEX 00; equals index1 when there is none) */
    int32_t index1;    /* start of the track proper (INDEX 01) */
    int32_t end;       /* first sector after the track (the next track's index0) */
} DiscTrack;

typedef struct Disc Disc;

/* Opens a disc image and maps its files: a .cue sheet, a .ccd (CloneCD), a .mds (Alcohol 120%), or the data track
   by itself (an .iso, or a .bin or .img of raw sectors). Returns NULL (and a message in error) on failure. */
Disc *DiscOpen(const char *cuePath, char *error, int errorSize);
void DiscClose(Disc *disc);
int DiscTrackCount(const Disc *disc);
const DiscTrack *DiscTrackInfo(const Disc *disc, int index);
int32_t DiscLeadOut(const Disc *disc);
/* The 2352 raw bytes of a sector, or NULL outside the image. */
const uint8_t *DiscRawSector(const Disc *disc, int32_t lba);
/* A raw sector made from 2048 bytes of mode 1 user data: the sync, the header with lba's address, the data; the
   error detection and correction codes are left zero. */
void DiscMakeRawSector(uint8_t *raw, int32_t lba, const uint8_t *data);
/* Hints that these sectors are about to be read, so the audio thread does not wait for the disk. */
void DiscPrefetch(const Disc *disc, int32_t lba, int sectors);
/* The track containing lba (by INDEX 00), or NULL. */
const DiscTrack *DiscTrackAt(const Disc *disc, int32_t lba);

/* ISO 9660 lookup in track 1, for CdSearchFile and the tests. path uses '\' or '/'
   separators and may end in ";1". */
bool DiscFindFile(const Disc *disc, const char *path, int32_t *lba, uint32_t *size);
/* Copies the 2048-byte user data of count sectors starting at lba; false if any is missing. */
bool DiscReadData(const Disc *disc, int32_t lba, int count, void *out);
/* Whether the disc holds the PC version's game: its root has the folders COMMON and TRACK01, and COMMON has
   VECTO.PRM. (The PlayStation game is known by its volume's system identifier instead.) */
bool DiscIsPcGame(const Disc *disc);

#endif
