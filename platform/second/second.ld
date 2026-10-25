/* The second player's game with GCC (cmake/second_game.cmake): its objects linked into one, their variables in
   sections of their own between the marks host.c finds them by (sections.h does this with Clang). */
SECTIONS
{
    p2data : {
        secondDataStart = .;
        *(.data .data.rel .data.rel.local)
        secondDataEnd = .;
    }
    p2bss : {
        secondBssStart = .;
        *(.bss .bss.* COMMON)
        secondBssEnd = .;
    }
}
