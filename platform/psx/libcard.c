/* libcard and the BIOS file calls for the memory card ("bu00:" and "bu10:"). The calls the
   game makes as open, close, read, write and rename are PsyqOpen ... PsyqRename here (the game's
   psyq.h maps them), so they cannot capture the C library's own calls.

   Each port has a 128 KiB card image kept in memory and saved to a file (memcard0.mcr,
   memcard1.mcr: the usual raw image format, so the files work in other programs too) whenever
   the game finishes writing. The image is a real card: header frame, 15 directory frames, 15
   blocks of 8 KiB, with the same allocation states and checksums. The file functions work on
   that structure; the _card_* calls report what they find through the BIOS events, as the
   BIOS does:

     class 0xF4000001 (software, _card_info / _card_load)   class 0xF0000011 (hardware, _card_write / _card_clear)
     spec 0x0004 done   0x8000 error   0x0100 no card   0x2000 new or unformatted card

   Port 0 always has a card (created formatted on first use); port 1 has one only if its image
   file exists. Events are posted before the call returns. */
#include <ctype.h>
#include <stdlib.h>
#include <string.h>

#include "internal.h"

#define SECTOR 128
#define SECTORS 1024
#define BLOCK_SECTORS 64
#define BLOCK_BYTES (SECTOR * BLOCK_SECTORS)
#define BLOCKS 15
#define PORTS 2
#define MAX_FDS 16

#define EVCLASS_SW 0xF4000001u
#define EVCLASS_HW 0xF0000011u
#define EV_DONE 0x0004u
#define EV_ERROR 0x8000u
#define EV_NO_CARD 0x0100u
#define EV_NEW 0x2000u

#define STATE_FIRST 0x51
#define STATE_MIDDLE 0x52
#define STATE_LAST 0x53
#define STATE_FREE 0xA0
#define STATE_DELETED_FIRST 0xA1
#define STATE_DELETED_MIDDLE 0xA2
#define STATE_DELETED_LAST 0xA3

#define FCREAT 0x0200
#define NAME_LENGTH 20

typedef struct DirEntry {
    char name[NAME_LENGTH];
    int attr;
    int size;
    struct DirEntry *next;
    char system[8];
} DirEntry;

typedef struct Card {
    uint8_t image[SECTORS * SECTOR];
    bool present;
    bool dirty;
    bool newCard;       /* not yet seen by the game since it was inserted */
    char path[1100];
} Card;

typedef struct Fd {
    bool used;
    int port;       /* -1 for the serial link device */
    int first;      /* first block, 0-14 */
    int size;
    int pos;
    int flags;
} Fd;

static Card cards[PORTS];
static Fd fds[MAX_FDS];
static char saveDirectory[1024];

/* iteration state for firstfile/nextfile */
static int iterPort, iterNext;
static char iterPattern[64];

/* ---- the card image ------------------------------------------------------------------------- */

static uint8_t checksum(const uint8_t *frame)
{
    uint8_t x = 0;
    int i;

    for (i = 0; i < SECTOR - 1; i++) {
        x ^= frame[i];
    }
    return x;
}

static uint8_t *frame(Card *c, int sector)
{
    return c->image + (size_t)sector * SECTOR;
}

static void set_checksum(Card *c, int sector)
{
    uint8_t *f = frame(c, sector);

    f[SECTOR - 1] = checksum(f);
}

static void format_image(Card *c)
{
    int i;
    uint8_t *f;

    memset(c->image, 0, sizeof(c->image));
    f = frame(c, 0);
    f[0] = 'M';
    f[1] = 'C';
    set_checksum(c, 0);
    for (i = 1; i <= BLOCKS; i++) {
        f = frame(c, i);
        f[0] = STATE_FREE;
        f[8] = f[9] = 0xFF;
        set_checksum(c, i);
    }
    for (i = 16; i < 36; i++) { /* the broken sector list: none */
        f = frame(c, i);
        memset(f, 0, SECTOR);
        f[0] = f[1] = f[2] = f[3] = 0xFF;
        f[8] = f[9] = 0xFF;
        set_checksum(c, i);
    }
    for (i = 36; i < 63; i++) { /* replacement sectors and unused frames */
        memset(frame(c, i), 0xFF, SECTOR);
    }
    memcpy(frame(c, 63), frame(c, 0), SECTOR);
    c->dirty = true;
}

