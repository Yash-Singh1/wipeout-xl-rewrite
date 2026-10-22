/* The player's own music: a folder of music files (MP3, FLAC, Ogg Vorbis, WAV, in the folder and those in it)
   heard in place of the disc's soundtrack. The game plays its music as the disc's audio tracks (psx/cdrom.c);
   while it does, each sample comes from here instead: each time the game starts another of the disc's tracks
   (the menus' music, a race's) the next song plays, from where it begins, and when a song ends the one after it.
   The game still runs the drive as it did (pausing it pauses the song, its music volume is the song's): only
   what is heard changes. The songs are decoded as they play, a little at a time, to the drive's 44.1 kHz stereo
   (miniaudio, platform/third_party/miniaudio, public domain or MIT-0; Ogg Vorbis with stb_vorbis, public
   domain). */
#include <SDL3/SDL.h>
#include <stdlib.h>
#include <string.h>

#include "music.h"
#include "platform.h"
#include "settings.h"
#include "psx/cdrom.h"
#include "psx/psx_io.h"

#define MA_NO_DEVICE_IO
#define MA_NO_ENCODING
#define MA_NO_GENERATION
#define MA_NO_ENGINE
#define MA_NO_NODE_GRAPH
#define MA_NO_RESOURCE_MANAGER
#define MA_NO_THREADING
#define STB_VORBIS_HEADER_ONLY
#include "third_party/stb/stb_vorbis.c"
#define MINIAUDIO_IMPLEMENTATION
#include "third_party/miniaudio/miniaudio.h"

static char folder[1024];
static char **songs;            /* the files, by path */
static int *order;              /* the order they play in */
static int songCount;

/* The playing (the audio thread's, under the audio lock). */
#define CHUNK 2048
static ma_decoder decoder;
static int decoderOpen;
static int playing = -1;        /* the place in order[] */
static int discTrack = -1;      /* the disc's track the game plays */
static int failed;              /* no song could be read: the disc's music again until the next folder */
static int skipSong;
static int16_t chunk[CHUNK * 2];
static int chunkFrames, chunkAt;

static int Wanted(const char *name)
{
    static const char *const kinds[] = {".mp3", ".flac", ".ogg", ".oga", ".wav", ".wave"};
    const char *dot = strrchr(name, '.');
    size_t i;

    for (i = 0; dot && i < SDL_arraysize(kinds); i++) {
        if (!SDL_strcasecmp(dot, kinds[i])) {
            return 1;
        }
    }
    return 0;
}

static char **found;
static int foundCount, foundCapacity;

static SDL_EnumerationResult SDLCALL Scan(void *user, const char *dir, const char *name)
{
    char path[2048];
    SDL_PathInfo info;

    (void)user;
    if (name[0] == '.') {
        return SDL_ENUM_CONTINUE;
    }
    SDL_snprintf(path, sizeof(path), "%s%s", dir, name);
    if (!SDL_GetPathInfo(path, &info)) {
        return SDL_ENUM_CONTINUE;
    }
    if (info.type == SDL_PATHTYPE_DIRECTORY) {
        SDL_EnumerateDirectory(path, Scan, NULL);
    } else if (Wanted(name)) {
        if (foundCount == foundCapacity) {
            foundCapacity = foundCapacity ? foundCapacity * 2 : 64;
            found = SDL_realloc(found, sizeof(char *) * (size_t)foundCapacity);
        }
        found[foundCount++] = SDL_strdup(path);
    }
    return SDL_ENUM_CONTINUE;
}

static int ComparePaths(const void *a, const void *b)
{
    return SDL_strcasecmp(*(char *const *)a, *(char *const *)b);
}

/* The order the songs play in: as the folder sorts them, or shuffled. */
static void Order(int *list, int count)
{
    int i;

    for (i = 0; i < count; i++) {
        list[i] = i;
    }
    for (i = count - 1; portSettings.musicShuffle && i > 0; i--) {
        int j = SDL_rand(i + 1), t = list[i];

        list[i] = list[j];
        list[j] = t;
    }
}

static void CloseSong(void)
{
    if (decoderOpen) {
        ma_decoder_uninit(&decoder);
        decoderOpen = 0;
    }
    chunkFrames = chunkAt = 0;
}

/* The next song that opens (none: failed). */
static void NextSong(void)
{
    ma_decoder_config config = ma_decoder_config_init(ma_format_s16, 2, 44100);
    int tries;

    CloseSong();
    for (tries = 0; tries < songCount; tries++) {
        playing = (playing + 1) % songCount;
        if (ma_decoder_init_file(songs[order[playing]], &config, &decoder) == MA_SUCCESS) {
            decoderOpen = 1;
            return;
        }
    }
    failed = 1;
}

