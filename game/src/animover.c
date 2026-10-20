#include "functions.h"
#include "animover.h"

extern ConfigData *gameData;
extern u_char sfxMode;
extern u_long OT[];
extern u_long OT2[];
extern int mainheapcopy;
extern void *reserveHeap;
extern int quadIndex;
extern DB doubleBuffer[];
extern char cdTrackLocs[];
extern short CurrentScreen;
#include "droid.h"
extern Header       progHeader[];
extern u_long       progLoadAddr;
extern int         progSize;
Header             *rambase;

int CdRead(u_long sectors, void *buf, int mode);
int CdReadSync(int mode, u_char *result);
void *CdSearchFile(void *fileInfo, int name);
void AsmDrawObjectLit(Object *object, Skeleton *camera, int roll, CVECTOR *colour);


int LoadProg(int progIndex)
{
    u_char fileInfo[24];
    int    mode;
    int    i;
    u_long addr;

    mode = 0x80;
    i = 0;
    do {
        CdSearchFile(&fileInfo, progIndex);
        i += 1;
    } while (i < 0xA);
    if (CdControl(2, fileInfo, 0) != 0) {
        if (CdRead(1U, rambase, mode) != 0) {
            do {
            } while (CdReadSync(1, 0) > 0);
            __builtin_memcpy(progHeader, (void *)rambase, 136);
            CdIntToPos(CdPosToInt(fileInfo) + 1, fileInfo);
            addr = progLoadAddr;
            if (addr <= 0x801F6FFFU) {
                do {
                    *(int *)addr = 0;
                    addr += 4;
                } while (addr <= 0x801F6FFFU);
            }
            if (CdControl(2, fileInfo, 0) != 0) {
                return CdRead((u_long)(progSize + 0x7FF) >> 0xB, (void *)progLoadAddr, mode);
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
    do {

    } while (LoadFile(fileName, (char *)0x80119000) == 0);
    __builtin_memcpy(progHeader, (void *)0x80119000, 136);
    LoadFile(fileName, (char *)0x80118800);
    InitDynamicMem(reserveHeap, 0xE1000);
    SpuQuit();
    close_iso_events();
    SetDispMask(0);
    StopCallback();
    ResetGraph(3);
    StopPAD();
    EnterCriticalSection();
    Exec((ExecBlock *)&progHeader[0].bytes[0x10], 0, 0);
    ExitCriticalSection();
    InitDynamicMem(reserveHeap, 0xE1000);
    CdInit((int)cdTrackLocs, 0);
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
    DrawOTag(OT[0] + 0x7FFC);
    DrawSync(0);
    PutDrawEnv(doubleBuffer[1].draw);
    DrawOTag(OT[0] + 0x7FFC);
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