static bool is_formatted(Card *c)
{
    const uint8_t *f = frame(c, 0);

    return f[0] == 'M' && f[1] == 'C' && f[SECTOR - 1] == checksum(f);
}

static void flush(Card *c)
{
    char temp[1110];
    FILE *f;

    if (!c->dirty || !c->path[0]) {
        return;
    }
    snprintf(temp, sizeof(temp), "%s.tmp", c->path);
    f = fopen(temp, "wb");
    if (!f) {
        PortLog("memory card: cannot write %s\n", temp);
        return;
    }
    if (fwrite(c->image, 1, sizeof(c->image), f) != sizeof(c->image)) {
        fclose(f);
        remove(temp);
        return;
    }
    fclose(f);
    if (rename(temp, c->path) != 0) {
        remove(c->path);
        if (rename(temp, c->path) != 0) {
            PortLog("memory card: cannot replace %s\n", c->path);
            return;
        }
    }
    c->dirty = false;
}

static void load_card(int port, bool create)
{
    Card *c = &cards[port];
    FILE *f;

    snprintf(c->path, sizeof(c->path), "%s/memcard%d.mcr", saveDirectory, port);
    f = fopen(c->path, "rb");
    if (f) {
        c->present = fread(c->image, 1, sizeof(c->image), f) == sizeof(c->image);
        fclose(f);
        if (!c->present) {
            PortLog("memory card: %s is not a 128 KiB image; ignored\n", c->path);
        }
    } else if (create) {
        format_image(c);
        c->present = true;
        flush(c);
    }
}

void PsxCardInit(const char *directory)
{
    int port;

    memset(cards, 0, sizeof(cards));
    memset(fds, 0, sizeof(fds));
    snprintf(saveDirectory, sizeof(saveDirectory), "%s", directory);
    for (port = 0; port < PORTS; port++) {
        load_card(port, port == 0);
        cards[port].newCard = true;
    }
}

void PsxCardShutdown(void)
{
    int port;

    for (port = 0; port < PORTS; port++) {
        flush(&cards[port]);
    }
}

/* ---- directory ------------------------------------------------------------------------------- */

static int state_of(Card *c, int block)
{
    return frame(c, block + 1)[0];
}

static bool is_free(int state)
{
    return (state & 0xF0) == 0xA0;
}

static int next_of(Card *c, int block)
{
    const uint8_t *f = frame(c, block + 1);
    int next = f[8] | f[9] << 8;

    return next == 0xFFFF ? -1 : next;
}

static void set_entry(Card *c, int block, int state, int size, int next)
{
    uint8_t *f = frame(c, block + 1);

    f[0] = (uint8_t)state;
    f[1] = f[2] = f[3] = 0;
    f[4] = (uint8_t)size;
    f[5] = (uint8_t)(size >> 8);
    f[6] = (uint8_t)(size >> 16);
    f[7] = 0;
    f[8] = (uint8_t)(next < 0 ? 0xFF : next);
    f[9] = (uint8_t)(next < 0 ? 0xFF : 0);
    set_checksum(c, block + 1);
    c->dirty = true;
}

static void entry_name(Card *c, int block, char out[NAME_LENGTH + 1])
{
    memcpy(out, frame(c, block + 1) + 10, NAME_LENGTH);
    out[NAME_LENGTH] = 0;
}

static int entry_size(Card *c, int block)
{
    const uint8_t *f = frame(c, block + 1);

    return f[4] | f[5] << 8 | f[6] << 16;
}

/* Splits "bu00:NAME" into the port and the name part; false for another device. */
static bool split_name(const char *path, int *port, const char **name)
{
    if (strncmp(path, "bu", 2) || !isdigit((unsigned char)path[2]) || !isdigit((unsigned char)path[3]) ||
        path[4] != ':') {
        return false;
    }
    *port = path[2] == '1' ? 1 : 0;
    *name = path + 5;
    return true;
}

