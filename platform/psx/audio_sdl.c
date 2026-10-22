/* The audio output device: SDL3 pulls 44.1 kHz stereo samples from the SPU/CD mixer on its
   own thread, in small blocks so register writes from the game reach the next block; or, with
   queued audio (psx_io.h), the game's fields render them and queue them here. */
#include <SDL3/SDL.h>

#include "internal.h"

#define BLOCK_FRAMES 256
/* queued samples: enough in hand to bridge the gaps between the game's fields, and not so many that the
   sound lags (a game that falls behind or races ahead, as after a stall, starts again from here) */
#define QUEUE_LEAD_FRAMES 2048
#define QUEUE_MAX_FRAMES 8192

static SDL_AudioStream *stream;

static void SDLCALL OnAudio(void *user, SDL_AudioStream *s, int additional, int total)
{
    int16_t block[BLOCK_FRAMES * 2];

    (void)user;
    (void)total;
    while (additional > 0) {
        int frames = additional / 4;

        if (frames > BLOCK_FRAMES) {
            frames = BLOCK_FRAMES;
        }
        if (frames <= 0) {
            break;
        }
        PsxAudioRender(block, (uint32_t)frames);
        SDL_PutAudioStreamData(s, block, frames * 4);
        additional -= frames * 4;
    }
}

bool PsxAudioDeviceOpen(bool queued)
{
    SDL_AudioSpec spec;

    if (stream) {
        return true;
    }
    if (!SDL_InitSubSystem(SDL_INIT_AUDIO)) {
        PortLog("audio: %s\n", SDL_GetError());
        return false;
    }
    SDL_SetHintWithPriority(SDL_HINT_AUDIO_DEVICE_SAMPLE_FRAMES, "512", SDL_HINT_DEFAULT);
    spec.format = SDL_AUDIO_S16;
    spec.channels = 2;
    spec.freq = SPU_SAMPLE_RATE;
    stream = SDL_OpenAudioDeviceStream(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, &spec, queued ? NULL : OnAudio, NULL);
    if (!stream) {
        PortLog("audio: %s\n", SDL_GetError());
        SDL_QuitSubSystem(SDL_INIT_AUDIO);
        return false;
    }
    SDL_ResumeAudioStreamDevice(stream);
    return true;
}

void PsxAudioDeviceQueue(const int16_t *samples, uint32_t frames)
{
    int queued;

    if (!stream) {
        return;
    }
    queued = SDL_GetAudioStreamQueued(stream) / 4;
    if (queued >= QUEUE_MAX_FRAMES) {
        return; /* far ahead of the device: these are not heard */
    }
    if (queued == 0) {
        /* ran dry (the start, or the game stalled): some silence in hand first */
        static const int16_t silence[QUEUE_LEAD_FRAMES * 2];

        SDL_PutAudioStreamData(stream, silence, sizeof(silence));
    }
    SDL_PutAudioStreamData(stream, samples, (int)(frames * 4));
}

/* The frame recorder's freeze: the device stops asking for samples, so the SPU and the CD's music stop where
   they are and go on from there. */
void PsxAudioDevicePause(bool paused)
{
    if (stream) {
        if (paused) {
            SDL_PauseAudioStreamDevice(stream);
        } else {
            SDL_ResumeAudioStreamDevice(stream);
        }
    }
}

void PsxAudioDeviceClose(void)
{
    if (stream) {
        SDL_DestroyAudioStream(stream);
        stream = NULL;
        SDL_QuitSubSystem(SDL_INIT_AUDIO);
    }
}
