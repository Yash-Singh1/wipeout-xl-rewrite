#ifndef PORT_PLATFORM_H_GAME
#define PORT_PLATFORM_H_GAME

/* What the game needs from the platform beyond the PsyQ libraries.  Implemented by platform/. */

/* Plays the intro/outro movie that the console played by loading a movie-player executable
   (fileName is that executable's name, e.g. "c:\\xtro1.exe"); returns when it has finished
   or was skipped. */
void PsxPlayMovie(const char *fileName);

/* The console's 1 KB scratchpad (0x1F800000), which the game uses for transformed vertices
   (screen coordinates at +0, depths at +0x200, clip flags at +0x300).  Defined in game/src/object.c. */
extern unsigned char PsxScratchpad[1024];

/* The window keeps showing its last picture until the area of video memory on display has been drawn
   again (an ordering table drawn there): for a wipe of video memory between two pictures that the player
   is not meant to see. */
void PsxHoldPicture(void);


/* Frame interpolation (platform/psx/libgpu.c): the ordering tables Swap will draw when its fields have passed (the
   second may be NULL), the first entry of the HUD's in the first, and the drawing environment they go to. While
   the fields pass the display shows them moving from the frame before. */
void PsxUpcomingTables(u_long *table, u_long *second, u_long *hudFirst, DRAWENV *env);

/* The HUD's place in the ordering tables Swap draws next: the last entry of the first and the start of
   the second (drawn at half size in a split window, platform/link.c). */
void PsxHudTables(u_long *lastEntry, u_long *secondTable);

/* A buffer of packets that gets link numbers of its own (libgpu numbers every packet address it links;
   a large buffer whose packets move from frame to frame would use up its table). */
void PsxLinkRegion(void *start, size_t bytes);

/* Whether the port draws the whole track in view (portfar.h): not in the lockstep tests (WIPEOUT_TRACE)
   or with WIPEOUT_FAR_VIEW=0. */
int PortFarViewEnabled(void);
/* Whether this run is recorded for the lockstep tests (platform/trace.h): the game decides as the console did. */
int PortTracing(void);
/* While on, models are drawn with both sides of their single-sided polygons (objdraw.c): a menu's turning track
   model, a surface of which is otherwise gone while its back is to the camera (a climb, seen from behind). Not in
   the lockstep tests. */
void PortTwoSided(int on);
int PortLogoFields(int fields); /* how long a start-up logo is shown: the console's, or none (platform/main.c) */
/* Widescreen (the settings): how far the picture reaches beyond each side of the console's 320 columns, in
   console pixels; 0 for the console's 4:3. A race's scene is shown wide (the game says so every frame it
   draws one, PortWideScene); a menu is too, its backdrop enlarged to fill the sides (PortWideBackdrop, each
   frame, for each of the backdrop's packets) while the rest stays in the console's 4:3. A loading screen is
   shown wide as well (PortWideLoadScreen, each picture it draws): its fills across the screen reach the sides,
   what is drawn on them stays in the middle. The other pictures keep the console's shape. */
int PortScreenMargin(void);
void PortWideScene(void);
void PortWideBackdrop(const void *packet);
void PortWideLoadScreen(void);
/* A packet the game places at a projected point of the scene but links into the ordering table's front entry,
   where the HUD is (the other player's sign over a near ship, the missile lock's boxes): widescreen's and the
   split window's HUD handling leave it where the scene puts it. */
void PortSceneSprite(const void *packet);
/* ... the same for every packet in that memory (the trails, the engine flares: near the camera they go in at depth
   0, the HUD's entry). */
void PortSceneRange(const void *start, size_t bytes);
/* The HUD packets from start to end, of the frame being built, stay where the game puts them: a menu over the race
   whose items stand apart in the middle of the screen (the pause's YES and NO), not an element of a side. */
void PortHudCentred(const void *start, const void *end);
/* ... and all of this frame's HUD (a screen of text over the race, the results: its scene is shown wide). */
void PortHudInPlace(void);
/* ShakeScreen's jolt (a menu landing, a hit in a race): the display moved x units right (of 1/256 of the screen's
   width) and y lines down from where it rests. The console moves the picture on the TV (DISPENV.screen), which the
   port has no TV for; the window's picture moves as much from the next PutDispEnv on. */
void PortDisplayShake(int x, int y);
/* The pads' motors (platform/main.c, psx/pad_sdl.c): the picture's jolt in the player's hands. The game says
   how hard a hit was, in the units SetShake shakes the screen by (2 a knock in a damaged ship, 20 a rocket),
   or 0 to stop them. The pads that plugged into the console had no motors; the port's gamepads shake as the
   picture does. */
void PortPadRumble(int amount);
/* The pads' motors held at a level: the game says the level again every frame it wants them running at it
   (0 stops them), and they stop by themselves a few fields after it stops saying. The launch thrust's
   growl is this: it rises and falls with the throttle. */
void PortPadRumbleLevel(int level);

/* The original Wipeout's tracks (platform/original.c), raced as guests in Talon's Reach's slot (trackNum 0,
   folder 1): the ones the player's copy of the original has, their names and classes (XL's: 1 Venom,
   2 Rapier); the one chosen (-1: an XL track); its start line's section and its sky's height. */
int PortOriginalTrackCount(void);
const char *PortOriginalTrackName(int index);
const char *PortOriginalTrackGame(int index); /* "THE ORIGINAL WIPEOUT" or "WIPEOUT 64" */
/* The name of the track's model in the guests' track menu models (origtrk.prm in the common folder: the original's
   and Wipeout 64's, while the player's copies are in use), or NULL. */
const char *PortOriginalTrackModel(int index);
int PortOriginalHasModels(void); /* the guests' track models can be loaded (origtrk.prm) */
int PortOriginalFoundCount(void);
int PortOriginalTrackClass(int index);
void PortOriginalTrackSelect(int index);
int PortOriginalTrack(void);
int PortOriginalStartLine(void);
int PortOriginalSkyY(void);
/* The game's file layer (iso9660.c): a file of the chosen track's (where, and how long), and its sectors. */
int PortOriginalFileInfo(const char *pathname, int *loc, int *size);
int PortOriginalRead(int loc, int sectors, void *buffer);
/* ... and with WipEout 2097's disc (Europe), its pieces in place of the US program's (platform/region.c). */
int PortRegionFileInfo(const char *pathname, int *loc, int *size);
int PortRegionRead(int loc, int sectors, void *buffer);
/* ... and with the Sega Saturn's disc, everything the game reads, made from the Saturn's (platform/saturn.c). */
int PortSaturnFileInfo(const char *pathname, int *loc, int *size);
int PortSaturnRead(int loc, int sectors, void *buffer);
/* The saves (platform/saves.c): the save in use (its card, 0 or 1, and the name after BASCUS-94351; -1 when
   none), remembered for the next start; whether the game saves itself; a save the debug console imported, to
   load now (its card, -1 when none); the outcome of the last autosave. */
int PortSaveSlot(char name[9]);
void PortSaveSlotSet(int card, const char *name);
int PortAutoSaveEnabled(void);
int PortSaveTakeImport(char name[9]);
void PortSaveWritten(int ok);
/* The distance fade's range in world units; whether it is on (the settings, platform/settings.c). */
int PortFadeRange(int *nearDistance, int *farDistance);

/* The frame recorder (platform/psx/framecap.h): the packets from start to end were built by `what` (an
   object's name, or NULL) for its item `index`. */
void PortTagPrims(const void *start, const void *end, const char *what, const char *name, int index);

#endif
