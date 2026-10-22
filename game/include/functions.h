#ifndef FUNCTIONS_H
#define FUNCTIONS_H

/* Prototypes for every game function, grouped by source file. */

#include "wipeout.h"
#include "scene.h"
#include "ships.h"
#include "control.h"

typedef struct CdlPos CdlPos;
typedef struct CheckPoints CheckPoints;
typedef struct DisplayTexPrims DisplayTexPrims;
typedef struct HiScoreData HiScoreData;
typedef struct MenuBox MenuBox;
typedef struct MenuPrims MenuPrims;
typedef struct MonoRailData MonoRailData;
typedef struct ResetData ResetData;
typedef struct SParticle SParticle;
typedef struct TexTemplate TexTemplate;
typedef struct Trail Trail;
typedef struct WeaponData WeaponData;
typedef struct WeaponGrid WeaponGrid;
typedef struct WeaponShape WeaponShape;
struct WeaponGridZone;
typedef struct WipeCdStatus WipeCdStatus;
typedef struct SpecialGrid SpecialGrid;
typedef struct AutoMeshData AutoMeshData;
typedef struct Checkpoint Checkpoint;
typedef struct DroidData DroidData;
typedef struct Shadow Shadow;
#include "menus.h"
struct Texture;
struct Speedo;
#include "iso9660.h"
#include "malloc.h"
#include "object2.h"
struct SelectBox;
#include "tim.h"
#include "port_platform.h"
struct InGameMenu;

struct PauseMenu;

/* main.c */
int PsxMain(void); /* Port: the game's main(); the platform owns the program entry point */

/* object.c */
void AsmDrawObjectLit(Object *object, Skeleton *camera, int roll, CVECTOR *colour);
void InitMatrix(MATRIX *m);
void InitialiseAmbientColour(void);
void InitialiseColourMatrix(void);
void InitialiseLightMatrix(void);
void MatrixMultiply(MATRIX *m1, MATRIX *m2, MATRIX *m3);
void SetCameraDirection(short x, short y, short z);
void SetCameraPosition(int x, int y, int z);
void SetupLights(Object *lights);
int DotProduct(VECTOR *a, VECTOR *b);
void DrawEnvMap(Object *object, void *unused, SVECTOR *normals, int xOffsetIn, int yOffset, int scale);
void InitFonts(void);
void MyCrossProduct(VECTOR *a, VECTOR *b);
void Normalise(VECTOR *vector);
void PreCalcNormals(Object *object, SVECTOR *normals);

/* malloc.c */
void *DAlloc(HeapBlock *head, int size);
void DFree(HeapBlock *head, char *ptr);
void InitDynamicMem(HeapBlock *head, int size);

/* file.c */
int FileLength(char *name);
int LoadFile(char *name, char *buffer);
void SaveFile(char *name, char *buffer, int length);
void InitCDFS(HeapBlock *heap);

/* tim.c */
Texture *LoadTexture(char *name, short translucent);
Texture *WADLoadTexture(WADFILE *wad, short translucent);
Texture *loadtexcom(Tim *tim, short translucent);

/* skeleton.c */
MATRIX * GetSkeletonMatrix(Skeleton *skeleton);
Skeleton * NewSkeleton(void);
void SetSkeletonDirection(Skeleton *skeleton, short xa, short ya, short za);
void SetSkeletonDirectionHPR(Skeleton *skeleton, short hdg, short pitch, short roll);
void SetSkeletonDirectionHRP(Skeleton *skeleton, short hdg, short roll, short pitch);
void SetSkeletonDirectionRPH(Skeleton *skeleton, short roll, short pitch, short hdg);
void SetSkeletonPosition(Skeleton *skeleton, int x, int y, int z);
void SetSkeletonTransform(Skeleton *skeleton);

/* track.c */
void CheckCracks(void);
void CheckVersion(void);
void DrawAllTrack(TrackCamera *camera, AutoMeshData *autoMesh);
void DrawNewTrack(TrackCamera *camera, AutoMeshData *autoMesh);
void IndexToPointers(void);
void IntelFace(void);
void IntelSection(void);
void IntelVertex(void);
TrackSection * NextIndexSection(TrackSection *section);
void PointersToIndex(void);
TrackSection * PrevIndexSection(TrackSection *section);
void WADInitFaces(WADFILE *wad);
void WADInitSections(WADFILE *wad);
void WADInitVertices(WADFILE *wad);
void WADInitViewList(WADFILE *wad);

/* draw.c */
void DrawLandscape(ShipData *playerShip, TrackCamera *camera);
char TrackDirection(TrackCamera *camera);

/* ttf.c */
TexTemplate * LoadTtfFile(char *file, short *count, short libraryTextures);
TexTemplate * WADLoadTtfFile(WADFILE *wad, short *count);

