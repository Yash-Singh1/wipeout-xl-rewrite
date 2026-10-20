#include "functions.h"

extern char msgInsufficientMemory[];
extern char msgAllocRequest[];
extern char msgDFreeNotFound[];
extern HeapBlock *mainheapcopy;


int lowTideMark;


void InitDynamicMem(HeapBlock *head, int size)
{
    int avail;

    avail = size - sizeof(HeapBlock);
    head->flags = 0;
    head->size = avail;
    head->data = (char *)(head + 1);
    head->next = 0;
    head->free = avail;
}


void *DAlloc(HeapBlock *head, int size)
{
    HeapBlock *block;
    HeapBlock *best;
    HeapBlock *allocated;
    int       smallest;
    int       remaining;
    int       adjust;

    block = head;
    best = 0;
    smallest = 0x800000;
    size += 4;
    size = (size | 3) ^ 3;
    while (block) {
        if (block->size >= size + sizeof(HeapBlock) && block->size < smallest && block->flags == 0) {
            smallest = block->size;
            best = block;
        }
        if (block->size == size && block->flags == 0) {
            block->flags = 1;
            head->free -= size;
            return block->data;
        }
        block = block->next;
    }
    if (!best) {
        printf(msgInsufficientMemory);
        printf(msgAllocRequest, size, head->free);
        return 0;
    }
    block = best;
    adjust = block->size - sizeof(HeapBlock);
    adjust = adjust - size;
    allocated = (HeapBlock *)(block->data + adjust);
    block->size = adjust;
    allocated->flags = 1;
    allocated->size = size;
    allocated->data = (char *)(allocated + 1);
    allocated->next = block->next;
    block->next = allocated;
    adjust = head->free - sizeof(HeapBlock);
    remaining = adjust - size;
    head->free = remaining;
    if (head == mainheapcopy && head->free < lowTideMark) {
        lowTideMark = head->free;
    }
    return allocated->data;
}


void DFree(HeapBlock *head, char *ptr)
{
    HeapBlock *block;
    HeapBlock *prevBlock;
    HeapBlock *nextBlock;

    block = head->next;
    prevBlock = head;
    while (block != 0) {
        if (block->data == ptr) {
            nextBlock = block->next;
            head->free += block->size;
            if (nextBlock != 0 && nextBlock->flags == 0) {
                block->size += nextBlock->size + sizeof(HeapBlock);
                block->next = nextBlock->next;
                head->free += sizeof(HeapBlock);
            }
            if (prevBlock->flags == 1) {
                block->flags = 0;
                return;
            }
            prevBlock->size += block->size + sizeof(HeapBlock);
            prevBlock->next = block->next;
            head->free += sizeof(HeapBlock);
            return;
        }
        prevBlock = block;
        block = block->next;
    }
    printf(msgDFreeNotFound);
}
