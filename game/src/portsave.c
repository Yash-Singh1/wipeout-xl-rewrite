/* Port: the game saves itself. The console's game kept its progress, records and options on a memory card only
   when the player saved from the LOAD/SAVE screen, and at start-up loaded the first save it found. Here the save
   in use (the "slot": a card and a save's name) is remembered (platform/saves.c): it is loaded at start-up, and
   whenever what a save holds (the records, the options, the progress: what WriteFile writes) differs from what
   was last written, it is written again, from the menus (where a new save can be made: its icon is in video
   memory) or when the game closes. The LOAD/SAVE screen still chooses the slot: loading a save, or saving one
   under a new name, makes it the save in use. With no save yet, the first change makes one, AUTOSAVE on the
   first card. */
#include "functions.h"
#include "card.h"
#include "port_platform.h"

#define CONTENTS (0x600 + 0x100)
#define NEW_SAVE "AUTOSAVE"

static u_char written[CONTENTS]; /* what the save in use holds (or would, made now) */
static int known;
static CardHeader newHeader;      /* a new save's header (title, icon), built in the menus (the icon is in video
                                     memory there), so that one can be made anywhere */
static int haveHeader;

/* What a save of the game now would hold (after its header). */
static void Contents(ConfigData *settings, u_char *out)
{
    u_long block[64];

    memset(block, 0, sizeof(block));
    memcpy(out, hiScoreTable, 0x600);
    BuildSaveBlock(block, settings);
    memcpy(out + 0x600, block, 0x100);
}

/* What the save in use holds: taken at the next menu frame (the menus set up some of what a save holds, the key
   configuration among it, after a save is loaded). */
static void Remember(ConfigData *settings)
{
    (void)settings;
    known = 0;
}

/* At start-up: the save in use last time, or (as the console did) the first save on the cards. */
void PortLoadSavedGame(ConfigData *settings)
{
    char name[9];
    int  card = PortSaveSlot(name);

    if (card < 0 || !LoadCardInitData(name, (short)card, settings)) {
        FirstSavedGame(settings); /* (which tells PortSaveChosen what it loaded) */
    }
    Remember(settings);
}

/* The player loaded a save, or saved one (the LOAD/SAVE screen, or the start-up's first save): the save in use. */
void PortSaveChosen(int slot, char *name, ConfigData *settings)
{
    PortSaveSlotSet(slot, name);
    Remember(settings);
}

/* A save was deleted: if it was the one in use, there is none (the next change makes AUTOSAVE). */
void PortSaveDeleted(int slot, char *name)
{
    char current[9];

    if (PortSaveSlot(current) == slot && !strncmp(current, name, 8)) {
        PortSaveSlotSet(-1, "");
    }
}

/* Writes the save in use when what it holds has changed (inMenus: a new one can be made). */
void PortAutoSave(ConfigData *settings, int inMenus)
{
    u_char now[CONTENTS];
    char   name[9];
    int    card, ok;

    {
        /* a save the debug console imported or chose: loaded, the save in use */
        int imported = inMenus ? PortSaveTakeImport(name) : -1;

        if (imported >= 0) {
            if (LoadCardData(name, imported, settings)) {
                PortSaveChosen(imported, name, settings);
            }
            return;
        }
    }
    if (!PortAutoSaveEnabled()) {
        return;
    }
    Contents(settings, now);
    if (inMenus && !haveHeader) {
        GetHeader(newHeader.bytes, (u_char *)NEW_SAVE);
        haveHeader = 1;
    }
    if (!known) {
        if (inMenus) {
            memcpy(written, now, CONTENTS);
            known = 1;
        }
        return;
    }
    if (!memcmp(now, written, CONTENTS)) {
        return;
    }
    card = PortSaveSlot(name);
    if (card >= 0) {
        ok = RewriteCardFile(name, card, settings);
        if (!ok && inMenus) {
            ok = SaveCardFile(name, card, settings); /* (gone from the card: made again) */
        }
    } else if (haveHeader) {
        char fileName[0x20];

        MakeFileName(NEW_SAVE, fileName, 0);
        ok = (CreateFile(fileName) & 0xFF) ? WriteFile(newHeader.bytes, fileName, settings)
                                           : RewriteCardFile(NEW_SAVE, 0, settings); /* (one there already) */
        if (ok) {
            PortSaveSlotSet(0, NEW_SAVE);
        }
    } else {
        return; /* (a new save waits for the menus) */
    }
    PortSaveWritten(ok);
    /* written or not (a full card): not tried again until it changes again */
    memcpy(written, now, CONTENTS);
}

/* The game closes: what changed since the last save, written (the save in use only: no new one is made here). */
void PortAutoSaveAtExit(void)
{
    extern ConfigData *gameData;

    if (gameData) {
        PortAutoSave(gameData, 0);
    }
}
