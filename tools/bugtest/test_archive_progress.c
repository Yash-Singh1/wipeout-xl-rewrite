/* Observe a solid block while it is decoded, before its first file can be written. */
#include "../../platform/discimage.c"
#include <assert.h>

static SDL_AtomicInt done;
static const char *input, *output;
static char error[512];
static int result;
static int SDLCALL Extract(void *unused)
{
    result = Extract7z(input, output, error, sizeof(error));
    SDL_SetAtomicInt(&done, 1);
    return 0;
}

int main(int argc, char **argv)
{
    assert(argc == 3);
    input = argv[1]; output = argv[2];
    SDL_Thread *thread = SDL_CreateThread(Extract, "extract-test", NULL);
    int last = 0, updates = 0, duringDecode = 0;
    assert(thread);
    while (!SDL_GetAtomicInt(&done)) {
        int current = PortArchiveProgress();
        assert(current >= last && current <= 999);
        if (current != last) updates++;
        if (current > 0 && current < 900) duringDecode = 1;
        last = current;
        SDL_Delay(1);
    }
    SDL_WaitThread(thread, NULL);
    if (!result) fprintf(stderr, "%s\n", error);
    assert(result && PortArchiveProgress() >= 990);
    assert(updates > 1 && duringDecode);
    printf("Solid 7z extraction reports progress during decoding, %d updates: OK\n", updates);
}
