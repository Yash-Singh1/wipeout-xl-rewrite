/* Playback of the player's own disc, with drawing observed at GpuShowImage.
   Exercises the actual player, sector reader, video decoder and XA decoder. */
#include "../../platform/movie.c"
#include <assert.h>
#include <stdarg.h>

PortConfig portConfig;
PortSettings portSettings;
static Disc *disc;
static int european, images, pumps, skip;
static size_t colourFrames;

const Disc *PsxIoDisc(void) { return disc; }
int PortRegionEurope(void) { return european; }
int PortPcData(void) { return 0; }
int PortSaturnDisc(void) { return 0; }
int PortPcPlayMovie(const char *path) { assert(0); return 0; }
int SaturnPlayMovie(const char *path) { assert(0); return 0; }
uint64_t PsxFieldCount(void) { return 0; }
int PortPumpEvents(void) { pumps++; return 1; }
uint16_t PsxPadButtons(int port)
{
    if (skip == 1 && images >= 5) return PAD_START;
    /* Cross held at entry must not skip; release, then press it again. */
    if (skip == 2 && (images < 3 || images >= 5)) return PAD_CROSS;
    return 0;
}
void PortLog(const char *format, ...)
{
    va_list args;
    va_start(args, format);
    vprintf(format, args);
    va_end(args);
}
void GpuShowImage(const uint8_t *rgb, int width, int height)
{
    int varied = 0;
    assert(width == 320 && height == 192);
    for (size_t i = 3; i < (size_t)width * height * 3; i++) varied |= rgb[i] != rgb[i % 3];
    colourFrames += varied != 0;
    images++;
}

/* Frame-count immediates passed to PlayMovie by the disc's own player programs.
   The European ending players' call setup is four bytes earlier than the intro's. */
static void CheckPlayer(int movie)
{
    char path[64];
    int32_t lba;
    uint32_t size;
    uint8_t code[2048];
    int offset = european && movie ? 0x25c : 0x260;
    int frames = european ? movies[movie].europeFrames : movies[movie].frames;

    snprintf(path, sizeof(path), "\\%s", european && !movie ? "palanim.exe" : movies[movie].program);
    assert(DiscFindFile(disc, path, &lba, &size));
    assert(DiscReadData(disc, lba + 4, 1, code));
    assert(code[offset + 2] == 5 && code[offset + 3] == 0x34); /* ori a1, zero, frame count */
    assert((code[offset] | code[offset + 1] << 8) == frames);
}

int main(int argc, char **argv)
{
    char error[512];
    int32_t lba;
    uint32_t size;
    int count, soundStart, nonzero = 0;
    Frame *frames;
    Sound sound = {0};
    const char *program;

    assert(argc == 2);
    disc = DiscOpen(argv[1], error, sizeof(error));
    if (!disc) { fprintf(stderr, "%s\n", error); return 1; }
    european = DiscFindFile(disc, "\\PALANIM.EXE", &lba, &size);
    program = european ? "C:\\palanim.exe" : "C:\\ntscanim.exe";
    for (int i = 0; i < (int)SDL_arraysize(movies); i++) CheckPlayer(i);
    assert(DiscFindFile(disc, "\\MAKE.AV", &lba, &size));
    frames = ScanMovie(disc, lba, size / 2048, &count, &sound, &soundStart);
    assert(count >= (european ? 1703 : 2044));
    assert(sound.frames && sound.rate == 37800 && sound.channels == 2 && soundStart >= 0);
    for (size_t i = 0; i < sound.frames * sound.channels; i++) nonzero |= sound.samples[i] != 0;
    assert(nonzero);
    printf("intro stream: %d frames, %zu audio samples per channel at %d Hz\n", count, sound.frames, sound.rate);
    SDL_free(sound.samples);
    SDL_free(frames);
    portConfig.fixedStep = portConfig.muted = 1;
    SDL_setenv_unsafe("WIPEOUT_MOVIES", "1", 1);
    PsxPlayMovie(program);
    assert(images == (european ? 1701 : 2042));
    assert(colourFrames > 1000);
    images = pumps = 0;
    skip = 1;
    PsxPlayMovie(program);
    assert(images == 5);
    images = pumps = 0;
    skip = 2;
    PsxPlayMovie(program);
    assert(images == 5);
    images = pumps = 0;
    portSettings.skipIntro = 1;
    PsxPlayMovie(program);
    assert(!images && !pumps);
    DiscClose(disc);
    puts("disc movie playback, frame limits, audio and skip controls: OK");
    return 0;
}
