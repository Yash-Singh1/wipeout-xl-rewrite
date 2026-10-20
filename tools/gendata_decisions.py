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

Alias = namedtuple('Alias', 'name container macro expr pair includes decls container_decl evidence use')


def A(name, container, macro, evidence, expr=None, pair=None, includes=(), decls=(), container_decl=None, use=None):
    """`macro` is the C expression the name stands for (the tests use it to name the storage); `expr` an lvalue
    (written with the alias name) whose address is the first byte of the aliased storage (default: the name
    itself); `pair` two lvalues that must be the same object (an element further into a strided alias and the
    container element it stands for); `includes`/`decls` what a test file needs to use the container;
    `container_decl` the line that replaces the removed declaration of the alias in a source file that does not
    declare the container. `use` says how the uses in game/src are rewritten (`--apply-aliases`): a string, the
    text that replaces the name (default: `macro`); ('index', container, n) for a name used as an array,
    `name[i]` becomes `container[n + i]`; ('rewrite', [(file, old, new)...]) for anything else."""
    return Alias(name, container, macro, expr or name, pair, tuple(includes), tuple(decls), container_decl, evidence, use if use is not None else macro)


# Uses the aliases need rewritten, (file, old text, new text): a file that saw the container as one word.
REWRITES = [
]

ALIASES = [
    # SetCharTextures fills fontGlyphs8/12/16 (vramX, vramY, width: 6 bytes) and menuGlyphWidth/glyphWidth
    # with the same stride 6, the width at +4: the width field of the 6-byte glyphs.
    A('menuGlyphWidth', 'fontGlyphs8', '((GlyphWidth *)&fontGlyphs8[0].width)',
      'SetCharTextures stores fontGlyphs8 + 0, +2, +4 and menuGlyphWidth + 0 with stride 6: the width field of the 6-byte glyphs',
      expr='menuGlyphWidth[0].width', pair=('menuGlyphWidth[7].width', 'fontGlyphs8[7].width'), use=('index', 'fontGlyphs8', 0),
      includes=['hiscores.h', 'hud.h'], decls=['extern FontGlyph fontGlyphs8[];'], container_decl='#include "hud.h"\nextern FontGlyph fontGlyphs8[];'),
    A('glyphWidth', 'fontGlyphs16', '((GlyphWidth *)&fontGlyphs16[0].width)', 'see menuGlyphWidth',
      expr='glyphWidth[0].width', pair=('glyphWidth[7].width', 'fontGlyphs16[7].width'), use=('index', 'fontGlyphs16', 0),
      includes=['hiscores.h', 'hud.h'], decls=['extern FontGlyph fontGlyphs16[];'], container_decl='#include "hud.h"\nextern FontGlyph fontGlyphs16[];'),
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
      use='ghostControl[0].thrust', decls=['extern ControlInput ghostControl[];'], includes=['control.h'], container_decl='extern ControlInput ghostControl[];'),
    A('ghostStickY', 'ghostControl', '(*(s8 *)&ghostControl[0].steering)', 'see ghostButtons',
      use='ghostControl[0].steering', decls=['extern ControlInput ghostControl[];'], includes=['control.h'], container_decl='extern ControlInput ghostControl[];'),
    # Per row the crack code uses two A and two B polygons: B is A + 64 bytes within a row of 128.
    A('hiMedCrackPolysB', 'hiMedCrackPolysA', '((PolyFT3 (*)[4])&hiMedCrackPolysA[0][2])',
      'hiMedCrackPolysA[i][screen] and hiMedCrackPolysB[i][screen] (crack.c) are 64 bytes apart and the rows 128 bytes: B shares the table',
      expr='hiMedCrackPolysB[0][0].tag', pair=('hiMedCrackPolysB[3][1].tag', 'hiMedCrackPolysA[3][3].tag'),
      use=('rewrite', [('crack.c', 'hiMedCrackPolysB[i][CurrentScreen]', 'hiMedCrackPolysA[i][CurrentScreen + 2]')]),
      includes=['crack.h'], decls=['extern PolyFT3 hiMedCrackPolysA[][4];'], container_decl='extern PolyFT3 hiMedCrackPolysA[][4];'),
    # The ship models are loaded into shipShapeTable; the two sharks are slots 2 and 4, the rescue droid 30.
    A('lightDroid', 'shipShapeTable', 'shipShapeTable[30]',
      'race() loads shipShapeTable + 120 (slot 30, the rescue droid model, RescueDroidModel) without index and nothing stores it by name',
      pair=('lightDroid', 'shipShapeTable[30]'), decls=['extern Object *shipShapeTable[];'], container_decl='extern Object *shipShapeTable[];'),
    A('sharkBodyModel', 'shipShapeTable', 'shipShapeTable[2]',
      'UpdateSharkModel loads shipShapeTable + 8 and CopySharkVerts + 16 without index: slots 2 and 4',
      decls=['extern Object *shipShapeTable[];'], container_decl='extern Object *shipShapeTable[];'),
    A('sharkLegModel', 'shipShapeTable', 'shipShapeTable[4]', 'see sharkBodyModel',
      decls=['extern Object *shipShapeTable[];'], container_decl='extern Object *shipShapeTable[];'),
    # The pad driver's packet: header, buttons, sticks. RadiusControl reads the buttons halfword on its own.
    A('debugPadInput', 'padBuf1', 'padBuf1[0].buttons',
      'RadiusControl loads padBuf1 + 2 and NegConConfig the byte at +4 (the packet\'s buttons and steering fields)',
      includes=['pad.h'], decls=['extern PadPacket padBuf1[];'], container_decl='#include "pad.h"'),
    # doSerialGame sends the player's ControlInput as a link packet whose fields it names control, padHead,
    # padButtons, padPressed, aux0, aux1 (menus.c, combat.c): the halfwords at +2, +4, +6 and the bytes at +8, +9.
    A('linkPadHead', 'playerControl', 'playerControl[0].buttons',
      'halfword loads at playerControl + 2, +4, +6 and byte loads at +8, +9 (doSerialGame, CheckForButtonPress): the ControlInput fields after head',
      includes=['control.h'], decls=['extern ControlInput playerControl[];'], container_decl='extern ControlInput playerControl[];'),
    A('linkPadAux0', 'playerControl', 'playerControl[0].thrust', 'see linkPadHead',
      includes=['control.h'], decls=['extern ControlInput playerControl[];'], container_decl='extern ControlInput playerControl[];'),
    A('linkPadAux1', 'playerControl', 'playerControl[0].steering', 'see linkPadHead',
      includes=['control.h'], decls=['extern ControlInput playerControl[];'], container_decl='extern ControlInput playerControl[];'),
    # The menus and the HUD name the first texture slots after the screens that use them; the loader fills them
    # as one table (800 slots up to the end of menuTexturePtrsB) and the code indexes each sub-table from its name.
    A('menuTexturePtrsD', 'textureTable', '(&textureTable[2])',
      'AudioConfig loads textureTable + 8; the pointer tables are 4 bytes apart and the last one runs to the 800th slot',
      expr='menuTexturePtrsD[0]', pair=('menuTexturePtrsD[3]', 'textureTable[5]'), use=('index', 'textureTable', 2),
      decls=['extern Texture *textureTable[];'], container_decl='extern Texture *textureTable[];'),
    A('deleteTextures', 'textureTable', '(&textureTable[3])', 'see menuTexturePtrsD',
      expr='deleteTextures[0]', pair=('deleteTextures[3]', 'textureTable[6]'), use=('index', 'textureTable', 3),
      decls=['extern Texture *textureTable[];'], container_decl='extern Texture *textureTable[];'),
    A('endTextures', 'textureTable', '(&textureTable[4])', 'see menuTexturePtrsD',
      expr='endTextures[0]', pair=('endTextures[3]', 'textureTable[7]'), use=('index', 'textureTable', 4),
      decls=['extern Texture *textureTable[];'], container_decl='extern Texture *textureTable[];'),
    A('menuTexturePtrsC', 'textureTable', '((void **)&textureTable[5])', 'see menuTexturePtrsD',
      expr='menuTexturePtrsC[0]', pair=('menuTexturePtrsC[3]', 'textureTable[8]'), use=('index', 'textureTable', 5),
      decls=['extern Texture *textureTable[];'], container_decl='extern Texture *textureTable[];'),
    A('menuTexturePtrs', 'textureTable', '((void **)&textureTable[6])', 'see menuTexturePtrsD',
      expr='menuTexturePtrs[0]', pair=('menuTexturePtrs[3]', 'textureTable[9]'), use=('index', 'textureTable', 6),
      decls=['extern Texture *textureTable[];'], container_decl='extern Texture *textureTable[];'),
    A('menuTexturePtrsB', 'textureTable', '(&textureTable[7])', 'see menuTexturePtrsD',
      expr='menuTexturePtrsB[0]', pair=('menuTexturePtrsB[3]', 'textureTable[10]'), use=('index', 'textureTable', 7),
      decls=['extern Texture *textureTable[];'], container_decl='extern Texture *textureTable[];'),
    # initShipData gives each of the 15 ships its grid position (the loop runs i < 15); the last three
    # entries, for the Phantom class, are set by name.
    A('phantomGridPos', 'gridPosition', '(&gridPosition[12])',
      'ships.c: for (i = 0; i < 15; i++) position = gridPosition[i]; phantomGridPos stores +0, +2, +4 of the entries 12..14',
      expr='phantomGridPos[0]', pair=('phantomGridPos[2]', 'gridPosition[14]'), use=('index', 'gridPosition', 12),
      decls=['extern short gridPosition[];'], container_decl='extern short gridPosition[];'),
    # The pause menu is one record: its two-player stage, the answer to the confirmation prompt and the choice
    # are the bytes after its five words (PauseMenu.stage, .confirmed, .confirmChoice at +23, +24, +25).
    A('twoPlayerPauseStatus', 'pauseState', 'pauseState.stage',
      'race() loads pauseState + 23 and stores +24, +25 as bytes; the PauseMenu fields stage, confirmed, confirmChoice sit there',
      includes=['ayrace.h'], decls=['extern struct PauseMenu pauseState;']),
    A('twoPlayerMode', 'pauseState', '(*(signed char *)&pauseState.confirmed)', 'see twoPlayerPauseStatus',
      use='pauseState.confirmed', includes=['ayrace.h'], decls=['extern struct PauseMenu pauseState;']),
    A('twoPlayerFlag', 'pauseState', '(*(signed char *)&pauseState.confirmChoice)', 'see twoPlayerPauseStatus',
      use='pauseState.confirmChoice', includes=['ayrace.h'], decls=['extern struct PauseMenu pauseState;']),
    # The score tab record (HiScoreData, 72 bytes) is followed on the console by one byte of its own, hudDisabled
    # at +36, which is HiScoreData.highScore.
    A('hudDisabled', 'scoreTabData', 'scoreTabData.highScore',
      'Restart stores and race() loads the byte at scoreTabData + 36 = HiScoreData.highScore; the record runs up to PrimitiveBuffer0',
      includes=['hiscores.h'], decls=['extern HiScoreData scoreTabData;'], container_decl='extern HiScoreData scoreTabData;'),
    # The mono-rail record (48 bytes) has three words of its padding that InitTrainLights and the train muzzle code
    # name on their own (+20, +24, +28).
    A('trainMuzzleX', 'monoRail', 'monoRail.pad10[1]',
      'the train muzzle position is stored as the words at monoRailFlag + 20, +24, +28, inside MonoRailData.pad10',
      includes=['effects.h'], decls=['extern MonoRailData monoRail;']),
    A('trainMuzzleY', 'monoRail', 'monoRail.pad10[2]', 'see trainMuzzleX', includes=['effects.h'], decls=['extern MonoRailData monoRail;']),
    A('trainMuzzleZ', 'monoRail', 'monoRail.pad10[3]', 'see trainMuzzleX', includes=['effects.h'], decls=['extern MonoRailData monoRail;']),
]

