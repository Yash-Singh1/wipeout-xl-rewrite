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
int PsyqOpen(const char *, int);
int PsyqClose(int);
int PsyqRead(int, void *, int);
int PsyqWrite(int, void *, int);
int PsyqRename(const char *, const char *);
int delete(const char *);
int format(const char *);
DIRENTRY *firstfile(const char *, DIRENTRY *);
DIRENTRY *nextfile(DIRENTRY *);

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
    int fd = PsyqOpen(name, 0x10200);

    if (fd < 0) {
        PsyqClose(fd);
        return 0;
    }
    PsyqClose(fd);
    fd = PsyqOpen(name, 2);
    if (fd < 0 || PsyqWrite(fd, (void *)header, 0x200) != 0x200 || PsyqWrite(fd, (void *)scores, 0x600) != 0x600 ||
        PsyqWrite(fd, (void *)settings, 0x100) != 0x100) {
        PsyqClose(fd);
        return 0;
    }
    PsyqClose(fd);
    return 1;
}

int main(void)
{
    uint8_t header[0x200], scores[0x600], settings[0x100], back[0x900];
    char image[1100], image2[1100];
    DIRENTRY entry;
    int fd, i;
    const char *dir;

    if (!test_io_start(1.0, NULL)) {
        printf("test_card: disc not found, skipped\n");
        return 0;
    }
    dir = test_save_dir();
    snprintf(image, sizeof(image), "%s/memcard0.mcr", dir);
    snprintf(image2, sizeof(image2), "%s/memcard1.mcr", dir);
    for (i = 0; i < 0x200; i++) header[i] = (uint8_t)(i * 7);
    for (i = 0; i < 0x600; i++) scores[i] = (uint8_t)(i * 13 + 1);
    for (i = 0; i < 0x100; i++) settings[i] = (uint8_t)(i * 29 + 5);

    open_events();
    InitCARD(1);
    StartCARD();
    _bu_init();

    /* both ports hold a formatted card, port 2's not written to its file until the game writes to it; both
       have been seen already */
    CHECK_EQ_INT(file_size(image), 128 * 1024);
    CHECK_EQ_INT(file_size(image2), -1);
    CHECK_EQ_INT(card_info(0), 0);
    CHECK_EQ_INT(card_info(0x10), 0);
    CHECK_EQ_INT(card_load(0), 0);
    CHECK_EQ_INT(card_load(0x10), 0);
    CHECK(firstfile("bu00:*", &entry) == NULL);
    CHECK(firstfile("bu10:*", &entry) == NULL);
    CHECK(PsyqOpen("bu10:BASCUS-94351X", 1) < 0);

    /* a save */
    CHECK_EQ_INT(save_file("bu00:BASCUS-94351SAVE0001", header, scores, settings), 1);
    CHECK(firstfile("bu00:*", &entry) == &entry);
    CHECK(!strncmp(entry.name, "BASCUS-94351SAVE0001", 20));
    CHECK_EQ_INT(entry.size, 8192);
    CHECK(nextfile(&entry) == NULL);
    CHECK(firstfile("bu00:BASCUS-94351*", &entry) == &entry);
    CHECK(firstfile("bu00:OTHER*", &entry) == NULL);

    /* creating it again fails; the game deletes first when overwriting */
    CHECK(PsyqOpen("bu00:BASCUS-94351SAVE0001", 0x10200) < 0);

    /* read it back (LoadCardData) */
    fd = PsyqOpen("bu00:BASCUS-94351SAVE0001", 1);
    CHECK(fd >= 0);
    CHECK_EQ_INT(PsyqRead(fd, back, 0x200), 0x200);
    CHECK(!memcmp(back, header, 0x200));
    CHECK_EQ_INT(PsyqRead(fd, back, 0x600), 0x600);
    CHECK(!memcmp(back, scores, 0x600));
    CHECK_EQ_INT(PsyqRead(fd, back, 0x100), 0x100);
    CHECK(!memcmp(back, settings, 0x100));
    PsyqClose(fd);
    /* writing a file opened for reading is refused */
    fd = PsyqOpen("bu00:BASCUS-94351SAVE0001", 1);
    CHECK(PsyqWrite(fd, header, 0x80) < 0);
    PsyqClose(fd);

    /* a new session finds the same card on disk */
    PsxCardShutdown();
    CHECK(test_io_restart());
    open_events();
    _bu_init();
    CHECK(firstfile("bu00:*", &entry) == &entry);
    fd = PsyqOpen("bu00:BASCUS-94351SAVE0001", 1);
    CHECK_EQ_INT(PsyqRead(fd, back, 0x200), 0x200);
    CHECK(!memcmp(back, header, 0x200));
    PsyqClose(fd);

    /* rename, then delete */
    CHECK_EQ_INT(PsyqRename("bu00:BASCUS-94351SAVE0001", "bu00:BASCUS-94351RENAMED1"), 1);
    CHECK(PsyqOpen("bu00:BASCUS-94351SAVE0001", 1) < 0);
    CHECK(firstfile("bu00:*", &entry) == &entry);
    CHECK(!strncmp(entry.name, "BASCUS-94351RENAMED1", 20));
    CHECK_EQ_INT(PsyqRename("bu00:BASCUS-94351RENAMED1", "bu00:BASCUS-94351RENAMED1"), 0);
    CHECK_EQ_INT(delete("bu00:BASCUS-94351RENAMED1"), 1);
    CHECK_EQ_INT(delete("bu00:BASCUS-94351RENAMED1"), 0);
    CHECK(firstfile("bu00:*", &entry) == NULL);

    /* files of several blocks, a full card, and space coming back */
    {
        static uint8_t big[3 * 8192], again[3 * 8192];
        int names;

        for (i = 0; i < (int)sizeof(big); i++) big[i] = (uint8_t)(i * 31 + (i >> 8));
        fd = PsyqOpen("bu00:BASCUS-94351BIG", 0x30200);
        CHECK(fd >= 0);
        PsyqClose(fd);
        fd = PsyqOpen("bu00:BASCUS-94351BIG", 2);
        CHECK_EQ_INT(PsyqWrite(fd, big, sizeof(big)), (int)sizeof(big));
        CHECK(PsyqWrite(fd, big, 128) <= 0); /* the end of the file */
        PsyqClose(fd);
        fd = PsyqOpen("bu00:BASCUS-94351BIG", 1);
        CHECK_EQ_INT(PsyqRead(fd, again, sizeof(again)), (int)sizeof(again));
        CHECK(!memcmp(big, again, sizeof(big)));
        PsyqClose(fd);
        CHECK(firstfile("bu00:*", &entry) == &entry);
        CHECK_EQ_INT(entry.size, 3 * 8192);
        for (names = 0; names < 12; names++) {
            char name[64];

            snprintf(name, sizeof(name), "bu00:BASCUS-94351F%03d", names);
            fd = PsyqOpen(name, 0x10200);
            CHECK(fd >= 0);
            PsyqClose(fd);
        }
        CHECK(PsyqOpen("bu00:BASCUS-94351FULL", 0x10200) < 0); /* 15 of 15 blocks used */
        CHECK_EQ_INT(delete("bu00:BASCUS-94351BIG"), 1);
        fd = PsyqOpen("bu00:BASCUS-94351FULL", 0x10200);
        CHECK(fd >= 0);
        PsyqClose(fd);
        for (names = 0; names < 12; names++) {
            char name[64];

            snprintf(name, sizeof(name), "bu00:BASCUS-94351F%03d", names);
            delete(name);
        }
        delete("bu00:BASCUS-94351FULL");
    }

    /* a save on the second card makes its file */
    CHECK_EQ_INT(save_file("bu10:BASCUS-94351SAVE0002", header, scores, settings), 1);
    CHECK_EQ_INT(file_size(image2), 128 * 1024);
    CHECK(firstfile("bu10:*", &entry) == &entry);
    CHECK(!strncmp(entry.name, "BASCUS-94351SAVE0002", 20));

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
        CHECK(PsyqOpen("bu00:BASCUS-94351X", 0x10200) < 0);
        CHECK_EQ_INT(format("bu00:"), 1);
        CHECK_EQ_INT(card_load(0), 0);
        CHECK(firstfile("bu00:*", &entry) == NULL);
        fd = PsyqOpen("bu00:BASCUS-94351X", 0x10200);
        CHECK(fd >= 0);
        PsyqClose(fd);
    }
    /* _card_clear, a write to the second card, and one beyond a card's end */
    {
        uint8_t zero[128] = {0};

        clear_events();
        _card_clear(0);
        CHECK_EQ_INT(which(hwEvents), 0);
        _card_write(0x10, 0, zero);
        CHECK_EQ_INT(which(hwEvents), 0);
        _card_write(0, 5000, zero);
        CHECK_EQ_INT(which(hwEvents), 1);
    }
    test_io_stop();
    return test_report("card");
}
