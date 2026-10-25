/* The second player's game (platform/second/): every variable of its copy of the game in sections of its own, so
   that the program can put them back as they were at its start (host.c) before each new link. Included in front
   of each of its sources (tools/second_game.py's header).

   Clang places them by this pragma; GCC has none, and its build gathers them when it links the second game's
   objects into one (cmake/second_game.cmake, second.ld). */
#ifndef PORT_SECOND_SECTIONS_H
#define PORT_SECOND_SECTIONS_H

#if defined(__clang__)
#if defined(__APPLE__)
#pragma clang section data = "__DATA,__p2data" bss = "__DATA,__p2bss,zerofill"
#elif defined(_WIN32)
#pragma clang section data = "p2data$b" bss = "p2bss$b"
#else
#pragma clang section data = "p2data" bss = "p2bss"
#endif
#endif

#endif