# Names the sources used to declare that the build branch has since rewritten (the field, the element or
# the container itself is used directly): they are no longer declared, so nothing defines or aliases
# them. Kept as the record of what the machine code says about each: name -> (container, evidence).
RESOLVED = {
    'dispOffX0': ('DispEnv0', 'halfword stores at DispEnv0 + 8 and + 10 only (ShakeScreen, InitGameDisplay...): DISPENV.screen.x and .y'),
    'dispOffY0': ('DispEnv0', 'see dispOffX0'),
    'dispOffX1': ('DispEnv1', 'see dispOffX0'),
    'dispOffY1': ('DispEnv1', 'see dispOffX0'),
    'cardUsed': ('cardSlots', 'InitCard stores cardSlots + 10 with strides (1, 90): the used byte of each 90-byte entry'),
    'cardFileNames': ('cardSlots', 'cardSlots + 11: the 32-byte file name of the entry'),
    'cardNames': ('cardSlots', 'cardSlots + 43: the label of the entry'),
    'cardFlags': ('cardSlots', 'cardSlots + 75 with strides (90, 1350) in DupFileCheck, LoadSave, DisplayCardBack: the type byte of the entries'),
    'quadClutField': ('quadPool', 'AddText/AddTime store quadPool + 14 and + 22 with stride 40: the clut and tpage fields of the 600 POLY_FT4s'),
    'quadTPageField': ('quadPool', 'see quadClutField'),
    'racingLine': ('raceLines', 'racingLine[i] is accessed at raceLines + 4 + 16 * i (x, y, z): the same 16-byte stride'),
    'shipOrder': ('shipRank', 'shipOrder[i][0] is shipRank + 2 + 4 * i = shipRank[i].shipNo (SortAllShips, getShipBehindLAP)'),
    'raceResetWord0': ('savedLapTimes', 'main() stores savedLapTimes + 0, 4, 8, 12, 16 in a row; hud.c indexes savedLapTimes[lap] (5 words up to recordBuffer)'),
    'screenFill1': ('screenFills', 'VsyncFunc loads screenFills + 4 * screen; InitPop and InitResidentMenu store +0 and +4 one after the other'),
    'TrackLoPolysAlt': ('TrackLoPolys', 'see TrackHiPolysAlt (AsmTrackLo)'),
    'smokeColourGreen': ('smokeColour', 'smokeColour is a CVECTOR (loaded as one word by TrackSpecificFunctions); race() stores bytes at +0, +1, +2'),
    'smokeColourBlue': ('smokeColour', 'see smokeColourGreen'),
    'raceCheckpointResults': ('checkCells', 'strides (6, 36, 144) from checkCells + 2 in race() and SetUpFirstCheckPoint: the bonusSeconds field of the 6-byte cells'),
    'raceCheckpointFlags': ('checkCells', 'strides (6, 36, 144) from checkCells + 4: the done field of the 6-byte cells'),
    'StaticRVertY': ('StaticRVert', 'DrawEnvMap loads +0, +4, +8 of StaticRVert with stride 16 and the three names are 4 bytes apart'),
    'StaticRVertZ': ('StaticRVert', 'see StaticRVertY'),
    'recordKeyCount': ('recordBuffer', 'RecordKeypresses and InitGhostShip access recordBuffer + 4 as a word; the buffer is 0x104 header bytes then the keys'),
    'recordKeyTotal': ('recordBuffer', 'see recordKeyCount'),
    'recordControl': ('recordBuffer', 'see recordKeyCount'),
    'recordDisabled': ('recordBuffer', 'see recordKeyCount'),
    'recordMagic': ('recordBuffer', 'see recordKeyCount'),
    'recordTrackNum': ('recordBuffer', 'see recordKeyCount'),
    'recordShipClass': ('recordBuffer', 'see recordKeyCount'),
    'replayKeyCount': ('replayBuffer', 'same layout as recordBuffer (it is a copy of one)'),
    'replayKeyTotal': ('replayBuffer', 'see replayKeyCount'),
    'replayControl': ('replayBuffer', 'see replayKeyCount'),
    'replayDisabled': ('replayBuffer', 'see replayKeyCount'),
    'replayMagic': ('replayBuffer', 'see replayKeyCount'),
    'replayTrackNum': ('replayBuffer', 'see replayKeyCount'),
    'replayShipClass': ('replayBuffer', 'see replayKeyCount'),
    'fadeTileRed': ('fadeTile', 'UpdateFadeScreen stores bytes at fadeTile + 4, 5, 6: the colour of the first TILE'),
    'fadeTileGreen': ('fadeTile', 'see fadeTileRed'),
    'fadeTileBlue': ('fadeTile', 'see fadeTileRed'),
    'progLoadAddr': ('progHeader', 'LoadProg loads progHeader + 24 and + 28 (t_addr, t_size of the 136-byte PS-X EXE header it just read)'),
    'progSize': ('progHeader', 'see progLoadAddr'),
    'linkPadButtons': ('playerControl', 'see linkPadHead'),
    'linkPadPressed': ('playerControl', 'see linkPadHead (the two pad bytes at +6)'),
    'remotePadHead': ('remoteControl', 'doSerialGame stores the received packet at remoteControl + 0, +2, +4, +6 (halfwords), +8, +9 (bytes)'),
    'remotePadButtons': ('remoteControl', 'see remotePadHead'),
    'remotePadPressed': ('remoteControl', 'see remotePadHead'),
    'remotePadAux0': ('remoteControl', 'see remotePadHead'),
    'remotePadAux1': ('remoteControl', 'see remotePadHead'),
    'raceResetWord1': ('savedLapTimes', 'see raceResetWord0'),
    'raceResetWord2': ('savedLapTimes', 'see raceResetWord0'),
    'raceResetWord3': ('savedLapTimes', 'see raceResetWord0'),
    'waterfallSaved': ('waterfallBuf', 'effects.c passes waterfallSaved - 4 (= waterfallBuf) to LoadImage; the three names are contiguous (+8, +30)'),
    'waterfallRow': ('waterfallBuf', 'see waterfallSaved'),
    'TrackHiPolysAlt': ('TrackHiPolys', 'AsmTrackHi loads TrackHiPolys + 4 * screen (stride 4); trackasm.c allocates TrackHiPolys and the pointer 4 bytes after it'),
    'TrackMedPolysAlt': ('TrackMedPolys', 'see TrackHiPolysAlt (AsmTrackMed)'),
}