/* The next samples of the song, or the next song's when it has ended. */
static int Fill(void)
{
    int tries;

    for (tries = 0; tries <= songCount && !failed; tries++) {
        ma_uint64 got = 0;

        if (decoderOpen && ma_decoder_read_pcm_frames(&decoder, chunk, CHUNK, &got) == MA_SUCCESS && got > 0) {
            chunkFrames = (int)got;
            chunkAt = 0;
            return 1;
        }
        NextSong();
    }
    failed = 1;
    return 0;
}

/* The drive's sample of the disc's track `track` (psx/cdrom.c, under the audio lock). */
static bool Replace(int track, int16_t out[2])
{
    if (!portSettings.customMusic || !songCount || failed) {
        return false;
    }
    if (skipSong || track != discTrack) {
        skipSong = 0;
        discTrack = track;
        NextSong();
    }
    if (chunkAt >= chunkFrames && !Fill()) {
        return false;
    }
    out[0] = chunk[chunkAt * 2];
    out[1] = chunk[chunkAt * 2 + 1];
    chunkAt++;
    return true;
}

int PortMusicOpen(const char *path, char *message, size_t messageSize)
{
    char **oldSongs;
    int *newOrder, oldCount, i;

    found = NULL;
    foundCount = foundCapacity = 0;
    if (path && path[0]) {
        SDL_PathInfo info;

        if (!SDL_GetPathInfo(path, &info) || info.type != SDL_PATHTYPE_DIRECTORY) {
            if (message) {
                SDL_snprintf(message, messageSize, "%s is not a folder.", path);
            }
            return 0;
        }
        SDL_EnumerateDirectory(path, Scan, NULL);
        if (!foundCount) {
            if (message) {
                SDL_snprintf(message, messageSize, "No music files there (MP3, FLAC, Ogg Vorbis or WAV).");
            }
            SDL_free(found);
            return 0;
        }
        SDL_qsort(found, (size_t)foundCount, sizeof(char *), ComparePaths);
    }
    newOrder = SDL_malloc(sizeof(int) * (size_t)(foundCount ? foundCount : 1));
    Order(newOrder, foundCount);
    PsxLock();
    CloseSong();
    oldSongs = songs;
    oldCount = songCount;
    SDL_free(order);
    songs = found;
    songCount = foundCount;
    order = newOrder;
    playing = discTrack = -1;
    failed = skipSong = 0;
    SDL_strlcpy(folder, path ? path : "", sizeof(folder));
    PsxUnlock();
    for (i = 0; i < oldCount; i++) {
        SDL_free(oldSongs[i]);
    }
    SDL_free(oldSongs);
    if (message) {
        SDL_snprintf(message, messageSize, "%d songs.", songCount);
    }
    return songCount;
}

void PortMusicInit(void)
{
    const char *path = SDL_getenv("WIPEOUT_MUSIC");

    if (path) {
        /* (tests, whose hidden runs read no settings) */
        SDL_strlcpy(portSettings.musicFolder, path, sizeof(portSettings.musicFolder));
        portSettings.customMusic = path[0] != 0;
    }
    if (portSettings.musicFolder[0]) {
        PortMusicOpen(portSettings.musicFolder, NULL, 0);
    }
    CdMusicReplacement = Replace;
}

const char *PortMusicFolder(void)
{
    return folder;
}

int PortMusicCount(void)
{
    return songCount;
}

const char *PortMusicPlaying(void)
{
    static char name[256];
    const char *base;

    name[0] = 0;
    PsxLock();
    if (portSettings.customMusic && decoderOpen && playing >= 0 && playing < songCount) {
        base = SDL_strrchr(songs[order[playing]], '/');
#ifdef _WIN32
        if (SDL_strrchr(songs[order[playing]], '\\') > base) {
            base = SDL_strrchr(songs[order[playing]], '\\');
        }
#endif
        SDL_strlcpy(name, base ? base + 1 : songs[order[playing]], sizeof(name));
    }
    PsxUnlock();
    return name;
}

void PortMusicSkip(void)
{
    PsxLock();
    skipSong = 1;
    PsxUnlock();
}

void PortMusicSettingsChanged(void)
{
    PsxLock();
    Order(order, songCount);
    playing = -1;
    discTrack = -1; /* (the next sample starts a song afresh) */
    failed = 0;
    CloseSong();
    PsxUnlock();
}

/* stb_vorbis's implementation, for miniaudio's Ogg Vorbis (last: its short macro names stay here) */
#undef STB_VORBIS_HEADER_ONLY
#include "third_party/stb/stb_vorbis.c"