/* camera.c */
void InitCamera(TrackCamera *camera, TrackSection *section);
void SetUnitFinVector(VECTOR *unitFinVector, ShipData *camShip);
void UpdateCameraAttractCircle(TrackCamera *camera, ShipData *camShip, RescueData *rescueDroid);
void UpdateCameraAttractDestroyed(TrackCamera *camera, ShipData *camShip, RescueData *rescueDroid);
void UpdateCameraAttractInternal(TrackCamera *camera, ShipData *camShip, RescueData *rescueDroid);
void UpdateCameraAttractMedal(TrackCamera *camera, ShipData *camShip, RescueData *rescueDroid);
void UpdateCameraAttractMode(TrackCamera *camera, ShipData *camShip, RescueData *rescueDroid);
void UpdateCameraDroidView(TrackCamera *camera, ShipData *camShip, RescueData *rescueDroid);
void UpdateCameraRaceExternal(TrackCamera *camera, ShipData *camShip, RescueData *rescueDroid);
void UpdateCameraRaceInternal(TrackCamera *camera, ShipData *camShip, RescueData *rescueDroid);
void UpdateCameraRaceStart(TrackCamera *camera, ShipData *camShip, RescueData *rescueDroid);
void UpdateCameraStaticFollow(TrackCamera *camera, ShipData *camShip, RescueData *rescueDroid);

/* ships.c */
void CopySharkVerts(Object *ship);
void DrawShips(ShipData *shipIndex, Object **shipShapes, int camShip, TrackCamera camera, int flg);
void InitTransShip(Object *lights, short flag);
void SetSpread(ShipData *shipIndex, ConfigData *snd, int vol, short idx, short a4);
void UpdateSharkModel(void);
void initShipData(TrackSection *section, ShipData *shipIndex, ConfigData *options);
void loadShipData(TrackSection *section, ShipData *shipIndex, Object **shipShapes, Skeleton *camPos);

/* control.c */
void GeneralStartProcs(ShipData *s);
void SetUpFirstCheckPoint(void);
void SortAllShips(ShipData *shipIndex);
void UpdatePlayerShip(ShipData *ships, Object **shapes, int shipNo, WeaponData *weapons, ControlInput *control);
void UpdatePlayerShipStart(ShipData *shipIndex, Object **shipShapes, int shipNo, WeaponData *weaponIndex, ControlInput *controller);
void UpdateShips(ShipData *shipIndex, WeaponShape *unused, Object **shipShapes, WeaponData *weaponIndex);

/* utils.c */
int abs(int arg0);
void CrossProduct(VECTOR *out, VECTOR a, VECTOR b);
int GetAng(VECTOR vec1, VECTOR vec2);
int GetMagnitude(VECTOR vec1);
void MinusVector(VECTOR v1, VECTOR v2, VECTOR *result);
int ScalarProduct(VECTOR v, VECTOR w);
int ang(int angle);
int arccos(int value);
void assign(VECTOR *out, VECTOR v);
int newRand(void);
int sar(int a, int n);
int scalar_product(int *a, int *b);

/* search.c */
TrackSection * FindNearestSection(TrackSection *trk, VECTOR pos);
int SectionSearch(ShipData *playerShip);

/* scene.c */
void IntelLong(int *lvalue);
short IntelPrim(Prm prm);
void IntelShort(short *svalue);
Object * LoadPrmCommon(WADFILE *wad, char *name, short textureStart);
Object * LoadPrm(char *arg0, short arg1);
Object * NewInstance(Object *source);
void * WADLoadPrm(WADFILE *wad, short textureStart);

