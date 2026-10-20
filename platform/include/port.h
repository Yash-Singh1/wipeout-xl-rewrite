/* Included before every game source (compiler option): maps the PlayStation
   compiler's (GCC 2.7, MIPS o32) extensions onto standard C for the native
   build.  The game was written for an ILP32 target: `long` is already `int` in
   the sources (tools/import.py), and the places where a 64-bit pointer or
   struct layout would matter are each marked "Port:". */
#ifndef PORT_H
#define PORT_H

/* The PlayStation had no fortified libc; the game's own overruns must behave
   the way they did there (see the report), not abort. */
#ifndef _FORTIFY_SOURCE
#define _FORTIFY_SOURCE 0
#endif

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#endif
