/* Exercise the actual C-to-JS sector bridge, including a music pointer retained
   while the same thread reads a cooked data sector. No audio device is opened. */
#include "disc.h"
#include "cdrom.h"
#include "spu.h"
#include <assert.h>
#include <emscripten.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

EM_ASYNC_JS(void, Setup, (int streamed), {
    phoneMusicCache = undefined;
    var data = _malloc(20 * 2048), size = 1000 * 2352;
    HEAPU8.fill(0x27, data, data + 20 * 2048);
    var audio = {size: size, data: _malloc(65536), external: 1};
    if (streamed) {
        var grown = false;
        globalThis.fetch = async function (url) {
            var index = Number(url.split('/').pop()), at = index * discChunkSize;
            await new Promise(function (resolve) { setTimeout(resolve, globalThis.audioNetworkDelay || 1); });
            if (index > 0 && !grown) { _malloc(32 << 20); grown = true; }
            var bytes = new Uint8Array(Math.min(discChunkSize, size - at));
            for (var i = 0; i < bytes.length; i++) bytes[i] = ((at + i) * 17 + ((at + i) >>> 8)) & 255;
            return {ok: true, arrayBuffer: async function () {return bytes.buffer;}};
        };
        var manifest = {name: 'music.bin', size: size, stream: true, chunks: []};
        for (var at = 0; at < size; at += discChunkSize)
            manifest.chunks.push({url: 'chunk/' + Math.floor(at / discChunkSize), size: Math.min(discChunkSize, size - at)});
        audio.stream = phoneTrackChunks(manifest).stream;
        var header = await audio.stream.header();
        HEAPU8.set(header, audio.data);
        var ensure = audio.stream.ensure;
        audio.requests = 0;
        audio.stream.ensure = function (offset, count) { audio.requests++; return ensure(offset, count); };
    } else {
        audio.chunks = [];
        for (var at = 0; at < size; at += discChunkSize) {
            var chunk = new Uint8Array(Math.min(discChunkSize, size - at));
            for (var i = 0; i < chunk.length; i++) chunk[i] = ((at + i) * 17 + ((at + i) >>> 8)) & 255;
            audio.chunks.push(chunk);
        }
        copyDiscBytes(audio, 0, 65536, HEAPU8.subarray(audio.data));
    }
    Module.discFiles = {'data.bin': {data: data, size: 20 * 2048}, 'music.bin': audio};
    Module.discExternal = [null, audio];
});

EM_ASYNC_JS(void, WaitForMusic, (int sector), {
    await Module.discExternal[1].stream.ensure(sector * 2352, 2352);
});

static void CheckDisc(int streamed)
{
    Setup(streamed);
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
}

static void CheckStarvedMusic(void)
{
    Setup(1);
    char error[256];
    Disc *disc = DiscOpen("test.cue", error, sizeof(error));
    assert(disc);
    const DiscTrack *music = DiscTrackInfo(disc, 1);
    CdDrive cd;
    CdDriveInit(&cd, disc, 1000);
    cd.state = CD_PLAYING;
    cd.pos = music->index1 + 446; /* first cold chunk, after the retained header */

    Spu *spu = calloc(1, sizeof(*spu));
    assert(spu);
    SpuReset(spu);
    uint8_t effect[100 * 16];
    memset(effect, 0x11, sizeof(effect));
    for (int i = 0; i < 100; i++) { effect[i * 16] = 0; effect[i * 16 + 1] = i == 99 ? 1 : 0; }
    SpuWriteRam(spu, 0x1000, effect, sizeof(effect));
    SpuWriteRegister(spu, SPU_CONTROL, SPU_CTRL_ENABLE | SPU_CTRL_UNMUTE);
    SpuWriteRegister(spu, SPU_MAIN_VOL_L, 0x3FFF);
    SpuWriteRegister(spu, SPU_MAIN_VOL_R, 0x3FFF);
    SpuWriteRegister(spu, 0, 0x3FFF); SpuWriteRegister(spu, 2, 0x3FFF);
    SpuWriteRegister(spu, 4, 0x1000); SpuWriteRegister(spu, 6, 0x1000 / 8);
    SpuWriteRegister(spu, 8, 0x000F); SpuWriteRegister(spu, 10, 0x1FC0);
    SpuWriteRegister(spu, SPU_KEY_ON, 1);
    EM_ASM({
        globalThis.audioNetworkDelay = 50;
        globalThis.audioYielded = false;
        setTimeout(function () { audioYielded = true; }, 0);
    });
    int loud = 0;
    for (int i = 0; i < DISC_SECTOR_FRAMES * 10; i++) {
        int16_t musicSample[2], mixed[2];
        CdDriveSample(&cd, musicSample);
        assert(musicSample[0] == 0 && musicSample[1] == 0);
        SpuSample(spu, musicSample, mixed);
        loud += mixed[0] != 0;
    }
    EM_ASM({
        if (audioYielded) throw new Error('A cold music read suspended the effects mixer');
        if (Module.discExternal[1].requests !== 1) throw new Error('Music retries accumulated pending promises');
    });
    assert(loud > 2000 && loud < 3000);
    assert(spu->voice[0].phase == ADSR_OFF); /* one-shot finished once while music waited */
    assert(spu->samples == DISC_SECTOR_FRAMES * 10);
    assert(cd.now == DISC_SECTOR_FRAMES * 10);
    assert(cd.pos == music->index1 + 446 && cd.phase == 0);

    WaitForMusic(446);
    /* The first complete sector resumes from sample zero, including heap growth
       during the fetch. No music bytes are skipped or repeated. */
    for (unsigned i = 0; i < DISC_SECTOR_FRAMES; i++) {
        int16_t sample[2];
        CdDriveSample(&cd, sample);
        for (unsigned channel = 0; channel < 2; channel++) {
            unsigned at = 446 * DISC_RAW_SECTOR + i * 4 + channel * 2;
            unsigned lo = (at * 17 + (at >> 8)) & 255;
            at++;
            unsigned hi = (at * 17 + (at >> 8)) & 255;
            assert(sample[channel] == (int16_t)(lo | hi << 8));
        }
    }
    assert(cd.pos == music->index1 + 447 && cd.phase == 0);
    free(spu);
    DiscClose(disc);
}

int main(void)
{
    CheckDisc(0);
    CheckDisc(1);
    CheckStarvedMusic();
    puts("Web disc bridge: exact sectors, heap growth, held audio, cold music never suspends effects or loses music samples: OK");
    return 0;
}
