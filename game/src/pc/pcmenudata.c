/* The PC version's menu data: its menus as InitMenus (menus.c) sets them up, its CD's music titles and its ship
   performance chart. */
#include "pcmenus.h"


const PcMenuDef pcMenuDefs[PC_MENU_COUNT] = {
    [PC_MENU_MAIN] = {"WIPEOUT", 5, 999, 0, {
        {{"RACE", "TYPE"}, 63, 87, PC_MENU_RACE_TYPE, 1},
        {{"TEAM"}, 161, 87, PC_MENU_TEAM, 1},
        {{"CLASS", "AND", "TRACK"}, 255, 87, PC_MENU_CLASS, 1},
        {{"START"}, 162, 185, PC_MENU_RACE_TYPE, 1}, /* (Accept here starts the race) */
        {{"OPTIONS"}, 162, 203, PC_MENU_OPTIONS, 1},
    }},
    /* the challenge is the third option once one is open (SetupUnlocks) */
    [PC_MENU_RACE_TYPE] = {"RACE TYPE", 2, PC_MENU_MAIN, 0, {
        {{"ARCADE"}, 30, 90, PC_MENU_MAIN, 1},
        {{"TIME TRIAL"}, 128, 64, PC_MENU_MAIN, 1},
        {{"CHALLENGE I"}, 167, 144, PC_MENU_MAIN, 1},
    }},
    [PC_MENU_CLASS] = {"RACING CLASS", 4, PC_MENU_MAIN, 0, {
        {{"VECTOR"}, 30, 90, PC_MENU_MAIN, 1},
        {{"VENOM"}, 128, 64, PC_MENU_MAIN, 1},
        {{"RAPIER"}, 205, 64, PC_MENU_MAIN, 1},
        {{"PHANTOM"}, 281, 64, PC_MENU_MAIN, 0},
    }},
    [PC_MENU_OPTIONS] = {"OPTIONS MENU", 6, PC_MENU_MAIN, 0, {
        {{"AUDIO CONFIG"}, 30, 90, PC_MENU_AUDIO, 1},
        {{"CONTROLLER SETUP"}, 128, 64, PC_MENU_CONTROLLER, 1},
        {{"PREFERENCES"}, 205, 64, PC_MENU_PREFERENCES, 1},
        {{"LOAD AND SAVE"}, 281, 64, PC_MENU_LOAD_SAVE, 1},
        {{"BEST ARCADE TIMES"}, 129, 144, PC_MENU_ARCADE_TIMES, 1},
        {{"BEST TIME TRIAL TIMES"}, 129, 144, PC_MENU_TIME_TRIAL_TIMES, 1},
    }},
    [PC_MENU_TEAM] = {"TEAM MENU", 0, PC_MENU_MAIN, PC_MENU_MAIN},
    [PC_MENU_VECTOR] = {"VECTOR CLASS", 0, PC_MENU_CLASS, PC_MENU_MAIN},
    [PC_MENU_VENOM] = {"VENOM CLASS", 0, PC_MENU_CLASS, PC_MENU_MAIN},
    [PC_MENU_RAPIER] = {"RAPIER CLASS", 0, PC_MENU_CLASS, PC_MENU_MAIN},
    [PC_MENU_PHANTOM] = {"PHANTOM CLASS", 0, PC_MENU_CLASS, PC_MENU_MAIN},
    [PC_MENU_AUDIO] = {"AUDIO CONFIG", 0, PC_MENU_OPTIONS, PC_MENU_OPTIONS},
    [PC_MENU_CONTROLLER] = {"CONTROLLER", 0, PC_MENU_OPTIONS, PC_MENU_OPTIONS},
    [PC_MENU_PREFERENCES] = {"GAME CONFIG", 0, PC_MENU_OPTIONS, PC_MENU_OPTIONS},
    [PC_MENU_LOAD_SAVE] = {"LOAD AND SAVE", 0, PC_MENU_OPTIONS, PC_MENU_OPTIONS},
    [PC_MENU_ARCADE_TIMES] = {"BEST TIMES", 0, PC_MENU_OPTIONS, PC_MENU_OPTIONS},
    [PC_MENU_TIME_TRIAL_TIMES] = {"BEST TIMES", 0, PC_MENU_OPTIONS, PC_MENU_OPTIONS},
};

const char *const pcCdTrackNames[13] = {
    "RANDOM", "BODY IN MOTION", "CANADA", "HAKAPIK MURDER", "PLASTICITY", "MESSIJ RECEIVED", "SURGEON",
    "TENATION", "MESSIJ XTND", "KINKONG", "KINKONG", "KINKONG", "KINKONG",
};
const char *const pcCdTrackArtists[13] = {
    "RANDOM", "COLD STORAGE", "COLD STORAGE", "COLD STORAGE", "COLD STORAGE", "COLD STORAGE", "COLD STORAGE",
    "COLD STORAGE", "COLD STORAGE", "COLD STORAGE", "COLD STORAGE", "COLD STORAGE", "",
};

const u_char pcTeamRatings[5][5] = {
    {7, 5, 6, 3, 6},
    {5, 6, 5, 5, 5},
    {4, 7, 3, 7, 5},
    {6, 3, 7, 6, 7},
    {10, 10, 10, 10, 10},
};