/* weapon.c */
void FireWeapon(ShipData *ship, WeaponData *weapons, int owner, u_char type);
void InsertWeapon(ShipData *ship, WeaponData *weapons, int owner, u_char type);
void UpdateWeapons(ShipData *ships, WeaponShape *shapes, WeaponData *weapons);
void CheckPitShips(void);
void DamageCollide(int damageType, int shipNo);
void FlipShip(ShipData *playerShip);
void GetCheatWeapon(ShipData *ships, int i);
void UpdateFadeScreen(void);
void UpdatePlasma(ShipData *shipIndex, WeaponShape *weaponShapes, WeaponData *weapon, WeaponData *weaponIndex);
int randWep(void);
void BumpDown(TrackSection *bump);
void BumpFlatten(TrackSection *bump);
void BumpUp(TrackSection *bump);
void ClearWeaponTable(void);
void CycleWeaponGrids(ShipData *ships, struct WeaponGridZone *grids, int noGrids);
void FlattenTrack(void);
void GetFireVector(VECTOR *result, VECTOR target, Face *facePtr, VECTOR shipPos);
void GetNewDroneWeapon(ShipData *ships, int shipNo);
void GetNewWeapon(ShipData *ships, int shipNo);
void InitElecBolt(ShipData *ships, WeaponShape *shapes, WeaponData *weapon, WeaponData *weapons);
void InitMine(ShipData *ships, WeaponShape *shapes, WeaponData *weapon, WeaponData *weapons);
void InitMinigun(ShipData *ships, WeaponShape *shapes, WeaponData *weapon, WeaponData *weapons);
void InitMissile(ShipData *ships, WeaponShape *shapes, WeaponData *weapon, WeaponData *weapons);
void InitPlasma(ShipData *ships, WeaponShape *shapes, WeaponData *weapon, WeaponData *weapons);
void InitQuake(ShipData *ships, WeaponShape *shapes, WeaponData *weapon, WeaponData *weapons);
void InitRocket(ShipData *ships, WeaponShape *shapes, WeaponData *weapon, WeaponData *weapons);
void InitRocketLeft(ShipData *ships, WeaponShape *shapes, WeaponData *weapon, WeaponData *weapons);
void InitRocketRight(ShipData *ships, WeaponShape *shapes, WeaponData *weapon, WeaponData *weapons);
void InitShield(ShipData *ships, WeaponShape *shapes, WeaponData *weapon, WeaponData *weapons);
void InitSmartBomb(ShipData *ships, WeaponShape *shapes, WeaponData *weapon, WeaponData *weapons);
void InitialiseBumps(void);
void KillMines(WeaponData *ship, WeaponData *bumps, WeaponShape *bars);
void MissileLock(int shipNo, ShipData *ships);
void RearMissileLock(int shipNo, ShipData *ships);
void TransformTransparentObject(Object *object, void *camera, int shieldID);
void UpdateElecBolt(ShipData *ships, WeaponShape *shapes, WeaponData *weapon, WeaponData *weapons);
void UpdateMine(ShipData *ships, WeaponShape *shapes, WeaponData *weapon, WeaponData *weapons);
void UpdateMinigun(ShipData *ships, WeaponShape *mineData, WeaponData *weapon, WeaponData *objects);
void UpdateMissile(ShipData *ships, WeaponShape *shapes, WeaponData *weapon, WeaponData *weapons);
void UpdateQuake(ShipData *ships, WeaponShape *unused, WeaponData *weapon, WeaponData *weapons);
void UpdateRocket(ShipData *shipIndex, WeaponShape *weaponShapes, WeaponData *weapon, WeaponData *weaponIndex);
void UpdateShield(ShipData *ships, WeaponShape *shapes, WeaponData *weapon, WeaponData *weapons);
void UpdateSmartBomb(ShipData *ships, WeaponShape *shapes, WeaponData *weapon, WeaponData *weapons);
void Weapon2Mine(WeaponData *weapon, WeaponData *weaponIndex, WeaponShape *weaponShapes);
void initWeapons(WeaponShape *shapes, Skeleton *camPos, WeaponData *weapons);

/* dynam.c */
int SetTrackAttributes(TrackSection *section, short sectionNo, WeaponGrid *weaponGrid, int noWeaponGrids, short *specialCount, SpecialGrid *specialGrid);
void SetUnitVectors(ShipData *shipIndex);
void GetTargetPos(VECTOR *result, VECTOR pos, Vector cent1, Vector cent2);
void ShipTrkReaction(ShipData *playerShip, Object **shipShapes);
void getWorldPoint(VECTOR *output, Object *object, Object *objectCollide, int pointNo);

/* collide.c */
int AngleOfCollision(SVector normal, VECTOR ship_point);
void BeamCollideReaction(ShipData *playerShip);
int CheckPolygonCollision(Face *facePtr, VECTOR pos, int alpha, int side);
void DroneShip2TrackBase(ShipData *playerShip, Face *facePtr, int alpha);
int NewCollideLineToPlane(Object *object, Object **shipShapes, ShipData *shipIndex, int shipNo);
void NewWingCollision(ShipData *playerShip, Face *facePtr, int direction, int alpha);
void NoseCollision(ShipData *playerShip, Face *facePtr, int direction, int alpha);
int PlaneLineCollision(VECTOR plane_point, VECTOR ship_pos, SVector normal);
void Reflection(VECTOR *incidence, SVector normal);
void Ship2ShipCollision(ShipData *shipIndex, Object **shipShapes, int i, int j);
void Ship2Track(ShipData *playerShip, Face *facePtr, Object **shipShapes);
void Ship2TrackBase(ShipData *playerShip, Face *facePtr, int alpha);
void WingCollision(ShipData *playerShip, Face *facePtr, int direction, int alpha);
int collideLineToPlane(int shipNo1, int shipNo2, Object **shipShapes, ShipData *shipIndex);
void doShipShipCollision(int shipNo, ShipData *shipIndex, Object **shipShapes);

/* hud.c */
void AddDelEnd(unsigned char isDelete, int x, int y, int height, int colour);
void AddJustifyNumber(int number, int xInput, int y, int size, int colour);
void AddNumber(int number, int x, int y, int size, int colour);
void AddText(unsigned char *textPointer, int x, int y, int size, int colour);
void AddTime(int time, int x, int y, int size, int colour, unsigned char flags);
void AssignMedals(ConfigData *config);
void BackPoly(POLY_F4 *selectBox, DR_MODE *selectTrans, short x, short y, short w, short h);
short CalcPosition(ShipData *ships);
void CentreText(unsigned char *text, int unusedX, int yInput, int sizeInput, int colour);
void CentreText2(unsigned char *text, int x, int y, int size, int colour);
void DisplayRaceInfo(ConfigData *state, HiScoreData *context, ShipData *scene);
void EndChallenge(ConfigData *config);
void InitScreenTex(Texture **textures, char *challengeName);
void InitSpeedo(struct Speedo *speedo);
void InitText(void);
void SetCharTextures(unsigned char size);
void UpdateRaceHud(ShipData *shipIndex, struct Speedo *speedo, int choice, ConfigData *gameData, HiScoreData *scoreData);
void copyInTempLapTimes(ShipData *ships);
void fill_speedo(struct Speedo *speedo, ShipData *ships, ConfigData *game, int player);
void AlterCheckPoint(CheckPoints *checkpoints, ShipData *shipIndex, int targetShip, ConfigData *gameData);
void DisplayContinues(Texture *texture, int x, int y);

