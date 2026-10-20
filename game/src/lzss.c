#include "functions.h"
#define WINDOW_BITS 13
#define WINDOW_SIZE (1 << WINDOW_BITS)
#define WINDOW_MASK (WINDOW_SIZE - 1)
#define LENGTH_BITS 4
#define MATCH_LENGTH_BIAS ((1 + WINDOW_BITS + LENGTH_BITS) / 9)

void exit(int status);

/* LZSS decoder. Flag bits are read MSB first; a set flag is a literal byte, a clear flag
   is a (position, length) back-reference into the sliding window. */
void ExpandData(unsigned char *inData, unsigned char *outData)
{
    short         k;
    short         windowPos;
    unsigned char ch;
    short         matchLen;
    short         matchPos;
    unsigned int bit;
    unsigned int bits;
    unsigned char bitMask;
    short         bitRack;
    short         flag;
    unsigned char window[WINDOW_SIZE];

    bitRack = 0;
    bitMask = 0x80;
    windowPos = 1;
    for (;;) {
        if (bitMask == 0x80) {
            bitRack = (short)(*(inData++));
        }
        flag = bitRack & bitMask;
        bitMask >>= 1;
        if (bitMask == 0) {
            bitMask = 0x80;
        }
        if (flag) {
            bit = 1 << (8 - 1);
            bits = 0;
            while (bit != 0) {
                if (bitMask == 0x80) {
                    bitRack = (short)(*(inData++));
                }
                if (bitRack & bitMask) {
                    bits |= bit;
                }
                bit >>= 1;
                bitMask >>= 1;
                if (bitMask == 0) {
                    bitMask = 0x80;
                }
            }
            ch = (unsigned char)bits;
            *(outData++) = ch;
            window[windowPos] = ch;
            windowPos = (windowPos + 1) & WINDOW_MASK;
        } else {
            bit = 1 << (WINDOW_BITS - 1);
            bits = 0;
            while (bit != 0) {
                if (bitMask == 0x80) {
                    bitRack = (short)(*(inData++));
                }
                if (bitRack & bitMask) {
                    bits |= bit;
                }
                bit >>= 1;
                bitMask >>= 1;
                if (bitMask == 0) {
                    bitMask = 0x80;
                }
            }
            matchPos = (short)bits;
            if (matchPos == 0) {
                break;
            }
            bit = 1 << (LENGTH_BITS - 1);
            bits = 0;
            while (bit != 0) {
                if (bitMask == 0x80) {
                    bitRack = (short)(*(inData++));
                }
                if (bitRack & bitMask) {
                    bits |= bit;
                }
                bit >>= 1;
                bitMask >>= 1;
                if (bitMask == 0) {
                    bitMask = 0x80;
                }
            }
            matchLen = (short)bits;
            matchLen += MATCH_LENGTH_BIAS;
            for (k = 0; k <= matchLen; k++) {
                ch = window[(matchPos + k) & WINDOW_MASK];
                *(outData++) = ch;
                window[windowPos] = ch;
                windowPos = (windowPos + 1) & WINDOW_MASK;
            }
        }
    }
}
