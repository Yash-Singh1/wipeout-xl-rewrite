/* Global data of menus.c, generated from the executable by tools/gendata.py. */
#include "functions.h"
#include "card.h"
#include "effects.h"
#include "hud.h"
#include "options.h"

/* Types that menus.c declares privately. */
typedef struct FontTexture { u8 pad0[4]; u16 clut; u8 pad6[8]; s16 tx,ty; u8 pad18[4]; u8 u0,unused,v0; } FontTexture;
typedef struct Str20 { s32 w[5]; } Str20;
typedef struct { s16 h[5]; } Table10;
typedef struct TrackList { char track[15]; } TrackList;
typedef struct ShipParameterBars {
    u8 bar[5][5];
} ShipParameterBars;
typedef union Chars4A { char bytes[4]; short align; } Chars4A;
typedef struct Chars5 { char bytes[5]; } Chars5;
typedef union Chars6A { char bytes[6]; short align; } Chars6A;
typedef union Chars7A { char bytes[7]; short align; } Chars7A;
typedef struct Chars8 { char bytes[8]; } Chars8;
typedef struct Chars9 { char bytes[9]; } Chars9;
typedef struct Chars10 { char bytes[10]; } Chars10;
typedef struct Chars11 { char bytes[11]; } Chars11;
typedef struct Chars12 { char bytes[12]; } Chars12;
typedef struct Chars13 { char bytes[13]; } Chars13;
typedef struct Chars14 { char bytes[14]; } Chars14;
typedef union Chars14A { char bytes[14]; short align; } Chars14A;
typedef union Chars18A { char bytes[18]; short align; } Chars18A;
typedef union Chars22A { char bytes[22]; short align; } Chars22A;

extern u8 msgNo[28];