/* raceinit.c */
void GetJoyPadPacketOnVsync(int arg0);
void InitAttractMode(int mode, ShipData *ships);
void InitCDAudio(ConfigData *menu, unsigned char *unused);
void InitCheckPoints(CheckPoints *checkpoints, ConfigData *config);
void InitInGameMenu(struct InGameMenu *menu);
void InitRaceData(struct PauseMenu *pause);
void InitRaceLine(TrackData *track, ConfigData *game);
void InitRaceResetData(struct ResetData *arg0, ShipData *arg1, ConfigData *arg2, WeaponData *arg3, WeaponShape *arg4, RescueData *arg5, Object **arg6, CheckPoints *arg7);
void InitShufflePlay(void);
void InitTrackTextures(ConfigData *fileId);
void InitWeaponGrids(WeaponGrid *weaponGrids, TrackData *context, int *noWeaponGrids, short *specialCount, SpecialGrid *specialGrid);
int myrand(void);

/* ayrace.c */
void Check2PlayerPause(u_char status, u_char *lastStatus, int *musicPaused);
int CheckAttractOver(ConfigData *config);
void CheckGamePause(ShipData *ships, int *pauseFlag, ConfigData *config, u_short *pauseChoice, int *pauseCounter);
void InitiateTwoPlayerPause(struct PauseMenu *pause);
void Perform2PlayerPauseCommand(struct PauseMenu *pause);
int PerformPauseOperations(struct PauseMenu *pause, struct SelectBox *box, ConfigData *config);
void QuitGame(void);
void RaceComplete(void *screen, void *unused, ConfigData *gameData, short *choice, HiScoreData *scoreData, ShipData *ships);
void Restart(ResetData *state);
void SelectPauseOption(int *selection, int canMove, ConfigData *config, struct PauseMenu *pause);
void CheckForButtonPress(void);
void CheckShipDestroyed(ShipData *ships, ConfigData *game, int *attract, int idleLimit);
void PlayCredits(int state);
void SetInGameCDTrack(signed char *bcd, struct WipeCdStatus *position, ConfigData *trackInfo, int *state);

/* racectrl.c */
void InitResident(void);
void ShowLinkBroken(void);
void TrackSpecificFunctions(void);
void race(ConfigData *gameData, int context, int scene);
void LoadModel(Object **modelShapes, char *fileName);
void AsmDrawObjectFixed(Object *object, Skeleton *camera, int scale);
void AsmDrawParticles(SParticle *particles, Skeleton *camera, int mode);
void racetidy(void);

/* init.c */
void GhostLoadCmpFiles(TIMlist *timPtr);
short InitScene(Object **objects, short *objectCount, Skeleton *camera, ConfigData *config);
void LoadCmpFiles(TIMlist *timPtr);
TIMlist * LoadCompressedTextureSequence(char *fileName);
struct Tim;
Texture *NewLoadTexture(struct Tim *tim, int context, int scene);
short WADInitScene(Object **objects, u_short *objectCount, Skeleton *camera, ConfigData *config, WADFILE *wad);
void getPath(char *trackPath, unsigned char trackNo, char *name);
TIMlist *WADLoadCompressedTextureSequence(WADFILE *wad);

/* sound.c */
void AdjustCrowdSound(Object **objects, int count);
void AdjustDirectionalNote(short voice, short unused1, short unused2, VECTOR pos);
void AdjustDopplerEngine(int shipNo, int channel, int unused);
void AdjustEngineSound(ShipData *ship, TrackCamera *camera);
void AdjustRemoteEngineSound(int ship1, int ship2, int extra2, int extra3);
int InitCD(CdlLOC *locs, int unused);
int InitCrowdSound(Object **prm, short prmCount, Object **destination, int count);
void PlayDirectionalNote(short sampleId, short pitch, short duration, VECTOR pos);
void SetPitch(short voice, int pitch);
void SetVolume(short voice, int volume);
void InitSpuSamples(void);
void InitSpuSound(void);
int KillControlledNote(short voice);
void LoadSample(int sampleId, char *name);
void PlayControlledNote(int sampleId, int volume, int lrPan, int fbPan, int pitch);
void PlayNote(int sampleId, short volume, short lrPan, short fbPan, int pitch);
void StartControlledSound(void);
void FindVagInVab(void);
void UpdateNotes(int unused);
void KillNote(void);
void KillNotes(void);
void KillFadeNotes(void);
void CdMusicPlay(int track, int unused);
void CdMusicStop(void);
void KillControlledSound(void);
void MuteControlledSound(void);
void TerminateSound(void);

