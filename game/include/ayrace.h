#ifndef AYRACE_H
#define AYRACE_H

#include "psyq.h"

/* Race result as the race-complete screens read it. */
typedef struct RaceState {
    unsigned char pad0[18];
    short         raceInfo, complete;
    unsigned char pad16[2];
    short         highScore;
    unsigned char pad1a[30];
    union {
        unsigned int flags;
        unsigned char bytes[4];
    } championship;
    unsigned char pad3c[2];
    unsigned char class1Unlocked, class2Unlocked, class1Race, class2Race;
} RaceState;

typedef struct ScoreData {
    unsigned char pad0[36];
    unsigned char displayScore;
    unsigned char pad25[31];
    unsigned char gotHiScore;
} ScoreData;

/* The in-race pause menu, also used for the two-player pause handshake over the link. */
typedef struct PauseMenu {
    int    selection;       /* highlighted option, -1 while the game is not paused */
    int    musicPaused;
    int    command;         /* option chosen when the menu closes, -1 for none */
    int    count;
    int    frameCount;      /* flash counter for the confirmation prompt */
    u_char pad14;
    u_char active;          /* a two-player pause is in progress */
    u_char showOptions;     /* 1 while the option list is shown, 0 while asking for confirmation */
    u_char stage;           /* two-player stage: 0 requested, 1 paused by the other player, 2 none */
    u_char confirmed;       /* answer to the confirmation prompt: 0 no, 1 yes, 2 pending */
    u_char confirmChoice;
} PauseMenu;

/* A CD position as the drive reports it: track and time in BCD. */
struct WipeCdStatus {
    u_char track;
    u_char pad1[3];
    u_char time;
};

#endif
