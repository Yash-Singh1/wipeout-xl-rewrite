"""Decisions about the PlayStation executable's symbol table that tools/gendata.py applies.

The decompilation names every address the code refers to, so a global that is really a field or an
element of a bigger object appears twice: as the object and as a name inside it. The original
sources had one object; the port must too. For each such name there is a decision here, taken from
the machine code (every access to the addresses involved, with the width and stride it uses; see
`tools/gendata.py --evidence NAME`) and the declarations in game/.

An alias is a macro in game/include/data_alias.h (generated from this table) that names the real
storage; the declarations of the alias names in game/src are removed (`--apply-aliases`), so the data
files define the container only. `--check` verifies each alias twice: for the 32-bit layout, that the
storage the macro reaches is exactly where the executable has the name (a typedef per alias), and
natively, that the 64-bit layout keeps it inside the container, that paired elements are the same
object and that the executable's initial bytes show through the alias (tools/aliastest/*.c, built,
linked with the data files and run).
"""
from collections import namedtuple

Alias = namedtuple('Alias', 'name container macro expr pair includes decls container_decl evidence')


def A(name, container, macro, evidence, expr=None, pair=None, includes=(), decls=(), container_decl=None):
    """`macro` is the C expression the name stands for; `expr` an lvalue (written with the alias name)
    whose address is the first byte of the aliased storage (default: the name itself); `pair` two
    lvalues that must be the same object (an element further into a strided alias and the container
    element it stands for); `includes`/`decls` what a test file needs to use the container;
    `container_decl` the extern declaration inserted in a source file that uses the alias but does not
    declare the container."""
    return Alias(name, container, macro, expr or name, pair, tuple(includes), tuple(decls), container_decl, evidence)


# Uses the aliases need rewritten, (file, old text, new text): a file that saw the container as one word.
REWRITES = [
    ('combat.c', 'extern u16 playerControl;', 'extern ControlInput playerControl[]; /* Port: the packet\'s control word is its head field */'),
    ('combat.c', 'extern u16 remoteControl;', 'extern ControlInput remoteControl[]; /* Port: the packet\'s control word is its head field */'),
    ('combat.c', '    send.control = playerControl;', '    send.control = playerControl[0].head;'),
    ('combat.c', '    remoteControl = controlIn;', '    remoteControl[0].head = controlIn;'),
    ('main.c', 'extern int savedLapTimes;', 'extern int savedLapTimes[]; /* Port: the lap times and the four words after them */'),
    ('main.c', '        savedLapTimes = 0;', '        savedLapTimes[0] = 0;'),
]

