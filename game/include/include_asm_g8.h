#ifndef INCLUDE_ASM_G8_H
#define INCLUDE_ASM_G8_H
/* With non-zero -G, GCC emits functions after top-level __asm__ blocks, so
 * wrap each include in a dummy function; maspsx unwraps lines marked
 * "maspsx-keep" (see tools/maspsx/README.md). Kept outside include_asm.h,
 * which splat regenerates on every split. */
#if !defined(M2CTX) && !defined(PERMUTER)
#define INCLUDE_ASM(FOLDER, NAME) \
    void __maspsx_include_asm_hack_##NAME() { \
        __asm__( \
            ".text # maspsx-keep \n" \
            "\t.align\t2 # maspsx-keep\n" \
            "\t.set noreorder # maspsx-keep\n" \
            "\t.set noat # maspsx-keep\n" \
            ".include \""FOLDER"/"#NAME".s\" # maspsx-keep\n" \
            "\t.set reorder # maspsx-keep\n" \
            "\t.set at # maspsx-keep\n" \
        ); \
    }
#endif
#include "include_asm.h"
#endif
