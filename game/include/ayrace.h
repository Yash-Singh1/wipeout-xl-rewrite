#ifndef AYRACE_H
#define AYRACE_H

#include "psyq.h"

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

/* A CD position as the drive reports it (CdlGetlocP: eight bytes): track and time in BCD. */
struct WipeCdStatus {
    u_char track;
    u_char pad1[3];
    u_char time;
    u_char pad5[3];  /* Port: the drive's result is 8 bytes; the console's objects of this type sat in 8-byte slots */
};

#endif
