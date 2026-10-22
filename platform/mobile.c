/* Phones and tablets (mobile.h): where the player's disc image comes in. iOS shows the app's Documents folder in
   the Files app ("On My iPhone", Wipeout XL; Info.plist), Android has the app's folder on the device's storage
   (Android/data/<app>/files, reached from a computer over USB); the game looks there for a disc image. Android's
   file picker hands over documents as content:// addresses, which SDL reads but which have no folder of their own:
   a picked file is copied into the app's folder under its own name (asked of Android: WipeoutActivity.java). */
#include <SDL3/SDL.h>
#include <string.h>

#include "discimage.h"
#include "mobile.h"
#include "platform.h"

#ifdef SDL_PLATFORM_ANDROID
#include <jni.h>
#endif

static SDL_AtomicInt importProgress; /* thousandths */

const char *PortImportFolder(void)
{
    static char folder[1024];

    if (!folder[0]) {
#if defined(SDL_PLATFORM_IOS)
        const char *documents = SDL_GetUserFolder(SDL_FOLDER_DOCUMENTS);

        if (documents) {
            SDL_strlcpy(folder, documents, sizeof(folder));
        }
#elif defined(SDL_PLATFORM_ANDROID)
        const char *storage = SDL_GetAndroidExternalStoragePath();

        if (storage) {
            SDL_snprintf(folder, sizeof(folder), "%s/", storage);
        }
#endif
        if (folder[0]) {
            SDL_CreateDirectory(folder);
        }
    }
    return folder[0] ? folder : NULL;
}

/* ---- a disc image in the folder ---- */

typedef struct {
    char best[1024];
    int rank, depth; /* the best's: 3 a .cue, 2 an archive, 1 a .bin or .img */
} Search;

static int Rank(const char *name)
{
    size_t length = strlen(name);
    const char *extension = length > 4 ? name + length - 4 : "";

    if (!PortIsDiscName(name)) {
        return 0;
    }
    if (!SDL_strcasecmp(extension, ".cue")) {
        return 3;
    }
    return !SDL_strcasecmp(extension, ".bin") || !SDL_strcasecmp(extension, ".img") ? 1 : 2;
}

static SDL_EnumerationResult SDLCALL SearchEntry(void *user, const char *dir, const char *name)
{
    Search *s = (Search *)user;
    char path[2048];
    SDL_PathInfo info;
    int rank;

    if (name[0] == '.') {
        return SDL_ENUM_CONTINUE;
    }
    SDL_snprintf(path, sizeof(path), "%s%s", dir, name);
    if (!SDL_GetPathInfo(path, &info)) {
        return SDL_ENUM_CONTINUE;
    }
    if (info.type == SDL_PATHTYPE_DIRECTORY) {
        if (s->depth < 3) {
            SDL_strlcat(path, "/", sizeof(path));
            s->depth++;
            SDL_EnumerateDirectory(path, SearchEntry, s);
            s->depth--;
        }
    } else if ((rank = Rank(name)) > s->rank) {
        SDL_strlcpy(s->best, path, sizeof(s->best));
        s->rank = rank;
    }
    return SDL_ENUM_CONTINUE;
}

int PortFindDisc(char *out, size_t size)
{
    const char *folder = PortImportFolder();
    Search search;

    if (!folder) {
        return 0;
    }
    memset(&search, 0, sizeof(search));
    SDL_EnumerateDirectory(folder, SearchEntry, &search);
    if (!search.rank) {
        return 0;
    }
    SDL_strlcpy(out, search.best, size);
    return 1;
}

/* ---- Android: a picked document copied in ---- */

int PortImportProgress(void)
{
    return SDL_GetAtomicInt(&importProgress);
}

#ifdef SDL_PLATFORM_ANDROID
/* The document's name, as Android tells it (WipeoutActivity.displayName). */
static int DisplayName(const char *uri, char *out, size_t size)
{
    JNIEnv *env = (JNIEnv *)SDL_GetAndroidJNIEnv();
    jobject activity = (jobject)SDL_GetAndroidActivity();
    jclass type;
    jmethodID method;
    jstring argument, result;
    int ok = 0;

    if (!env || !activity) {
        return 0;
    }
    type = (*env)->GetObjectClass(env, activity);
    method = (*env)->GetMethodID(env, type, "displayName", "(Ljava/lang/String;)Ljava/lang/String;");
    if (method) {
        argument = (*env)->NewStringUTF(env, uri);
        result = (jstring)(*env)->CallObjectMethod(env, activity, method, argument);
        if (result) {
            const char *name = (*env)->GetStringUTFChars(env, result, NULL);

            if (name && name[0] && !strchr(name, '/')) {
                SDL_strlcpy(out, name, size);
                ok = 1;
            }
            (*env)->ReleaseStringUTFChars(env, result, name);
            (*env)->DeleteLocalRef(env, result);
        }
        (*env)->DeleteLocalRef(env, argument);
    }
    if ((*env)->ExceptionCheck(env)) {
        (*env)->ExceptionClear(env);
    }
    (*env)->DeleteLocalRef(env, type);
    (*env)->DeleteLocalRef(env, activity);
    return ok;
}
#endif

int PortImportFile(const char *uri, char *out, size_t outSize, char *message, size_t messageSize)
{
    const char *folder = PortImportFolder();
    char name[512];
    SDL_IOStream *in, *copy;
    Sint64 total, done = 0;
    static char buffer[1 << 20];
    size_t got;
    int ok = 1;

    if (strncmp(uri, "content://", 10)) {
        SDL_strlcpy(out, uri, outSize); /* a file of its own: as it is */
        return 1;
    }
#ifdef SDL_PLATFORM_ANDROID
    if (!DisplayName(uri, name, sizeof(name)))
#endif
    {
        const char *slash = SDL_strrchr(uri, '/');

        SDL_snprintf(name, sizeof(name), "%s", slash ? slash + 1 : "disc");
    }
    if (!folder) {
        SDL_snprintf(message, messageSize, "There is no folder to copy %s into.", name);
        return 0;
    }
    SDL_snprintf(out, outSize, "%s%s", folder, name);
    in = SDL_IOFromFile(uri, "rb");
    if (!in) {
        SDL_snprintf(message, messageSize, "%s could not be read: %s", name, SDL_GetError());
        return 0;
    }
    copy = SDL_IOFromFile(out, "wb");
    if (!copy) {
        SDL_snprintf(message, messageSize, "%s could not be written: %s", out, SDL_GetError());
        SDL_CloseIO(in);
        return 0;
    }
    total = SDL_GetIOSize(in);
    SDL_SetAtomicInt(&importProgress, 0);
    while ((got = SDL_ReadIO(in, buffer, sizeof(buffer))) > 0) {
        if (SDL_WriteIO(copy, buffer, got) != got) {
            SDL_snprintf(message, messageSize, "%s could not be written (is the storage full?).", out);
            ok = 0;
            break;
        }
        done += (Sint64)got;
        if (total > 0) {
            SDL_SetAtomicInt(&importProgress, (int)(done * 1000 / total));
        }
    }
    SDL_CloseIO(in);
    if (!SDL_CloseIO(copy)) {
        ok = 0;
    }
    if (!ok) {
        SDL_RemovePath(out);
    }
    SDL_SetAtomicInt(&importProgress, 1000);
    return ok;
}