ALIASES = [
    # smokeColour is passed to AnimateTexture by value (TrackSpecificFunctions loads the whole word) and
    # race() stores three bytes into it, one after the other: the r, g, b of one CVECTOR.
    A('smokeColourGreen', 'smokeColour', '(*(signed char *)&smokeColour.g)',
      'smokeColour is a CVECTOR (loaded as one word by TrackSpecificFunctions); race() stores bytes at +0, +1, +2',
      decls=['extern CVECTOR smokeColour;'], container_decl='extern CVECTOR smokeColour;'),
    A('smokeColourBlue', 'smokeColour', '(*(signed char *)&smokeColour.b)', 'see smokeColourGreen',
      decls=['extern CVECTOR smokeColour;'], container_decl='extern CVECTOR smokeColour;'),
    # checkCells[track][class][i] is a table of 6-byte cells (section, bonusSeconds, done). race() resets
    # a whole row with three loops over the same cells that name the three fields as arrays of their own,
    # each stepping 6 bytes: bonusSeconds starts at +2, done at +4.
    A('raceCheckpointResults', 'checkCells', '((CheckPointShort (*)[4][6])&checkCells[0][0][0].bonusSeconds)',
      'strides (6, 36, 144) from checkCells + 2 in race() and SetUpFirstCheckPoint: the bonusSeconds field of the 6-byte cells',
      expr='raceCheckpointResults[0][0][0].value',
      pair=('raceCheckpointResults[5][3][5].value', 'checkCells[5][3][5].bonusSeconds'),
      includes=['control.h'],
      decls=['typedef struct { short value; unsigned char pad[4]; } CheckPointShort;', 'extern CheckCell checkCells[][4][6];'],
      container_decl='extern CheckCell checkCells[][4][6];'),
    A('raceCheckpointFlags', 'checkCells', '((CheckPointByte (*)[4][6])&checkCells[0][0][0].done)',
      'strides (6, 36, 144) from checkCells + 4: the done field of the 6-byte cells',
      expr='raceCheckpointFlags[0][0][0].value',
      pair=('raceCheckpointFlags[5][3][5].value', 'checkCells[5][3][5].done'),
      includes=['control.h'],
      decls=['typedef struct { signed char value; unsigned char pad[5]; } CheckPointByte;', 'extern CheckCell checkCells[][4][6];'],
      container_decl='extern CheckCell checkCells[][4][6];'),
    # DrawEnvMap steps StaticRVert, StaticRVertY and StaticRVertZ together by 16 bytes: the vx, vy, vz of one
    # VECTOR array (140 of them up to StaticDepth).
    A('StaticRVertY', 'StaticRVert', '((int (*)[4])&StaticRVert[0].vy)',
      'DrawEnvMap loads +0, +4, +8 of StaticRVert with stride 16 and the three names are 4 bytes apart',
      expr='StaticRVertY[0][0]', pair=('StaticRVertY[9][0]', 'StaticRVert[9].vy'),
      decls=['extern VECTOR StaticRVert[];'], container_decl='extern VECTOR StaticRVert[];'),
    A('StaticRVertZ', 'StaticRVert', '((int (*)[4])&StaticRVert[0].vz)', 'see StaticRVertY',
      expr='StaticRVertZ[0][0]', pair=('StaticRVertZ[9][0]', 'StaticRVert[9].vz'),
      decls=['extern VECTOR StaticRVert[];'], container_decl='extern VECTOR StaticRVert[];'),
    # The header words of a ghost take are separate globals in ghost.c; the take is copied as one block
    # (replayBuffer[0] = recordBuffer[0]), so they are the first words of the buffer.
    A('recordKeyCount', 'recordBuffer', '(*(volatile s32 *)(recordBuffer[0].header + 4))',
      'RecordKeypresses and InitGhostShip access recordBuffer + 4 as a word; the buffer is 0x104 header bytes then the keys',
      includes=['ghost.h'], container_decl='#include "ghost.h"'),
    A('recordKeyTotal', 'recordBuffer', '(*(s32 *)(recordBuffer[0].header + 8))', 'see recordKeyCount',
      includes=['ghost.h'], container_decl='#include "ghost.h"'),
    A('recordControl', 'recordBuffer', '(*(s16 *)(recordBuffer[0].header + 12))', 'see recordKeyCount',
      includes=['ghost.h'], container_decl='#include "ghost.h"'),
    A('recordDisabled', 'recordBuffer', '(*(s16 *)(recordBuffer[0].header + 14))', 'see recordKeyCount',
      includes=['ghost.h'], container_decl='#include "ghost.h"'),
    A('recordMagic', 'recordBuffer', '(*(s16 *)(recordBuffer[0].header + 16))', 'see recordKeyCount',
      includes=['ghost.h'], container_decl='#include "ghost.h"'),
    A('recordTrackNum', 'recordBuffer', '(*(u8 *)(recordBuffer[0].header + 18))', 'see recordKeyCount',
      includes=['ghost.h'], container_decl='#include "ghost.h"'),
    A('recordShipClass', 'recordBuffer', '(*(u8 *)(recordBuffer[0].header + 19))', 'see recordKeyCount',
      includes=['ghost.h'], container_decl='#include "ghost.h"'),
    A('replayKeyCount', 'replayBuffer', '(*(s32 *)(replayBuffer[0].header + 4))', 'same layout as recordBuffer (it is a copy of one)',
      includes=['ghost.h'], container_decl='#include "ghost.h"'),
    A('replayKeyTotal', 'replayBuffer', '(*(s32 *)(replayBuffer[0].header + 8))', 'see replayKeyCount',
      includes=['ghost.h'], container_decl='#include "ghost.h"'),
    A('replayControl', 'replayBuffer', '(*(s16 *)(replayBuffer[0].header + 12))', 'see replayKeyCount',
      includes=['ghost.h'], container_decl='#include "ghost.h"'),
    A('replayDisabled', 'replayBuffer', '(*(s16 *)(replayBuffer[0].header + 14))', 'see replayKeyCount',
      includes=['ghost.h'], container_decl='#include "ghost.h"'),
    A('replayMagic', 'replayBuffer', '(*(s16 *)(replayBuffer[0].header + 16))', 'see replayKeyCount',
      includes=['ghost.h'], container_decl='#include "ghost.h"'),
    A('replayTrackNum', 'replayBuffer', '(*(u8 *)(replayBuffer[0].header + 18))', 'see replayKeyCount',
      includes=['ghost.h'], container_decl='#include "ghost.h"'),
    A('replayShipClass', 'replayBuffer', '(*(u8 *)(replayBuffer[0].header + 19))', 'see replayKeyCount',
      includes=['ghost.h'], container_decl='#include "ghost.h"'),
    # SetCharTextures fills fontGlyphs8/12/16 (vramX, vramY, width: 6 bytes) and menuGlyphWidth/glyphWidth
    # with the same stride 6, the width at +4.
    A('menuGlyphWidth', 'fontGlyphs8', '((GlyphWidth *)&fontGlyphs8[0].width)',
      'SetCharTextures stores fontGlyphs8 + 0, +2, +4 and menuGlyphWidth + 0 with stride 6: the width field of the 6-byte glyphs',
      expr='menuGlyphWidth[0].width', pair=('menuGlyphWidth[7].width', 'fontGlyphs8[7].width'),
      includes=['hiscores.h', 'hud.h'], decls=['extern FontGlyph fontGlyphs8[];'], container_decl='#include "hud.h"\nextern FontGlyph fontGlyphs8[];'),
    A('glyphWidth', 'fontGlyphs16', '((GlyphWidth *)&fontGlyphs16[0].width)', 'see menuGlyphWidth',
      expr='glyphWidth[0].width', pair=('glyphWidth[7].width', 'fontGlyphs16[7].width'),
      includes=['hiscores.h', 'hud.h'], decls=['extern FontGlyph fontGlyphs16[];'], container_decl='#include "hud.h"\nextern FontGlyph fontGlyphs16[];'),
    # The display offsets are the screen rectangle of the DISPENVs (the halfword stores at +8 and +10).
    A('dispOffX0', 'DispEnv0', '(*(u_short *)&DispEnv0.screen.x)',
      'halfword stores at DispEnv0 + 8 and + 10 only (ShakeScreen, InitGameDisplay...): DISPENV.screen.x and .y',
      decls=['extern DISPENV DispEnv0;'], container_decl='extern DISPENV DispEnv0;'),
    A('dispOffY0', 'DispEnv0', '(*(u_short *)&DispEnv0.screen.y)', 'see dispOffX0',
      decls=['extern DISPENV DispEnv0;'], container_decl='extern DISPENV DispEnv0;'),
    A('dispOffX1', 'DispEnv1', '(*(u_short *)&DispEnv1.screen.x)', 'see dispOffX0',
      decls=['extern DISPENV DispEnv1;'], container_decl='extern DISPENV DispEnv1;'),
    A('dispOffY1', 'DispEnv1', '(*(u_short *)&DispEnv1.screen.y)', 'see dispOffX0',
      decls=['extern DISPENV DispEnv1;'], container_decl='extern DISPENV DispEnv1;'),
    # Two memory cards of 15 files of 90 bytes (CardEntry); the menu code addresses used/filename/label/type
    # of the entries with byte arrays of their own (strides 90 and 1350, InitCard, LoadSave, DisplayCardBack).
    A('cardUsed', 'cardSlots', '(&cardSlots[0].entry[0].used)',
      'InitCard stores cardSlots + 10 with strides (1, 90): the used byte of each 90-byte entry',
      expr='cardUsed[0]', pair=('cardUsed[1350 + 90]', 'cardSlots[1].entry[1].used'),
      includes=['card.h'], decls=['extern CardSlot cardSlots[];'], container_decl='extern CardSlot cardSlots[];'),
    A('cardFileNames', 'cardSlots', '((u8 *)cardSlots[0].entry[0].filename)', 'cardSlots + 11: the 32-byte file name of the entry',
      expr='cardFileNames[0]', pair=('cardFileNames[1350 + 90 + 3]', 'cardSlots[1].entry[1].filename[3]'),
      includes=['card.h'], decls=['extern CardSlot cardSlots[];'], container_decl='extern CardSlot cardSlots[];'),
    A('cardNames', 'cardSlots', '((u8 *)&cardSlots[0].entry[0].label)', 'cardSlots + 43: the label of the entry',
      expr='cardNames[0]', pair=('cardNames[1350 + 90 + 3]', 'cardSlots[1].entry[1].label.bytes[3]'),
      includes=['card.h'], decls=['extern CardSlot cardSlots[];'], container_decl='extern CardSlot cardSlots[];'),
    A('cardFlags', 'cardSlots', '((CardFlags (*)[15])&cardSlots[0].entry[0].type)',
      'cardSlots + 75 with strides (90, 1350) in DupFileCheck, LoadSave, DisplayCardBack: the type byte of the entries',
      expr='cardFlags[0][0].flags', pair=('cardFlags[1][2].flags', 'cardSlots[1].entry[2].type'),
      includes=['card.h', 'menus.h'], decls=['extern CardSlot cardSlots[];'], container_decl='extern CardSlot cardSlots[];'),
    # The text sprites are POLY_FT4s (40 bytes); the code fills their clut and tpage through arrays of their own.
    A('quadClutField', 'quadPool', '((u_short *)&quadPool[0].clut)',
      'AddText/AddTime store quadPool + 14 and + 22 with stride 40: the clut and tpage fields of the 600 POLY_FT4s',
      expr='quadClutField[0]', pair=('*(u_short *)((char *)quadClutField + 40 * 7)', 'quadPool[7].clut'),
      decls=['extern POLY_FT4 quadPool[];'], container_decl='extern POLY_FT4 quadPool[];'),
    A('quadTPageField', 'quadPool', '((u_short *)&quadPool[0].tpage)', 'see quadClutField',
      expr='quadTPageField[0]', pair=('*(u_short *)((char *)quadTPageField + 40 * 7)', 'quadPool[7].tpage'),
      decls=['extern POLY_FT4 quadPool[];'], container_decl='extern POLY_FT4 quadPool[];'),
    # followRaceLine/InitRaceLine read and write the point at +4, +8, +12 of each 16-byte RaceLine.
    A('racingLine', 'raceLines', '((VECTOR *)&raceLines[0].x)',
      'racingLine[i] is accessed at raceLines + 4 + 16 * i (x, y, z): the same 16-byte stride',
      expr='racingLine[0].vx', pair=('racingLine[9].vz', 'raceLines[9].z'),
      decls=['extern RaceLine raceLines[];'], container_decl='extern RaceLine raceLines[];'),
    # SortAllShips/initShipData write shipRank (position, shipNo) and read the ship numbers back through
    # shipOrder with stride 4 from +2.
    A('shipOrder', 'shipRank', '((short (*)[2])&shipRank[0].shipNo)',
      'shipOrder[i][0] is shipRank + 2 + 4 * i = shipRank[i].shipNo (SortAllShips, getShipBehindLAP)',
      expr='shipOrder[0][0]', pair=('shipOrder[3][0]', 'shipRank[3].shipNo'),
      includes=['ships.h'], decls=['extern ShipRank shipRank[];'], container_decl='extern ShipRank shipRank[];'),
    A('fadeTileRed', 'fadeTile', '(&fadeTile[0].r0)', 'UpdateFadeScreen stores bytes at fadeTile + 4, 5, 6: the colour of the first TILE',
      expr='fadeTileRed[0]', decls=['extern TILE fadeTile[];'], container_decl='extern TILE fadeTile[];'),
    A('fadeTileGreen', 'fadeTile', '(&fadeTile[0].g0)', 'see fadeTileRed', expr='fadeTileGreen[0]', decls=['extern TILE fadeTile[];'], container_decl='extern TILE fadeTile[];'),
    A('fadeTileBlue', 'fadeTile', '(&fadeTile[0].b0)', 'see fadeTileRed', expr='fadeTileBlue[0]', decls=['extern TILE fadeTile[];'], container_decl='extern TILE fadeTile[];'),
    # LoadProg copied the 136-byte header of the program it loaded into progHeader and read the load address
    # and size at +24 and +28 (the PS-X EXE header).
    A('progLoadAddr', 'progHeader', '(*(u_long *)(progHeader[0].bytes + 24))',
      'LoadProg loads progHeader + 24 and + 28 (t_addr, t_size of the 136-byte PS-X EXE header it just read)',
      includes=['droid.h'], decls=['extern Header progHeader[];'], container_decl='extern Header progHeader[];'),
    A('progSize', 'progHeader', '(*(int *)(progHeader[0].bytes + 28))', 'see progLoadAddr',
      includes=['droid.h'], decls=['extern Header progHeader[];'], container_decl='extern Header progHeader[];'),
    # buildGhostReplayData/InitGhostShip fill one ControlInput (head, buttons, pressed, pad6, thrust, steering)
    # by the halfwords and bytes at +2, +4, +6, +8, +9; cameraOffset starts at +12.
    A('ghostButtons', 'ghostControl', 'ghostControl[0].buttons',
      'halfword accesses at ghostControl + 2, +4, +6 and bytes at +8, +9 (buildGhostReplayData): the ControlInput fields',
      decls=['extern ControlInput ghostControl[];'], includes=['control.h'], container_decl='extern ControlInput ghostControl[];'),
    A('ghostButtonsPressed', 'ghostControl', 'ghostControl[0].pressed', 'see ghostButtons',
      decls=['extern ControlInput ghostControl[];'], includes=['control.h'], container_decl='extern ControlInput ghostControl[];'),
    A('ghostButtonsPrev', 'ghostControl', '(*(u16 *)ghostControl[0].pad6)', 'see ghostButtons (the two pad bytes at +6)',
      decls=['extern ControlInput ghostControl[];'], includes=['control.h'], container_decl='extern ControlInput ghostControl[];'),
    A('ghostStickX', 'ghostControl', '(*(s8 *)&ghostControl[0].thrust)', 'see ghostButtons',
      decls=['extern ControlInput ghostControl[];'], includes=['control.h'], container_decl='extern ControlInput ghostControl[];'),
    A('ghostStickY', 'ghostControl', '(*(s8 *)&ghostControl[0].steering)', 'see ghostButtons',
      decls=['extern ControlInput ghostControl[];'], includes=['control.h'], container_decl='extern ControlInput ghostControl[];'),
    # Per row the crack code uses two A and two B polygons: B is A + 64 bytes within a row of 128.
    A('hiMedCrackPolysB', 'hiMedCrackPolysA', '((PolyFT3 (*)[4])&hiMedCrackPolysA[0][2])',
      'hiMedCrackPolysA[i][screen] and hiMedCrackPolysB[i][screen] (crack.c) are 64 bytes apart and the rows 128 bytes: B shares the table',
      expr='hiMedCrackPolysB[0][0].tag', pair=('hiMedCrackPolysB[3][1].tag', 'hiMedCrackPolysA[3][3].tag'),
      includes=['crack.h'], decls=['extern PolyFT3 hiMedCrackPolysA[][4];'], container_decl='extern PolyFT3 hiMedCrackPolysA[][4];'),
    # The ship models are loaded into shipShapeTable; the two sharks are slots 2 and 4.
    A('lightDroid', 'shipShapeTable', 'shipShapeTable[30]',
      'race() loads shipShapeTable + 120 (slot 30, the rescue droid model, RescueDroidModel) without index and nothing stores it by name',
      pair=('lightDroid', 'shipShapeTable[30]'), decls=['extern Object *shipShapeTable[];'], container_decl='extern Object *shipShapeTable[];'),
    # The pad driver's packet: header, buttons, sticks. RadiusControl reads the buttons halfword on its own.
    A('debugPadInput', 'padBuf1', 'padBuf1.buttons',
      'RadiusControl loads padBuf1 + 2 and NegConConfig the byte at +4 (the packet\'s buttons and steering fields)',
      includes=['pad.h'], decls=['extern PadPacket padBuf1;'], container_decl='#include "pad.h"\nextern PadPacket padBuf1;'),
    # doSerialGame sends the player's ControlInput as a link packet whose fields it names control, padHead,
    # padButtons, padPressed, aux0, aux1 (menus.c, combat.c): the halfwords at +2, +4, +6 and the bytes at +8, +9.
    A('linkPadHead', 'playerControl', 'playerControl[0].buttons',
      'halfword loads at playerControl + 2, +4, +6 and byte loads at +8, +9 (doSerialGame, CheckForButtonPress): the ControlInput fields after head',
      includes=['control.h'], decls=['extern ControlInput playerControl[];'], container_decl='extern ControlInput playerControl[];'),
    A('linkPadButtons', 'playerControl', 'playerControl[0].pressed', 'see linkPadHead',
      includes=['control.h'], decls=['extern ControlInput playerControl[];'], container_decl='extern ControlInput playerControl[];'),
    A('linkPadPressed', 'playerControl', '(*(u16 *)playerControl[0].pad6)', 'see linkPadHead (the two pad bytes at +6)',
      includes=['control.h'], decls=['extern ControlInput playerControl[];'], container_decl='extern ControlInput playerControl[];'),
    A('linkPadAux0', 'playerControl', 'playerControl[0].thrust', 'see linkPadHead',
      includes=['control.h'], decls=['extern ControlInput playerControl[];'], container_decl='extern ControlInput playerControl[];'),
    A('linkPadAux1', 'playerControl', 'playerControl[0].steering', 'see linkPadHead',
      includes=['control.h'], decls=['extern ControlInput playerControl[];'], container_decl='extern ControlInput playerControl[];'),
    A('remotePadHead', 'remoteControl', 'remoteControl[0].buttons',
      'doSerialGame stores the received packet at remoteControl + 0, +2, +4, +6 (halfwords), +8, +9 (bytes)',
      includes=['control.h'], decls=['extern ControlInput remoteControl[];'], container_decl='extern ControlInput remoteControl[];'),
    A('remotePadButtons', 'remoteControl', 'remoteControl[0].pressed', 'see remotePadHead',
      includes=['control.h'], decls=['extern ControlInput remoteControl[];'], container_decl='extern ControlInput remoteControl[];'),
    A('remotePadPressed', 'remoteControl', '(*(u16 *)remoteControl[0].pad6)', 'see remotePadHead',
      includes=['control.h'], decls=['extern ControlInput remoteControl[];'], container_decl='extern ControlInput remoteControl[];'),
    A('remotePadAux0', 'remoteControl', 'remoteControl[0].thrust', 'see remotePadHead',
      includes=['control.h'], decls=['extern ControlInput remoteControl[];'], container_decl='extern ControlInput remoteControl[];'),
    A('remotePadAux1', 'remoteControl', 'remoteControl[0].steering', 'see remotePadHead',
      includes=['control.h'], decls=['extern ControlInput remoteControl[];'], container_decl='extern ControlInput remoteControl[];'),
    # main() clears savedLapTimes and the four words after it one by one; the lap times are indexed by lap.
    A('raceResetWord0', 'savedLapTimes', 'savedLapTimes[1]',
      'main() stores savedLapTimes + 0, 4, 8, 12, 16 in a row; hud.c indexes savedLapTimes[lap] (5 words up to recordBuffer)',
      pair=('raceResetWord0', 'savedLapTimes[1]'), decls=['extern int savedLapTimes[];'], container_decl='extern int savedLapTimes[];'),
    A('raceResetWord1', 'savedLapTimes', 'savedLapTimes[2]', 'see raceResetWord0',
      pair=('raceResetWord1', 'savedLapTimes[2]'), decls=['extern int savedLapTimes[];'], container_decl='extern int savedLapTimes[];'),
    A('raceResetWord2', 'savedLapTimes', 'savedLapTimes[3]', 'see raceResetWord0',
      pair=('raceResetWord2', 'savedLapTimes[3]'), decls=['extern int savedLapTimes[];'], container_decl='extern int savedLapTimes[];'),
    A('raceResetWord3', 'savedLapTimes', 'savedLapTimes[4]', 'see raceResetWord0',
      pair=('raceResetWord3', 'savedLapTimes[4]'), decls=['extern int savedLapTimes[];'], container_decl='extern int savedLapTimes[];'),
    # The menus and the HUD name the first texture slots after the screens that use them; the loader fills them
    # as one table (800 slots up to the end of menuTexturePtrsB) and the code indexes each sub-table from its name.
    A('menuTexturePtrsD', 'textureTable', '(&textureTable[2])',
      'AudioConfig loads textureTable + 8; the pointer tables are 4 bytes apart and the last one runs to the 800th slot',
      expr='menuTexturePtrsD[0]', pair=('menuTexturePtrsD[3]', 'textureTable[5]'),
      decls=['extern Texture *textureTable[];'], container_decl='extern Texture *textureTable[];'),
    A('deleteTextures', 'textureTable', '(&textureTable[3])', 'see menuTexturePtrsD',
      expr='deleteTextures[0]', pair=('deleteTextures[3]', 'textureTable[6]'),
      decls=['extern Texture *textureTable[];'], container_decl='extern Texture *textureTable[];'),
    A('endTextures', 'textureTable', '(&textureTable[4])', 'see menuTexturePtrsD',
      expr='endTextures[0]', pair=('endTextures[3]', 'textureTable[7]'),
      decls=['extern Texture *textureTable[];'], container_decl='extern Texture *textureTable[];'),
    A('menuTexturePtrsC', 'textureTable', '((void **)&textureTable[5])', 'see menuTexturePtrsD',
      expr='menuTexturePtrsC[0]', pair=('menuTexturePtrsC[3]', 'textureTable[8]'),
      decls=['extern Texture *textureTable[];'], container_decl='extern Texture *textureTable[];'),
    A('menuTexturePtrs', 'textureTable', '((void **)&textureTable[6])', 'see menuTexturePtrsD',
      expr='menuTexturePtrs[0]', pair=('menuTexturePtrs[3]', 'textureTable[9]'),
      decls=['extern Texture *textureTable[];'], container_decl='extern Texture *textureTable[];'),
    A('menuTexturePtrsB', 'textureTable', '(&textureTable[7])', 'see menuTexturePtrsD',
      expr='menuTexturePtrsB[0]', pair=('menuTexturePtrsB[3]', 'textureTable[10]'),
      decls=['extern Texture *textureTable[];'], container_decl='extern Texture *textureTable[];'),
    # WaterFall saves a row of the screen next to a RECT and passes `waterfallSaved - 4` to LoadImage: the 8 bytes
    # of waterfallBuf, eleven saved halfwords and the row word are one 16-halfword buffer.
    A('waterfallSaved', 'waterfallBuf', '((u_short *)(waterfallBuf + 8))',
      'effects.c passes waterfallSaved - 4 (= waterfallBuf) to LoadImage; the three names are contiguous (+8, +30)',
      expr='waterfallSaved[0]', pair=('waterfallSaved[2]', 'waterfallBuf[12]'),
      decls=['extern u8 waterfallBuf[];'], container_decl='extern u8 waterfallBuf[];'),
    A('waterfallRow', 'waterfallBuf', '((u_short *)(waterfallBuf + 30))', 'see waterfallSaved',
      expr='waterfallRow[0]', pair=('waterfallRow[0]', 'waterfallBuf[30]'),
      decls=['extern u8 waterfallBuf[];'], container_decl='extern u8 waterfallBuf[];'),
    A('sharkBodyModel', 'shipShapeTable', 'shipShapeTable[2]',
      'UpdateSharkModel loads shipShapeTable + 8 and CopySharkVerts + 16 without index: slots 2 and 4',
      decls=['extern Object *shipShapeTable[];'], container_decl='extern Object *shipShapeTable[];'),
    A('sharkLegModel', 'shipShapeTable', 'shipShapeTable[4]', 'see sharkBodyModel',
      decls=['extern Object *shipShapeTable[];'], container_decl='extern Object *shipShapeTable[];'),
]

