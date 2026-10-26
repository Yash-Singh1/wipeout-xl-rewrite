#ifndef VERSION_H
#define VERSION_H

/*
 * The three releases: Wipeout XL (USA, SCUS-94351, NTSC), WipEout 2097 (Europe, SLES-00327, PAL) and Wipeout XL
 * (Japan, SIPS-60010, NTSC). The decompilation builds each from the same sources with these macros; the port is
 * one program that plays all three, so they are decided when it starts (platform/region.c), by things that need
 * not go together:
 *
 *   portPal          the video standard: PAL's 50 fields a second and 256 lines, with the European program's
 *                    timing, physics and layout (the settings; by default the disc's).
 *   portEuropeDisc   the disc is WipEout 2097's: the files the game asks for, its text.
 *   portJapanDisc    the disc is the Japanese release's. Its program is NTSC like the US one, but otherwise follows
 *                    the European one (US_PROGRAM), with a few changes of its own: its text, no Red Book logo,
 *                    shorter exhaust trails. Its disc's files are the US disc's.
 *
 * REGION and the PAL_ macros follow the video standard, REGION_STR, JAPAN_STR and US_PROGRAM the disc; the memory
 * card none of them (portEuropeCard).
 */
extern int portPal;
extern int portEuropeDisc;
extern int portJapanDisc;
/* The memory card's saves in WipEout 2097's own format (BESLES-00327, a header of 256 bytes): off, so that both
   discs share Wipeout XL's saves and a player's progress goes from one to the other (platform/saves.c converts a
   European save it imports). */
extern int portEuropeCard;

/* REGION(us, eu): a value that differs between the releases and follows no rule. */
#define REGION(us, eu) (portPal ? (eu) : (us))

/* REGION_STR(us, eu): a string the European program has in place of the US one (each a whole literal). */
#define REGION_STR(us, eu) (portEuropeDisc ? (eu) : (us))

/* JAPAN_STR(us, jp): a string the Japanese program has in place of the US (and European) one. */
#define JAPAN_STR(us, jp) (portJapanDisc ? (jp) : (us))

/* US_PROGRAM: the US program's own changes, made after the European and Japanese programs were finished (the
   decompilation's #ifdef VERSION_US): the replay lock, multi-block saves, the collision damage test. The other two
   have the code from before them. */
#define US_PROGRAM (!portEuropeDisc && !portJapanDisc)

/* Frames per second (the field rate) and per tenth of a second. */
#define FRAMES_PER_SECOND REGION(60, 50)
#define FRAMES_PER_TENTH  REGION(6, 5)

/* PAL_TIME(n): a duration written as n frames at 60 Hz, kept to the same time at 50: n * 50 / 60, rounded down. */
#define PAL_TIME(n) (portPal ? (n) * 50 / 60 : (n))

/* PAL_SPEED(x): an amount applied every frame (a speed, an acceleration, a turn rate), raised on PAL so the same
   amount happens each second: x * 60 / 50. PAL_SPEED_INV(x) is the inverse, x * 50 / 60. */
#define PAL_SPEED(x)     (portPal ? (x) * 60 / 50 : (x))
#define PAL_SPEED_INV(x) (portPal ? (x) * 50 / 60 : (x))

/* PAL_SPEED_VECTOR(v): PAL_SPEED on each component of a vector, in place; PAL_SPEED_UP(lvalue) on one field (the
   European program's separate step after the value is computed; nothing on NTSC). Statements. */
#define PAL_SPEED_VECTOR(v)                                                                                          \
    ((void)(portPal && ((v).vx = (v).vx * 60 / 50, (v).vy = (v).vy * 60 / 50, (v).vz = (v).vz * 60 / 50, 1)))
#define PAL_SPEED_UP(lvalue) ((void)(portPal && ((lvalue) = (lvalue) * 60 / 50, 1)))

/* Video lines per field: GsGetVcount's count advances once a line. */
#define LINES_PER_FIELD REGION(262, 312)

/* PAL_TEXT_Y(y): a text row or icon placed at y on NTSC, 8 lines higher on PAL. */
#define PAL_TEXT_Y(y) REGION((y), (y) - 8)

/* The display: 240 lines (NTSC) or 256 (PAL); the PAL picture starts 2 pixels right and 24 lines lower. */
#define SCREEN_HEIGHT   REGION(240, 256)
#define DISPLAY_SHIFT_X REGION(0, 2)
#define DISPLAY_SHIFT_Y REGION(0, 24)

/* PAL_SCREEN_HEIGHT(env): a DISPENV's window spans PAL's 256 lines; on NTSC the default (240) stays. A statement. */
#define PAL_SCREEN_HEIGHT(env) ((void)(portPal && ((env).screen.h = SCREEN_HEIGHT, 1)))

/* SetVideoMode's argument: 0 NTSC, 1 PAL. */
#define VIDEO_MODE REGION(0, 1)

#endif /* VERSION_H */
