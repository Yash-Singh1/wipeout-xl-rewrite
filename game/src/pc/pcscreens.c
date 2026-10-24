/* The PC version's own screens around the menus (main.c): its copyright picture at the start and its title picture,
   640x480 pictures of its BLITSURF folder, which it blits over the screen (blit.c's AddBlit320). */
#include "pc.h"
#include "../../include/port_platform.h"


static POLY_FT4 blit;

void PcLoadPages(void)
{
    /* as the PC's main() loads them in its 16-bit mode */
    PortPcLoadPages("menuhf.tga", PC_PAGE_MENU_SCENE, 11);
    PortPcLoadPages("menukey.tga", PC_PAGE_KEYS, 2);
    PortPcLoadPages("backgrnd.tga", PC_PAGE_BACKGROUND, 1);
    PortPcLoadPages("testmenu.tga", PC_PAGE_MENU, 3);
    PortPcLoadPages("hud2.tga", PC_PAGE_HUD, 3);
    PortPcLoadPages("xllogo.tga", PC_PAGE_LOGO, 1);
    PortPcLoadPages("yfont.tga", PC_PAGE_YELLOW_FONT, 1);
}

void PcBlit(int x0, int y0, int x1, int y1, float depth)
{
    int page;

    /* the picture's part from (x0, y0) to (x1, y1) of the console's 320x240, drawn there: over each of its six pages
       (256x256 of the 640x480 picture) the part that falls on it */
    for (page = 0; page < 6; page++) {
        int left = page % 3 * 128, top = page / 3 * 128;
        int l = x0 > left ? x0 : left, t = y0 > top ? y0 : top;
        int r = x1 + 1 < left + 128 ? x1 + 1 : left + 128, b = y1 + 1 < top + 128 ? y1 + 1 : top + 128;

        if (l >= r || t >= b) {
            continue;
        }
        SetPolyFT4(&blit);
        blit.r0 = blit.g0 = blit.b0 = 255;
        blit.tpage = (u_short)(PC_PAGE_PICTURE + page + 10); /* (the PC's texture code of the page: page + 10) */
        blit.u0 = blit.u2 = (u_char)((l - left) * 2);
        blit.u1 = blit.u3 = (u_char)((r - left) * 2 - 1);
        blit.v0 = blit.v1 = (u_char)((t - top) * 2);
        blit.v2 = blit.v3 = (u_char)((b - top) * 2 - 1);
        blit.x0 = blit.x2 = (short)(l * 2);
        blit.x1 = blit.x3 = (short)(r * 2);
        blit.y0 = blit.y1 = (short)(t * 2);
        blit.y2 = blit.y3 = (short)(b * 2);
        DrawScaledTexturedQuad(depth, &blit, 0);
    }
}

/* One frame of the PC's screens: its tables cleared, what draw() adds shown. */
static void Frame(void)
{
    ClearOTagR(OT[CurrentScreen], 0x2000);
    ClearOTagR(OT2[CurrentScreen], 0x640);
}

void PcCopyrightScreen(void)
{
    int i;

    activateOT2 = 1;
    if (!PortPcLoadPicture("woxlcopy.ppm", PC_PAGE_PICTURE)) {
        return;
    }
    for (i = 0; i < PortLogoFields(300); i++) {
        Frame();
        PcBlit(0, 0, 319, 239, 0.5f);
        Swap();
        if (i > 10 && GetMenuKey(5)) {
            break;
        }
    }
}

int PcLoadTitlePicture(void)
{
    return PortPcLoadPicture("rstartxl.ppm", PC_PAGE_PICTURE);
}

void PcTitleFrame(int prompt, const char *pressStart)
{
    /* the picture, but for where the prompt goes (main.c's TitleScreen) */
    PcBlit(0, 0, 160, 239, 0.5f);
    PcBlit(160, 0, 319, 219, 0.5f);
    if (prompt) {
        PcAddText(pressStart, 160, 220, 8, 0);
    }
}
