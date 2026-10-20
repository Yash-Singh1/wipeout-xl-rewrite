/* The memory card as the game uses it: the BIOS events (_card_info, _card_load, _card_clear,
   _card_write), and the file calls (open with FCREAT and a block count, read, write, close,
   firstfile, nextfile, rename, delete, format) over a real card image saved to disk. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#include "../../platform/psx/internal.h"
#include "testenv.h"
#include "testutil.h"

typedef struct DIRENTRY {
    char name[20];
    int attr;
    int size;
    struct DIRENTRY *next;
    char system[8];
} DIRENTRY;

int OpenEvent(uint32_t, int, int, void (*)(void));
int EnableEvent(int);
int TestEvent(int);
void _card_info(int);
void _card_load(int);
void _card_clear(int);
void _card_write(int, int, void *);
void _new_card(void);
int _bu_init(void);
int InitCARD(int);
int StartCARD(void);
int PsxBiosOpen(const char *, int);
int PsxBiosClose(int);
int PsxBiosRead(int, void *, int);
int PsxBiosWrite(int, void *, int);
int PsxBiosRename(const char *, const char *);
int PsxBiosDelete(const char *);
int PsxBiosFormat(const char *);
DIRENTRY *PsxBiosFirstFile(const char *, DIRENTRY *);
DIRENTRY *PsxBiosNextFile(DIRENTRY *);

static int swEvents[4], hwEvents[4];

static void open_events(void)
{
    static const int specs[4] = {4, 0x8000, 0x100, 0x2000};
    int i;

    for (i = 0; i < 4; i++) {
        swEvents[i] = OpenEvent(0xF4000001u, specs[i], 0x2000, 0);
        hwEvents[i] = OpenEvent(0xF0000011u, specs[i], 0x2000, 0);
    }
    for (i = 0; i < 4; i++) {
        EnableEvent(swEvents[i]);
        EnableEvent(hwEvents[i]);
    }
}

/* Which event fired: 0 done, 1 error, 2 no card, 3 new/unformatted, -1 none (CheckCardEvents) */
static int which(const int *events)
{
    int i, found = -1;

    for (i = 0; i < 4; i++) {
        if (TestEvent(events[i]) == 1) {
            CHECK_EQ_INT(found, -1); /* only one */
            found = i;
        }
    }
    return found;
}

static void clear_events(void)
{
    which(swEvents);
    which(hwEvents);
}

/* GetCardInfo / LoadCard */
static int card_info(int channel)
{
    clear_events();
    _card_info(channel);
    return which(swEvents);
}

static int card_load(int channel)
{
    clear_events();
    _card_load(channel);
    return which(swEvents);
}

static long file_size(const char *path)
{
    struct stat st;

    return stat(path, &st) == 0 ? (long)st.st_size : -1;
}

/* SaveCardFile: CreateFile (one block), then WriteFile (header 0x200, scores 0x600, settings 0x100) */
static int save_file(const char *name, const uint8_t *header, const uint8_t *scores, const uint8_t *settings)
{
    int fd = PsxBiosOpen(name, 0x10200);

    if (fd < 0) {
        PsxBiosClose(fd);
        return 0;
    }
    PsxBiosClose(fd);
    fd = PsxBiosOpen(name, 2);
    if (fd < 0 || PsxBiosWrite(fd, (void *)header, 0x200) != 0x200 || PsxBiosWrite(fd, (void *)scores, 0x600) != 0x600 ||
        PsxBiosWrite(fd, (void *)settings, 0x100) != 0x100) {
        PsxBiosClose(fd);
        return 0;
    }
    PsxBiosClose(fd);
    return 1;
}

