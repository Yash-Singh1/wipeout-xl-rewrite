/* Global data of sfx.c, generated from the executable by tools/gendata.py. */
#include "functions.h"
#include "sfx.h"

char birdTextureFile[] = "c:\\wipeout2\\textures\\bird.cmp";
char msgSfxTransEffectMalloc[] = "trans effect: can't malloc memory\n";
char msgSfxTransEffectNoIds[] = "trans effect: no free ids\n";
char msgSfxTransEffectRateMismatch[] = "trans effect: incompatible samples\n";
u_char reverbDepth;
u_char driverMode;
void (*sfxCallback)(int);
SfxEffect effectList[128];
SfxVoice voiceList[24];
SfxSound playList[24];
char pbSpuMallocRec[1032];
int pwVolumeStore[24][2];
