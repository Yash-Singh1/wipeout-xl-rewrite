/* ECM files: a disc image with what each sector's error codes can be worked out from taken out, so that it
   compresses better (Neill Corlett's ECM, as copies of PlayStation discs were kept about 2009).

   The file is "ECM\0", then runs of sectors of one type, each run a count and its sectors, then the EDC of the
   whole image made again. A run's head is one byte (type in its lowest 2 bits, the count less one in the next 5,
   and more bytes of 7 bits after while the top bit is set); a count of 0xFFFFFFFF ends the file. The types:
     0  bytes as they are;
     1  Mode 1 sectors of 2352 bytes: the address (3 bytes) and the data (2048) kept, the sync, mode, EDC, the
        zeros and the ECC made again;
     2  Mode 2 Form 1 sectors without their sync and header (2336 bytes): the subheader (4 bytes, kept once of its
        two copies) and the data (2048) kept, the EDC and the ECC made again (the ECC over a header of zeros, as
        Mode 2 has it);
     3  Mode 2 Form 2, likewise: the subheader and 2324 bytes of data kept, the EDC made again.
   A disc's Mode 2 sectors come as their 16 bytes of sync and header in a run of type 0, then the rest in a run of
   type 2 or 3. The error codes are the CD-ROM's (ECMA-130): the EDC a CRC of 32 bits (polynomial 0xD8018001,
   bits reflected), the ECC the P and Q parities of the Reed-Solomon product code over GF(2^8) (x^8+x^4+x^3+x^2+1). */
#include <SDL3/SDL.h>
#include <stdio.h>
#include <string.h>

#include "ecm.h"

static uint8_t eccForward[256], eccBackward[256];
static uint32_t edcTable[256];

static void Tables(void)
{
    static int made;
    int i, k;

    if (made) {
        return;
    }
    for (i = 0; i < 256; i++) {
        uint32_t edc = (uint32_t)i;
        int j = (i << 1) ^ (i & 0x80 ? 0x11D : 0);

        eccForward[i] = (uint8_t)j;
        eccBackward[i ^ (j & 0xFF)] = (uint8_t)i;
        for (k = 0; k < 8; k++) {
            edc = (edc >> 1) ^ (edc & 1 ? 0xD8018001u : 0);
        }
        edcTable[i] = edc;
    }
    made = 1;
}

static uint32_t Edc(uint32_t edc, const uint8_t *data, size_t size)
{
    while (size--) {
        edc = (edc >> 8) ^ edcTable[(edc ^ *data++) & 0xFF];
    }
    return edc;
}

static void PutLittle32(uint8_t *at, uint32_t value)
{
    at[0] = (uint8_t)value;
    at[1] = (uint8_t)(value >> 8);
    at[2] = (uint8_t)(value >> 16);
    at[3] = (uint8_t)(value >> 24);
}

/* One of the parities (P: 86 columns of 24, Q: 52 diagonals of 43) over the sector from its header (byte 12). */
static void Parity(uint8_t *sector, int majorCount, int minorCount, int majorMultiply, int minorIncrement, int out)
{
    int size = majorCount * minorCount, major, minor;

    for (major = 0; major < majorCount; major++) {
        int index = (major >> 1) * majorMultiply + (major & 1);
        uint8_t a = 0, b = 0;

        for (minor = 0; minor < minorCount; minor++) {
            uint8_t value = sector[12 + index];

            index += minorIncrement;
            if (index >= size) {
                index -= size;
            }
            a ^= value;
            b ^= value;
            a = eccForward[a];
        }
        a = eccBackward[eccForward[a] ^ b];
        sector[out + major] = a;
        sector[out + major + majorCount] = a ^ b;
    }
}

static void Ecc(uint8_t *sector, int zeroHeader)
{
    uint8_t header[4];

    memcpy(header, sector + 12, 4);
    if (zeroHeader) {
        memset(sector + 12, 0, 4);
    }
    Parity(sector, 86, 24, 2, 86, 0x81C);  /* P */
    Parity(sector, 52, 43, 86, 88, 0x8C8); /* Q */
    memcpy(sector + 12, header, 4);
}

