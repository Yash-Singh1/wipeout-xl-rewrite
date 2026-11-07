#include "internal.h"
#include <emscripten.h>
#include <emscripten/heap.h>
#include <assert.h>
#include <stdlib.h>

int main(void)
{
    int16_t *pcm = malloc(1024 * 2 * sizeof(*pcm));
    int i;

    assert(pcm);
    assert(PsxAudioDeviceOpen(true));
    for (i = 0; i < 2048; i++) pcm[i] = i & 1 ? 16384 : -32768;
    PsxAudioDeviceQueue(pcm, 1024);
    assert(emscripten_resize_heap(emscripten_get_heap_size() + 32 * 1024 * 1024));
    for (i = 0; i < 2048; i++) pcm[i] = i & 1 ? -16384 : 32767;
    PsxAudioDeviceQueue(pcm, 1024);
    EM_ASM({
        if (audioBuffers.length !== 1) throw new Error('Missing scheduled PCM buffer');
        var left = audioBuffers[0].getChannelData(0);
        var right = audioBuffers[0].getChannelData(1);
        for (var i = 0; i < 2048; i++) {
            if (left[i] !== (i < 1024 ? -1 : 32767 / 32768) ||
                right[i] !== (i < 1024 ? 0.5 : -0.5)) throw new Error('PCM changed across heap growth');
        }
    });
    PsxAudioDevicePause(true);
    PsxAudioDevicePause(false);
    PsxAudioDeviceClose();
    EM_ASM({ if (Module.gameAudioOutput.snapshot() !== null) throw new Error('Output still open'); });
    free(pcm);
    puts("Web audio C bridge: exact samples survive heap growth; open, queue, pause and close: OK");
    return 0;
}