# Symbols defined under another C name: the executable's symbol -> the identifier in the data file.
RENAMES = {}

# Symbols declared with different types in different files: use the declaration with this spelling.
PREFER = {
    'padBuf1': 'PadPacket',        # the 8-byte packet the pad driver fills (pad.c declares it as bytes)
}

# Entries of incomplete arrays that the extent cannot give because the names inside them cut it short.
SIZES = {
    'textureTable': (800, 'the slots run up to the end of menuTexturePtrsB (3172 bytes at slot 7): 800 pointers'),
    'waterfallBuf': (32, 'with waterfallSaved and waterfallRow after it: sixteen halfwords'),
    'shipShapeTable': (31, '30 ship shapes and the rescue droid model at [30] (RescueDroidModel); menuModels follows'),
}

# Declared bigger than the object really is: the code only reads the part inside the extent (the
# next declared global starts there), so the definition keeps the declared type and its initialiser
# holds the executable's bytes of the extent only; the rest is zero.
TRUNCATE = {
    'pauseTextContinue': 'declared as a 16-byte MenuText but the string slot is 12 bytes; options.c copies 9 bytes of it',
    'savedClassSelection': 'declared unsigned short[2]; menus.c only uses [0] and savedTrackSelection follows at +2',
    'advertAngle': 'declared short[4]; effects.c uses [0] and [1] only and trainCmpExt follows at +4',
}

# Definitions that must be aligned stricter than their type (the aliases read words from them).
ALIGN = {
    'recordBuffer': (4, 'the take header holds words (recordKeyCount...); the executable has it word-aligned'),
    'replayBuffer': (4, 'the take header holds words (replayKeyCount...); the executable has it word-aligned'),
}

# Pointers that hold an address of the console's RAM outside the program's image (the area above the end of
# .bss): written as 0, they mean nothing to a native program. name -> why.
ABSOLUTE = {
    'rambase': 'holds 0x80100000, the first byte of console RAM above the program; only LoadProg (dead code) used it',
}

SYNTHETIC = []

SEPARATE = {}

ARENAS = {}
