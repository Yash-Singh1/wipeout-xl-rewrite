#ifndef MALLOC_H
#define MALLOC_H

/* Header in front of every block of a dynamic heap. The heap object itself has the
   same header; its free field counts the bytes still available. */
typedef struct HeapBlock {
    int              flags;
    int              size;
    int              free;
    char             *data;
    struct HeapBlock *next;
} HeapBlock;

#endif
