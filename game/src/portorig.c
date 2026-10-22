/* Port: the original Wipeout's tracks, raced as guests in Talon's Reach's slot (see platform/original.c).

   The RACING CLASS menu has a fifth line, WIPEOUT, when the player's copy of the original has been found
   (the debug console's Files tab) and the race is an arcade race or a time trial. On it LEFT and RIGHT go
   through the original's tracks, each raced at the class it was made for (Venom or Rapier). A chosen
   track takes the place of Talon's Reach (trackNum 0, folder 1): the platform answers the game's reads of
   that track's files with ones made from the original's, and the slot's start line, sky height and
   droid section are the guest's until an XL track is chosen again. Its times are not records: the
   records and medals of Talon's Reach stay its own. */
#include "functions.h"
#include "menus.h"
#include "effects.h"

extern s32       raceType;
extern short     startLinePos[];
extern int       skyPos[];
extern SideTable droidPickTable;
extern MenuEntry *menu;

#define HOST_TRACK  0 /* trackNum: Talon's Reach */
#define HOST_FOLDER 1 /* actTrackNum: its folder, track01 */
#define WIPEOUT_OPTION 4

static int   saved;
static short savedStartLine, savedPick;
static int   savedSky;

/* Chooses an original track (index in PortOriginalTrack's list) or none (-1) for the next race. */
void PortOriginalChoose(int index)
{
    PortOriginalTrackSelect(index);
    if (index >= 0) {
        if (!saved) {
            savedStartLine = startLinePos[HOST_FOLDER];
            savedSky = skyPos[HOST_TRACK];
            savedPick = droidPickTable.value[HOST_TRACK];
            saved = 1;
        }
        startLinePos[HOST_FOLDER] = (short)PortOriginalStartLine();
        skyPos[HOST_TRACK] = PortOriginalSkyY();
        droidPickTable.value[HOST_TRACK] = (short)PortOriginalStartLine();
    } else if (saved) {
        startLinePos[HOST_FOLDER] = savedStartLine;
        skyPos[HOST_TRACK] = savedSky;
        droidPickTable.value[HOST_TRACK] = savedPick;
        saved = 0;
    }
}

/* A race begins: Talon's Reach's own values again if no original track is chosen any more (the player
   chose another copy of the original in the debug console meanwhile). */
void PortOriginalRaceBegins(void)
{
    if (PortOriginalTrack() < 0) {
        PortOriginalChoose(-1);
    }
}

/* Whether the class menu offers the original's tracks: there are some, in an arcade race or a time trial. */
int PortOriginalOffered(void)
{
    return PortOriginalTrackCount() > 0 && (raceType == 0 || raceType == 1);
}

/* The class menu's line moved up (step -1) or down (+1) from current over the lines shown: the classes
   (the fourth only when unlocked) and WIPEOUT. */
int PortClassMove(int current, int step, int classCount)
{
    int lines[5], count = 0, i, at = 0;

    for (i = 0; i < classCount; i++) {
        lines[count++] = i;
    }
    if (PortOriginalOffered()) {
        lines[count++] = WIPEOUT_OPTION;
    }
    for (i = 0; i < count; i++) {
        if (lines[i] == current) {
            at = i;
        }
    }
    at += step;
    if (at < 0) {
        at = 0;
    }
    if (at >= count) {
        at = count - 1;
    }
    return lines[at];
}

/* The class menu on the WIPEOUT line: how many tracks LEFT and RIGHT go through, and (draw) the track
   chosen, its name and class. Returns 1 when that line is the current one (the menu shows the guest
   instead of an XL track). */
int PortOriginalClassMenu(MenuSelection *selection, short *trackCount, int draw)
{
    char text[40];

    menu[2].options[WIPEOUT_OPTION].enabled = (short)PortOriginalOffered();
    if (selection->current != WIPEOUT_OPTION || !PortOriginalOffered()) {
        if (selection->current == WIPEOUT_OPTION) {
            selection->current = 0; /* (no longer offered) */
        }
        PortOriginalChoose(-1);
        return 0;
    }
    *trackCount = (short)PortOriginalTrackCount();
    if (selection->trackSelection >= *trackCount) {
        selection->trackSelection = *trackCount - 1;
    }
    if (selection->trackSelection < 0) {
        selection->trackSelection = 0;
    }
    if (!draw) {
        return 1;
    }
    PortOriginalChoose(selection->trackSelection);
    selection->trackNum = HOST_TRACK;
    selection->actTrackNum = HOST_FOLDER;
    selection->shipClass = (char)PortOriginalTrackClass(selection->trackSelection);
    PrintCX((u_char *)"ORIGINAL", 68, 186);
    Print((u_char *)"FROM THE ORIGINAL WIPEOUT", 120, 182, 8);
    sprintf(text, "%s CLASS  TRACK %d OF %d", selection->shipClass == 2 ? "RAPIER" : "VENOM",
            selection->trackSelection + 1, *trackCount);
    Print((u_char *)text, 120, 189, 8);
    return 1;
}

/* Back in the class menu after a race on an original track: its line and track again. */
void PortOriginalMenuRestore(MenuSelection *selection)
{
    if (PortOriginalTrack() >= 0 && PortOriginalOffered()) {
        selection->current = WIPEOUT_OPTION;
        selection->trackSelection = (short)PortOriginalTrack();
    }
}

/* The name of the track of the coming race (a guest's in Talon's Reach's folder). */
char *PortRaceTrackName(short actTrackNum)
{
    if (actTrackNum == HOST_FOLDER && PortOriginalTrack() >= 0) {
        return (char *)PortOriginalTrackName(PortOriginalTrack());
    }
    return GetTrackName(actTrackNum, 0, 0, 0);
}
