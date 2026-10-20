#ifndef CARD_H
#define CARD_H

#include "wipeout.h"

/* Memory card directory entry (libapi DIRENTRY layout). */
typedef struct DIRENTRY {
    char            name[20];
    int            attr;
    int            size;
    struct DIRENTRY *next;
    char            system[8];
} DIRENTRY;

DIRENTRY *firstfile(char *name, DIRENTRY *dir);
DIRENTRY *nextfile(DIRENTRY *dir);

/* Icon rectangles of the save-icon image, in VRAM coordinates. */
typedef struct CardImage {
    u_char reserved[6];
    RECT   image;
    u_short cx, cy, cw, ch;
} CardImage;

typedef struct Label6 {
    signed char bytes[6];
} Label6;

typedef struct Label5 {
    signed char bytes[5];
} Label5;

/* One file shown in the card browser. */
typedef struct CardEntry {
    char      name[9];
    u_char    alignment;
    u_char    used;
    char      filename[32];
    Label6    label;
    char      reserved[26];
    u_char    type;
    u_char    first;
    u_char    multi;
    u_char    icon[8];
    u_short   x, y;
} CardEntry;

typedef struct CardSlot {
    CardEntry entry[15];
} CardSlot;

void LoadCardFiles(CardSlot *cards, int slot);

/* Name buffer built for a card file. The union keeps the 4-byte alignment of the original local. */
typedef union CardFilename {
    char bytes[40];
    int words[10];
} CardFilename;

/* Device prefix written before a file name ("bu00" style). */
typedef union CardDevice {
    char bytes[6];
    int align;
} CardDevice;

/* Key block: 2 key sets of 9 pad keys and 2 sets of 6 keys. */
typedef struct CardKeys {
    u_short pad[2][9];
    u_short reserved;
    u_short other[2][6];
} CardKeys;

/* A whole card file as read by LoadCardInitData: header, key/key-block and settings. */
typedef struct CardFileBuffer {
    u_char   header[512];
    char     name[32];
    u_short  data[128];
} CardFileBuffer;

#endif