/* The sector of the type, its kept bytes in place, completed. */
static void Complete(uint8_t *sector, int type)
{
    switch (type) {
    case 1:
        sector[0] = 0;
        memset(sector + 1, 0xFF, 10);
        sector[11] = 0;
        sector[15] = 1;
        PutLittle32(sector + 0x810, Edc(0, sector, 0x810));
        memset(sector + 0x814, 0, 8);
        Ecc(sector, 0);
        break;
    case 2:
        memcpy(sector + 0x10, sector + 0x14, 4);
        PutLittle32(sector + 0x818, Edc(0, sector + 0x10, 0x808));
        Ecc(sector, 1);
        break;
    case 3:
        memcpy(sector + 0x10, sector + 0x14, 4);
        PutLittle32(sector + 0x92C, Edc(0, sector + 0x10, 0x91C));
        break;
    }
}

int PortIsEcm(const char *path)
{
    FILE *f = fopen(path, "rb");
    char head[4] = {0};
    size_t got = f ? fread(head, 1, 4, f) : 0;

    if (f) {
        fclose(f);
    }
    return got == 4 && !memcmp(head, "ECM\0", 4);
}

int PortEcmDecode(const char *in, const char *out, char *message, size_t messageSize)
{
    FILE *src = fopen(in, "rb"), *dst = src ? fopen(out, "wb") : NULL;
    uint8_t sector[2352], stored[4];
    uint32_t edc = 0;
    int c, ok = 0;

    Tables();
    if (!src || !dst) {
        SDL_snprintf(message, messageSize, "%s could not be %s.", src ? out : in, src ? "written" : "read");
        goto done;
    }
    if (fread(sector, 1, 4, src) != 4 || memcmp(sector, "ECM\0", 4)) {
        SDL_snprintf(message, messageSize, "%s is not an ECM file.", in);
        goto done;
    }
    for (;;) {
        uint32_t count;
        int type, bits = 5;

        if ((c = fgetc(src)) == EOF) {
            goto truncated;
        }
        type = c & 3;
        count = (uint32_t)(c >> 2) & 0x1F;
        while (c & 0x80) {
            if ((c = fgetc(src)) == EOF || bits > 31) {
                goto truncated;
            }
            count |= (uint32_t)(c & 0x7F) << bits;
            bits += 7;
        }
        if (count == 0xFFFFFFFFu) {
            break; /* the end */
        }
        for (count++; count > 0; count--) {
            const uint8_t *write = sector;
            size_t size;

            switch (type) {
            case 0:
                size = count > sizeof(sector) ? sizeof(sector) : count;
                if (fread(sector, 1, size, src) != size) {
                    goto truncated;
                }
                count -= (uint32_t)(size - 1);
                break;
            case 1:
                if (fread(sector + 12, 1, 3, src) != 3 || fread(sector + 16, 1, 0x800, src) != 0x800) {
                    goto truncated;
                }
                Complete(sector, 1);
                size = 2352;
                break;
            case 2:
                if (fread(sector + 0x14, 1, 0x804, src) != 0x804) {
                    goto truncated;
                }
                Complete(sector, 2);
                write = sector + 0x10;
                size = 2336;
                break;
            default:
                if (fread(sector + 0x14, 1, 0x918, src) != 0x918) {
                    goto truncated;
                }
                Complete(sector, 3);
                write = sector + 0x10;
                size = 2336;
                break;
            }
            edc = Edc(edc, write, size);
            if (fwrite(write, 1, size, dst) != size) {
                SDL_snprintf(message, messageSize, "%s could not be written (is the disk full?).", out);
                goto done;
            }
        }
    }
    /* the whole image's EDC, made again, against the file's */
    if (fread(stored, 1, 4, src) != 4) {
        goto truncated;
    }
    if (((uint32_t)stored[0] | (uint32_t)stored[1] << 8 | (uint32_t)stored[2] << 16 | (uint32_t)stored[3] << 24) !=
        edc) {
        SDL_snprintf(message, messageSize, "%s is damaged (its image does not check out).", in);
        goto done;
    }
    ok = 1;
    goto done;
truncated:
    SDL_snprintf(message, messageSize, "%s is cut short.", in);
done:
    if (src) {
        fclose(src);
    }
    if (dst && fclose(dst) != 0 && ok) {
        SDL_snprintf(message, messageSize, "%s could not be written (is the disk full?).", out);
        ok = 0;
    }
    if (!ok && dst) {
        remove(out);
    }
    return ok;
}
