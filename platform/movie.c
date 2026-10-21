/* The intro and ending movies.

   On the console the game loads a movie-player program (NTSCANIM.EXE for the
   intro, XTRO1-4.EXE for the endings) and runs it; the player streams the
   movie from the disc and stops at its end or when Start or Cross is newly
   pressed. The port plays the same movies from the PC release's files, with
   the same way to skip them.

   The PC release's movies (MPEG/WXLINTR.MPG, XTRO1-4.MPG) are MPEG-1 video
   streams, 320x192 at 25 frames a second, that carry their sound in the
   stream's user data: after a user-data start code and the bytes BE EF come
   blocks of a 16-bit little-endian length and that many bytes, up to the next
   start code. The sound is 44.1 kHz stereo delta coding in 18-byte frames: a
   loudness step for the left and for the right channel, then eight pairs of
   bytes (left, right), each byte two 4-bit deltas, high nibble first. A delta
   adds deltaScale[nibble] * loudness(step) to the channel's running sample. */
#include <SDL3/SDL.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "platform.h"
#include "settings.h"
#include "psx/gpu.h"
#include "psx/psx_io.h"

#define PL_MPEG_IMPLEMENTATION
#include "third_party/pl_mpeg.h"

#define PAD_START 0x0008
#define PAD_CROSS 0x4000

#define SOUND_RATE 44100
#define SOUND_FRAME_BYTES 18
#define SOUND_FRAME_SAMPLES 16

static const float deltaScale[16] = {
    1.0f, 0.866f, 0.733f, 0.6f, 0.466f, 0.333f, 0.2f, 0.066f,
    -0.066f, -0.2f, -0.333f, -0.466f, -0.6f, -0.733f, -0.866f, -1.0f};

/* The PC release's file for the console's movie program. */
static const char *MovieFile(const char *program)
{
    static const struct {
        const char *program, *file;
    } movies[] = {{"ntscanim.exe", "WXLINTR.MPG"}, {"xtro1.exe", "XTRO1.MPG"}, {"xtro2.exe", "XTRO2.MPG"},
                  {"xtro3.exe", "XTRO3.MPG"},     {"xtro4.exe", "XTRO4.MPG"}};
    const char *name = strrchr(program, '\\');
    size_t i;

    name = name ? name + 1 : program;
    for (i = 0; i < SDL_arraysize(movies); i++) {
        if (!SDL_strcasecmp(name, movies[i].program)) {
            return movies[i].file;
        }
    }
    return NULL;
}

static uint8_t *LoadMovie(const char *file, size_t *size)
{
    char path[1024];
    const char *dirs[] = {"MPEG", "mpeg"};
    int i;

    if (!portConfig.pcDataPath) {
        return NULL;
    }
    for (i = 0; i < 2; i++) {
        void *data;

        SDL_snprintf(path, sizeof(path), "%s/%s/%s", portConfig.pcDataPath, dirs[i], file);
        data = SDL_LoadFile(path, size);
        if (data) {
            return data;
        }
    }
    return NULL;
}

/* The sound blocks of the stream's user data, joined. */
static uint8_t *ExtractSound(const uint8_t *data, size_t size, size_t *soundSize)
{
    uint8_t *sound = SDL_malloc(size);
    size_t i, n = 0;

    for (i = 0; i + 6 <= size; i++) {
        size_t p;

        if (data[i] || data[i + 1] || data[i + 2] != 1 || data[i + 3] != 0xB2 || data[i + 4] != 0xBE ||
            data[i + 5] != 0xEF) {
            continue;
        }
        p = i + 6;
        while (p + 3 <= size && !(data[p] == 0 && data[p + 1] == 0 && data[p + 2] == 1)) {
            size_t length = data[p] | (size_t)data[p + 1] << 8;

            p += 2;
            if (p + length > size) {
                break;
            }
            memcpy(sound + n, data + p, length);
            n += length;
            p += length;
        }
        i = p - 1;
    }
    *soundSize = n;
    return sound;
}