static int find_file(Card *c, const char *name)
{
    int block;

    for (block = 0; block < BLOCKS; block++) {
        char entry[NAME_LENGTH + 1];

        if (state_of(c, block) != STATE_FIRST) {
            continue;
        }
        entry_name(c, block, entry);
        if (!strncmp(entry, name, NAME_LENGTH) && strlen(name) <= NAME_LENGTH) {
            return block;
        }
    }
    return -1;
}

static bool pattern_matches(const char *pattern, const char *name)
{
    for (; *pattern; pattern++) {
        if (*pattern == '*') {
            return true;
        }
        if (!*name || (*pattern != '?' && *pattern != *name)) {
            return false;
        }
        name++;
    }
    return !*name;
}

/* ---- the BIOS file calls --------------------------------------------------------------------------- */

int PsyqOpen(const char *path, int flags)
{
    int port, fd, block;
    const char *name;
    Card *c;
    Fd *f;

    for (fd = 0; fd < MAX_FDS && fds[fd].used; fd++) {
    }
    if (fd == MAX_FDS) {
        return -1;
    }
    f = &fds[fd];
    if (!strncmp(path, "sio:", 4)) { /* the serial link: nothing answers */
        memset(f, 0, sizeof(*f));
        f->used = true;
        f->port = -1;
        return fd;
    }
    if (!split_name(path, &port, &name)) {
        return -1;
    }
    c = &cards[port];
    if (!c->present || !is_formatted(c)) {
        return -1;
    }
    block = find_file(c, name);
    if (flags & FCREAT) {
        int wanted = (flags >> 16) & 0xFFFF, found = 0, chain[BLOCKS], i;

        if (block >= 0 || wanted < 1 || strlen(name) > NAME_LENGTH) {
            return -1;
        }
        for (i = 0; i < BLOCKS && found < wanted; i++) {
            if (is_free(state_of(c, i))) {
                chain[found++] = i;
            }
        }
        if (found < wanted) {
            return -1;
        }
        for (i = 0; i < wanted; i++) {
            int state = i == 0 ? STATE_FIRST : i == wanted - 1 ? STATE_LAST : STATE_MIDDLE;

            set_entry(c, chain[i], state, i == 0 ? wanted * BLOCK_BYTES : 0, i == wanted - 1 ? -1 : chain[i + 1]);
            memset(frame(c, (chain[i] + 1) * BLOCK_SECTORS), 0, BLOCK_BYTES);
        }
        memset(frame(c, chain[0] + 1) + 10, 0, NAME_LENGTH);
        memcpy(frame(c, chain[0] + 1) + 10, name, strlen(name));
        set_checksum(c, chain[0] + 1);
        block = chain[0];
        flush(c);
    }
    if (block < 0) {
        return -1;
    }
    f->used = true;
    f->port = port;
    f->first = block;
    f->size = entry_size(c, block);
    f->pos = 0;
    f->flags = flags;
    return fd;
}

int PsyqClose(int fd)
{
    if (fd < 0 || fd >= MAX_FDS || !fds[fd].used) {
        return -1;
    }
    if (fds[fd].port >= 0) {
        flush(&cards[fds[fd].port]);
    }
    fds[fd].used = false;
    return fd;
}

/* Copies between the buffer and the file's data, following the block chain. */
static int transfer(Fd *f, void *buffer, int count, bool write)
{
    Card *c = &cards[f->port];
    uint8_t *bytes = buffer;
    int done = 0;

    if (count > f->size - f->pos) {
        count = f->size - f->pos;
    }
    while (done < count) {
        int blockIndex = f->pos / BLOCK_BYTES, block = f->first, offset = f->pos % BLOCK_BYTES, n, i;
        uint8_t *data;

        for (i = 0; i < blockIndex; i++) {
            block = next_of(c, block);
            if (block < 0 || block >= BLOCKS) {
                return done ? done : -1;
            }
        }
        data = frame(c, (block + 1) * BLOCK_SECTORS) + offset;
        n = count - done < BLOCK_BYTES - offset ? count - done : BLOCK_BYTES - offset;
        if (write) {
            memcpy(data, bytes + done, (size_t)n);
        } else {
            memcpy(bytes + done, data, (size_t)n);
        }
        f->pos += n;
        done += n;
    }
    if (write && done) {
        c->dirty = true;
    }
    return done;
}