/* options.c */
void DeSelectScreen(struct SelectBox *sel);
void InGameConfirm(u_char *confirmed, int mode, int unused, u_char *selection, int *frameCount);
void InGameReset(struct PauseMenu *pause, ConfigData *game, struct InGameMenu *polys);
void UpdateMusicBar(POLY_G4 bars[][2], ConfigData *game);
void UpdateSfxBar(POLY_G4 bars[][2], ConfigData *game);
char *GetCdTrackArtist(int track);
char *GetCdTrackName(int track);

/* radscn.c */
void CalcNewRadius(TrackSection *section, Object *objects);
void RadiusControl(TrackCamera *camera, Object **prm);
void SceneRadiusCheck(Object *objects);

/* error.c */
void Error(char *string, short errorLevel);

/* effects.c */
void AddEffects(WeaponData *weapons, ShipData *ships);
void AnimateSprite(Object **objects, short count, short speed, int first, short frames);
void AnimateTexture(Object **objects, short count, short period, short arg3, short arg4, CVECTOR colour, int *frame, int *arg7, short arg8);
void ColourDroidLights(Object *droid);
void ColourRedLight(Object *light);
void ColourStartBoom(int noToLight);
void DoNeonSign(Object **objects, int count);
void DrawShadow(void *shapes, ShipData *ships, Shadow *shadow);
void DrawWeaponIcons(POLY_FT4 *icons, Texture **textures, ShipData *ship);
void InitDroidThings(Object **objects, char *names, Skeleton *parent, Checkpoint *points, ConfigData *options);
void InitMonoData(Object **objects, short unused, volatile MonoRailData *mono);
void InitObject(Object **prm, short prmCount, Object **found, char *name, short len, short *count);
void InitRedLight(Object **prm, short prmCount, Object **redLight);
Shadow *InitShadow(void);
void InitShadowCluts(void);
void InitStartBoom(Object **prm, short prmCount);
void InitTrainLights(Object **unused0, short unused1, Object **objects, int mode);
void InitWaterFall(void);
void InitWeaponIcons(POLY_FT4 *icons, Texture **textures);
void LoadStartDroid(Object **objects, char *names, Skeleton *parent);
void LoadTrain(Object **arg0, char *arg1, Skeleton *arg2, VECTOR *arg3);
void MoveAdvert(Object **objects, int count);
void MoveCameraDroid(Object **objects, short count, ShipData *ships, int *bobCount);
void MoveFan(Object **objects, int count);
void MoveMonoRail(Object **objects, short count, MonoRailData *mono, ShipData *ships, int mode);
void MoveRadar(Object **objects, short count);
void MoveZeppelin(Object **objects, int unused, ShipData *ships);
void RedBullAnimate(Object **objects, short count, short arg2, short arg3, short arg6);
void ResetAnimatedObjects(void);
void ResetLightStart(void);
void ResetMonoRail(void);
void ResetStartBoom(void);
void ResetZeppelin(void);
void SetShake(int amount);
void ShakeScreen(void);
void UpdateLightStart(Object **objects, ShipData *ships, DroidData *droids, short textureBase, ConfigData *game, int mode);
void UpdateStartDroid(Object **shapes, ShipData *ships, DroidData *droids, short textureBase, ConfigData *game, int player);
void Video(Object **objects, int count);
void WaterFall(void);
void InitPitCycle(void);
void InitTrackCycle(void);

/* combat.c */
void _init_cont(void);
void DrawLinkSign(ShipData *ships);
void InitLinkUpSign(void);
int _error(void);
void doSerialGame(ConfigData *config, ShipData *ships);
void doSerialMenu(ConfigData *config, MenuSelection *selection);
int doSerialTransfer(u_char *send, u_char *recv, int count);
int receiveSerialPacket(int unused, u_char *data, int len);
int sendSerialPacket(int type, u_char *data, int count);
int _comb_control(int op, int reg, int value);

/* lzss.c */
void ExpandData(unsigned char *inData, unsigned char *outData);

/* iso9660.c */
void LogicalSectorToLoc(u_int sector, CdlLOC *loc);
u_char *NextPath(u_char *dirRecord);
void close_iso_events(void);
int find_file(char *filename, int loc, IsoFileInfo *fi);
void iso_close(int fd);
int iso_get_file_info(char *pathname, IsoFileInfo *fi);
int iso_init(HeapBlock *heap);
int iso_open(char *pathname, int flags);
int iso_read(void *buffer, int nsec, int fd);
int match_dir(char *dirname, char *testname, u_char test_len);
int match_file(char *filename, char *testname, u_char test_len);
int search(char *filename, IsoFileInfo *fi);
int iso_fileloc(int arg0);
int iso_filesize(int arg0);
u_char *iso_toc(void);
int my_error_handler(void);