/* Decodes the delta-coded sound to 16-bit stereo samples. */
static int16_t *DecodeSound(const uint8_t *sound, size_t size, size_t *samples)
{
    size_t frames = size / SOUND_FRAME_BYTES, f, out = 0;
    int16_t *pcm = SDL_malloc(frames * SOUND_FRAME_SAMPLES * 2 * sizeof(int16_t) + 1);
    int loudness[256];
    int left = 0, right = 0, step, k;

    for (step = 0; step < 256; step++) {
        loudness[step] = (int)(pow(11.0, -(255.0f - step) / 65.161) * 65535.0);
    }
    for (f = 0; f < frames; f++) {
        const uint8_t *q = sound + f * SOUND_FRAME_BYTES;
        int leftStep = loudness[q[0]], rightStep = loudness[q[1]];

        q += 2;
        for (k = 0; k < 8; k++, q += 2) {
            left += (int)(deltaScale[q[0] >> 4] * leftStep);
            right += (int)(deltaScale[q[1] >> 4] * rightStep);
            pcm[out++] = (int16_t)left;
            pcm[out++] = (int16_t)right;
            left += (int)(deltaScale[q[0] & 15] * leftStep);
            right += (int)(deltaScale[q[1] & 15] * rightStep);
            pcm[out++] = (int16_t)left;
            pcm[out++] = (int16_t)right;
        }
    }
    *samples = out / 2;
    return pcm;
}

void PsxPlayMovie(const char *program)
{
    const char *file = MovieFile(program);
    uint8_t *data, *rgb;
    size_t size;
    plm_video_t *video;
    plm_frame_t *frame;
    SDL_AudioStream *audio = NULL;
    uint64_t start;
    uint16_t previous;
    double rate;
    int shown = 0;

    if (portConfig.linkName) {
        return; /* the second player's game shows no movies */
    }
    if (portSettings.skipIntro && file && !strcmp(file, "WXLINTR.MPG")) {
        return; /* the settings: no intro */
    }
    data = file ? LoadMovie(file, &size) : NULL;
    if (!data) {
        PortLog("movie %s: no MPEG file (pass --pc-data); skipped\n", program);
        return;
    }
    video = plm_video_create_with_buffer(plm_buffer_create_with_memory(data, size, 0), 1);
    rate = plm_video_get_framerate(video);
    if (!video || rate <= 0) {
        PortLog("movie %s: not an MPEG video stream; skipped\n", program);
        if (video) {
            plm_video_destroy(video);
        }
        SDL_free(data);
        return;
    }
    rgb = SDL_malloc((size_t)plm_video_get_width(video) * plm_video_get_height(video) * 3);
    if (!portConfig.muted && !portConfig.fixedStep) {
        size_t soundSize, samples;
        uint8_t *sound = ExtractSound(data, size, &soundSize);
        int16_t *pcm = DecodeSound(sound, soundSize, &samples);
        SDL_AudioSpec spec;

        spec.format = SDL_AUDIO_S16;
        spec.channels = 2;
        spec.freq = SOUND_RATE;
        audio = SDL_OpenAudioDeviceStream(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, &spec, NULL, NULL);
        if (audio) {
            SDL_PutAudioStreamData(audio, pcm, (int)(samples * 2 * sizeof(int16_t)));
            SDL_FlushAudioStream(audio);
        }
        SDL_free(pcm);
        SDL_free(sound);
    }

    previous = PsxPadButtons(0);
    if (audio) {
        SDL_ResumeAudioStreamDevice(audio);
    }
    start = SDL_GetTicksNS();
    for (;;) {
        uint16_t buttons, pressed;
        int due;

        if (!PortPumpEvents()) {
            exit(0);
        }
        buttons = PsxPadButtons(0);
        pressed = buttons & (uint16_t)~previous;
        previous = buttons;
        if (pressed & (PAD_START | PAD_CROSS)) {
            break;
        }
        /* real time, or one frame per step in the fixed-step test mode */
        due = portConfig.fixedStep ? shown + 1 : (int)((SDL_GetTicksNS() - start) / 1e9 * rate) + 1;
        if (shown >= due) {
            SDL_Delay(2);
            continue;
        }
        frame = NULL;
        while (shown < due) {
            plm_frame_t *next = plm_video_decode(video);
            if (!next) {
                break;
            }
            frame = next;
            shown++;
        }
        if (!frame) {
            break; /* the end of the movie */
        }
        plm_frame_to_rgb(frame, rgb, frame->width * 3);
        GpuShowImage(rgb, frame->width, frame->height);
    }
    if (audio) {
        SDL_DestroyAudioStream(audio);
    }
    SDL_free(rgb);
    plm_video_destroy(video);
    SDL_free(data);
}
