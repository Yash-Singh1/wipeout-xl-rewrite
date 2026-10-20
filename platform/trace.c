/* Test recordings: the drawing commands the game sends, in the console
   recorder's trace format, so a run can be compared with a recording made on
   the console (tools/lockstep.py), and scripted pad input in the recorder's
   own form, so both runs see the same buttons at the same pad reads.

   WIPEOUT_TRACE=FILE      write the trace (gzip when built with zlib and FILE
                           ends in .gz)
   WIPEOUT_PADS=pads.txt   the recording's pad script: lines "COUNT BUTTONS",
                           buttons active high in hex, applied from the
                           COUNT-th read of the fixed pad on */
#include <SDL3/SDL.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef PORT_HAVE_ZLIB
#include <zlib.h>
#endif

#include "platform.h"
#include "trace.h"

enum {
    KIND_BOUNDARY = 1,
    KIND_GP0 = 3,
    KIND_GP1 = 4,
    KIND_PAD = 11,
    KIND_END = 18
};

static FILE *file;
#ifdef PORT_HAVE_ZLIB
static gzFile gz;
#endif
static uint32_t iteration;

static void Write(const void *data, size_t size)
{
#ifdef PORT_HAVE_ZLIB
    if (gz) {
        gzwrite(gz, data, (unsigned)size);
        return;
    }
#endif
    fwrite(data, 1, size, file);
}

static void Event(uint32_t kind, const uint32_t *words, int count)
{
    uint64_t lines = PsxScanlines();
    uint32_t header[7];

    header[0] = kind;
    header[1] = (uint32_t)count * 4;
    header[2] = iteration;
    header[3] = (uint32_t)PsxFieldCount();
    header[4] = (uint32_t)lines;
    header[5] = (uint32_t)(lines >> 32);
    header[6] = 0;
    Write(header, sizeof(header));
    if (count) {
        Write(words, (size_t)count * 4);
    }
}

static void Close(void)
{
    if (!PortTracing()) {
        return;
    }
    Event(KIND_END, NULL, 0);
#ifdef PORT_HAVE_ZLIB
    if (gz) {
        gzclose(gz);
        gz = NULL;
    }
#endif
    if (file) {
        fclose(file);
        file = NULL;
    }
}

int PortTracing(void)
{
#ifdef PORT_HAVE_ZLIB
    if (gz) {
        return 1;
    }
#endif
    return file != NULL;
}

/* ---- the pad script ---- */

typedef struct PadStep {
    uint32_t count;
    uint16_t buttons;
} PadStep;

static PadStep *padSteps;
static int padStepCount;
static uint32_t padReads;

static void LoadPads(const char *path)
{
    FILE *f = fopen(path, "r");
    unsigned long count;
    unsigned buttons;
    int capacity = 0;

    if (!f) {
        fprintf(stderr, "trace: cannot read the pad script %s\n", path);
        exit(1);
    }
    while (fscanf(f, "%lu %x", &count, &buttons) == 2) {
        if (padStepCount == capacity) {
            capacity = capacity ? capacity * 2 : 64;
            padSteps = SDL_realloc(padSteps, (size_t)capacity * sizeof(*padSteps));
        }
        padSteps[padStepCount].count = (uint32_t)count;
        padSteps[padStepCount].buttons = (uint16_t)buttons;
        padStepCount++;
    }
    fclose(f);
}

int PortPadScripted(void)
{
    return padSteps != NULL;
}

void PortScriptFixedPad(unsigned char *packet)
{
    uint16_t buttons = 0;
    int i;

    if (!padSteps) {
        return;
    }
    for (i = 0; i < padStepCount && padSteps[i].count <= padReads; i++) {
        buttons = padSteps[i].buttons;
    }
    /* a digital pad's packet: status, type 0x41, buttons active low */
    packet[0] = 0x00;
    packet[1] = 0x41;
    packet[2] = (unsigned char)~buttons;
    packet[3] = (unsigned char)(~buttons >> 8);
    padReads++;
}

/* ---- recording ---- */

void PortTraceInit(void)
{
    const char *path = SDL_getenv("WIPEOUT_TRACE");
    const char *pads = SDL_getenv("WIPEOUT_PADS");
    static const unsigned char magic[12] = {'W', 'X', 'L', 'T', 'R', 'C', '0', '1', 1, 0, 0, 0};
    size_t length;

    if (pads) {
        LoadPads(pads);
    }
    if (!path) {
        return;
    }
    length = strlen(path);
#ifdef PORT_HAVE_ZLIB
    if (length > 3 && !strcmp(path + length - 3, ".gz")) {
        gz = gzopen(path, "wb1");
    } else
#endif
    {
        (void)length;
        file = fopen(path, "wb");
    }
    if (!PortTracing()) {
        fprintf(stderr, "trace: cannot write %s\n", path);
        exit(1);
    }
    Write(magic, sizeof(magic));
    atexit(Close);
}

void PortTraceGp0(const uint32_t *words, int count)
{
    if (PortTracing() && count > 0) {
        Event(KIND_GP0, words, count);
    }
}

void PortTraceGp1(uint32_t word)
{
    if (PortTracing()) {
        Event(KIND_GP1, &word, 1);
    }
}

void PortTraceBoundary(uint32_t marker)
{
    if (PortTracing()) {
        uint32_t words[2] = {marker, 0};
        Event(KIND_BOUNDARY, words, 2);
    }
    iteration++;
}