/* menus.c */
void AudioConfig(ConfigData *gameData, MenuPolys *polys, MenuSelection *selection, MusicScreen *musicScreen);
int CheckCardFull(int card);
void ClassSelect(ConfigData *gameData, MenuSelection *selection);
void CopyGameData(ConfigData *gameData, MenuSelection *selection, short saveSelection, KeyConfig *keyConfig, HiScoreData *cardState);
u_char DecodePassword(ConfigData *gameData, u_char *password);
void DispHiScores(UnlockData *unlock, void *unused, unsigned char raceMode);
int DisplayCardBack(MenuPolys *polys, int cursorArg, int cardArg, SaveSelection *selectionArg);
void DisplayConfig(ConfigData *gameData, MenuSelection *selection, HiScoreData *score, MenuPolys *polys);
void DisplayPassword(ConfigData *gameData);
void DisplayTexture(Texture *texture, s32 left, s32 top, POLY_FT4 *polys);
void DoButton(short x, short y, int requestedIcon, char *text);
void DoConfirm(SaveSelection *selection, MenuPolys *polys, short messageType);
void DrawDrBox(Texture *unused, DrBox *box);
void InitDrBox(Texture *texture, DrBox *box);
void DisplayScreen(Texture *texture, DrBox *box);
void DrawHighLight(POLY_F4 (*prim)[2], int x, int y, int width, int height, short lineNum);
void DrawMusicScreen(MusicScreen *menu);
void DrawTeamShips(int x, int y, int z, int dir, int scale, Object *obj);
short DupFileCheck(SaveSelection *selection);
short EnterFileName(MenuPolys *polys, SaveSelection *selection);
char *GetKeyFunction(int index);
char * GetTrackName(short type, short x, short y, short draw);
void InitCard(int card);
void InitDefaultName(ConfigData *gameData, HiScoreData *data, char mode);
void EnterName(ConfigData *gameData, HiScoreData *data);
void InitGameData(ConfigData *gameData);
void InitKeyConfig(KeyConfig *config);
void InitMenuPolys(MenuPrims *polys);
void InitMenuTextures(u_short *arg0);
void InitMenus(void);
void InitMusicScreen(MusicScreen *musicScreen, int textureBase);
void InitScreenBox(MenuPolys *screen);
void InitTempData(ConfigData *gameData, MenuSelection *selection);
short IsTeamCardEmpty(int team);
LoadedImage *Load16BitTexture(char *fileName, int loadBothBuffers);
void LoadMenuLineData(void);
void InitLineEditor(void);
int LoadSaveIcons(char *fileName, int arg1, u_char *arg2, u_char *arg3, u_short *arg4, u_short *arg5, int arg6);
void MakePassword(ConfigData *gameData);
int MenuControl(ConfigData *gameData, void *menuTexture, int *state, int raceMemorySize);
void MenuFunction(void *gameData, void *polys, void *keyConfig, void *selection, void *cardState, void *musicScreen);
void Print(u_char *text, int x, int y, int size);
void PrintCX(u_char *text, int x, int y);
void ResetKeyConfig(MenuSelection *selection);
void ResetLoadMenu(SaveSelection *selection);
void ScreenConfig(ConfigData *gameData, MenuPolys *polys, short *screenPosition);
void ShipParameters(MenuSelection *selection, ConfigData *gameData, void *shipShapes, int x, int y);
void ShowMenuBackground(POLY_FT4 *menuPrim, POLY_FT4 *menu1Prim, LoadedImage *image);
void ShowScreen(POLY_FT4 *menuPrim, POLY_FT4 *menu1Prim, LoadedImage *image, int x, int y);
void TeamSelect(ConfigData *gameData, MenuSelection *selection, void *shipShapes);
void setupBackgrndPolys(short firstCard, int endCard);
void EstablishLink(void);
void InitChallenges(void);
void InitMovies(void);
void InitRaceLoad(void);
void InitResidentMenu(void);
void InitSlider(void);
void LoadButtons(void);
void ReInitGameProgress(void);
void ShowLoadMessage(void);
void SlideScreen(void *menu);
void killLink(void);

/* trackasm.c */
void AsmGenerateTiles(TexTemplate *tiles, short tileCount);
void AsmHighResTile(TexTemplate *template, short index, short reversed);
void AsmLowResTile(TexTemplate *template, short index, short reversed);
void AsmMediumResTile(TexTemplate *template, short index, short reversed);
void AsmNextFrame(void);

/* card.c */
int CardContentStatus(void);
int CheckCardEvents(void);
int CheckHWCardEvents(void);
void ClearEventHW(void);
void ClearEventSW(void);
int CreateFile(char *fileName);
void FirstSavedGame(void *data);
void GetHeader(u_char *buffer, u_char *name);
void LoadCard(int slot);
u_char LoadCardData(char *name, int slot, ConfigData *settings);
int LoadCardInitData(char *name, short port, ConfigData *settings);
void LoadFirstFile(u_short *count, int slot, char *name);
void MakeFileName(char *suffix, void *header, int slot);
int QuickUnFormat(int slot);
int RenameFile(char *name, int slot, ConfigData *settings);
int RenameFile1(char *card, char *source, int type, void *output);
u_char SaveCardFile(char *name, int slot, ConfigData *settings);
int UnFormat(int slot);
/* Port: linked under a name of its own, not kernel32's WriteFile (a Windows build links both) */
#define WriteFile CardWriteFile
int WriteFile(void *header, char *filename, ConfigData *settings);
void BuildSaveBlock(u_long *block, ConfigData *settings);
int RewriteCardFile(char *name, int slot, ConfigData *settings);
/* Port: the autosave (portsave.c) */
void PortLoadSavedGame(ConfigData *settings);
void PortAutoSave(ConfigData *settings, int inMenus);
void PortSaveChosen(int slot, char *name, ConfigData *settings);
void PortSaveDeleted(int slot, char *name);
void CloseCardEvents(void);
int GetCardInfo(int slot);
void InitCardEvents(void);
void LoadCardTexture(void);