int PsyqRead(int fd, void *buffer, int size)
{
    if (fd < 0 || fd >= MAX_FDS || !fds[fd].used) {
        return -1;
    }
    if (fds[fd].port < 0) {
        return 0; /* the serial link delivers nothing */
    }
    return transfer(&fds[fd], buffer, size, false);
}

int PsyqWrite(int fd, void *buffer, int size)
{
    if (fd < 0 || fd >= MAX_FDS || !fds[fd].used) {
        return -1;
    }
    if (fds[fd].port < 0) {
        return size;
    }
    if (!(fds[fd].flags & 2)) {
        return -1;
    }
    return transfer(&fds[fd], buffer, size, true);
}

int delete(const char *path)
{
    int port, block;
    const char *name;
    Card *c;

    if (!split_name(path, &port, &name) || !cards[port].present) {
        return 0;
    }
    c = &cards[port];
    block = find_file(c, name);
    if (block < 0) {
        return 0;
    }
    while (block >= 0 && block < BLOCKS) {
        int state = state_of(c, block), next = next_of(c, block);

        set_entry(c, block, state == STATE_FIRST ? STATE_DELETED_FIRST
                            : state == STATE_MIDDLE ? STATE_DELETED_MIDDLE : STATE_DELETED_LAST,
                  entry_size(c, block), next);
        block = state == STATE_LAST ? -1 : next;
    }
    flush(c);
    return 1;
}

int PsyqRename(const char *from, const char *to)
{
    int portA, portB, block;
    const char *nameA, *nameB;
    Card *c;

    if (!split_name(from, &portA, &nameA) || !split_name(to, &portB, &nameB) || portA != portB ||
        !cards[portA].present || strlen(nameB) > NAME_LENGTH) {
        return 0;
    }
    c = &cards[portA];
    block = find_file(c, nameA);
    if (block < 0 || find_file(c, nameB) >= 0) {
        return 0;
    }
    memset(frame(c, block + 1) + 10, 0, NAME_LENGTH);
    memcpy(frame(c, block + 1) + 10, nameB, strlen(nameB));
    set_checksum(c, block + 1);
    c->dirty = true;
    flush(c);
    return 1;
}

int format(const char *device)
{
    int port;
    const char *name;
    char path[16];

    snprintf(path, sizeof(path), "%.5s", device);
    if (!split_name(path, &port, &name) || !cards[port].present) {
        return 0;
    }
    format_image(&cards[port]);
    flush(&cards[port]);
    return 1;
}

static DirEntry *fill_entry(DirEntry *dir)
{
    Card *c = &cards[iterPort];

    for (; iterNext < BLOCKS; iterNext++) {
        char name[NAME_LENGTH + 1];

        if (state_of(c, iterNext) != STATE_FIRST) {
            continue;
        }
        entry_name(c, iterNext, name);
        if (!pattern_matches(iterPattern, name)) {
            continue;
        }
        memset(dir, 0, sizeof(*dir));
        memcpy(dir->name, name, NAME_LENGTH);
        dir->size = entry_size(c, iterNext);
        dir->system[0] = (char)iterNext;
        iterNext++;
        return dir;
    }
    return NULL;
}

DirEntry *firstfile(const char *pattern, DirEntry *dir)
{
    int port;
    const char *name;

    if (!split_name(pattern, &port, &name) || !cards[port].present || !is_formatted(&cards[port])) {
        return NULL;
    }
    iterPort = port;
    iterNext = 0;
    snprintf(iterPattern, sizeof(iterPattern), "%s", name);
    return fill_entry(dir);
}

