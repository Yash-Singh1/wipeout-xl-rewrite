#ifndef INCLUDE_ASM_H
#define INCLUDE_ASM_H

/* Port: the decompilation pulled assembly functions into the C files with
   INCLUDE_ASM.  Every function has C in this build (nothing uses these), so
   the macros expand to nothing. */
#ifndef INCLUDE_ASM
#define INCLUDE_ASM(FOLDER, NAME)
#endif
#ifndef INCLUDE_RODATA
#define INCLUDE_RODATA(FOLDER, NAME)
#endif

#endif /* INCLUDE_ASM_H */
