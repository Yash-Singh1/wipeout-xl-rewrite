/* The PC CD-ROM version's movies (MPEG\WXLINTR.MPG, XTRO1-4.MPG; docs/PC.md), played where the PlayStation's are
   (movie.c).

   Each file is an MPEG-1 video stream with nothing around it, decoded with pl_mpeg. The sound is in the stream too:
   the PC's player takes it from the user data that follows each picture header (00 00 01 B2, then BE EF and
   blocks of a 16-bit length and that many bytes, up to the next start code). The blocks joined are frames of 18
   bytes, each 16 stereo samples at 44100 Hz of a differential code: a step for each channel, then eight bytes
   for each, two 4-bit codes apiece. A code picks one of 16 evenly spaced fractions of the step's amplitude (the
   `fractions` below, the PC's pan table), which is added to the channel's running sum. */
#include <SDL3/SDL.h>
#include <math.h>
#include <string.h>

#define PL_MPEG_IMPLEMENTATION
#include "third_party/pl_mpeg.h"

#include "pc.h"
#include "platform.h"
#include "psx/gpu.h"
#include "psx/psx_io.h"

#define PAD_START 0x0008
#define PAD_CROSS 0x4000

#define AUDIO_RATE 44100
#define AUDIO_FRAME_BYTES 18
#define AUDIO_FRAME_SAMPLES 16

static const float fractions[16] = {1.0f,   0.866f,  0.733f,  0.6f,    0.466f,  0.333f,  0.2f,    0.066f,
                                    -0.066f, -0.2f,  -0.333f, -0.466f, -0.6f,   -0.733f, -0.866f, -1.0f};

/* The sound of the file: the user data's blocks joined. */
static uint8_t *ExtractSound(const uint8_t *data, size_t size, size_t *length)
{
    uint8_t *sound = SDL_malloc(size / 4 + 1);
    size_t at = 0, used = 0;

    while (at + 6 < size) {
        const uint8_t *p = data + at;
        size_t next;

        if (p[0] || p[1] || p[2] != 1 || p[3] != 0xB2 || p[4] != 0xBE || p[5] != 0xEF) {
            at++;
            continue;
        }
        next = at + 6;
        while (next + 2 <= size && !(next + 3 <= size && !data[next] && !data[next + 1] && data[next + 2] == 1)) {
            size_t block = data[next] | data[next + 1] << 8;

            if (next + 2 + block > size || used + block > size / 4) {
                break;
            }
            memcpy(sound + used, data + next + 2, block);
            used += block;
            next += 2 + block;
        }
        at = next;
    }
    *length = used;
    return sound;
}

/* The differential code to 16-bit stereo. */
static int16_t *DecodeSound(const uint8_t *sound, size_t length, size_t *frames)
{
    size_t count = length / AUDIO_FRAME_BYTES, i;
    int16_t *out = SDL_malloc((count * AUDIO_FRAME_SAMPLES * 2 + 2) * sizeof(int16_t));
    float amplitude[256];
    int32_t sum[2] = {0, 0};
    int step;

    for (step = 0; step < 256; step++) {
        amplitude[step] = (float)(int)(pow(11.0, -(255.0f - step) / 65.161) * 65535.0);
    }
    for (i = 0; i < count; i++) {
        const uint8_t *frame = sound + i * AUDIO_FRAME_BYTES;
        int16_t *o = out + i * AUDIO_FRAME_SAMPLES * 2;
        int pair;

        for (pair = 0; pair < 8; pair++) {
            int channel, half;

            for (half = 0; half < 2; half++) {
                for (channel = 0; channel < 2; channel++) {
                    int code = frame[2 + pair * 2 + channel];
                    float scale = amplitude[frame[channel]];

                    sum[channel] += (int)(fractions[half ? code & 15 : code >> 4] * scale);
                    *o++ = (int16_t)sum[channel];
                }
            }
        }
    }
    *frames = count * AUDIO_FRAME_SAMPLES;
    return out;
}

int PortPcPlayMovie(const char *path)
{
    size_t size = 0, soundBytes, soundFrames = 0;
    uint8_t *data = PortPcLoadFile(path, &size), *sound, *rgb;
    int16_t *samples = NULL;
    plm_buffer_t *buffer;
    plm_video_t *video;
    SDL_AudioStream *audio = NULL;
    uint64_t start;
    uint16_t previous;
    int width, height, shown = 0;
    double fps;

    if (!data) {
        return 0;
    }
    buffer = plm_buffer_create_with_memory(data, size, 0);
    video = plm_video_create_with_buffer(buffer, 1);
    plm_video_set_no_delay(video, 1);
    width = plm_video_get_width(video);
    height = plm_video_get_height(video);
    fps = plm_video_get_framerate(video);
    if (width <= 0 || height <= 0 || width > 1024 || height > 768 || fps <= 0) {
        plm_video_destroy(video);
        SDL_free(data);
        return 0;
    }
    sound = ExtractSound(data, size, &soundBytes);
    samples = DecodeSound(sound, soundBytes, &soundFrames);
    SDL_free(sound);
    if (!portConfig.muted && !portConfig.fixedStep && soundFrames) {
        SDL_AudioSpec spec;

        spec.format = SDL_AUDIO_S16;
        spec.channels = 2;
        spec.freq = AUDIO_RATE;
        audio = SDL_OpenAudioDeviceStream(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, &spec, NULL, NULL);
        if (audio) {
            SDL_PutAudioStreamData(audio, samples, (int)(soundFrames * 2 * sizeof(int16_t)));
            SDL_FlushAudioStream(audio);
            SDL_ResumeAudioStreamDevice(audio);
        }
    }
    rgb = SDL_malloc((size_t)width * height * 3);
    previous = PsxPadButtons(0);
    PortLog("movie %s (PC) at field %d\n", path, (int)PsxFieldCount());
    start = SDL_GetTicksNS();
    while (!plm_video_has_ended(video)) {
        plm_frame_t *frame;
        uint16_t buttons, pressed;

        if (!PortPumpEvents()) {
            exit(0);
        }
        buttons = PsxPadButtons(0);
        pressed = buttons & (uint16_t)~previous;
        previous = buttons;
        if (pressed & (PAD_START | PAD_CROSS)) {
            break;
        }
        /* a picture is shown at its time (in the sound) */
        while (!portConfig.fixedStep && (SDL_GetTicksNS() - start) / 1e9 < shown / fps) {
            SDL_Delay(2);
        }
        frame = plm_video_decode(video);
        if (!frame) {
            break;
        }
        plm_frame_to_rgb(frame, rgb, width * 3);
        GpuShowImage(rgb, width, height);
        shown++;
    }
    if (audio) {
        SDL_DestroyAudioStream(audio);
    }
    plm_video_destroy(video);
    SDL_free(samples);
    SDL_free(rgb);
    SDL_free(data);
    return 1;
}
