#ifndef GLOBALS_H
#define GLOBALS_H

/* Declarations of globals that more than one source file uses under the same type.  The
   console linked every file against one namespace without comparing declared types; natively a
   disagreement (pointer vs int, array vs scalar) is a bug, so each shared global is declared
   once here (tools/xdecl.py checks the whole tree).  Included by functions.h. */

/* --- group 1 (menus.c) --- */

extern HeapBlock       *heap;
extern HeapBlock       *mainheapcopy;
/* The race camera.  The console's symbols residentSkeletons (its first word: the camera skeleton) and
   cameraSections (the following 24 bytes) are the one TrackCamera that cameraPtr points at and that
   CameraUpdate and DrawShips take.  (Symbols 0x800BD2E0 and 0x800BD2E4.) */
extern TrackCamera      raceCamera;
extern Skeleton        *upperSkeleton;
extern Object          *shipShapeTable[];
extern unsigned short   TextureTableCount;
extern short            highResTable[];
extern unsigned char    cheatWeapon;
extern unsigned char    cheatDamage;
extern unsigned char    cheatRain;
extern unsigned char    cheatTime;
extern unsigned char    cheatMinigun;
extern unsigned char    continuesLeft;
extern unsigned char    gameType;
extern unsigned char    sillyShips;
extern unsigned char    menuSendFlags;
extern short            menuSendKey;
extern short            menuTextures;
extern HiScore          hiScoreTable[][4];

/* --- group 2 (weapon.c, effects.c) --- */

extern int              ambientVoice2;
extern u_char           classLaps[];
/* The mono-rail carriage state (its first field, the step index, was the console symbol monoRailFlag). */
extern MonoRailData     monoRail;
extern Object          *monoRailObjs[];
extern PrimPtr          nextPrim;
extern short            numTrackSections;
extern SParticle       *particleSlots;
extern u_char           perfectLap;
extern VECTOR           posMonoRail[];
extern short            remoteRacePos;
extern int              shipTargetIndex[];
extern WeaponShape     *weaponBars;
extern POLY_FT4         weaponIconPrims[];
extern WeaponData      *weaponIndex;
extern Object          *zeppelinObj;

/* --- group 3 (hud.c, sparks.c, trails.c, camera.c) --- */

extern CameraUpdateFunc CameraUpdate;
extern TrackCamera     *cameraPtr;
extern short            finishPosition;
extern int              linkSendFlag;
extern u_long          *OT[];
extern u_long          *OT2[];
/* The in-race pause menu state (the console symbol pauseState is its first word, selection). */
extern struct PauseMenu pauseState;
extern short            playerPosition;
extern HeapBlock       *reserveHeap;
/* The team ship's model: LoadModel stores the objects of the model file here (each ship file has one
   object); DrawTeamShips and its callers use the first. */
extern Object          *teamShip[];
extern ConfigData      *tempGameData;
extern WADFILE         *textureWad;

/* --- group 4 (collide.c, combat.c, control.c, crack.c, credits.c, display.c, draw.c, droid.c, drone.c, dynam.c, error.c) --- */

extern signed char      activateOT2;
extern u_short          clipLevel;
extern int              controlledSfxId;
/* The screen offsets are DispEnv0.screen.x/y and DispEnv1.screen.x/y (the console had them as the
   separate names dispOffX0, dispOffY0, dispOffX1 and dispOffY1 for the same storage). */
extern DISPENV          DispEnv0;
extern DISPENV          DispEnv1;

/* --- group 5 (card.c, ayrace.c, hiscores.c, ghost.c, init.c, iso9660.c, lzss.c, file.c, malloc.c, object.c, options.c, pad.c) --- */

extern Texture         *cardIconInfo;
extern Texture         *continuesTexture;
extern u_short          keysPressed;
extern u_short          padType;
extern ControlInput     playerControl[];
extern ControlInput     remoteControl[];
extern u_char           raceCompleteFlag;
extern struct CardKeys  savedKeyConfig;
extern char             scenePath[];
extern u_short          shipClassIndex;

/* --- group 6 (progress.c, raceinit.c, radscn.c, scene.c, search.c, sfx.c, ships.c, skeleton.c, sound.c, tim.c, track.c, trackasm.c, ttf.c, utils.c, wadfile.c) --- */

/* The load screen's state (defined in progress.c, used by wadfile.c). */
extern short     loadFrame;
extern WADFILE  *currentWad;
extern int       lastWadCurrent;
extern int       loadTicks;
extern LINE_F2  *linePrims;
extern int       waveIndex;
extern BLK_FILL *screenFills[2];
extern s32      *particleX;

/* Where the next texture without a VRAM position is placed (tim.c, init.c, main.c). */
extern short     gClutX;
extern short     gClutY;
extern short     gTextureX;
extern short     gTextureY;

extern u_char   cdMusicTable[];

/* --- shared by several groups (add here, alphabetical) --- */

#endif