u_char msgLinkBroken[12] = { 76, 73, 78, 75, 32, 66, 82, 79, 75, 69, 78, 0 };
u_char msgReturningToMenus[1136] = {
    82, 69, 84, 85, 82, 78, 73, 78, 71, 32, 84, 79, 32, 77, 69, 78, 85, 83, 0, 0, 92, 115, 99, 101,
    110, 101, 46, 99, 109, 112, 0, 0, 92, 115, 107, 121, 46, 99, 109, 112, 0, 0, 0, 0, 92, 115, 99,
    101, 110, 101, 46, 112, 114, 109, 0, 0, 92, 115, 107, 121, 46, 112, 114, 109, 0, 0, 0, 0, 99,
    58, 92, 119, 105, 112, 101, 111, 117, 116, 50, 92, 116, 114, 97, 99, 107, 0, 0, 0, 116, 105,
    109, 46, 99, 58, 76, 111, 97, 100, 84, 101, 120, 116, 117, 114, 101, 40, 41, 58, 32, 67, 97,
    110, 110, 111, 116, 32, 97, 117, 116, 111, 32, 112, 108, 97, 99, 101, 32, 56, 32, 98, 105, 116,
    32, 116, 101, 120, 116, 117, 114, 101, 115, 0, 0, 0, 84, 105, 109, 32, 100, 101, 99, 111, 109,
    112, 114, 101, 115, 115, 58, 32, 102, 105, 108, 101, 32, 37, 115, 32, 110, 111, 116, 32, 102,
    111, 117, 110, 100, 10, 0, 0, 68, 101, 99, 111, 109, 112, 114, 101, 115, 115, 32, 76, 111, 97,
    100, 58, 32, 70, 97, 105, 108, 101, 100, 32, 116, 111, 32, 97, 108, 108, 111, 99, 97, 116, 101,
    32, 109, 101, 109, 111, 114, 121, 32, 102, 111, 114, 32, 116, 114, 97, 99, 107, 32, 118, 101,
    114, 116, 105, 99, 101, 115, 0, 0, 0, 68, 101, 99, 111, 109, 112, 114, 101, 115, 115, 40, 41,
    58, 32, 70, 97, 105, 108, 101, 100, 32, 116, 111, 32, 108, 111, 97, 100, 32, 102, 105, 108,
    101, 32, 37, 115, 10, 0, 0, 0, 99, 58, 92, 119, 105, 112, 101, 111, 117, 116, 50, 92, 115, 111,
    117, 110, 100, 92, 0, 0, 87, 65, 82, 78, 73, 78, 71, 58, 32, 100, 117, 114, 105, 110, 103, 32,
    108, 111, 97, 100, 32, 115, 97, 109, 112, 108, 101, 58, 32, 37, 115, 10, 0, 0, 0, 0, 87, 65,
    82, 78, 73, 78, 71, 58, 32, 115, 97, 109, 112, 108, 101, 32, 110, 111, 116, 32, 108, 111, 97,
    100, 101, 100, 32, 99, 111, 114, 114, 101, 99, 116, 108, 121, 10, 0, 0, 0, 99, 58, 92, 119,
    105, 112, 101, 111, 117, 116, 50, 92, 115, 111, 117, 110, 100, 92, 115, 97, 109, 112, 108, 101,
    115, 46, 119, 97, 100, 0, 0, 0, 115, 104, 105, 112, 115, 104, 105, 112, 46, 118, 97, 103, 0, 0,
    0, 0, 119, 101, 97, 112, 110, 104, 105, 116, 46, 118, 97, 103, 0, 0, 0, 0, 101, 108, 101, 99,
    116, 114, 111, 46, 118, 97, 103, 0, 101, 110, 103, 105, 110, 101, 115, 46, 118, 97, 103, 0,
    119, 101, 112, 110, 119, 97, 108, 108, 46, 118, 97, 103, 0, 0, 0, 0, 104, 105, 116, 119, 97,
    108, 108, 46, 118, 97, 103, 0, 109, 101, 110, 117, 109, 111, 118, 101, 46, 118, 97, 103, 0, 0,
    0, 0, 109, 101, 110, 117, 115, 101, 108, 46, 118, 97, 103, 0, 114, 101, 99, 104, 97, 114, 103,
    101, 46, 118, 97, 103, 0, 0, 0, 0, 109, 105, 110, 101, 46, 118, 97, 103, 0, 0, 0, 0, 109, 105,
    115, 115, 105, 108, 101, 46, 118, 97, 103, 0, 112, 105, 99, 107, 117, 112, 46, 118, 97, 103, 0,
    0, 115, 104, 105, 101, 108, 100, 50, 46, 118, 97, 103, 0, 115, 112, 97, 117, 116, 111, 112,
    108, 46, 118, 97, 103, 0, 0, 0, 0, 100, 114, 111, 105, 100, 46, 118, 97, 103, 0, 0, 0, 116,
    117, 114, 98, 108, 110, 99, 101, 46, 118, 97, 103, 0, 0, 0, 0, 99, 114, 111, 119, 100, 46, 118,
    97, 103, 0, 0, 0, 115, 112, 109, 105, 110, 101, 115, 46, 118, 97, 103, 0, 115, 112, 109, 105,
    115, 115, 108, 46, 118, 97, 103, 0, 115, 112, 114, 111, 99, 107, 101, 116, 46, 118, 97, 103, 0,
    0, 0, 0, 115, 112, 101, 108, 101, 99, 116, 114, 46, 118, 97, 103, 0, 0, 0, 0, 116, 104, 114,
    101, 101, 46, 118, 97, 103, 0, 0, 0, 110, 101, 119, 116, 117, 114, 98, 111, 46, 118, 97, 103,
    0, 0, 0, 0, 116, 104, 117, 110, 100, 101, 114, 46, 118, 97, 103, 0, 115, 104, 112, 115, 116,
    97, 108, 108, 46, 118, 97, 103, 0, 0, 0, 0, 108, 111, 99, 107, 101, 100, 46, 118, 97, 103, 0,
    0, 113, 117, 97, 107, 101, 46, 118, 97, 103, 0, 0, 0, 112, 108, 97, 115, 109, 97, 46, 118, 97,
    103, 0, 0, 99, 104, 107, 112, 111, 105, 110, 116, 46, 118, 97, 103, 0, 0, 0, 0, 100, 116, 99,
    104, 119, 101, 112, 110, 46, 118, 97, 103, 0, 0, 0, 0, 101, 112, 97, 107, 46, 118, 97, 103, 0,
    0, 0, 0, 119, 97, 114, 110, 105, 110, 103, 46, 118, 97, 103, 0, 109, 101, 110, 117, 115, 108,
    105, 100, 46, 118, 97, 103, 0, 0, 0, 0, 115, 99, 114, 97, 112, 101, 108, 112, 46, 118, 97, 103,
    0, 0, 0, 0, 101, 108, 105, 109, 105, 110, 97, 116, 46, 118, 97, 103, 0, 0, 0, 0, 115, 104, 108,
    100, 108, 111, 119, 46, 118, 97, 103, 0, 66, 79, 68, 89, 32, 73, 78, 32, 77, 79, 84, 73, 79,
    78, 0, 0, 70, 73, 82, 69, 83, 84, 65, 82, 84, 69, 82, 32, 73, 78, 83, 84, 82, 46, 0, 0, 84, 73,
    78, 32, 84, 72, 69, 82, 69, 32, 91, 69, 68, 73, 84, 93, 0, 0, 0, 0, 84, 72, 69, 32, 84, 72, 73,
    82, 68, 32, 83, 69, 81, 85, 69, 78, 67, 69, 0, 0, 76, 79, 79, 80, 83, 32, 79, 70, 32, 70, 85,
    82, 89, 0, 0, 0, 68, 85, 83, 84, 32, 85, 80, 32, 66, 69, 65, 84, 83, 0, 0, 0, 65, 84, 79, 77,
    66, 79, 77, 66, 32, 73, 78, 83, 84, 82, 46, 0, 76, 65, 78, 68, 77, 65, 83, 83, 0, 0, 0, 0, 87,
    69, 32, 72, 65, 86, 69, 32, 69, 88, 80, 76, 79, 83, 73, 86, 69, 0, 0, 0, 67, 79, 76, 68, 32,
    83, 84, 79, 82, 65, 71, 69, 0, 0, 0, 0, 84, 72, 69, 32, 80, 82, 79, 68, 73, 71, 89, 0, 85, 78,
    68, 69, 82, 87, 79, 82, 76, 68, 0, 0, 67, 72, 69, 77, 73, 67, 65, 76, 32, 66, 82, 79, 84, 72,
    69, 82, 83, 0, 0, 0, 70, 46, 83, 46, 79, 46, 76, 46, 0, 0, 0, 0,
};
char msgControllerRemoved[20] = "CONTROLLER REMOVED";
/* Port: declared as a 16-byte MenuText but the string slot is 12 bytes; options.c copies 9 bytes of it */
MenuText pauseTextContinue = { { 0x544e4f43, 0x45554e49, 0, 0 } };
MenuText pauseTextReturnToMenu = { { 0x55544552, 0x54204e52, 0x454d204f, 21838 } };
MenuText pauseTextAudioConfig = { { 0x49445541, 0x4f43204f, 0x4749464e, 0 } };
char labelCdTrack[12] = "CD TRACK";
char msgAreYouSure[20] = "ARE YOU SURE YOU";
char msgWantToRestart[] = "WANT TO RESTART";
char msgWantToQuit[16] = "WANT TO QUIT";
char msgTooManyStartBooms[] = "Too many start booms !!!!!\n";
u_long commonPath[1] = { 0x775c3a63 };
SideTable droidSideTable = { { 1, 0, 0, 0, 1, 1, 0, 1 } };
SideTable droidPickTable = { { 255, 298, 438, 352, 12, 350, 310, 64 } };
char texShadow1[36] = "c:\\wipeout2\\textures\\shadow1.tim";
char texShadow2[36] = "c:\\wipeout2\\textures\\shadow2.tim";
char texSilly1[] = "c:\\wipeout2\\textures\\silly1.tim";
char texSilly2[] = "c:\\wipeout2\\textures\\silly2.tim";
TrackList defaultTrackList[1] = { { { 3, 4, 1, 12, 8, 9, 10, 3, 5, 6, 7, 11, 13, 14, 15 } } };
u8 msgCannotSelect[16] = { 67, 65, 78, 78, 79, 84, 32, 83, 69, 76, 69, 67, 84, 0, 0, 0 };
u8 msgAsOtherPlayer[16] = { 65, 83, 32, 79, 84, 72, 69, 82, 32, 80, 76, 65, 89, 69, 82, 0 };
u8 msgHasStarted[12] = { 72, 65, 83, 32, 83, 84, 65, 82, 84, 69, 68, 0 };
u8 msgWaitingForOtherPlayer[28] = {
    87, 65, 73, 84, 73, 78, 71, 32, 70, 79, 82, 32, 79, 84, 72, 69, 82, 32, 80, 76, 65, 89, 69, 82,
    0, 0, 0, 0,
};
char textureSequenceName[36] = "c:\\wipeout2\\textures\\menutims.cmp";
char msgNoCard[12] = "NO CARD \n";
char msgEnterFileName[] = "ENTER FILE NAME";
u8 msgErrorOpenRead[32] = {
    69, 114, 114, 111, 114, 32, 111, 112, 101, 110, 105, 110, 103, 32, 102, 105, 108, 101, 32, 102,
    111, 114, 32, 114, 101, 97, 100, 32, 10, 0, 0, 0,
};
u8 msgErrorReadHeader[32] = {
    69, 114, 114, 111, 114, 32, 114, 101, 97, 100, 105, 110, 103, 32, 104, 101, 97, 100, 101, 114,
    32, 102, 114, 111, 109, 32, 102, 105, 108, 101, 10, 0,
};
u8 msgOverwriteFile[16] = { 79, 86, 69, 82, 87, 82, 73, 84, 69, 32, 70, 73, 76, 69, 0, 0 };
u8 msgDeleteFile[12] = { 68, 69, 76, 69, 84, 69, 32, 70, 73, 76, 69, 0 };
u8 msgMemoryCardIs[16] = { 77, 69, 77, 79, 82, 89, 32, 67, 65, 82, 68, 32, 73, 83, 0, 0 };
u8 msgUnformattedDoYou[20] = { 85, 78, 70, 79, 82, 77, 65, 84, 84, 69, 68, 32, 68, 79, 32, 89, 79, 85, 32, 0 };
u8 msgWishToFormat[20] = { 87, 73, 83, 72, 32, 84, 79, 32, 70, 79, 82, 77, 65, 84, 32, 63, 0, 0, 0, 0 };
LabelList loadModeLabels = { { (char *)&msgNo[4], (char *)&msgNo[12], (char *)&msgNo[20] } };
u8 msgCheckingMemory[16] = { 67, 72, 69, 67, 75, 73, 78, 71, 32, 77, 69, 77, 79, 82, 89, 0 };
u8 msgFileExists[12] = { 70, 73, 76, 69, 32, 69, 88, 73, 83, 84, 83, 0 };
u8 msgNotAWipeoutXl[20] = { 78, 79, 84, 32, 65, 32, 87, 73, 80, 69, 79, 85, 84, 32, 88, 76, 0, 0, 0, 0 };
u8 msgSaveGame[12] = { 83, 65, 86, 69, 32, 71, 65, 77, 69, 0, 0, 0 };
u8 msgNotFormatted[16] = { 78, 79, 84, 32, 70, 79, 82, 77, 65, 84, 84, 69, 68, 0, 0, 0 };
u8 msgErrorFormatting[20] = { 69, 82, 82, 79, 82, 32, 70, 79, 82, 77, 65, 84, 84, 73, 78, 71, 0, 0, 0, 0 };
u8 msgMemoryCard[12] = { 77, 69, 77, 79, 82, 89, 32, 67, 65, 82, 68, 0 };
u8 msgCannotSaveFileNo[24] = { 67, 65, 78, 78, 79, 84, 32, 83, 65, 86, 69, 32, 70, 73, 76, 69, 32, 45, 32, 78, 79, 0, 0, 0 };
u8 msgMemoryCardPresent[20] = { 77, 69, 77, 79, 82, 89, 32, 67, 65, 82, 68, 32, 80, 82, 69, 83, 69, 78, 84, 0 };
u8 msgErrorLoadingFile[20] = { 69, 82, 82, 79, 82, 32, 76, 79, 65, 68, 73, 78, 71, 32, 70, 73, 76, 69, 0, 0 };
u8 msgErrorSavingFile[20] = { 69, 82, 82, 79, 82, 32, 83, 65, 86, 73, 78, 71, 32, 70, 73, 76, 69, 0, 0, 0 };
u8 msgErrorDeletingFile[20] = { 69, 82, 82, 79, 82, 32, 68, 69, 76, 69, 84, 73, 78, 71, 32, 70, 73, 76, 69, 0 };
u8 msgLoadingGameData[20] = { 76, 79, 65, 68, 73, 78, 71, 32, 71, 65, 77, 69, 32, 68, 65, 84, 65, 0, 0, 0 };
u8 msgSavingGameData[20] = { 83, 65, 86, 73, 78, 71, 32, 71, 65, 77, 69, 32, 68, 65, 84, 65, 0, 0, 0, 0 };
u8 msgDeletingGameData[20] = { 68, 69, 76, 69, 84, 73, 78, 71, 32, 71, 65, 77, 69, 32, 68, 65, 84, 65, 0, 0 };
u8 msgSelectMemory[16] = { 83, 69, 76, 69, 67, 84, 32, 77, 69, 77, 79, 82, 89, 0, 0, 0 };
u8 msgCardBlockTo[16] = { 67, 65, 82, 68, 32, 66, 76, 79, 67, 75, 32, 84, 79, 0, 0, 0 };
u8 msgLoadGameFrom[16] = { 76, 79, 65, 68, 32, 71, 65, 77, 69, 32, 70, 82, 79, 77, 0, 0 };
u8 msgSaveGameTo[16] = { 83, 65, 86, 69, 32, 71, 65, 77, 69, 32, 84, 79, 0, 0, 0, 0 };
u8 msgDeleteSaveFrom[20] = { 68, 69, 76, 69, 84, 69, 32, 83, 65, 86, 69, 32, 70, 82, 79, 77, 0, 0, 0, 0 };
u8 msgSelectMemoryCard[20] = { 83, 69, 76, 69, 67, 84, 32, 77, 69, 77, 79, 82, 89, 32, 67, 65, 82, 68, 0, 0 };
s8 fmtMemoryCard[16] = { 77, 69, 77, 79, 82, 89, 32, 67, 65, 82, 68, 32, 37, 100, 10, 0 };
u8 msgUnformatted[12] = { 85, 78, 70, 79, 82, 77, 65, 84, 84, 69, 68, 0 };
u8 msgNoMemory[12] = { 78, 79, 32, 77, 69, 77, 79, 82, 89, 0, 0, 0 };
PasswordSeeds passwordSeeds[1] = { { { 3, 2, 3, 1, 2, 1, 0, 3, 0, 2, 1, 2, 0, 3 } } };
PasswordSeeds passwordSeedsClear[1] = { 0 };
char msgPasswordIsIncorrect[24] = "PASSWORD IS INCORRECT";
char msgUseUpAndDownOnTheDirectional[36] = "USE UP AND DOWN ON THE DIRECTIONAL";
char msgButtonsToChangeTheSymbol[32] = "BUTTONS TO CHANGE THE SYMBOL";
char msgCurrentPassword[20] = "CURRENT PASSWORD";
char msgNewPasswordEntry[20] = "NEW PASSWORD ENTRY";
DigitalButtons padDigitalKeys = { { 0x1000, 0x4000, 0x8000, 0x2000, 0x400, 256, 0x800, 512 } };
AnalogButtons padAnalogKeys = { { 0x1000, 0x8ff0, 0x8ff1, 0x2000, 0x8ff2, 0x800 } };
char teamNameAgSystems[12] __attribute__((aligned(4))) = "AG SYSTEMS";
char msgRedBull[12] = "RED BULL";
u8 msgInactive[12] = { 73, 78, 65, 67, 84, 73, 86, 69, 0, 0, 0, 0 };
u8 msgDefaultView[16] = { 68, 69, 70, 65, 85, 76, 84, 32, 86, 73, 69, 87, 0, 0, 0, 0 };
u8 msgMoveScreen[12] = { 77, 79, 86, 69, 32, 83, 67, 82, 69, 69, 78, 0 };
u8 msgDefaultName[16] = { 68, 69, 70, 65, 85, 76, 84, 32, 78, 65, 77, 69, 0, 0, 0, 0 };
u8 msgUseUpDownLeftAndRightOnThe[44] = {
    85, 83, 69, 32, 85, 80, 44, 32, 68, 79, 87, 78, 44, 32, 76, 69, 70, 84, 32, 65, 78, 68, 32, 82,
    73, 71, 72, 84, 32, 79, 78, 32, 84, 72, 69, 32, 68, 45, 80, 65, 68, 0, 0, 0,
};
u8 msgToAdjustScreenPosition[28] = {
    84, 79, 32, 65, 68, 74, 85, 83, 84, 32, 83, 67, 82, 69, 69, 78, 32, 80, 79, 83, 73, 84, 73, 79,
    78, 0, 0, 0,
};
u8 msgSaveScreenPosition[24] = { 83, 65, 86, 69, 32, 83, 67, 82, 69, 69, 78, 32, 80, 79, 83, 73, 84, 73, 79, 78, 0, 0, 0, 0 };
u8 msgActivateMoveScreen[24] = { 65, 67, 84, 73, 86, 65, 84, 69, 32, 77, 79, 86, 69, 32, 83, 67, 82, 69, 69, 78, 0, 0, 0, 0 };
u8 msgConfirmLetterEntry[24] = { 67, 79, 78, 70, 73, 82, 77, 32, 76, 69, 84, 84, 69, 82, 32, 69, 78, 84, 82, 89, 0, 0, 0, 0 };
u8 msgNameIsUsedAsDefaultHighScore[104] = {
    78, 65, 77, 69, 32, 73, 83, 32, 85, 83, 69, 68, 32, 65, 83, 32, 68, 69, 70, 65, 85, 76, 84, 32,
    72, 73, 71, 72, 32, 83, 67, 79, 82, 69, 32, 69, 78, 84, 82, 89, 0, 0, 0, 0, 82, 73, 71, 72, 84,
    32, 65, 73, 82, 32, 66, 82, 65, 75, 69, 0, 76, 69, 70, 84, 32, 65, 73, 82, 32, 66, 82, 65, 75,
    69, 0, 0, 68, 73, 83, 67, 65, 82, 68, 32, 87, 69, 65, 80, 79, 78, 0, 0, 67, 72, 65, 78, 71, 69,
    32, 86, 73, 69, 87, 0,
};
char fmtNewAngle[16] = "NEW ANGLE %d";
char msgMaximumLock[16] = "MAXIMUM LOCK";
char msgCentreSetting[16] = "CENTRE SETTING";
char msgDeadSpot[12] = "DEAD SPOT";
char fmtCurrent[12] = "CURRENT %d";
char msgToSetOrResetNegconPosition[] = "TO SET OR RESET NEGCON POSITION";
HighlightWidths keyHighlightWidth = { { 39, 24, 63, 81, 78, 80 } };
char msgAButtonIsDefined[] = "A BUTTON IS DEFINED";
char msgMoreThanOnce[16] = "MORE THAN ONCE";
char msgL1Button[12] = "L1 BUTTON";
char msgL2Button[12] = "L2 BUTTON";
char msgRButton[12] = "R BUTTON";
char msgR1Button[12] = "R1 BUTTON";
char msgR2Button[12] = "R2 BUTTON";
char msgLButton[12] = "L BUTTON";
char msgDefaultConfig[16] = "DEFAULT CONFIG";
char msgUseTheLeftAndRightDirectional[44] = "USE THE LEFT AND RIGHT DIRECTIONAL BUTTONS";
char msgToChangeCurrentButtonSettings[36] = "TO CHANGE CURRENT BUTTON SETTINGS";
char labelCdTrackMenu[12] = "CD TRACK";
ShipParameterBars shipParameterBars = {
    {
        { 7, 5, 6, 3, 6 },
        { 5, 6, 5, 5, 5 },
        { 4, 7, 3, 7, 5 },
        { 6, 3, 7, 6, 7 },
        { 10, 10, 10, 10, 10 }
    }
};
char msgTopSpeed[12] = "TOP SPEED";
char msgTurningAbility[] = "TURNING ABILITY";
char msgShieldEnergy[16] = "SHIELD ENERGY";
char msgAerodynamics[16] = "AERODYNAMICS";
TeamOrder teamShipGroup[1] = { { { 3, 0, 1, 2, 4 } } };
char msgTeamCheatActive[20] = "TEAM CHEAT ACTIVE";
char msgExcellentTurningAbilityAnd[48] = "EXCELLENT TURNING ABILITY AND EASY TO CONTROL";
char msgButNotTooQuickBeginner[32] = "BUT NOT TOO QUICK.  [BEGINNER]";
char msgGoodAccelerationAndTurning[44] = "GOOD ACCELERATION AND TURNING ABILITY BUT";
char msgWeakShieldAmateur[] = "WEAK SHIELD.  [AMATEUR]";
char msgGoodAllRounderButAverage[44] = "GOOD ALL ROUNDER BUT AVERAGE PERFORMANCE.";
char msgIntermediate[16] = "[INTERMEDIATE]";
char msgFastWithGoodShieldCapability[48] = "FAST WITH GOOD SHIELD CAPABILITY BUT SLIGHTLY";
char msgHeavyAtTurningExpert[] = "HEAVY AT TURNING.  [EXPERT]";
char msgVeryFastWithExcellentShield[40] = "VERY FAST WITH EXCELLENT SHIELD BUT NO";
char msgWeaponTechnologyMaster[32] = "WEAPON TECHNOLOGY.  [MASTER]";
char msgPhantomTrackCheatActive[28] = "PHANTOM TRACK CHEAT ACTIVE";
char msgTrackCheatActive[20] = "TRACK CHEAT ACTIVE";
char msgIntermediate2[16] = "INTERMEDIATE";
char msgSpeedDemon[] = "SPEED DEMON";
char msgAMajorCanadianIndustrial[] = "A MAJOR CANADIAN INDUSTRIAL";
char msgComplexEasy[] = "COMPLEX. [EASY]";
char msgASnowyTibetanMountainCircuit[36] = "A SNOWY TIBETAN MOUNTAIN CIRCUIT.";
char msgASouthAmericanJungleCircuit[36] = "A SOUTH AMERICAN JUNGLE CIRCUIT.";
char msgTricky[12] = "[TRICKY]";
char msgANewlyConstructedGerman[28] = "A NEWLY CONSTRUCTED GERMAN";
char msgCommercialParkTricky[28] = "COMMERCIAL PARK.  [TRICKY]";
char msgADisusedFrenchMetroSystem[32] = "A DISUSED FRENCH METRO SYSTEM.";
char msgAHugeSuspendedCircuitOver[36] = "A HUGE SUSPENDED CIRCUIT OVER THE";
char msgBlackSeaHard[20] = "BLACK SEA. [HARD]";
char msgAnObscureVolcanicIslandLocated[36] = "AN OBSCURE VOLCANIC ISLAND LOCATED";
char msgInTheSouthPacificVeryHard[36] = "IN THE SOUTH PACIFIC.  [VERY HARD]";
char msgAnAmericanCityCircuitAffected[36] = "AN AMERICAN CITY CIRCUIT AFFECTED";
char msgBySeismicActivityVeryHard[36] = "BY SEISMIC ACTIVITY.  [VERY HARD]";
char msgLinkedUp[12] __attribute__((aligned(4))) = "LINKED UP";
char msgTimeTrial[12] __attribute__((aligned(4))) = "TIME TRIAL";
char msgOneOnOne[12] __attribute__((aligned(4))) = "ONE ON ONE";
char msgArcadeLink[] __attribute__((aligned(4))) = "ARCADE LINK";
char msgChallengeI[] __attribute__((aligned(4))) = "CHALLENGE I";
char msgChallengeII[16] __attribute__((aligned(4))) = "CHALLENGE II";
char msgEssentialOptions[20] __attribute__((aligned(4))) = "ESSENTIAL OPTIONS";
char msgRacingMode[] __attribute__((aligned(4))) = "RACING MODE";
char msgStructureMode[16] __attribute__((aligned(4))) = "STRUCTURE MODE";
char msgOptionsMode[16] __attribute__((aligned(4))) = "OPTIONS MODE";
char msgTrackMode[12] __attribute__((aligned(4))) = "TRACK MODE";
char msgTeamMode[12] __attribute__((aligned(4))) = "TEAM MODE";
char msgPrecisionLevels[20] __attribute__((aligned(4))) = "PRECISION LEVELS";
char msgInteractionMode[20] __attribute__((aligned(4))) = "INTERACTION MODE";
char msgNegconSelectionMode[24] __attribute__((aligned(4))) = "NEGCON SELECTION MODE";
char msgNegconMode[] __attribute__((aligned(4))) = "NEGCON MODE";
char msgNegconTwistMode[20] __attribute__((aligned(4))) = "NEGCON TWIST MODE";
char msgPasswordMode[16] __attribute__((aligned(4))) = "PASSWORD MODE";
char msgProgressionMode[20] __attribute__((aligned(4))) = "PROGRESSION MODE";
char msgDataMode[12] __attribute__((aligned(4))) = "DATA MODE";
char msgPreferencesMode[20] __attribute__((aligned(4))) = "PREFERENCES MODE";
char msgScreenMode[] __attribute__((aligned(4))) = "SCREEN MODE";
char msgTimesMode[12] __attribute__((aligned(4))) = "TIMES MODE";
char msgArcadeMode[] __attribute__((aligned(4))) = "ARCADE MODE";
char msgTimeTrialMode[] __attribute__((aligned(4))) = "TIME TRIAL MODE";
char msgOnePlayerSingleRaceAgainst[44] = "[ONE PLAYER]  SINGLE RACE AGAINST A FULL";
char msgGridOfCompetitors[24] = "GRID OF COMPETITORS.";
char msgOnePlayerSingleRaceTimeTrial[40] = "[ONE PLAYER]  SINGLE RACE TIME TRIAL.";
char msgNoCompetitors[] = "NO COMPETITORS.";
char msgTwoPlayerSingleRaceAgainst[44] = "[TWO PLAYER]  SINGLE RACE AGAINST A FULL";
char msgTwoPlayerSingleRaceWithNo[] = "[TWO PLAYER]  SINGLE RACE WITH NO OTHER";
char msgCompetitors[16] = "COMPETITORS.";
char msgOnePlayerPhantomChallenge[44] = "[ONE PLAYER]  PHANTOM CHALLENGE. SIX RACES";
char msgAtRapierClassWeaponsAreActivated[] = "AT RAPIER CLASS. WEAPONS ARE ACTIVATED.";
char msgOnePlayerPiranhaChallenge[48] = "[ONE PLAYER]  PIRANHA CHALLENGE. EIGHT RACES";
char msgAtPhantomClassWeaponsAre[44] = "AT PHANTOM CLASS. WEAPONS ARE ACTIVATED.";
char pathMenuDat[] = "c:\\wipeout2\\common\\menu.dat";
char errLoadMenuLineDataFileHasZero[44] = "LoadMenuLineData: File has zero length \n";
char errLoadMenuLineDataFailedTo[60] = "LoadMenuLineData (): Failed to allocate memory for file \n";
char errLoadMenuLineDataFailedTo2[44] = "LoadMenuLineData(): Failed to load file \n";
Chars13 msgRacingClass = { "RACING CLASS" };
Chars10 msgRaceType = { "RACE TYPE" };
Chars13 msgAudioConfig = { "AUDIO CONFIG" };
Chars18A msgControllerConfig = { "CONTROLLER CONFIG" };
Chars12 msgPreferences = { "PREFERENCES" };
Chars14A msgLoadAndSave = { "LOAD AND SAVE" };
Chars9 msgPassword = { "PASSWORD" };
Chars18A msgBestArcadeTimes = { "BEST ARCADE TIMES" };
Chars22A msgBestTimeTrialTimes = { "BEST TIME TRIAL TIMES" };
Chars13 msgOptionsMenu = { "OPTIONS MENU" };
Chars13 msgVectorClass = { "VECTOR CLASS" };
Chars12 msgVenomClass = { "VENOM CLASS" };
Chars13 msgRapierClass = { "RAPIER CLASS" };
Chars14 msgPhantomClass = { "PHANTOM CLASS" };
Chars10 msgTeamMenu = { "TEAM MENU" };
Chars11 msgController = { "CONTROLLER" };
Chars12 msgGameConfig = { "GAME CONFIG" };
Chars11 msgBestTimes = { "BEST TIMES" };
Chars14A msgButtonConfig = { "BUTTON CONFIG" };
Chars13 msgTwistConfig = { "TWIST CONFIG" };
Chars14 msgNegconConfig = { "NEGCON CONFIG" };
char msgTalonSReach[16] = "TALON'S REACH";
char msgGareDEuropa[16] = "GARE D'EUROPA";
char msgVostokIsland[16] = "VOSTOK ISLAND";
char msgSpilskinanke[16] = "SPILSKINANKE";
char msgSagarmatha[12] = "SAGARMATHA";
char msgValparaiso[12] = "VALPARAISO";
char msgOdessaKeys[] = "ODESSA KEYS";
char msgPhenitiaPark[16] = "PHENITIA PARK";
char errLoadTextureFileNotFound[32] = "LoadTexture: File not found\n";
char errLoadTextureFailedToAllocate[] = "LoadTexture(): Failed to allocate memory for file \n";
char errLoadTextureFailedToLoadFile[40] = "LoadTexture(): Failed to load file \n";
Str20 pathCommonDir = { { 0x775c3a63, 0x6f657069, 0x5c327475, 0x6d6d6f63, 0x5c6e6f } };
char msgInitializingCombatLink[28] = "INITIALIZING COMBAT LINK";
char msgLinkFailed[] = "LINK FAILED";
char msgMasterLinkEstablished[] = "MASTER LINK ESTABLISHED";
char msgSlaveLinkEstablished[24] = "SLAVE LINK ESTABLISHED";
Table10 shipGroupTeam = { { 1, 2, 3, 0, 4 } };
int currScreen = 0;
unsigned char twist = 0;
int negconCentre = 0;
int remoteStartGame = 0;
int currMenu = 0;
short menuEditor = 0;
short prevMenu = 0;
unsigned char raceTracks[8] = { 1, 8, 13, 20, 2, 17, 6, 7 };
u_short menuTextures[1] = { 0 };
short buttonCount = 0;
s32 screenCount = 0;
u32 slideCount = 0;
u16 topOffSet = 0;
u16 middleOffSet = 0;
u16 bottomOffSet = 0;
short negconEditing[4] = { 0 };
s32 fadescreen = 0;
s32 myFinished = 0;
s32 prevCardStatus0 = 0;
s32 prevCardStatus1 = 0;
s32 cardSettleCount0 = 0;
s32 cardSettleCount1 = 0;
int cardNum = 0;
int heading = 0;
int buttonTextures = 0;
int cdFlag[1] = { 1 };
s32 menuShip = 0;
s32 menuActTrack = 0;
s32 menuNormTrack = 0;
s32 menuTrackNo = 0;
s32 menuFlag = 0;
short keyDefineThrust = 0;
char modelNameHarry[8] = "harry";
char modelNameJune[8] = "june";
Label6 defaultCardName[1] = { { { 69, 77, 80, 84, 89, 0 } } };
s16 card1Exists = 1;
s16 cardPresent1 = 1;
s32 cardFlash = 0;
s32 prevCard = 0;
u8 msgE[4] = { 69, 0, 0, 0 };
s32 cardUnformatted0 = 0;
s32 cardUnformatted1 = 0;
int currentCard = 0;
char msgDel[] = "DEL";
char msgEnd[] = "END";
u8 msgYes[4] = { 89, 69, 83, 0 };
u8 msgNo[28] = {
    78, 79, 0, 0, 76, 79, 65, 68, 0, 0, 0, 0, 83, 65, 86, 69, 0, 0, 0, 0, 68, 69, 76, 69, 84, 69,
    0, 0,
};
CardTimer cardNotice = { 0 };
CardTimer cardPending = { 0 };
char msgCards[8] = "CARDS";
CardDevice cardDevice0 = { "bu00:" };
CardDevice cardDevice1 = { "bu10:" };
char msgIsFull[] = "IS FULL";
char msgCard[8] = "CARD";
char numTex[4] = { 0 };
s32 passwordErrorTimer = 0;
int myseed = 0;
char hiScoreLabelName[8] = "BEST";
char hiScoreLabelTime[8] = "RACE";
char hiScoreLabelShip[8] = "TIME";
char hiScoreLabelLap[] = "LAP";
char teamNameAuricom[8] __attribute__((aligned(4))) = "AURICOM";
char teamNameQirex[8] __attribute__((aligned(4))) = "QIREX";
char teamNameFeisar[8] __attribute__((aligned(4))) = "FEISAR";
char teamNamePiranha[8] __attribute__((aligned(4))) = "PIRANHA";
Chars7A hiScoreRank0 = { "VECTOR" };
Chars6A hiScoreRank1 = { "VENOM" };
Chars7A hiScoreRank2 = { "RAPIER" };
char hiScoreRank3[] = "PHANTOM";
s32 blinkCounter = 0;
u8 msgOn[4] = { 79, 78, 0, 0 };
u8 msgOff[4] = { 79, 70, 70, 0 };
u8 msgInt[4] = { 73, 78, 84, 0 };
u8 msgExt[4] = { 69, 88, 84, 0 };
u8 msgActive[8] = { 65, 67, 84, 73, 86, 69, 0, 0 };
u8 msgWeapons[16] = { 87, 69, 65, 80, 79, 78, 83, 0, 70, 73, 82, 69, 0, 0, 0, 0 };
char msgThrust[8] = "THRUST";
Positions knobPosX = { { 68, 162, 257 } };
Positions knobPosY = { { 125, 125, 125 } };
TwistAngles twistAngles = { { 0, 6, 12, 18 } };
int negconBlink = 0;
int duplicateKeyTimer = 0;
s32 audioBlink = 0;
char labelMusic[8] = "MUSIC";
char labelSfx[] = "SFX";
char labelSound[8] = "SOUND";
char labelArtist[8] = "ARTIST";
char labelTitle[8] = "TITLE";
char labelMono[8] = "MONO";
char labelStereo[8] = "STEREO";
s32 teamSelectFrames = 0;
s32 classBlink = 0;
ClassOrder classOrderTable[1] = { { { 1, 0, 3, 5 } } };
char msgNovice[8] = "NOVICE";
char msgExpert[8] = "EXPERT";
char msgEasy[8] = "[EASY]";
char msgHard[8] = "[HARD]";
char msgGold[8] = "GOLD";
char tempheapcopy[] = "AWARDED";
char msgSilver[8] = "SILVER";
char msgBronze[8] = "BRONZE";
Chars7A msgArcade __attribute__((aligned(4))) = { "ARCADE" };
Chars8 msgLink __attribute__((aligned(4))) = { "LINK UP" };
short lastTopOption = 0;
RaceOrder raceOrderTable[1] = { { { 9, 2, 8, 10, 6, 4 } } };
char msgSelect2[8] = "SELECT";
char msgAccept[8] = "ACCEPT";
char msgBack[8] = "BACK";
Chars5 msgType = { "TYPE" };
Chars6A msgClass = { "CLASS" };
Chars4A msgAnd = { "AND" };
Chars6A msgTrack = { "TRACK" };
Chars5 msgTeam = { "TEAM" };
Chars8 msgOptions = { "OPTIONS" };
Chars6A msgStart = { "START" };
Chars8 msgWipeout = { "WIPEOUT" };
char extPrm[8] = ".prm";
s32 shipSpin = 3968;
u8 loadMessage[8] = { 76, 79, 65, 68, 73, 78, 71, 0 };
short loTileCount = 0;
short medTileCount = 0;
unsigned char TrackHiCount = 0;
unsigned char cardDirLoaded1 = 0;
unsigned char cardDirChanged0 = 0;
unsigned char cardDirChanged1 = 0;
s32 menuTitleBlink;
s32 linkBlinkCount;
BLK_FILL *screenFills[1];
BLK_FILL *screenFill1;
u16 keyDefineThrustPad[2];
s16 linkRetries;
Skeleton *upperSkeleton;
signed char cdPlaySeconds[16];
s32 prevLinkInB;
s32 prevLinkInA;
FontTexture *fontTexture;
MenuLayout *menuLayout;
void *preCalcObject;
s16 linkToggleB;
s16 linkWaitCount;
s16 linkToggleA;
u16 keyDefineFire[2];
CardImage *cardIconInfo;
u_short keysDown;
Object *teamShip;
u16 keyDefineView[2];
s16 linkReady;
s32 linkInB;
s32 linkInA;
s16 linkCloseCount;
s16 linkTimedOut;
s16 linkState;
MenuEntry *menu;
s16 linkTickCount;
u16 keyDefineLBrake[2];
u16 keyDefineRBrake[2];
u8 loadPolys0[80];
u8 loadPolys1[80];
Object *menuModels[10];
DB doubleBuffer[2];
Object *shipModels[8];
POLY_FT4 menuButtons[30][2];
/* Port: the slots run up to the end of menuTexturePtrsB (3172 bytes at slot 7): 800 pointers */
Texture *textureTable[800];
FontGlyph fontGlyphs8[40];
DISPENV DispEnv0;
DISPENV DispEnv1;
s32 VertexNormal[280];
FontGlyph fontGlyphs16[40];
CardSlot cardSlots[2];
HiScore hiScoreTable[16][4];
CardKeys savedKeyConfig;
