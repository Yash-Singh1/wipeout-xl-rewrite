/* Exercise the production fill packets against the original HUD's curved edge. */
#include "../../game/src/hud.c"
#include <assert.h>

int portPal, ownShip, shipDamage[16], energyBlinkFrame;
short CurrentScreen;
CameraUpdateFunc CameraUpdate;
SpeedDividers speedDividerTable = {{1, 1, 1, 1}}, speedDividerTableAlt;
Texture *speedoFrameTex, *speedoSpeedTex, *speedoShieldTex, *speedoMarkerTex;
u_long *OT[2];
static int tracing;
int PortTracing(void) { return tracing; }
int PortPcData(void) { return 0; }
void PcFillSpeedo(ShipData *ships, ConfigData *game, int player) { abort(); }
void UpdateCameraAttractInternal(TrackCamera *camera, ShipData *ship, RescueData *droid) {}
void AddPrim(u_long *ot, void *p) {}
void PortHudColourBars(const void *thrust, const void *energy, const void *table, int screen) {}

int main(void)
{
    /* Last coloured thrust texel and last open frame pixel, row by row. These
       profiles expose the unfilled one-pixel strip hidden by additive washout. */
    static const unsigned char texelRight[22] = {
        95,94,94,93,93,92,92,91,91,90,90,89,89,88,88,87,86,85,84,82,80,77};
    static const unsigned char frameRight[22] = {
        107,106,106,105,105,104,104,103,103,102,102,101,101,100,100,99,98,97,96,94,92,88};
    Texture marker = {.textureW = 24}, energy = {.textureW = 48}, speed = {.textureW = 48};
    Speedo panel = {0};
    ShipData ship = {.maxThrust = 1130};
    ConfigData game = {0};
    speedoMarkerTex = &marker; speedoShieldTex = &energy; speedoSpeedTex = &speed;
    for (portPal = 0; portPal <= 1; portPal++) {
        for (CurrentScreen = 0; CurrentScreen < 2; CurrentScreen++) {
            POLY_FT4 *fill = &panel.marker[CurrentScreen];
            for (tracing = 0; tracing <= 1; tracing++) {
                ship.thrust = ship.maxThrust;
                fill_speedo(&panel, &ship, &game, 0);
                int gaps = 0;
                for (int y = 0; y < 22; y++) {
                    int right = fill->x0 + texelRight[y];
                    gaps += right < 195 + frameRight[y];
                }
                assert(gaps == (tracing ? 21 : 0));
                assert(fill->x1 - fill->x0 == 96 && fill->x3 - fill->x2 == 86);
                assert(fill->y0 == PAL_PANEL_Y(173) && fill->y2 == PAL_PANEL_Y(195));
                assert(panel.shield[CurrentScreen].x0 == 198);
                assert(panel.shield[CurrentScreen].x1 == 283 && panel.shield[CurrentScreen].x3 == 294);
                assert(panel.shield[CurrentScreen].y0 == PAL_PANEL_Y(201));
                int previousRight = 0;
                for (int thrust = 0; thrust <= ship.maxThrust; thrust += 16) {
                    ship.thrust = thrust;
                    fill_speedo(&panel, &ship, &game, 0);
                    assert(fill->x0 == (tracing ? 206 : 207));
                    assert(fill->x1 >= previousRight); previousRight = fill->x1;
                    assert(fill->x1 - fill->x0 == fill->u1 - fill->u0);
                    assert(fill->x3 - fill->x2 == fill->u3 - fill->u2);
                }
            }
        }
    }
    puts("full thrust reaches the frame; partial fills, both buffers and PAL/NTSC align; console tracing preserved: OK");
    return 0;
}
