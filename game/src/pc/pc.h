/* The PC CD-ROM version's front end (docs/PC.md): with its data in use (PortPcData), the game runs the PC version's
   title, menus, HUD and text, taken from the PC decompilation, over the race every disc plays.

   The PC version draws its 2D from 256x256 texture pages addressed by number, loaded from its atlases
   (PortPcLoadPages), with PlayStation primitives laid out in 320x240 or 640x480 and a depth for the Z-buffer
   (d3dpoly.c). pcdraw.c takes those primitives as the PC's calls are made and links them into the frame's second
   ordering table (OT2, drawn over the race) by their depth when the frame is shown (PcDrawEndFrame, from Swap). */
#ifndef PC_H
#define PC_H

#include "../../include/functions.h"

/* ---- pcmain.c ---- */

/* The PC version's flow from the first picture on (PsxMain hands over to it once the system is up). Never returns. */
int PcMain(ConfigData *config);

/* ---- pcdraw.c ---- */

/* The texture page code of PC page n (0-62) for a primitive: colour depth 3 (the port's pages, gpu.h), its number
   n + 1, and the semi-transparency mode abr. */
u_short PcTPage(int page, int abr);
/* The PC's page of one of its texture page codes (d3dpoly.c's AdjustTexturePage: 5-15 are pages 0-10, 16-99 are
   6-89). */
int PcPageOfCode(int code);

/* The coordinate spaces of the PC's primitives: d3d.c's SetScreenVertex (the console's 320x240), SetScaledScreenVertex
   (640x480) and SetPixelScreenVertex (the screen's pixels, 640x480 in its high resolution). */
enum { PC_SPACE_320, PC_SPACE_640 };

/* A textured quad of the PC's: its corners in the space given, its UVs on PC page PcPageOfCode(poly->tpage), its
   colour Direct3D's (255 white); depth 0 (front) to 1 (back); blendMode non-zero for the PC's translucent pass
   (added). The PC's DrawPolyFT4, DrawScaledTexturedQuad and DrawPixelTexturedQuad. */
void PcDrawFT4(float depth, const POLY_FT4 *poly, int blendMode, int space);
/* A flat quad (the PC's DrawPolyF4, 320x240) and a Gouraud one (DrawPolyG4). */
void PcDrawF4(float depth, const POLY_F4 *poly, int space);
void PcDrawG4(float depth, const POLY_G4 *poly, int blendMode, int space);
/* The frame's primitives linked into OT2[CurrentScreen] by depth, front last (from Swap, display.c). */
void PcDrawEndFrame(void);

/* The PC's own names for them (d3dpoly.c, d3d.c). */
#define DrawPolyFT4(z, poly, blend) PcDrawFT4((z), (poly), (blend), PC_SPACE_320)
#define DrawScaledTexturedQuad(z, poly, blend) PcDrawFT4((z), (poly), (blend), PC_SPACE_640)
#define DrawPixelTexturedQuad(z, poly, blend) PcDrawFT4((z), (poly), (blend), PC_SPACE_640)
#define DrawPolyF4(z, poly) PcDrawF4((z), (poly), PC_SPACE_320)

/* ---- pctext.c: the PC's text (hud.c), from its HUD2 and YFONT pages ---- */

void PcInitText(void);
void PcAddText(const char *text, int x, int y, int size, int colour);
void PcAddNumber(int number, int x, int y, int size, int colour);
void PcAddJustifyNumber(int number, int x, int y, int size, int colour);
void PcCentreText(const char *text, int x, int y, int size, int colour);
void PcSetTextQuad(POLY_FT4 *quad, short x, short y, u_char u, u_char v, u_char width, u_char height, short tpage,
                   int colour);
extern float pcTextDepth; /* the PC's textDepth */

/* The PC's texture pages (main.c, racectrl.c): where its atlases are loaded. */
enum {
    PC_PAGE_BACKGROUND = 0, /* backgrnd.tga */
    PC_PAGE_MENU = 1,       /* testmenu.tga, 3 pages */
    PC_PAGE_KEYS = 6,       /* menukey.tga, 2 pages */
    PC_PAGE_MENU_SCENE = 11, /* menuhf.tga, 11 pages */
    PC_PAGE_HUD = 22,       /* hud2.tga, 3 pages */
    PC_PAGE_LOGO = 25,      /* xllogo.tga */
    PC_PAGE_YELLOW_FONT = 26, /* yfont.tga */
    PC_PAGE_PICTURE = 40    /* a 640x480 picture (blitsurf), 6 pages (the port's) */
};

#endif