DirEntry *nextfile(DirEntry *dir)
{
    return fill_entry(dir);
}

/* ---- the card driver ------------------------------------------------------------------------------- */

static int port_of(int channel)
{
    return (channel & 0x10) || channel == 1 ? 1 : 0;
}

int InitCARD(int mode)
{
    (void)mode;
    return 1;
}

int StartCARD(void)
{
    return 1;
}

/* Initialises the BIOS file system: it reads each card, so the game sees the cards it finds as
   already known, not as newly inserted. */
int _bu_init(void)
{
    int port;

    for (port = 0; port < PORTS; port++) {
        cards[port].newCard = false;
    }
    return 1;
}

void _new_card(void) {}

/* Checks the card: done, no card, or newly inserted. */
void _card_info(int channel)
{
    Card *c = &cards[port_of(channel)];

    if (!c->present) {
        PsxDeliverEvent(EVCLASS_SW, EV_NO_CARD);
    } else if (c->newCard) {
        PsxDeliverEvent(EVCLASS_SW, EV_NEW);
    } else {
        PsxDeliverEvent(EVCLASS_SW, EV_DONE);
    }
}

/* Reads the card's directory: done, no card, or a card that is not formatted. */
void _card_load(int channel)
{
    Card *c = &cards[port_of(channel)];

    if (!c->present) {
        PsxDeliverEvent(EVCLASS_SW, EV_NO_CARD);
    } else if (!is_formatted(c)) {
        PsxDeliverEvent(EVCLASS_SW, EV_NEW);
    } else {
        PsxDeliverEvent(EVCLASS_SW, EV_DONE);
    }
}

/* Clears the "new card" state so later checks report the card as known. */
void _card_clear(int channel)
{
    Card *c = &cards[port_of(channel)];

    if (!c->present) {
        PsxDeliverEvent(EVCLASS_HW, EV_NO_CARD);
        return;
    }
    c->newCard = false;
    PsxDeliverEvent(EVCLASS_HW, EV_DONE);
}

/* Writes one 128-byte sector of the raw card. */
void _card_write(int channel, int sector, void *buffer)
{
    Card *c = &cards[port_of(channel)];

    if (!c->present) {
        PsxDeliverEvent(EVCLASS_HW, EV_NO_CARD);
    } else if (sector < 0 || sector >= SECTORS) {
        PsxDeliverEvent(EVCLASS_HW, EV_ERROR);
    } else {
        memcpy(frame(c, sector), buffer, SECTOR);
        c->dirty = true;
        if (sector == SECTORS - 1 || sector == 0) {
            flush(c);
        }
        PsxDeliverEvent(EVCLASS_HW, EV_DONE);
    }
}

/* ---- for the platform (psx_io.h): the files on a card, and a file put onto one ---- */

int PsxCardList(int port, char names[15][21], int sizes[15])
{
    Card *c;
    int block, count = 0;

    if (port < 0 || port >= PORTS || !cards[port].present || !is_formatted(&cards[port])) {
        return 0;
    }
    c = &cards[port];
    for (block = 0; block < BLOCKS; block++) {
        if (state_of(c, block) == STATE_FIRST) {
            entry_name(c, block, names[count]);
            sizes[count++] = entry_size(c, block);
        }
    }
    return count;
}

int PsxCardWriteFile(int port, const char *name, const void *data, int size)
{
    char path[40];
    int blocks = (size + BLOCK_BYTES - 1) / BLOCK_BYTES, fd, written;

    if (port < 0 || port >= PORTS || blocks < 1 || strlen(name) > NAME_LENGTH) {
        return 0;
    }
    snprintf(path, sizeof(path), "bu%d0:%s", port, name);
    delete(path);
    fd = PsyqOpen(path, FCREAT | blocks << 16);
    if (fd < 0) {
        return 0;
    }
    PsyqClose(fd);
    fd = PsyqOpen(path, 2);
    if (fd < 0) {
        return 0;
    }
    written = PsyqWrite(fd, (void *)data, size);
    PsyqClose(fd);
    return written == size;
}
