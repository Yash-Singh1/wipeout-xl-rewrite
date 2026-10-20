#include "functions.h"
#include "animover.h"

extern ConfigData *gameData;
extern u_char sfxMode;
extern u_long *OT[];
extern u_long *OT2[];
extern HeapBlock *mainheapcopy;
extern HeapBlock *reserveHeap;
extern int quadIndex;
extern DB doubleBuffer[];
extern CdlLOC cdTrackLocs[];
extern short CurrentScreen;

void AsmDrawObjectLit(Object *object, Skeleton *camera, int roll, CVECTOR *colour);


/* Port: LoadProg is gone.  It loaded a PS-X EXE from the disc into console RAM (zeroing RAM below
   0x801F7000 first); nothing called it, and anim() below now hands the movie to the platform. */
void anim(char *fileName)
{
    RECT move;
    RECT clear;

    InitDynamicMem(reserveHeap, RESERVE_HEAP_SIZE);
    move.x = 0x140;
    move.y = 0x100;
    move.w = 0xA0;
    move.h = 0x100;
    SfxUnInitReverb();
    SfxUnInit();
    MoveImage(&move, 0x1E0, 0);
    DrawSync(0);
    /* Port: the original loaded the movie player (a PS-X EXE named by fileName) into
       console RAM here and started it with Exec below; the platform plays the movie instead. */
    InitDynamicMem(reserveHeap, RESERVE_HEAP_SIZE);
    SpuQuit();
    close_iso_events();
    SetDispMask(0);
    StopCallback();
    ResetGraph(3);
    StopPAD();
    EnterCriticalSection();
    PsxPlayMovie(fileName); /* Port: replaces Exec of the player executable */
    ExitCriticalSection();
    InitDynamicMem(reserveHeap, RESERVE_HEAP_SIZE);
    CdInit(); /* Port: the original also passed (int)cdTrackLocs and 0, which CdInit does not read */
    iso_init(mainheapcopy);
    InitController();
    SpuStart();
    SetDispMask(0);
    clear.w = 0x140;
    clear.x = 0;
    clear.y = 0;
    clear.h = 0x200;
    ClearImage(&clear, 0, 0, 0);
    DrawSync(0);
    InitSystem();
    move.x = 0x1E0;
    move.w = 0xA0;
    move.y = 0;
    move.h = 0x100;
    MoveImage(&move, 0x140, 0x100);
    DrawSync(0);
    SetDispMask(1);
    ShowLoadMessage();
    InitSpuSound();
    InitSpuSamples();
    ClearOTagR(OT[0], 0x2000);
    ClearOTagR(OT2[CurrentScreen], 0x640);
    CurrentScreen = 0;
    quadIndex = 0;
    PutDrawEnv(doubleBuffer[0].draw);
    DrawOTag((u_long *)((char *)OT[0] + 0x7FFC));
    DrawSync(0);
    PutDrawEnv(doubleBuffer[1].draw);
    DrawOTag((u_long *)((char *)OT[0] + 0x7FFC));
    DrawSync(0);
    if (gameData->soundMode != 0) {
        sfxMode = 1;
    } else {
        sfxMode = 4;
    }
    TerminateSound();
    SfxSetMode(sfxMode);
    StartControlledSound();
    SetSoundVolume(((gameData->effectVolume * 0xFF) / 100) & 0xFF);
    SetCdVolume(((gameData->musicVolume * 0xFF) / 100) & 0xFF);
}
