/* The BIOS file calls the game makes under their libc names (open, close, read, write, rename,
   plus delete, format, firstfile and nextfile) are implemented in libcard.c under Psx names, so
   they cannot capture the C library's own calls. Include this in the game's psyq.h, after the
   system headers and before the declarations of open() and friends: the game's prototypes and
   calls then both become the Psx names. */
#ifndef PSX_BIOS_NAMES_H
#define PSX_BIOS_NAMES_H

#define open PsxBiosOpen
#define close PsxBiosClose
#define read PsxBiosRead
#define write PsxBiosWrite
#define rename PsxBiosRename
#define delete PsxBiosDelete
#define format PsxBiosFormat
#define firstfile PsxBiosFirstFile
#define nextfile PsxBiosNextFile

#endif
