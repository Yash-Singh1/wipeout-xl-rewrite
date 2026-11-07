/* Exercise the actual C-to-JS sector bridge, including a music pointer retained
   while the same thread reads a cooked data sector. No audio device is opened. */
#include "disc.h"
#include <assert.h>
#include <emscripten.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

EM_JS(void, Setup, (), {
    var data = _malloc(20 * 2048), size = 1000 * 2352;
    HEAPU8.fill(0x27, data, data + 20 * 2048);
    var audio = {size: size, data: _malloc(65536), external: 1, chunks: []};
    for (var at = 0; at < size; at += discChunkSize) {
        var chunk = new Uint8Array(Math.min(discChunkSize, size - at));
        for (var i = 0; i < chunk.length; i++) chunk[i] = ((at + i) * 17 + ((at + i) >>> 8)) & 255;
        audio.chunks.push(chunk);
    }
    copyDiscBytes(audio, 0, 65536, HEAPU8.subarray(audio.data));
    Module.discFiles = {'data.bin': {data: data, size: 20 * 2048}, 'music.bin': audio};
    Module.discExternal = [null, audio];
});

int main(void)
{
    Setup();
    FILE *cue = fopen("test.cue", "w");
    assert(cue);
    fputs("FILE \"data.bin\" BINARY\n TRACK 01 MODE1/2048\n INDEX 01 00:00:00\n"
          "FILE \"music.bin\" BINARY\n TRACK 02 AUDIO\n INDEX 01 00:00:00\n", cue);
    fclose(cue);
    char error[256];
    Disc *disc = DiscOpen("test.cue", error, sizeof(error));
    if (!disc) fprintf(stderr, "%s\n", error);
    assert(disc && DiscTrackCount(disc) == 2);
    const DiscTrack *music = DiscTrackInfo(disc, 1);
    assert(music->audio && music->index1 == 20);
    int sectors[] = {0, 1, 445, 446, 891, 999};
    for (size_t s = 0; s < sizeof(sectors) / sizeof(sectors[0]); s++) {
        const uint8_t *audio = DiscRawSector(disc, music->index1 + sectors[s]);
        uint8_t saved[DISC_RAW_SECTOR], data[2048];
        assert(audio);
        memcpy(saved, audio, sizeof(saved));
        assert(DiscReadData(disc, 0, 1, data));
        assert(!memcmp(audio, saved, sizeof(saved)));
        for (int i = 0; i < 2048; i++) assert(data[i] == 0x27);
        for (int i = 0; i < DISC_RAW_SECTOR; i++) {
            unsigned at = (unsigned)sectors[s] * DISC_RAW_SECTOR + i;
            assert(audio[i] == ((at * 17 + (at >> 8)) & 255));
        }
    }
    assert(!DiscRawSector(disc, DiscLeadOut(disc)));
    DiscClose(disc);
    puts("Web disc bridge: exact sectors across chunks, cooked reads preserve held audio: OK");
    return 0;
}