/* crack.c */
void FillHiMedCrack(TrackSection *hiSection, Skeleton *camera);
void FillMedLoCrack(TrackSection *section, Skeleton *view);

/* pad.c */
short GetMenuKey(short a);
void InitController(void);
void InitPadSettings(void);
void SetBrake(ShipData *ship, ControlInput *control);
void SetDitch(ShipData *ship, WeaponData *weapons, int shipNo, ControlInput *control);
void SetFire(ShipData *ship, WeaponData *weapons, int shipNo, ControlInput *control);
void SetView(ShipData *ship, ControlInput *control);
void UpdateCheatSequence(int arg0);
struct PadPacket;
void buildFixedPad(struct PadPacket *pad);
void buildGameKeyPad(struct PadPacket *pad, ControlInput *control);

/* droid.c */
void InitRescueDroid(RescueData *rescueDroid, Object **shipShapes, ShipData *shipIndex);
void SetDroidVectors(RescueData *d, Object **shapes);
void UpdateDroidCircle(RescueData *d, ShipData *ship, Object **shapes);
void UpdateDroidRescue(RescueData *rescueDroid, ShipData *ship, Object **shapes);
void UpdateDroidStart(RescueData *droid, ShipData *ships, Object **shapes);
void UpdateRescueDroid(ShipData *ships, Object **unused1, int shipNo, WeaponData *unused3, ControlInput *control);

/* animover.c */
void anim(char *fileName);

/* credits.c */
void InitCredits(void);
void ShowCredits(int y);

/* display.c */
BLK_FILL *BlockFill(short x, short y, short w, short h, short red, short green, short blue);
void CallbackSwap(void);
POLY_F4 *ClearScreen(short red, short green, short blue);
void ClearVram(void);
void InitGameDisplay(int arg0);
void InitMenuDisplay(int arg0, int arg1);
void InitSonyDisplay(void);
void InitSystem(void);
void Swap(void);

/* progress.c */
void DrawBox(int x, int y, int w, int h, unsigned char r, unsigned char g, unsigned char b);
void Close2DParticle(void);
void DrawLine(int x0, int y0, int x1, int y1, unsigned char r, unsigned char g, unsigned char b);
void Init2DParticle(void);
void InitPop(void);
void Insert2DParticle(int x, int y);
void RefreshCache(void);
void Update2DParticle(void);
void WDclose(int handle);
int WDopen(char *name, int unused1, int unused2);
void WDread(int unused, u_char *destArg, int count);
void VsyncFunc(void);

/* wadfile.c */
void closewadfile(WADFILE *wad);
void closewadfilenvc(WADFILE *wad);
void readnextfile(WADFILE *arg0, char *arg1);
WADFILE *openwadfile(char *name);
WADFILE *openwadfilenvc(char *name);

/* sparks.c */
SParticle *InitParticle(void);
void AddRandomParticles(SParticle *particles, int count, VECTOR *pos, VECTOR *vel);
void AddScrapingParticles(SParticle *particles, int count, VECTOR *pos, VECTOR *vel);
void AddSnow(SParticle *particles, int unused, VECTOR *camera);
void AddRain(SParticle *particles, int unused, VECTOR *camera);
void AddExplodingParticles(SParticle *particles, int count, VECTOR *pos);
void AddEboltParticles(SParticle *particles, int count, VECTOR *pos);
void AddRisingParticles(SParticle *particles, int count, VECTOR *pos, int fallSpeed);
void AddFireyParticles(SParticle *particles, int count, VECTOR *pos, int dir, int type, VECTOR *vel);
void AddPitParticles(SParticle *particles, int count, VECTOR *pos, int fallSpeed);
void ClearParticleTable(SParticle *particles);
void LoadExplodeGFX(void);
void InitBirds(void);
void LoadBirdGFX(void);
void InitExplode(void);
void DrawExplode(void);
void DrawBirds(void);
void InitRing(void);
void DrawRing(void);
void AddSpurtExplode(int x, int y, int z, short ship);
void AddFireExplode(int x, int y, int z, short ship);
void AddExplode(int x, int y, int z, short ship);
void AddTendrilExplode(int x, int y, int z);
void AddMinigunExplode(int x, int y, int z, int unused);
void AddSmallSpurtExplode(int x, int y, int z, short ship);
void AddSmallSmokeExplode(int x, int y, int z, short ship);
void AddSmokeExplode(int x, int y, int z, int unused);
void AddRing(int x, int y, int z, int unused);
void DrawMissileFlare(int xpos, int ypos, int zpos);
void DrawRocketFlare(int xpos, int ypos, int zpos);
void InitElectric(void);
void AddElectric(VECTOR *position, int mode);
void resetMiscFx(void);
void UnsetParticles(SParticle *particles);

