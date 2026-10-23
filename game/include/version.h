#ifndef VERSION_H
#define VERSION_H

/*
 * The two releases: Wipeout XL (USA, SCUS-94351, NTSC) and WipEout 2097 (Europe, SLES-00327, PAL). The
 * decompilation builds either from the same sources with these macros; the port is one program that plays both,
 * so they are decided when it starts (platform/region.c), by two things that need not go together:
 *
 *   portPal          the video standard: PAL's 50 fields a second and 256 lines, with the European program's
 *                    timing, physics and layout (the settings; by default the disc's).
 *   portEuropeDisc   the disc is WipEout 2097's: the files the game asks for, the memory card's saves.
 *
 * REGION and the PAL_ macros follow the video standard, REGION_STR and the memory card the disc.
 */
extern int portPal;
extern int portEuropeDisc;

/* REGION(us, eu): a value that differs between the releases and follows no rule. */
#define REGION(us, eu) (portPal ? (eu) : (us))

/* REGION_STR(us, eu): a string the European program has in place of the US one (each a whole literal). */
#define REGION_STR(us, eu) (portEuropeDisc ? (eu) : (us))

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

/* SetVideoMode's argument: 0 NTSC, 1 PAL. */
#define VIDEO_MODE REGION(0, 1)

#endif /* VERSION_H */
