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
#include "droid.h"
extern Header       progHeader[];
extern u_long       progLoadAddr;
extern int         progSize;
Header             *rambase;

void AsmDrawObjectLit(Object *object, Skeleton *camera, int roll, CVECTOR *colour);


int LoadProg(int progIndex)
{
    CdlFILE fileInfo;
    int    mode;
    int    i;
    u_long addr;

    mode = 0x80;
    i = 0;
    do {
        CdSearchFile(&fileInfo, (char *)(intptr_t)progIndex); /* Port: dead code (nothing calls LoadProg); the int is used as the name pointer */
        i += 1;
    } while (i < 0xA);
    if (CdControl(2, (u_char *)&fileInfo.pos, 0) != 0) {
        if (CdRead(1, (u_long *)rambase, mode) != 0) {
            do {
            } while (CdReadSync(1, 0) > 0);
            __builtin_memcpy(progHeader, (void *)rambase, 136);
            CdIntToPos(CdPosToInt(&fileInfo.pos) + 1, &fileInfo.pos);
            addr = progLoadAddr;
            if (addr <= 0x801F6FFFU) {
                do {
                    *(int *)(uintptr_t)addr = 0; /* Port: PlayStation RAM address; LoadProg is dead code */
                    addr += 4;
                } while (addr <= 0x801F6FFFU);
            }
            if (CdControl(2, (u_char *)&fileInfo.pos, 0) != 0) {
                return CdRead((u_long)(progSize + 0x7FF) >> 0xB, (u_long *)(uintptr_t)progLoadAddr, mode);
            }
            goto loadFailed;
        }
        return 0;
    }
loadFailed:
    return 0;
}
void anim(char *fileName)
{
    short rects[28];

    InitDynamicMem(reserveHeap, 0xE1000);
    rects[0] = 0x140;
    rects[1] = 0x100;
    rects[2] = 0xA0;
    rects[3] = 0x100;
    SfxUnInitReverb();
    SfxUnInit();
    MoveImage(&rects[0], 0x1E0, 0);
    DrawSync(0);
    /* Port: the original loaded the movie player (a PS-X EXE named by fileName) to
       0x80119000 and started it with Exec below; the platform plays the movie instead. */
    InitDynamicMem(reserveHeap, 0xE1000);
    SpuQuit();
    close_iso_events();
    SetDispMask(0);
    StopCallback();
    ResetGraph(3);
    StopPAD();
    EnterCriticalSection();
    PortPlayMovie(fileName); /* Port: replaces Exec of the player executable */
    ExitCriticalSection();
    InitDynamicMem(reserveHeap, 0xE1000);
    CdInit(); /* Port: the original also passed (int)cdTrackLocs and 0, which CdInit does not read */
    iso_init(mainheapcopy);
    InitController();
    SpuStart();
    SetDispMask(0);
    rects[6] = 0x140;
    rects[4] = 0;
    rects[5] = 0;
    rects[7] = 0x200;
    ClearImage(&rects[4], 0, 0, 0);
    DrawSync(0);
    InitSystem();
    rects[0] = 0x1E0;
    rects[2] = 0xA0;
    rects[1] = 0;
    rects[3] = 0x100;
    MoveImage(&rects[0], 0x140, 0x100);
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