/* trails.c */
void BuildOwnShipTrail(int ship, VECTOR *left, VECTOR *right);
void DrawFlare(int x, int y, int distance, int ship);
void DrawFlashNozzle(int ship, TrackCamera *camera);
void DrawShipTrails(TrackCamera *camera);
void DrawTrail(TrackCamera *camera, Trail *trail, int ship);
void DrawTrailLoRes(TrackCamera *camera, Trail *trail, int ship);
void InitPlasmaShot(void);
void InitTrails(void);
void NewPlasmaSeg(VECTOR *pos, Trail *trail);
void NewPlasmaSegment(VECTOR *pos);
void NewTrailSegment(int ship, VECTOR *left, VECTOR *right);
void UpdateShipTrails(ShipData *ships, int allShips);
void UpdateTarget(int x, int y, int distance, int target);

/* ghost.c */
void InitGhostShip(ConfigData *config, ShipData *ships);
void RecordKeypresses(ShipData *ships);
void buildGhostReplayData(ShipData *ships);
void storeGhostTrail(ConfigData *config, HiScoreData *score);

/* drone.c */
void UpdateDroneShip(ShipData *ships, Object **shapes, int shipNo, WeaponData *weapons, ControlInput *control);
void UpdateDroneShipStart(ShipData *ships, Object **unused, int shipNo, WeaponData *unusedWeapons, ControlInput *unusedControl);
void calcMyXPos(ShipData *arg0, short arg1, short *arg2, short *arg3);
void followRaceLine(VECTOR *relative, ShipData *ships, Face *segment, int ship);
short getShipBehindLAP(ShipData *ships, short ship);
short getShipInfrontLAP(ShipData *ships, short ship);
void goLeft(VECTOR *result, ShipData *shipIndex, Face *facePtr, int shipNo);
void goRight(VECTOR *result, ShipData *shipIndex, Face *facePtr, int shipNo);
void humanToDroneSpeed(ShipData *arg0, int arg1);
void lookForWayPast(VECTOR *arg0, ShipData *arg1, short arg2, short arg3);
void moveDownCentre(VECTOR *arg0, ShipData *arg1, Face *arg2, int arg3);

/* sfx.c */
int Big2Little(u_int value);
int GetFreePlayId(void);
int GetFreeVoice(void);
void SetCdVolume(int volume);
void SetDolbyVolume(int sound);
void SetEffectVolume(int sound);
void SetGlobalVolume(int sound, int cd);
void SetMonoVolume(int sound);
void SetQSoundVolume(int sound);
void SetSoundVolume(int volume);
void SetStereoVolume(int sound);
int SfxChainId(int parent, int sample, int volume, int lrPan, u_char fbPan, int pitch, int flags);
void SfxContinue(void);
void SfxDoSfx(void);
void SfxEffectFBPan(u_int sound, u_char pan);
void SfxEffectLRPan(u_int sound, u_char pan);
void SfxEffectPitch(u_int sound, int pitch);
void SfxEffectVol(u_int sound, u_char volume);
int SfxFreeEffect(int sample);
void SfxInit(u_char mode);
void SfxInitReverb(void);
void SfxKillAllVoices(void);
void SfxOff(void);
void SfxOn(void);
void SfxPause(void);
void SfxPlay(int sample, u_short volume, int lrPan, int fbPan, int pitch, int flags, short requestedVoice);
int SfxPlayId(int sample, u_char volume, u_char lrPan, u_char fbPan, int pitch, int flags);
void SfxReverbOff(void);
void SfxReverbOn(void);
void SfxSetCallBack(void (*callback)(int sound));
void SfxSetCdVolume(u_char volume);
void SfxSetMode(u_char mode);
void SfxSetReverbDepth(u_char depth);
void SfxSetVolume(u_char gain);
void SfxStop(u_int sound);
int SfxTransEffect(void *left, void *right);
void SfxUnInit(void);
void SfxUnInitReverb(void);

/* hiscores.c */
void CheckForHiScore(ConfigData *config, HiScoreData *score, ShipData *ships, int shipNum, u8 lap);
void DisplayScoreTab(ConfigData *arg0, HiScoreData *arg1);
void EnterHighScore(ConfigData *gameData, HiScoreData *hiScoreData);
void InitScoreTabData(HiScoreData *arg0);
void ScoreTabInit(ConfigData *arg0);

/* asm */
/* The assembly routine reads roll (a2) when it rotates sprite primitives; callers that pass only two
   arguments leave a2 as the console had it, an undefined value. */
void AsmDrawObject(Object *object, Skeleton *camera, int roll);
void AsmTrackHi(TrackSection *firstSection, Skeleton *camera, AutoMeshData *autoMesh, short hiCount);
void AsmTrackLo(TrackSection *firstSection, Skeleton *camera, short loCount);
void AsmTrackMed(TrackSection *firstSection, Skeleton *camera, short medCount);
void RotTrans(SVECTOR *v, VECTOR *result, int *flag);

#include "globals.h"

#endif
