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


/* Port: heap sizes.  The console's heaps were 0x18F04 bytes (main) and 0xE1000 bytes (reserve; the
   race and the loading screens use it).  The game's records that hold pointers are larger natively
   (ShipData 240 -> 256 bytes, Object 64 -> 104, TrackSection 156 -> 240, ...), so the blocks are
   scaled; docs/RECORDS.md has the measurements behind the factor. */
#define PORT_HEAP_SCALE   2
#define MAIN_HEAP_SIZE    (0x18F04 * PORT_HEAP_SCALE)
#define RESERVE_HEAP_SIZE (0xE1000 * PORT_HEAP_SCALE)

#endif
