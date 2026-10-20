#ifndef ANIMOVER_H
#define ANIMOVER_H

#include "wipeout.h"

/* One heading of the end credits: a title line followed by up to 28 name lines. */
typedef struct CreditEntry {
    char *title;
    char *lines[28];
    int   count;
} CreditEntry;

/* The entry block of a program header (PS-X EXE fields from offset 0x10), as Exec takes it. */
typedef struct ExecBlock {
    u_long pc0;
    u_long gp0;
    u_long tAddr;
    u_long tSize;
    u_long dAddr;
    u_long dSize;
    u_long bAddr;
    u_long bSize;
    u_long sAddr;
    u_long sSize;
} ExecBlock;

#endif