# Globals the sources declare under a name the console's tables do not have: the build branch made one object
# of several symbols. name -> (the symbol the object starts at, why).
RENAMED = {
    'raceCamera': ('residentSkeletons', 'residentSkeletons (the camera skeleton pointer) and cameraSections (the 24 bytes after it) are the '
                                         'one TrackCamera that cameraPtr points at and CameraUpdate and DrawShips take'),
    'monoRail': ('monoRailFlag', 'the mono-rail carriage state: its first field, the step index, was monoRailFlag; trainMuzzleX/Y/Z follow it'),
}

# Symbols defined under another C name: the executable's symbol -> the identifier in the data file.
RENAMES = {}

# Symbols declared with different types in different files: use the declaration with this spelling.
PREFER = {
    'padBuf1': 'PadPacket',        # the 8-byte packet the pad driver fills (pad.c declares it as bytes)
}

# Entries of incomplete arrays that the extent cannot give because the names inside them cut it short.
SIZES = {
    'gridPosition': (15, 'one entry per ship (initShipData loops i < 15); the Phantom class entries 12..14 are phantomGridPos'),
    'TrackHiPolys': (2, 'one polygon pool per screen: AsmTrackHi picks TrackHiPolys[screen]; trackasm.c calls the second TrackHiPolysAlt'),
    'TrackMedPolys': (2, 'one polygon pool per screen (see TrackHiPolys)'),
    'TrackLoPolys': (2, 'one polygon pool per screen (see TrackHiPolys)'),
    'tileSurfaces': (24, 'indexed by the face texture number (crack.c, trackasm.c, AsmLowResTile...): 24 textures of 2 surfaces, 252 bytes each; the object renderers keep their projected vertices in the 3584 bytes that follow, up to muzzleright (no face of any track uses a texture above 14)'),
    'textureTable': (800, 'the slots run up to the end of menuTexturePtrsB (3172 bytes at slot 7): 800 pointers'),
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
}

# Pointers that hold an address of the console's RAM outside the program's image (the area above the end of
# .bss): written as 0, they mean nothing to a native program. name -> why.
ABSOLUTE = {
}

SYNTHETIC = []

SEPARATE = {}

ARENAS = {}
