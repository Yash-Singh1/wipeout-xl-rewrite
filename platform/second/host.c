/* The second player's game, from the first game's side (second.h): its thread, its hidden window for drawing, and
   its variables put back as they were when the program started before each new start, as a new console's would be.
   Compiled once, as part of the first game. */
#include <SDL3/SDL.h>
#include "second/second.h"

/* The second game's variables, all in sections of their own (sections.h): where those begin and end. */
#if defined(__APPLE__)
extern char secondDataStart[] __asm("section$start$__DATA$__p2data");
extern char secondDataEnd[] __asm("section$end$__DATA$__p2data");
extern char secondBssStart[] __asm("section$start$__DATA$__p2bss");
extern char secondBssEnd[] __asm("section$end$__DATA$__p2bss");
#elif defined(_WIN32) && defined(__clang__)
/* (the linker joins p2data$a, $b and $c in that order: the second game's are $b) */
__attribute__((section("p2data$a"))) char secondDataStart[1] = {1};
__attribute__((section("p2data$c"))) char secondDataEnd[1] = {1};
__attribute__((section("p2bss$a"))) char secondBssStart[1];
__attribute__((section("p2bss$c"))) char secondBssEnd[1];
#elif defined(__clang__)
/* (the linker's for sections named as C names) */
extern char __start_p2data[], __stop_p2data[], __start_p2bss[], __stop_p2bss[];
#define secondDataStart __start_p2data
#define secondDataEnd __stop_p2data
#define secondBssStart __start_p2bss
#define secondBssEnd __stop_p2bss
#else
/* (GCC: second.ld's, where the second game's objects are linked into one) */
extern char secondDataStart[], secondDataEnd[], secondBssStart[], secondBssEnd[];
#endif

static void *shared;
static SDL_Window *window;
static SDL_GLContext context;
static SDL_Thread *thread;
static SDL_AtomicInt ended;
static int argCount;
static char argText[16][1100];
static char *args[17];
static void *dataAtStart; /* the second game's initialised variables as the program started */

/* (byte by byte, not checked by AddressSanitizer, and not made memcpy() or memset() by the compiler, which it would
   check: the sections hold the guard zones it keeps between variables) */
#if defined(__clang__) || defined(__GNUC__)
#define UNCHECKED __attribute__((no_sanitize_address))
#else
#define UNCHECKED
#endif

UNCHECKED static void Copy(volatile char *to, const char *from, size_t size)
{
    size_t i;

    for (i = 0; i < size; i++) {
        to[i] = from[i];
    }
}

UNCHECKED static void Zero(volatile char *to, size_t size)
{
    size_t i;

    for (i = 0; i < size; i++) {
        to[i] = 0;
    }
}

/* As the program started: the second game's variables as they were then (copied before its first start), its
   zeroed ones zero. */
static int FreshVariables(void)
{
    /* (as addresses: the marks are different objects to the compiler) */
    uintptr_t dataStart = (uintptr_t)secondDataStart, dataEnd = (uintptr_t)secondDataEnd;
    uintptr_t bssStart = (uintptr_t)secondBssStart, bssEnd = (uintptr_t)secondBssEnd;

    if (dataEnd <= dataStart || bssEnd < bssStart) {
        SDL_Log("second game: its variables are not where they should be");
        return 0;
    }
    if (!dataAtStart) {
        if (!(dataAtStart = SDL_malloc(dataEnd - dataStart))) {
            return 0;
        }
        Copy(dataAtStart, (const char *)dataStart, dataEnd - dataStart);
        return 1; /* (it has never run: all as at the start) */
    }
    Copy((char *)dataStart, dataAtStart, dataEnd - dataStart);
    Zero((char *)bssStart, bssEnd - bssStart);
    return 1;
}

static int Run(void *unused)
{
    (void)unused;
    PortSecondGameMain(argCount, args);
    SDL_GL_MakeCurrent(window, NULL);
    SDL_SetAtomicInt(&ended, 1);
    return 0;
}

int PortSecondGameStart(void *memory, int argc, const char **argv)
{
    SDL_Window *firstWindow = SDL_GL_GetCurrentWindow();
    SDL_GLContext firstContext = SDL_GL_GetCurrentContext();
    SDL_PropertiesID props;
    int i;

    if (thread) {
        if (!SDL_GetAtomicInt(&ended)) {
            SDL_SetError("the second player's game is still running");
            return 0;
        }
        PortSecondGameStop(0);
    }
    if (argc > (int)SDL_arraysize(argText) || !FreshVariables()) {
        SDL_SetError("the second player's game cannot start again");
        return 0;
    }
    for (i = 0; i < argc; i++) {
        SDL_strlcpy(argText[i], argv[i], sizeof(argText[i]));
        args[i] = argText[i];
    }
    args[argc] = NULL;
    argCount = argc;
    shared = memory;
    /* its window, never shown, only for its drawing context (made here: windows are the main thread's) */
    window = SDL_CreateWindow("Wipeout XL: the second player", 640, 480, SDL_WINDOW_OPENGL | SDL_WINDOW_HIDDEN);
    context = window ? SDL_GL_CreateContext(window) : NULL;
    if (firstWindow && firstContext) {
        SDL_GL_MakeCurrent(firstWindow, firstContext);
    }
    if (!context) {
        if (window) {
            SDL_DestroyWindow(window);
            window = NULL;
        }
        return 0;
    }
    SDL_SetAtomicInt(&ended, 0);
    props = SDL_CreateProperties();
    SDL_SetPointerProperty(props, SDL_PROP_THREAD_CREATE_ENTRY_FUNCTION_POINTER, (void *)Run);
    SDL_SetStringProperty(props, SDL_PROP_THREAD_CREATE_NAME_STRING, "second player");
    SDL_SetNumberProperty(props, SDL_PROP_THREAD_CREATE_STACKSIZE_NUMBER, 16 * 1024 * 1024);
    thread = SDL_CreateThreadWithProperties(props);
    SDL_DestroyProperties(props);
    if (!thread) {
        SDL_GL_DestroyContext(context);
        SDL_DestroyWindow(window);
        context = NULL;
        window = NULL;
        return 0;
    }
    return 1;
}

int PortSecondGameRunning(void)
{
    return thread && !SDL_GetAtomicInt(&ended);
}

int PortSecondGameStop(int milliseconds)
{
    uint64_t start = SDL_GetTicks();

    if (!thread) {
        return 1;
    }
    while (!SDL_GetAtomicInt(&ended)) {
        if (SDL_GetTicks() - start >= (uint64_t)milliseconds) {
            return 0; /* (a thread cannot be stopped from outside: it is left to end, and waited for at the next start) */
        }
        SDL_Delay(5);
    }
    SDL_WaitThread(thread, NULL);
    thread = NULL;
    SDL_GL_DestroyContext(context);
    SDL_DestroyWindow(window);
    context = NULL;
    window = NULL;
    shared = NULL;
    return 1;
}

void *PortSecondGameShared(void)
{
    return shared;
}

SDL_Window *PortSecondGameWindow(void)
{
    return window;
}

SDL_GLContext PortSecondGameContext(void)
{
    return context;
}
