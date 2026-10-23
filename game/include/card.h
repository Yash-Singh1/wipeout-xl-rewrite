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

/* The header of a card file: 512 bytes (CARD_HEADER_SIZE: 256 in WipEout 2097's saves). StoreImage fills the
   icon into it a word at a time, so it is word aligned like the console's local buffers. */
typedef union CardHeader {
    u_char bytes[512];
    u_long align[128];
} CardHeader;

typedef struct Label6 {
    signed char bytes[6];
} Label6;

typedef struct Label8 {
    signed char bytes[8];
} Label8;

typedef struct Label5 {
    signed char bytes[5];
} Label5;

/* One file shown in the card browser. */
typedef struct CardEntry {
    char      name[9];
    u_char    alignment;
    u_char    used;
    char      filename[32];
    Label8    label;        /* Port: eight bytes, for WipEout 2097's "WO2097 "; Wipeout XL's labels are six and
                               its reserved bytes start two earlier (the label text runs on into them either way) */
    char      reserved[24];
    u_char    type;
    u_char    first;        /* (not in WipEout 2097's entries: its browser has no multi-block files) */
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

/* The game's product code: the memory card directory name of its saves (WipEout 2097's on the European disc). */
#define CARD_GAME_CODE REGION_STR("BASCUS-94351", "BESLES-00327")

/* Size of the header block at the start of a save file; WipEout 2097's card format uses half. */
#define CARD_HEADER_SIZE (portEuropeDisc ? 0x100 : 0x200)

/* Offset in the save header where the file's own name starts, after the game title. */
#define CARD_NAME_OFFSET (portEuropeDisc ? 18 : 14)

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
