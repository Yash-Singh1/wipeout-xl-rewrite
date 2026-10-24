/* The PC version's menus (pcmenus.c) and their data (pcmenudata.c). */
#ifndef PCMENUS_H
#define PCMENUS_H

#include "pc.h"
#include "../../include/menus.h"

/* The PC's menus by number (its currMenu): the main menu, its three choices and the options' screens. The class
   menus 5-8 are set up but nothing leads to them (the class menu shows its tracks itself). The PC's 15, the
   controls' redefinition, is left out (pcmenus.c's SelectController). */
enum {
    PC_MENU_MAIN,
    PC_MENU_RACE_TYPE,
    PC_MENU_CLASS,
    PC_MENU_OPTIONS,
    PC_MENU_TEAM,
    PC_MENU_VECTOR,
    PC_MENU_VENOM,
    PC_MENU_RAPIER,
    PC_MENU_PHANTOM,
    PC_MENU_AUDIO,
    PC_MENU_CONTROLLER,
    PC_MENU_PREFERENCES,
    PC_MENU_LOAD_SAVE,
    PC_MENU_ARCADE_TIMES,
    PC_MENU_TIME_TRIAL_TIMES,
    PC_MENU_COUNT
};

/* The menu layout file (COMMON\MENU.DAT): the boxes of the PC's 17 menus. */
#define PC_MENU_LAYOUTS 17

/* One option of a menu as InitMenus sets it up: its lines of text, where (the main menu centres each on x; the
   other menus place their options from the first one's), the menu it leads to and whether it can be chosen. */
typedef struct PcMenuOptionDef {
    const char *lines[3];
    short x, y;
    short nextMenu;
    short enabled;
} PcMenuOptionDef;

/* A menu: its title, its options, the menus Back and Accept lead to (999: none, the main menu's Back quits). */
typedef struct PcMenuDef {
    const char *title;
    short numOptions;
    short prevMenu;
    short nextMenu;
    PcMenuOptionDef options[6];
} PcMenuDef;

extern const PcMenuDef pcMenuDefs[PC_MENU_COUNT];

/* The PC CD's music: each audio track's title and artist (ayrace.c's cdTrackNames, cdTrackArtists); 0 the shuffle. */
extern const char *const pcCdTrackNames[13];
extern const char *const pcCdTrackArtists[13];

/* The team screen's performance bars: thrust, top speed, turning, shield and aerodynamics of each team (by ship / 3:
   AG Systems, Auricom, Qirex, Feisar, Piranha), out of 10. */
extern const u_char pcTeamRatings[5][5];

#endif
