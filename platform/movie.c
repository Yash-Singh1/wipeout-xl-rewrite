/* The intro and ending movies.

   On the console the game loads a movie-player program (NTSCANIM.EXE for the
   intro, XTRO1-4.EXE for the endings) and runs it; the player streams the
   movie from the disc and stops at its end or when Start or Cross is newly
   pressed. The port plays the same movies from the PC release's MPEG files
   (the same footage, encoded at a higher quality), with the same way to
   skip them. */
#include <SDL3/SDL.h>
#include <string.h>

#include "platform.h"
#include "psx/gpu.h"

#define PL_MPEG_IMPLEMENTATION
#include "third_party/pl_mpeg.h"

/* The pad buttons held now, in the console's bit layout (platform/psx). */
uint16_t PsxPadButtons(int port);
#define PAD_START 0x0008
#define PAD_CROSS 0x4000

typedef struct Movie {
    uint8_t *rgb;
    int width, height;
    int newFrame;
    SDL_AudioStream *audio;
} Movie;

static void OnVideo(plm_t *mpeg, plm_frame_t *frame, void *user)
{
    Movie *movie = user;

    (void)mpeg;
    plm_frame_to_rgb(frame, movie->rgb, movie->width * 3);
    movie->newFrame = 1;
}

static void OnAudio(plm_t *mpeg, plm_samples_t *samples, void *user)
{
    Movie *movie = user;

    (void)mpeg;
    if (movie->audio) {
        SDL_PutAudioStreamData(movie->audio, samples->interleaved, (int)(samples->count * 2 * sizeof(float)));
    }
}

/* The PC release's file for the console's movie program. */
static const char *MovieFile(const char *program)
{
    const char *name = strrchr(program, '\\');

    name = name ? name + 1 : program;
    if (!SDL_strcasecmp(name, "ntscanim.exe")) {
        return "WXLINTR.MPG";
    }
    if (!SDL_strcasecmp(name, "xtro1.exe")) {
        return "XTRO1.MPG";
    }
    if (!SDL_strcasecmp(name, "xtro2.exe")) {
        return "XTRO2.MPG";
    }
    if (!SDL_strcasecmp(name, "xtro3.exe")) {
        return "XTRO3.MPG";
    }
    if (!SDL_strcasecmp(name, "xtro4.exe")) {
        return "XTRO4.MPG";
    }
    return NULL;
}

static FILE *OpenMovie(const char *file)
{
    char path[1024];
    const char *dirs[] = {"MPEG", "mpeg"};
    int i;
    FILE *f;

    if (!portConfig.pcDataPath) {
        return NULL;
    }
    for (i = 0; i < 2; i++) {
        SDL_snprintf(path, sizeof(path), "%s/%s/%s", portConfig.pcDataPath, dirs[i], file);
        f = fopen(path, "rb");
        if (f) {
            return f;
        }
    }
    return NULL;
}

void PsxPlayMovie(const char *program)
{
    const char *file = MovieFile(program);
    FILE *stream = file ? OpenMovie(file) : NULL;
    plm_t *mpeg;
    Movie movie;
    uint64_t start;
    uint16_t previous;
    double played = 0.0;

    if (!stream) {
        PortLog("movie %s: no MPEG file (pass --pc-data); skipped\n", program);
        return;
    }
    mpeg = plm_create_with_file(stream, 1);
    if (!mpeg) {
        return;
    }
    memset(&movie, 0, sizeof(movie));
    movie.width = plm_get_width(mpeg);
    movie.height = plm_get_height(mpeg);
    movie.rgb = SDL_calloc(1, (size_t)movie.width * movie.height * 3);
    plm_set_video_decode_callback(mpeg, OnVideo, &movie);
    plm_set_loop(mpeg, 0);
    if (!portConfig.muted && plm_get_num_audio_streams(mpeg) > 0) {
        SDL_AudioSpec spec;
        spec.format = SDL_AUDIO_F32;
        spec.channels = 2;
        spec.freq = plm_get_samplerate(mpeg);
        movie.audio = SDL_OpenAudioDeviceStream(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, &spec, NULL, NULL);
        if (movie.audio) {
            plm_set_audio_enabled(mpeg, 1);
            plm_set_audio_decode_callback(mpeg, OnAudio, &movie);
            plm_set_audio_lead_time(mpeg, 0.1);
            SDL_ResumeAudioStreamDevice(movie.audio);
        }
    } else {
        plm_set_audio_enabled(mpeg, 0);
    }

    previous = PsxPadButtons(0);
    start = SDL_GetTicksNS();
    while (!plm_has_ended(mpeg)) {
        uint16_t buttons, pressed;
        double now;

        if (!PortPumpEvents()) {
            exit(0);
        }
        buttons = PsxPadButtons(0);
        pressed = buttons & (uint16_t)~previous;
        previous = buttons;
        if (pressed & (PAD_START | PAD_CROSS)) {
            break;
        }
        /* real time, or one movie frame per step in the fixed-step test mode */
        now = portConfig.fixedStep ? played + 1.0 / plm_get_framerate(mpeg)
                                   : (SDL_GetTicksNS() - start) / 1e9;
        plm_decode(mpeg, now - played);
        played = now;
        if (movie.newFrame) {
            GpuShowImage(movie.rgb, movie.width, movie.height);
            movie.newFrame = 0;
        } else if (!portConfig.fixedStep) {
            SDL_Delay(2);
        }
    }
    if (movie.audio) {
        SDL_DestroyAudioStream(movie.audio);
    }
    SDL_free(movie.rgb);
    plm_destroy(mpeg);
}
