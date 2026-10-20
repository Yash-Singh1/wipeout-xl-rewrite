#ifndef GLOBALS_H
#define GLOBALS_H

/* Declarations of globals that more than one source file uses under the same type.  The
   console linked every file against one namespace without comparing declared types; natively a
   disagreement (pointer vs int, array vs scalar) is a bug, so each shared global is declared
   once here (tools/xdecl.py checks the whole tree).  Included by functions.h. */

/* --- group 1 (menus.c) --- */

/* --- group 2 (weapon.c, effects.c) --- */

/* --- group 3 (hud.c, sparks.c, trails.c, camera.c) --- */

/* --- group 4 (collide.c, combat.c, control.c, crack.c, credits.c, display.c, draw.c, droid.c, drone.c, dynam.c, error.c) --- */

/* --- group 5 (card.c, ayrace.c, hiscores.c, ghost.c, init.c, iso9660.c, lzss.c, file.c, malloc.c, object.c, options.c, pad.c) --- */

/* --- group 6 (progress.c, raceinit.c, radscn.c, scene.c, search.c, sfx.c, ships.c, skeleton.c, sound.c, tim.c, track.c, trackasm.c, ttf.c, utils.c, wadfile.c) --- */

/* --- shared by several groups (add here, alphabetical) --- */

#endif