int main(void)
{
    uint8_t header[0x200], scores[0x600], settings[0x100], back[0x900];
    char image[1100];
    DIRENTRY entry;
    int fd, i;
    const char *dir;

    if (!test_io_start(1.0, NULL)) {
        printf("test_card: disc not found, skipped\n");
        return 0;
    }
    dir = test_save_dir();
    snprintf(image, sizeof(image), "%s/memcard0.mcr", dir);
    for (i = 0; i < 0x200; i++) header[i] = (uint8_t)(i * 7);
    for (i = 0; i < 0x600; i++) scores[i] = (uint8_t)(i * 13 + 1);
    for (i = 0; i < 0x100; i++) settings[i] = (uint8_t)(i * 29 + 5);

    open_events();
    InitCARD(1);
    StartCARD();
    _bu_init();

    /* port 1 holds a formatted card; port 2 is empty; both have been seen already */
    CHECK_EQ_INT(file_size(image), 128 * 1024);
    CHECK_EQ_INT(card_info(0), 0);
    CHECK_EQ_INT(card_info(0x10), 2);
    CHECK_EQ_INT(card_load(0), 0);
    CHECK_EQ_INT(card_load(0x10), 2);
    CHECK(PsxBiosFirstFile("bu00:*", &entry) == NULL);
    CHECK(PsxBiosOpen("bu10:BASCUS-94351X", 1) < 0);

    /* a save */
    CHECK_EQ_INT(save_file("bu00:BASCUS-94351SAVE0001", header, scores, settings), 1);
    CHECK(PsxBiosFirstFile("bu00:*", &entry) == &entry);
    CHECK(!strncmp(entry.name, "BASCUS-94351SAVE0001", 20));
    CHECK_EQ_INT(entry.size, 8192);
    CHECK(PsxBiosNextFile(&entry) == NULL);
    CHECK(PsxBiosFirstFile("bu00:BASCUS-94351*", &entry) == &entry);
    CHECK(PsxBiosFirstFile("bu00:OTHER*", &entry) == NULL);

    /* creating it again fails; the game deletes first when overwriting */
    CHECK(PsxBiosOpen("bu00:BASCUS-94351SAVE0001", 0x10200) < 0);

    /* read it back (LoadCardData) */
    fd = PsxBiosOpen("bu00:BASCUS-94351SAVE0001", 1);
    CHECK(fd >= 0);
    CHECK_EQ_INT(PsxBiosRead(fd, back, 0x200), 0x200);
    CHECK(!memcmp(back, header, 0x200));
    CHECK_EQ_INT(PsxBiosRead(fd, back, 0x600), 0x600);
    CHECK(!memcmp(back, scores, 0x600));
    CHECK_EQ_INT(PsxBiosRead(fd, back, 0x100), 0x100);
    CHECK(!memcmp(back, settings, 0x100));
    PsxBiosClose(fd);
    /* writing a file opened for reading is refused */
    fd = PsxBiosOpen("bu00:BASCUS-94351SAVE0001", 1);
    CHECK(PsxBiosWrite(fd, header, 0x80) < 0);
    PsxBiosClose(fd);

    /* a new session finds the same card on disk */
    PsxCardShutdown();
    CHECK(test_io_restart());
    open_events();
    _bu_init();
    CHECK(PsxBiosFirstFile("bu00:*", &entry) == &entry);
    fd = PsxBiosOpen("bu00:BASCUS-94351SAVE0001", 1);
    CHECK_EQ_INT(PsxBiosRead(fd, back, 0x200), 0x200);
    CHECK(!memcmp(back, header, 0x200));
    PsxBiosClose(fd);

    /* rename, then delete */
    CHECK_EQ_INT(PsxBiosRename("bu00:BASCUS-94351SAVE0001", "bu00:BASCUS-94351RENAMED1"), 1);
    CHECK(PsxBiosOpen("bu00:BASCUS-94351SAVE0001", 1) < 0);
    CHECK(PsxBiosFirstFile("bu00:*", &entry) == &entry);
    CHECK(!strncmp(entry.name, "BASCUS-94351RENAMED1", 20));
    CHECK_EQ_INT(PsxBiosRename("bu00:BASCUS-94351RENAMED1", "bu00:BASCUS-94351RENAMED1"), 0);
    CHECK_EQ_INT(PsxBiosDelete("bu00:BASCUS-94351RENAMED1"), 1);
    CHECK_EQ_INT(PsxBiosDelete("bu00:BASCUS-94351RENAMED1"), 0);
    CHECK(PsxBiosFirstFile("bu00:*", &entry) == NULL);

    /* files of several blocks, a full card, and space coming back */
    {
        static uint8_t big[3 * 8192], again[3 * 8192];
        int names;

        for (i = 0; i < (int)sizeof(big); i++) big[i] = (uint8_t)(i * 31 + (i >> 8));
        fd = PsxBiosOpen("bu00:BASCUS-94351BIG", 0x30200);
        CHECK(fd >= 0);
        PsxBiosClose(fd);
        fd = PsxBiosOpen("bu00:BASCUS-94351BIG", 2);
        CHECK_EQ_INT(PsxBiosWrite(fd, big, sizeof(big)), (int)sizeof(big));
        CHECK(PsxBiosWrite(fd, big, 128) <= 0); /* the end of the file */
        PsxBiosClose(fd);
        fd = PsxBiosOpen("bu00:BASCUS-94351BIG", 1);
        CHECK_EQ_INT(PsxBiosRead(fd, again, sizeof(again)), (int)sizeof(again));
        CHECK(!memcmp(big, again, sizeof(big)));
        PsxBiosClose(fd);
        CHECK(PsxBiosFirstFile("bu00:*", &entry) == &entry);
        CHECK_EQ_INT(entry.size, 3 * 8192);
        for (names = 0; names < 12; names++) {
            char name[64];

            snprintf(name, sizeof(name), "bu00:BASCUS-94351F%03d", names);
            fd = PsxBiosOpen(name, 0x10200);
            CHECK(fd >= 0);
            PsxBiosClose(fd);
        }
        CHECK(PsxBiosOpen("bu00:BASCUS-94351FULL", 0x10200) < 0); /* 15 of 15 blocks used */
        CHECK_EQ_INT(PsxBiosDelete("bu00:BASCUS-94351BIG"), 1);
        fd = PsxBiosOpen("bu00:BASCUS-94351FULL", 0x10200);
        CHECK(fd >= 0);
        PsxBiosClose(fd);
        for (names = 0; names < 12; names++) {
            char name[64];

            snprintf(name, sizeof(name), "bu00:BASCUS-94351F%03d", names);
            PsxBiosDelete(name);
        }
        PsxBiosDelete("bu00:BASCUS-94351FULL");
    }

    /* the image is a real card: header "MC", each directory frame checksummed */
    {
        FILE *f = fopen(image, "rb");
        uint8_t card[128 * 1024];
        int frame;

        CHECK(f != NULL);
        CHECK_EQ_INT((long)fread(card, 1, sizeof(card), f), (long)sizeof(card));
        fclose(f);
        CHECK(card[0] == 'M' && card[1] == 'C');
        for (frame = 0; frame < 16; frame++) {
            uint8_t x = 0;

            for (i = 0; i < 127; i++) {
                x ^= card[frame * 128 + i];
            }
            CHECK_EQ_INT(card[frame * 128 + 127], x);
        }
        CHECK_EQ_INT(card[128], 0xA1); /* the deleted first block of the last file (deleted, free) */
    }

    /* unformatting (UnFormat's sector writes) then formatting */
    {
        uint8_t zero[128];

        memset(zero, 0xFF, sizeof(zero));
        clear_events();
        _new_card();
        _card_write(0, 0, zero);
        CHECK_EQ_INT(which(hwEvents), 0);
        CHECK_EQ_INT(card_load(0), 3); /* not formatted: reported as new */
        CHECK(PsxBiosOpen("bu00:BASCUS-94351X", 0x10200) < 0);
        CHECK_EQ_INT(PsxBiosFormat("bu00:"), 1);
        CHECK_EQ_INT(card_load(0), 0);
        CHECK(PsxBiosFirstFile("bu00:*", &entry) == NULL);
        fd = PsxBiosOpen("bu00:BASCUS-94351X", 0x10200);
        CHECK(fd >= 0);
        PsxBiosClose(fd);
    }
    /* _card_clear, and a write to a card that is not there */
    {
        uint8_t zero[128] = {0};

        clear_events();
        _card_clear(0);
        CHECK_EQ_INT(which(hwEvents), 0);
        _card_write(0x10, 0, zero);
        CHECK_EQ_INT(which(hwEvents), 2);
        _card_write(0, 5000, zero);
        CHECK_EQ_INT(which(hwEvents), 1);
    }
    test_io_stop();
    return test_report("card");
}
