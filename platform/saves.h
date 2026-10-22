/* The saves (saves.c): the save in use, the autosave (game/src/portsave.c), and other emulators' saves of
   Wipeout XL found on this computer, for the debug console to import. */
#ifndef PORT_SAVES_H
#define PORT_SAVES_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* A save of Wipeout XL (or WipEout 2097) found in another program's memory card or save file. */
typedef struct FoundSave {
    char file[1024];      /* the card image or save file */
    char program[48];     /* whose ("DuckStation", ...), from where it was found */
    char format[32];      /* the file's format */
    char name[9];         /* the save's name in the game (after BASCUS-94351) */
    char region[8];       /* "USA", "Europe", "Japan" */
    int blocks;
} FoundSave;

/* Looks for saves in the places other emulators keep memory cards (and in extra, a file or folder the player
   chose, when not NULL); returns how many (FoundSaves holds them). */
int PortSavesScan(const char *extra);
const FoundSave *PortSavesFound(void);
int PortSavesCount(void);
/* Copies a found save onto the first memory card (replacing a save of the same name) and has the game load it,
   the save in use from then on; 1 when done, else message says why. */
int PortSavesImport(int index, char *message, int messageSize);
/* The game loads a save of its own cards (card 0 or 1, the name after BASCUS-94351): the save in use. */
void PortSavesUse(int card, const char *name);
/* The last autosave: 0 none yet, 1 written, -1 failed (a full card); and when (SDL ticks). */
int PortSavesLastWrite(uint64_t *when);

#ifdef __cplusplus
}
#endif

#endif
