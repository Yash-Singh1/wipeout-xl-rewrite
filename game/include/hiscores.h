#ifndef HISCORES_H
#define HISCORES_H

/* Width and metrics of one glyph of the menu font, indexed by character code - 'A'. */
typedef struct GlyphWidth {
    u16 width, height, offset;
} GlyphWidth;

/* State of the race that has just finished and of the name entry for a new record.
   pendingRecords holds the records waiting for a name (1: lap, 2: race); displayedRecords the ones
   being shown on the score tab. */
struct HiScoreData {
    u8  nameEntered[9];     /* 0: entered name, NUL terminated */
    u8  currChar;           /* 9: letter under the cursor */
    short charVal;          /* 10: cursor position in the alphabet; 26 = delete, 27 = end */
    short numChars;         /* 12: letters entered so far */
    u8  dispChar[2];        /* 14: the letter under the cursor, as a string */
    int toggleDisplay;      /* 16: frame counter for the blinking cursor */
    u8  userEntry;          /* 20: name entry in progress */
    u8  checkVal;           /* 21: lowest cursor position allowed */
    u8  enterHs;            /* 22: a new record is waiting to be stored */
    u8  pad23;
    int count;              /* 24: frames since the entry finished */
    int padDelay;           /* 28: frames until the pad is read again */
    int totalTime;          /* 32: total time of the race */
    u8  nameEntryActive;    /* 36: the name entry is on screen */
    u8  displayHiScore;     /* 37 */
    short charX;            /* 38: x position after the letters entered so far */
    u8  confirm;            /* 40 */
    u8  lConfirm;           /* 41 */
    u8  pad42[6];
    int raceTime;           /* 48: race time for the race record */
    u8  racePilot;          /* 52: ship that set the race record */
    u8  pad53[7];
    int lapTime;            /* 60: lap time for the lap record */
    u8  lapPilot;           /* 64: ship that set the lap record */
    u8  pad65[3];
    u8  pendingRecords;     /* 68 */
    u8  displayedRecords;   /* 69 */
};

/* Default high-score table: a three-letter name (NUL padded) for each of eight track
   slots and four ship classes. Tracks 8 to 15 start with the entries of tracks 0 to 7. */
typedef struct HiScoreNames {
    char name[8][4][4];
} HiScoreNames;

/* Default race or lap times for the same eight track slots and four classes. */
typedef struct HiScoreTimes {
    s32 time[8][4];
} HiScoreTimes;

#endif
