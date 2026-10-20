#include "functions.h"
#include "trails.h"
#include "include_asm_g8.h"

/* $gp-relative globals are tentative definitions; the rest are extern. */
extern Trail *shipTrails;
extern int    plasmaPhaseA;
extern int    plasmaPhaseB;
extern int    plasmaPhaseC;
extern int    flashNozzleShip;
extern u_char plasmaShotBusy;
extern short  lastTargetShip;
extern u_char targetBank;
extern int    targetSpin;
extern u_char targetvalid;
extern short      missileLockTimer;
extern u_char     targetFlags[];
extern Trail      Plasma[];
extern ShipData  *shipIndex;
extern int        ownShip;
extern short      CurrentScreen;
extern LINE_F2    targetPrims[8][4][2];
extern DR_MODE    targetDrawModes[2];
extern POLY_G3    muzzletop[2][2];
extern POLY_G3    muzzleleft[2][2];
extern POLY_G3    muzzleright[2][2];
extern POLY_G3    muzzlebot[2][2];
extern DR_MODE    muzzletrans[2];
extern POLY_G4    flare[19][12][2];

void DrawFlare(int x, int y, int distance, int ship) {
    int speed, value, half, quarter, size;
    ShipData *s;
    POLY_G4 *p;
    int offset;
    if (ship < 15) {
        s = &shipIndex[ship];
        if (s->combatAttr & DESTROYED) return;
        if (ship != ownShip || !(s->attr & RACING)) speed = s->speed >> 1;
        else speed = s->thrust >> 5;
    } else speed = 6000;
    if (distance == 0) distance = 1;
    value = (speed * 8) / distance;
    if (value > 80) value = 80;
    half = value >> 1;
    quarter = value >> 2;
    size = value / 32;
    if (size <= 0) size = 1;

    offset = CurrentScreen * sizeof(POLY_G4) + ship * sizeof(flare[0]);
    p = (POLY_G4 *)((char *)&flare[0][4][0] + offset);
    p->x0 = x + size;
    p->y0 = y + size;
    p->x1 = x;
    p->y1 = y;
    p->x2 = x + half + 1;
    p->y2 = y - half + 1;
    p->x3 = x + half;
    p->y3 = y - half;
    p = (POLY_G4 *)((char *)&flare[0][5][0] + offset);
    p->x0 = x - size;
    p->y0 = y - size;
    p->x1 = x;
    p->y1 = y;
    p->x2 = x + half - 1;
    p->y2 = y - half - 1;
    p->x3 = x + half;
    p->y3 = y - half;
    p = (POLY_G4 *)((char *)&flare[0][6][0] + offset);
    p->x0 = x + size;
    p->y0 = y + size;
    p->x1 = x;
    p->y1 = y;
    p->x2 = x - half + 1;
    p->y2 = y + half + 1;
    p->x3 = x - half;
    p->y3 = y + half;
    p = (POLY_G4 *)((char *)&flare[0][7][0] + offset);
    p->x0 = x - size;
    p->y0 = y - size;
    p->x1 = x;
    p->y1 = y;
    p->x2 = x - half - 1;
    p->y2 = y + half - 1;
    p->x3 = x - half;
    p->y3 = y + half;
    p = (POLY_G4 *)((char *)&flare[0][8][0] + offset);
    p->x0 = x - size;
    p->y0 = y + size;
    p->x1 = x;
    p->y1 = y;
    p->x2 = x - (quarter + 1);
    p->y2 = y - quarter + 1;
    p->x3 = x - quarter;
    p->y3 = y - quarter;
    p = (POLY_G4 *)((char *)&flare[0][9][0] + offset);
    p->x0 = x + size;
    p->y0 = y - size;
    p->x1 = x;
    p->y1 = y;
    p->x2 = x - quarter + 1;
    p->y2 = y - quarter - 1;
    p->x3 = x - quarter;
    p->y3 = y - quarter;
    p = (POLY_G4 *)((char *)&flare[0][10][0] + offset);
    p->x0 = x - size;
    p->y0 = y + size;
    p->x1 = x;
    p->y1 = y;
    p->x2 = x + quarter - 1;
    p->y2 = y + quarter + 1;
    p->x3 = x + quarter;
    p->y3 = y + quarter;
    p = (POLY_G4 *)((char *)&flare[0][11][0] + offset);
    p->x0 = x + size;
    p->y0 = y - size;
    p->x1 = x;
    p->y1 = y;
    p->x2 = x + quarter + 1;
    p->y2 = y + quarter - 1;
    p->x3 = x + quarter;
    p->y3 = y + quarter;
    if (distance < 1000) {
        p = (POLY_G4 *)((char *)&flare[0][0][0] + offset);
        p->x0 = x;
        p->y0 = y;
        p->x1 = x;
        p->y1 = y;
        p->x2 = x;
        p->y2 = y - quarter;
        p->x3 = x + quarter;
        p->y3 = y;
        p = (POLY_G4 *)((char *)&flare[0][1][0] + offset);
        p->x0 = x;
        p->y0 = y;
        p->x1 = x;
        p->y1 = y;
        p->x2 = x;
        p->y2 = y - quarter;
        p->x3 = x - quarter;
        p->y3 = y;
        p = (POLY_G4 *)((char *)&flare[0][2][0] + offset);
        p->x0 = x;
        p->y0 = y;
        p->x1 = x;
        p->y1 = y;
        p->x2 = x + quarter;
        p->y2 = y;
        p->x3 = x;
        p->y3 = y + quarter;
        p = (POLY_G4 *)((char *)&flare[0][3][0] + offset);
        p->x0 = x;
        p->y0 = y;
        p->x1 = x;
        p->y1 = y;
        p->x2 = x - quarter;
        p->y2 = y;
        p->x3 = x;
        p->y3 = y + quarter;
        AddPrim(OT[CurrentScreen] + (distance >> 1), &flare[0][0][0] + ship * 24 + CurrentScreen);
        AddPrim(OT[CurrentScreen] + (distance >> 1), &flare[0][1][0] + ship * 24 + CurrentScreen);
        AddPrim(OT[CurrentScreen] + (distance >> 1), &flare[0][2][0] + ship * 24 + CurrentScreen);
        AddPrim(OT[CurrentScreen] + (distance >> 1), &flare[0][3][0] + ship * 24 + CurrentScreen);
    }
    AddPrim(OT[CurrentScreen] + (distance >> 1), &flare[0][4][0] + ship * 24 + CurrentScreen);
    AddPrim(OT[CurrentScreen] + (distance >> 1), &flare[0][5][0] + ship * 24 + CurrentScreen);
    AddPrim(OT[CurrentScreen] + (distance >> 1), &flare[0][6][0] + ship * 24 + CurrentScreen);
    AddPrim(OT[CurrentScreen] + (distance >> 1), &flare[0][7][0] + ship * 24 + CurrentScreen);
    AddPrim(OT[CurrentScreen] + (distance >> 1), &flare[0][8][0] + ship * 24 + CurrentScreen);
    AddPrim(OT[CurrentScreen] + (distance >> 1), &flare[0][9][0] + ship * 24 + CurrentScreen);
    AddPrim(OT[CurrentScreen] + (distance >> 1), &flare[0][10][0] + ship * 24 + CurrentScreen);
    AddPrim(OT[CurrentScreen] + (distance >> 1), &flare[0][11][0] + ship * 24 + CurrentScreen);
}


void UpdateTarget(int x, int y, int distance, int target) {
    int copy, value, radius, i;
    if (target < 15) {
        if (shipIndex[target].combatAttr & DESTROYED) return;
        if (target != ownShip || !(shipIndex[target].attr & RACING)) value = shipIndex[target].speed >> 1;
        else value = shipIndex[target].thrust;
    } else value = 6000;
    if (distance == 0) distance = 1;
    value = (value << 3) / distance;
    copy = value;
    if (copy > 80) copy = 80;
    radius = copy >> 1;
    value = copy / 32;
    if (((u_int)value * 32) & 31) radius++;
    if (shipIndex[ownShip].targetShip == target && (shipIndex[ownShip].attr & RACING) && (shipIndex[ownShip].weapon == 2 || shipIndex[ownShip].weapon == 5)) {
        radius += missileLockTimer << 3;
        if (radius < 8) radius = 8;
        targetFlags[targetBank] = 1;
        for (i = 0; i < 2; i++) {
            targetPrims[targetBank][0][i].x0 = x + ((radius * rsin(targetSpin)) >> 12);
            targetPrims[targetBank][0][i].y0 = y + ((radius * rcos(targetSpin)) >> 12);
            targetPrims[targetBank][0][i].x1 = x + ((radius * rsin(targetSpin + 1024)) >> 12);
            targetPrims[targetBank][0][i].y1 = y + ((radius * rcos(targetSpin + 1024)) >> 12);
            targetPrims[targetBank][1][i].x0 = x + ((radius * rsin(targetSpin + 1024)) >> 12);
            targetPrims[targetBank][1][i].y0 = y + ((radius * rcos(targetSpin + 1024)) >> 12);
            targetPrims[targetBank][1][i].x1 = x + ((radius * rsin(targetSpin + 2048)) >> 12);
            targetPrims[targetBank][1][i].y1 = y + ((radius * rcos(targetSpin + 2048)) >> 12);
            targetPrims[targetBank][2][i].x0 = x + ((radius * rsin(targetSpin + 2048)) >> 12);
            targetPrims[targetBank][2][i].y0 = y + ((radius * rcos(targetSpin + 2048)) >> 12);
            targetPrims[targetBank][2][i].x1 = x + ((radius * rsin(targetSpin + 3072)) >> 12);
            targetPrims[targetBank][2][i].y1 = y + ((radius * rcos(targetSpin + 3072)) >> 12);
            targetPrims[targetBank][3][i].x0 = x + ((radius * rsin(targetSpin + 3072)) >> 12);
            targetPrims[targetBank][3][i].y0 = y + ((radius * rcos(targetSpin + 3072)) >> 12);
            targetPrims[targetBank][3][i].x1 = x + ((radius * rsin(targetSpin)) >> 12);
            targetPrims[targetBank][3][i].y1 = y + ((radius * rcos(targetSpin)) >> 12);
            targetPrims[targetBank][0][i].g0 = 128;
            targetPrims[targetBank][1][i].g0 = 128;
            targetPrims[targetBank][2][i].g0 = 128;
            targetPrims[targetBank][3][i].g0 = 128;
        }
        targetvalid = 1;
    }
}


void InitTrails(void) {
    int i, j, k, l, m;
    /* Port: was the literal 0x8700 (15 * 2304); sizeof(Trail) is 2304 on both layouts. */
    shipTrails = DAlloc(reserveHeap, 15 * sizeof(Trail));
    targetvalid = 0;
    for (i = 0; i < 2; i++) {
        for (l = 0; l < 8; l++) {
            for (j = 0; j < 4; j++) {
                SetLineF2(&targetPrims[l][j][i]);
                SetSemiTrans(&targetPrims[l][j][i], 1);
                targetPrims[l][j][i].r0 = 128;
                targetPrims[l][j][i].g0 = 128;
                targetPrims[l][j][i].b0 = 0;
            }
            targetFlags[l] = 0;
        }
    }
    SetDrawMode(&targetDrawModes[0], 1, 1, GetTPage(0, 1, 0, 0), 0);
    SetDrawMode(&targetDrawModes[1], 1, 1, GetTPage(0, 1, 0, 0), 0);
    for (k = 0; k < 15; k++) {
        shipTrails[k].current = 0;
        shipTrails[k].colour[0][0] = 32;
        shipTrails[k].colour[0][1] = 64;
        shipTrails[k].colour[0][2] = 255;
        shipTrails[k].colour[1][0] = 192;
        shipTrails[k].colour[1][1] = 255;
        shipTrails[k].colour[1][2] = 255;
        shipTrails[k].colour[2][0] = 255;
        shipTrails[k].colour[2][1] = 255;
        shipTrails[k].colour[2][2] = 255;
        for (m = 0; m < TRAIL_SEGMENTS; m++) {
            shipTrails[k].segment[m].active = 0;
            SetPolyG4(&shipTrails[k].segment[m].leftHalf[0]);
            SetPolyG4(&shipTrails[k].segment[m].leftHalf[1]);
            SetPolyG4(&shipTrails[k].segment[m].rightHalf[0]);
            SetPolyG4(&shipTrails[k].segment[m].rightHalf[1]);
            SetSemiTrans(&shipTrails[k].segment[m].leftHalf[0], 1);
            SetSemiTrans(&shipTrails[k].segment[m].leftHalf[1], 1);
            SetSemiTrans(&shipTrails[k].segment[m].rightHalf[0], 1);
            SetSemiTrans(&shipTrails[k].segment[m].rightHalf[1], 1);
            SetDrawMode(&shipTrails[k].segment[m].drawMode[0], 1, 1, GetTPage(0, 1, 0, 0), 0);
            SetDrawMode(&shipTrails[k].segment[m].drawMode[1], 1, 1, GetTPage(0, 1, 0, 0), 0);
        }
    }
    for (i = 0; i < 2; i++) {
        for (j = 0; j < 2; j++) {
            SetPolyG3(&muzzletop[i][j]);
            SetSemiTrans(&muzzletop[i][j], 1);
            muzzletop[i][j].r0 = 255;
            muzzletop[i][j].g0 = 8;
            muzzletop[i][j].b0 = 8;
            muzzletop[i][j].r1 = 0;
            muzzletop[i][j].g1 = 0;
            muzzletop[i][j].b1 = 0;
            muzzletop[i][j].r2 = 0;
            muzzletop[i][j].g2 = 0;
            muzzletop[i][j].b2 = 0;
            SetPolyG3(&muzzleleft[i][j]);
            SetSemiTrans(&muzzleleft[i][j], 1);
            muzzleleft[i][j].r0 = 255;
            muzzleleft[i][j].g0 = 8;
            muzzleleft[i][j].b0 = 8;
            muzzleleft[i][j].r1 = 0;
            muzzleleft[i][j].g1 = 0;
            muzzleleft[i][j].b1 = 0;
            muzzleleft[i][j].r2 = 0;
            muzzleleft[i][j].g2 = 0;
            muzzleleft[i][j].b2 = 0;
            SetPolyG3(&muzzleright[i][j]);
            SetSemiTrans(&muzzleright[i][j], 1);
            muzzleright[i][j].r0 = 255;
            muzzleright[i][j].g0 = 8;
            muzzleright[i][j].b0 = 8;
            muzzleright[i][j].r1 = 0;
            muzzleright[i][j].g1 = 0;
            muzzleright[i][j].b1 = 0;
            muzzleright[i][j].r2 = 0;
            muzzleright[i][j].g2 = 0;
            muzzleright[i][j].b2 = 0;
            SetPolyG3(&muzzlebot[i][j]);
            SetSemiTrans(&muzzlebot[i][j], 1);
            muzzlebot[i][j].r0 = 255;
            muzzlebot[i][j].g0 = 8;
            muzzlebot[i][j].b0 = 8;
            muzzlebot[i][j].r1 = 0;
            muzzlebot[i][j].g1 = 0;
            muzzlebot[i][j].b1 = 0;
            muzzlebot[i][j].r2 = 0;
            muzzlebot[i][j].g2 = 0;
            muzzlebot[i][j].b2 = 0;
        }
        SetDrawMode(&muzzletrans[i], 1, 1, GetTPage(0, 1, 0, 0), 0);
    }
    for (j = 0; j < 19; j++) {
        for (k = 0; k < 12; k++) {
            for (i = 0; i < 2; i++) {
                SetPolyG4(&flare[j][k][i]);
                SetSemiTrans(&flare[j][k][i], 1);
                if (j < 15) {
                    flare[j][k][i].r0 = 32;
                    flare[j][k][i].g0 = 64;
                    flare[j][k][i].b0 = 255;
                    flare[j][k][i].r1 = 32;
                    flare[j][k][i].g1 = 64;
                    flare[j][k][i].b1 = 255;
                } else {
                    flare[j][k][i].r0 = 224;
                    flare[j][k][i].g0 = 128;
                    flare[j][k][i].b0 = 32;
                    flare[j][k][i].r1 = 224;
                    flare[j][k][i].g1 = 128;
                    flare[j][k][i].b1 = 32;
                }
                flare[j][k][i].r2 = 0;
                flare[j][k][i].g2 = 0;
                flare[j][k][i].b2 = 0;
                flare[j][k][i].r3 = 0;
                flare[j][k][i].g3 = 0;
                flare[j][k][i].b3 = 0;
                if (k >= 4) {
                    flare[j][k][i].r0 = 0;
                    flare[j][k][i].g0 = 0;
                    flare[j][k][i].b0 = 0;
                }
            }
        }
    }
}


void NewTrailSegment(int ship, VECTOR *left, VECTOR *right)
{
    int           i;
    int           speed;
    short         intensity;
    int           current;
    TrailSegment *segment;

    speed = shipIndex[ship].speed >> 1;
    intensity = speed;
    if (speed > 255) intensity = 255;
    for (i = 0; i < TRAIL_SEGMENTS; i++) {
        segment = &shipTrails[ship].segment[i];
        segment->r -= 27;
        segment->g -= 27;
        segment->b -= 27;
        if (segment->r < 0) segment->r = 0;
        if (segment->g < 0) segment->g = 0;
        if (segment->b < 0) segment->b = 0;
    }
    current = shipTrails[ship].current + 10;
    current %= TRAIL_SEGMENTS;
    segment = &shipTrails[ship].segment[current];
    shipTrails[ship].current = current;
    segment->active = 1;
    segment->left.vx = left->vx;
    segment->left.vy = left->vy;
    segment->left.vz = left->vz;
    segment->right.vx = right->vx;
    segment->right.vy = right->vy;
    segment->right.vz = right->vz;
    if (!(newRand() & 7)) {
        segment->r = (shipTrails[ship].colour[1][0] * intensity) >> 8;
        segment->g = (shipTrails[ship].colour[1][1] * intensity) >> 8;
        segment->b = (shipTrails[ship].colour[1][2] * intensity) >> 8;
    } else {
        segment->r = (shipTrails[ship].colour[0][0] * intensity) >> 8;
        segment->g = (shipTrails[ship].colour[0][1] * intensity) >> 8;
        segment->b = (shipTrails[ship].colour[0][2] * intensity) >> 8;
    }
}


void BuildOwnShipTrail(int ship, VECTOR *left, VECTOR *right)
{
    int           i;
    int           speed;
    short         intensity;
    TrailSegment *segment;
    ShipData     *own;

    speed = shipIndex[ship].speed >> 6;
    intensity = speed;
    if (speed >= 256) intensity = 255;
    shipTrails[ship].current = 0;
    for (i = 0; i < TRAIL_SEGMENTS; i++) {
        shipTrails[ship].segment[i].active = 1;
        shipTrails[ship].segment[i].left.vx = left->vx;
        shipTrails[ship].segment[i].left.vy = left->vy;
        shipTrails[ship].segment[i].left.vz = left->vz;
        shipTrails[ship].segment[i].right.vx = right->vx;
        shipTrails[ship].segment[i].right.vy = right->vy;
        shipTrails[ship].segment[i].right.vz = right->vz;
        left->vx -= shipIndex[ownShip].unitVecNose.vx >> 5;
        left->vy -= shipIndex[ownShip].unitVecNose.vy >> 5;
        left->vz -= shipIndex[ownShip].unitVecNose.vz >> 5;
        right->vx -= shipIndex[ownShip].unitVecNose.vx >> 5;
        right->vy -= shipIndex[ownShip].unitVecNose.vy >> 5;
        right->vz -= shipIndex[ownShip].unitVecNose.vz >> 5;
    }
    segment = &shipTrails[ship].segment[0];
    if (!(newRand() & 7)) {
        segment->r = (shipTrails[ship].colour[1][0] * intensity) >> 8;
        segment->g = (shipTrails[ship].colour[1][1] * intensity) >> 8;
        segment->b = (shipTrails[ship].colour[1][2] * intensity) >> 8;
    } else {
        segment->r = (shipTrails[ship].colour[0][0] * intensity) >> 8;
        segment->g = (shipTrails[ship].colour[0][1] * intensity) >> 8;
        segment->b = (shipTrails[ship].colour[0][2] * intensity) >> 8;
    }
    for (i = TRAIL_SEGMENTS - 1; i > 0; i--) {
        shipTrails[ship].segment[i].r = shipTrails[ship].segment[i - 1].r - 27;
        shipTrails[ship].segment[i].g = shipTrails[ship].segment[i - 1].g - 27;
        shipTrails[ship].segment[i].b = shipTrails[ship].segment[i - 1].b - 27;
        if (shipTrails[ship].segment[i].r < 0) shipTrails[ship].segment[i].r = 0;
        if (shipTrails[ship].segment[i].g < 0) shipTrails[ship].segment[i].g = 0;
        if (shipTrails[ship].segment[i].b < 0) shipTrails[ship].segment[i].b = 0;
    }
}


void DrawTrailLoRes(TrackCamera *camera, Trail *trail, int ship) {
    SVECTOR vertex[6];
    SVECTOR screen[6];
    int shipP;
    int flag;
    int segmentP;
    Skeleton *view;
    int *cam;
    POLY_G4 *poly;
    int oldest, secondOldest;
    int i, n;
    int lx, ly, lz, rx, ry, rz;
    int depth;

    oldest = (trail->current + 10) % 11;
    secondOldest = (oldest + 10) % 11;
    view = camera->camPos;
    vertex[0].vx = shipIndex[ship].ppivot.vx + view->relative.t[0];
    vertex[0].vy = shipIndex[ship].ppivot.vy + view->relative.t[1];
    vertex[0].vz = shipIndex[ship].ppivot.vz + view->relative.t[2];
    cam = view->relative.t;
    RotTransPers(&vertex[0], (int *)&screen[0], &shipP, &flag);
    i = trail->current & 1;
    if (i < TRAIL_SEGMENTS) {
        do {
            if ((i != oldest && i != secondOldest && shipP < 4000) || i == trail->current) {
                n = (i + 2) % 11;
                if (trail->segment[i].active && trail->segment[n].active) {
                    lx = trail->segment[i].left.vx;
                    ly = trail->segment[i].left.vy;
                    lz = trail->segment[i].left.vz;
                    rx = trail->segment[i].right.vx;
                    ry = trail->segment[i].right.vy;
                    rz = trail->segment[i].right.vz;
                    vertex[0].vx = lx + cam[0];
                    vertex[0].vy = ly + cam[1];
                    vertex[0].vz = lz + cam[2];
                    vertex[2].vx = rx + cam[0];
                    vertex[2].vy = ry + cam[1];
                    vertex[2].vz = rz + cam[2];
                    vertex[1].vx = ((lx + rx) >> 1) + cam[0];
                    vertex[1].vy = ((ly + ry) >> 1) + cam[1];
                    vertex[1].vz = ((lz + rz) >> 1) + cam[2];
                    vertex[3].vx = trail->segment[n].left.vx + cam[0];
                    vertex[3].vy = trail->segment[n].left.vy + cam[1];
                    vertex[3].vz = trail->segment[n].left.vz + cam[2];
                    poly = &trail->segment[i].leftHalf[CurrentScreen];
                    depth = RotTransPers3(&vertex[0], &vertex[1], &vertex[3], (int *)&screen[0], (int *)&screen[1], (int *)&screen[3], &segmentP, &flag);
                    if (!(flag & 0x80460000)) {
                        depth -= 160;
                        poly->x0 = screen[0].vx;
                        poly->x1 = screen[0].vx + 2;
                        poly->x2 = screen[3].vx;
                        poly->x3 = screen[3].vx + 2;
                        poly->y0 = screen[0].vy;
                        poly->y1 = screen[0].vy;
                        poly->y2 = screen[3].vy;
                        poly->y3 = screen[3].vy;
                        poly->r0 = trail->segment[i].r;
                        poly->g0 = trail->segment[i].g;
                        poly->b0 = trail->segment[i].b;
                        poly->r1 = poly->r0;
                        poly->g1 = poly->g0;
                        poly->b1 = poly->b0;
                        poly->r2 = trail->segment[n].r;
                        poly->g2 = trail->segment[n].g;
                        poly->b2 = trail->segment[n].b;
                        poly->r3 = poly->r2;
                        poly->g3 = poly->g2;
                        poly->b3 = poly->b2;
                        if (depth < 0) depth = 0;
                        if (sillyShips == 0 || ship >= 16) {
                            AddPrim(OT[CurrentScreen] + (depth >> 1), &trail->segment[i].leftHalf[CurrentScreen]);
                            AddPrim(OT[CurrentScreen] + (depth >> 1), &trail->segment[i].drawMode[CurrentScreen]);
                        }
                        if (i == trail->current) {
                            if (sillyShips == 0 || ship >= 16) {
                                DrawFlare(screen[1].vx, screen[1].vy, depth, ship);
                            }
                            UpdateTarget(screen[1].vx, screen[1].vy, depth, ship);
                        }
                    }
                }
            }
            i += 2;
        } while (i < TRAIL_SEGMENTS);
    }
}


void DrawTrail(TrackCamera *camera, Trail *trail, int ship) {
    struct {
        SVECTOR vertex[6];
        VECTOR pivot;
        SVECTOR screen[6];
        int p;
        int flag;
    } ws;
    int oldest;
    int lx, ly, lz, rx, ry, rz;
    int distance;
    int depth;
    int i, n;
    POLY_G4 *left;
    POLY_G4 *right;
    TrailSegment *seg;
    Skeleton *view;
    int *cam;

    view = camera->camPos;
    ws.pivot.vx = (shipIndex[ship].ppivot.vx + view->relative.t[0]) >> 3;
    ws.pivot.vy = (shipIndex[ship].ppivot.vy + view->relative.t[1]) >> 3;
    ws.pivot.vz = (shipIndex[ship].ppivot.vz + view->relative.t[2]) >> 3;
    cam = view->relative.t;
    distance = GetMagnitude(ws.pivot);
    if (distance >= 0) {
        if (ship < 15) {
            if (distance >= 901) {
                DrawTrailLoRes(camera, trail, ship);
                return;
            }
            if (shipIndex[ship].combatAttr & 0x80) return;
        }
        oldest = (trail->current + 10) % TRAIL_SEGMENTS;
        i = 0;
        do {
            if (i != oldest) {
                n = (i + 1) % TRAIL_SEGMENTS;
                if (trail->segment[i].active != 0 && trail->segment[n].active != 0) {
                    seg = &trail->segment[i];
                    left = &seg->leftHalf[CurrentScreen];
                    right = &seg->rightHalf[CurrentScreen];
                    lx = trail->segment[i].left.vx;
                    ly = trail->segment[i].left.vy;
                    lz = trail->segment[i].left.vz;
                    rx = trail->segment[i].right.vx;
                    ry = trail->segment[i].right.vy;
                    rz = trail->segment[i].right.vz;
                    ws.vertex[0].vx = lx + cam[0];
                    ws.vertex[0].vy = ly + cam[1];
                    ws.vertex[0].vz = lz + cam[2];
                    ws.vertex[2].vx = rx + cam[0];
                    ws.vertex[2].vy = ry + cam[1];
                    ws.vertex[2].vz = rz + cam[2];
                    ws.vertex[1].vx = ((lx + rx) >> 1) + cam[0];
                    ws.vertex[1].vy = ((ly + ry) >> 1) + cam[1];
                    ws.vertex[1].vz = ((lz + rz) >> 1) + cam[2];
                    lx = trail->segment[n].left.vx;
                    ly = trail->segment[n].left.vy;
                    lz = trail->segment[n].left.vz;
                    rx = trail->segment[n].right.vx;
                    ry = trail->segment[n].right.vy;
                    rz = trail->segment[n].right.vz;
                    ws.vertex[3].vx = lx + cam[0];
                    ws.vertex[3].vy = ly + cam[1];
                    ws.vertex[3].vz = lz + cam[2];
                    ws.vertex[5].vx = rx + cam[0];
                    ws.vertex[5].vy = ry + cam[1];
                    ws.vertex[5].vz = rz + cam[2];
                    ws.vertex[4].vx = ((lx + rx) >> 1) + cam[0];
                    ws.vertex[4].vy = ((ly + ry) >> 1) + cam[1];
                    ws.vertex[4].vz = ((lz + rz) >> 1) + cam[2];
                    RotTransPers3(&ws.vertex[3], &ws.vertex[4], &ws.vertex[5], (int *)&ws.screen[3], (int *)&ws.screen[4], (int *)&ws.screen[5], &ws.p, &ws.flag);
                    depth = RotTransPers3(&ws.vertex[0], &ws.vertex[1], &ws.vertex[2], (int *)&ws.screen[0], (int *)&ws.screen[1], (int *)&ws.screen[2], &ws.p, &ws.flag);
                    if (!(ws.flag & 0x80460000)) {
                        depth -= 160;
                        left->x0 = ws.screen[0].vx;
                        left->x1 = ws.screen[1].vx;
                        left->x2 = ws.screen[3].vx;
                        left->x3 = ws.screen[4].vx;
                        left->y0 = ws.screen[0].vy;
                        left->y1 = ws.screen[1].vy;
                        left->y2 = ws.screen[3].vy;
                        left->y3 = ws.screen[4].vy;
                        right->x0 = ws.screen[1].vx;
                        right->x1 = ws.screen[2].vx;
                        right->x2 = ws.screen[4].vx;
                        right->x3 = ws.screen[5].vx;
                        right->y0 = ws.screen[1].vy;
                        right->y1 = ws.screen[2].vy;
                        right->y2 = ws.screen[4].vy;
                        right->y3 = ws.screen[5].vy;
                        left->r0 = (u_char) ((u_short) trail->segment[i].r >> 4);
                        left->g0 = (u_char) ((u_short) trail->segment[i].g >> 4);
                        left->b0 = (u_char) ((u_short) trail->segment[i].b >> 4);
                        left->r1 = (u_char) trail->segment[i].r;
                        left->g1 = (u_char) trail->segment[i].g;
                        left->b1 = (u_char) trail->segment[i].b;
                        left->r2 = (u_char) ((u_short) trail->segment[n].r >> 4);
                        left->g2 = (u_char) ((u_short) trail->segment[n].g >> 4);
                        left->b2 = (u_char) ((u_short) trail->segment[n].b >> 4);
                        left->r3 = (u_char) trail->segment[n].r;
                        left->g3 = (u_char) trail->segment[n].g;
                        left->b3 = (u_char) trail->segment[n].b;
                        right->r0 = (u_char) trail->segment[i].r;
                        right->g0 = (u_char) trail->segment[i].g;
                        right->b0 = (u_char) trail->segment[i].b;
                        right->r1 = (u_char) ((u_short) trail->segment[i].r >> 4);
                        right->g1 = (u_char) ((u_short) trail->segment[i].g >> 4);
                        right->b1 = (u_char) ((u_short) trail->segment[i].b >> 4);
                        right->r2 = (u_char) trail->segment[n].r;
                        right->g2 = (u_char) trail->segment[n].g;
                        right->b2 = (u_char) trail->segment[n].b;
                        right->r3 = (u_char) ((u_short) trail->segment[n].r >> 4);
                        right->g3 = (u_char) ((u_short) trail->segment[n].g >> 4);
                        right->b3 = (u_char) ((u_short) trail->segment[n].b >> 4);
                        if (depth < 0) depth = 0;
                        if (sillyShips == 0 || ship >= 16) {
                            AddPrim(OT[CurrentScreen] + (depth >> 1), &seg->leftHalf[CurrentScreen]);
                            AddPrim(OT[CurrentScreen] + (depth >> 1), &seg->rightHalf[CurrentScreen]);
                            AddPrim(OT[CurrentScreen] + (depth >> 1), &seg->drawMode[CurrentScreen]);
                        }
                        if (i == trail->current) {
                            if (sillyShips == 0 || ship >= 16) {
                                DrawFlare(ws.screen[1].vx, ws.screen[1].vy, depth, ship);
                            }
                            UpdateTarget(ws.screen[1].vx, ws.screen[1].vy, depth, ship);
                        }
                    }
                }
            }
            i++;
        } while (i < TRAIL_SEGMENTS);
    }
}


void DrawShipTrails(TrackCamera *camera) {
    int i;
    u_char *flag;

    if (shipIndex[ownShip].weapon != 0) {
        if (shipIndex[ownShip].targetShip != lastTargetShip) {
            lastTargetShip = shipIndex[ownShip].targetShip;
            i = 7;
            flag = &targetFlags[7];
            do {
                *flag = 0;
                i--;
                flag--;
            } while (i >= 0);
        }
        if ((shipIndex[ownShip].attr & RACING) && targetvalid != 0 && (shipIndex[ownShip].weapon == 2 || shipIndex[ownShip].weapon == 5) && shipIndex[ownShip].targetShip < 15) {
            i = 0;
            if (missileLockTimer > 0) {
                do {
                    if (targetPrims[i][0][0].g0 != 0) {
                        targetPrims[i][0][0].g0 -= 14;
                        targetPrims[i][0][1].g0 -= 14;
                        targetPrims[i][1][0].g0 -= 14;
                        targetPrims[i][1][1].g0 -= 14;
                        targetPrims[i][2][0].g0 -= 14;
                        targetPrims[i][2][1].g0 -= 14;
                        targetPrims[i][3][0].g0 -= 14;
                        targetPrims[i][3][1].g0 -= 14;
                    }
                    if (targetFlags[i] != 0) {
                        AddPrim(OT[CurrentScreen], &targetPrims[i][0][CurrentScreen]);
                        AddPrim(OT[CurrentScreen], &targetPrims[i][1][CurrentScreen]);
                        AddPrim(OT[CurrentScreen], &targetPrims[i][2][CurrentScreen]);
                        AddPrim(OT[CurrentScreen], &targetPrims[i][3][CurrentScreen]);
                    }
                    i++;
                } while (i < 8);
            } else {
                AddPrim(OT[CurrentScreen], &targetPrims[targetBank][0][CurrentScreen]);
                AddPrim(OT[CurrentScreen], &targetPrims[targetBank][1][CurrentScreen]);
                AddPrim(OT[CurrentScreen], &targetPrims[targetBank][2][CurrentScreen]);
                AddPrim(OT[CurrentScreen], &targetPrims[targetBank][3][CurrentScreen]);
            }
        } else {
            i = 7;
            flag = &targetFlags[7];
            do {
                *flag = 0;
                i--;
                flag--;
            } while (i >= 0);
        }
        AddPrim(OT[CurrentScreen], &targetDrawModes[CurrentScreen]);
        targetvalid = 0;
        targetBank = (targetBank + 1) & 7;
        targetSpin += 0x80;
        if (missileLockTimer > 0) missileLockTimer--;
    }
    i = 0;
    do {
        if ((shipIndex[i].attr & VISIBLE) && !(shipIndex[i].combatAttr & DESTROYED) && (i != ownShip || !(shipIndex[i].attr & VIEW_IN))) {
            DrawTrail(camera, &shipTrails[i], i);
        }
        i++;
    } while (i < 15);
    if (plasmaShotBusy != 0) {
        DrawTrail(camera, &Plasma[0], 0xF);
        DrawTrail(camera, &Plasma[1], 0x10);
        DrawTrail(camera, &Plasma[2], 0x11);
        DrawTrail(camera, &Plasma[3], 0x12);
    }
    if (flashNozzleShip >= 0) {
        DrawFlashNozzle(flashNozzleShip, camera);
    }
    flashNozzleShip = -1;
}


void UpdateShipTrails(ShipData *ships, int allShips)
{
    int    i;
    int    j;
    VECTOR left;
    VECTOR right;

    for (i = 0; i < 15; i++) {
        if (ships[i].attr & 0x200) {
            left.vx = ships[i].ppivot.vx - ((ships[i].unitVecNose.vx * 420) >> 12);
            left.vy = ships[i].ppivot.vy - ((ships[i].unitVecNose.vy * 420) >> 12);
            left.vz = ships[i].ppivot.vz - ((ships[i].unitVecNose.vz * 420) >> 12);
            left.vy -= 15;
            right.vx = left.vx;
            right.vy = left.vy;
            right.vz = left.vz;
            left.vx -= (ships[i].unitVecWing.vx * 40) >> 12;
            left.vy -= (ships[i].unitVecWing.vy * 40) >> 12;
            left.vz -= (ships[i].unitVecWing.vz * 40) >> 12;
            right.vx += (ships[i].unitVecWing.vx * 40) >> 12;
            right.vy += (ships[i].unitVecWing.vy * 40) >> 12;
            right.vz += (ships[i].unitVecWing.vz * 40) >> 12;
            if (i != ownShip || allShips) {
                NewTrailSegment(i, &left, &right);
            } else if (!(ships[i].attr & 4)) {
                BuildOwnShipTrail(i, &left, &right);
            }
        } else if (shipTrails[i].segment[0].active) {
            for (j = 0; j < TRAIL_SEGMENTS; j++) {
                shipTrails[i].segment[j].active = 0;
            }
        }
    }
}


void InitPlasmaShot(void) {
    int i;
    Plasma[0].current = 0;
    Plasma[0].colour[0][0] = 0xA0;
    Plasma[0].colour[0][1] = 0x60;
    Plasma[0].colour[0][2] = 0;
    Plasma[0].colour[1][0] = 0xFF;
    Plasma[0].colour[1][1] = 0xFF;
    Plasma[0].colour[1][2] = 0xFF;
    Plasma[1].current = 0;
    Plasma[1].colour[0][0] = 0xA0;
    Plasma[1].colour[0][1] = 0x60;
    Plasma[1].colour[0][2] = 0;
    Plasma[1].colour[1][0] = 0xFF;
    Plasma[1].colour[1][1] = 0xFF;
    Plasma[1].colour[1][2] = 0xFF;
    Plasma[2].current = 0;
    Plasma[2].colour[0][0] = 0xA0;
    Plasma[2].colour[0][1] = 0x60;
    Plasma[2].colour[0][2] = 0;
    Plasma[2].colour[1][0] = 0xFF;
    Plasma[2].colour[1][1] = 0xFF;
    Plasma[2].colour[1][2] = 0xFF;
    Plasma[3].current = 0;
    Plasma[3].colour[0][0] = 0xA0;
    Plasma[3].colour[0][1] = 0x60;
    Plasma[3].colour[0][2] = 0;
    Plasma[3].colour[1][0] = 0xFF;
    Plasma[3].colour[1][1] = 0xFF;
    Plasma[3].colour[1][2] = 0xFF;
    for (i = 0; i < TRAIL_SEGMENTS; i++) {
        Plasma[0].segment[i].active = 0;
        SetDrawMode(&Plasma[0].segment[i].drawMode[0], 1, 1, GetTPage(0, 1, 0, 0), 0);
        SetDrawMode(&Plasma[0].segment[i].drawMode[1], 1, 1, GetTPage(0, 1, 0, 0), 0);
        Plasma[1].segment[i].active = 0;
        SetDrawMode(&Plasma[1].segment[i].drawMode[0], 1, 1, GetTPage(0, 1, 0, 0), 0);
        SetDrawMode(&Plasma[1].segment[i].drawMode[1], 1, 1, GetTPage(0, 1, 0, 0), 0);
        Plasma[2].segment[i].active = 0;
        SetDrawMode(&Plasma[2].segment[i].drawMode[0], 1, 1, GetTPage(0, 1, 0, 0), 0);
        SetDrawMode(&Plasma[2].segment[i].drawMode[1], 1, 1, GetTPage(0, 1, 0, 0), 0);
        Plasma[3].segment[i].active = 0;
        SetDrawMode(&Plasma[3].segment[i].drawMode[0], 1, 1, GetTPage(0, 1, 0, 0), 0);
        SetDrawMode(&Plasma[3].segment[i].drawMode[1], 1, 1, GetTPage(0, 1, 0, 0), 0);
        SetPolyG4(&Plasma[0].segment[i].leftHalf[0]);
        SetPolyG4(&Plasma[0].segment[i].rightHalf[0]);
        SetPolyG4(&Plasma[0].segment[i].leftHalf[1]);
        SetPolyG4(&Plasma[0].segment[i].rightHalf[1]);
        SetPolyG4(&Plasma[1].segment[i].leftHalf[0]);
        SetPolyG4(&Plasma[1].segment[i].rightHalf[0]);
        SetPolyG4(&Plasma[1].segment[i].leftHalf[1]);
        SetPolyG4(&Plasma[1].segment[i].rightHalf[1]);
        SetPolyG4(&Plasma[2].segment[i].leftHalf[0]);
        SetPolyG4(&Plasma[2].segment[i].rightHalf[0]);
        SetPolyG4(&Plasma[2].segment[i].leftHalf[1]);
        SetPolyG4(&Plasma[2].segment[i].rightHalf[1]);
        SetPolyG4(&Plasma[3].segment[i].leftHalf[0]);
        SetPolyG4(&Plasma[3].segment[i].rightHalf[0]);
        SetPolyG4(&Plasma[3].segment[i].leftHalf[1]);
        SetPolyG4(&Plasma[3].segment[i].rightHalf[1]);
        SetSemiTrans(&Plasma[0].segment[i].leftHalf[0], 1);
        SetSemiTrans(&Plasma[0].segment[i].rightHalf[0], 1);
        SetSemiTrans(&Plasma[0].segment[i].leftHalf[1], 1);
        SetSemiTrans(&Plasma[0].segment[i].rightHalf[1], 1);
        SetSemiTrans(&Plasma[1].segment[i].leftHalf[0], 1);
        SetSemiTrans(&Plasma[1].segment[i].rightHalf[0], 1);
        SetSemiTrans(&Plasma[1].segment[i].leftHalf[1], 1);
        SetSemiTrans(&Plasma[1].segment[i].rightHalf[1], 1);
        SetSemiTrans(&Plasma[2].segment[i].leftHalf[0], 1);
        SetSemiTrans(&Plasma[2].segment[i].rightHalf[0], 1);
        SetSemiTrans(&Plasma[2].segment[i].leftHalf[1], 1);
        SetSemiTrans(&Plasma[2].segment[i].rightHalf[1], 1);
        SetSemiTrans(&Plasma[3].segment[i].leftHalf[0], 1);
        SetSemiTrans(&Plasma[3].segment[i].rightHalf[0], 1);
        SetSemiTrans(&Plasma[3].segment[i].leftHalf[1], 1);
        SetSemiTrans(&Plasma[3].segment[i].rightHalf[1], 1);
    }
    plasmaShotBusy = 1;
    plasmaPhaseA = 0;
    plasmaPhaseB = 0;
    plasmaPhaseC = 0;
}


void NewPlasmaSeg(VECTOR *pos, Trail *trail)
{
    int           i;
    short         intensity;

    intensity = 255;
    for (i = 0; i < TRAIL_SEGMENTS; i++) {
        if (trail->segment[i].active) {
            trail->segment[i].r -= 27;
            trail->segment[i].g -= 27;
            trail->segment[i].b -= 27;
            if (trail->segment[i].r < 0) trail->segment[i].r = 0;
            if (trail->segment[i].g < 0) trail->segment[i].g = 0;
            if (trail->segment[i].b < 0) trail->segment[i].b = 0;
        }
    }
    trail->current += 10;
    trail->current %= TRAIL_SEGMENTS;
    trail->segment[trail->current].active = 1;
    trail->segment[trail->current].left.vx = pos->vx - 40;
    trail->segment[trail->current].left.vy = pos->vy;
    trail->segment[trail->current].left.vz = pos->vz - 40;
    trail->segment[trail->current].right.vx = pos->vx + 40;
    trail->segment[trail->current].right.vy = pos->vy;
    trail->segment[trail->current].right.vz = pos->vz + 40;
    if (!(newRand() & 7)) {
        trail->segment[trail->current].r = (trail->colour[1][0] * intensity) >> 8;
        trail->segment[trail->current].g = (trail->colour[1][1] * intensity) >> 8;
        trail->segment[trail->current].b = (trail->colour[1][2] * intensity) >> 8;
    } else {
        trail->segment[trail->current].r = (trail->colour[0][0] * intensity) >> 8;
        trail->segment[trail->current].g = (trail->colour[0][1] * intensity) >> 8;
        trail->segment[trail->current].b = (trail->colour[0][2] * intensity) >> 8;
    }
}


void NewPlasmaSegment(VECTOR *pos)
{
    VECTOR offset;
    VECTOR point;

    plasmaPhaseA += 300;
    plasmaPhaseB += 400;
    plasmaPhaseC += 500;
    offset.vx = rsin(plasmaPhaseA) >> 4;
    offset.vy = rsin(plasmaPhaseB) >> 4;
    point.vx = pos->vx + offset.vx;
    point.vy = pos->vy + offset.vy;
    point.vz = pos->vz;
    NewPlasmaSeg(&point, &Plasma[0]);
    offset.vx = rsin(plasmaPhaseB) >> 4;
    offset.vz = rsin(plasmaPhaseC) >> 4;
    point.vx = pos->vx + offset.vx;
    point.vy = pos->vy;
    point.vz = pos->vz + offset.vz;
    NewPlasmaSeg(&point, &Plasma[1]);
    offset.vy = rsin(plasmaPhaseB) >> 4;
    offset.vz = rcos(plasmaPhaseC) >> 4;
    point.vx = pos->vx;
    point.vy = pos->vy + offset.vy;
    point.vz = pos->vz + offset.vz;
    NewPlasmaSeg(&point, &Plasma[2]);
    offset.vx = rsin(plasmaPhaseC) >> 4;
    offset.vz = rcos(plasmaPhaseA) >> 4;
    point.vx = pos->vx + offset.vx;
    point.vy = pos->vy;
    point.vz = pos->vz + offset.vz;
    NewPlasmaSeg(&point, &Plasma[3]);
}


void DrawFlashNozzle(int shipNum, TrackCamera *camera) {
    struct {
        SVECTOR vertex[4];
        SVECTOR screen[4];
        int p;
        int flag;
    } ws;
    int depth;
    int yOffset;
    ShipData *ship;
    Skeleton *view;

    view = camera->camPos;
    /* Port: was shipNum * sizeof(ShipData) + (int)shipIndex; ShipData is bigger natively. */
    ship = &shipIndex[shipNum];
    yOffset = (ship->attr & VIEW_IN) ? 0x60 : 0;
    ws.vertex[0].vx = ship->ppivot.vx + (ship->unitVecNose.vx >> 3) - (ship->unitVecNose.vx >> 5) + view->relative.t[0];
    ws.vertex[0].vy = ship->ppivot.vy + (ship->unitVecNose.vy >> 3) - (ship->unitVecNose.vy >> 5) + view->relative.t[1];
    ws.vertex[0].vz = ship->ppivot.vz + (ship->unitVecNose.vz >> 3) - (ship->unitVecNose.vz >> 5) + view->relative.t[2];
    ws.vertex[1].vx = ship->ppivot.vx + (ship->unitVecNose.vx >> 3) - (ship->unitVecNose.vx >> 5) - (ship->unitVecWing.vx >> 7) - (ship->unitVecFin.vx >> 6) + view->relative.t[0];
    ws.vertex[1].vy = ship->ppivot.vy + (ship->unitVecNose.vy >> 3) - (ship->unitVecNose.vy >> 5) - (ship->unitVecWing.vy >> 7) - (ship->unitVecFin.vy >> 6) + view->relative.t[1];
    ws.vertex[1].vz = ship->ppivot.vz + (ship->unitVecNose.vz >> 3) - (ship->unitVecNose.vz >> 5) - (ship->unitVecWing.vz >> 7) - (ship->unitVecFin.vz >> 6) + view->relative.t[2];
    ws.vertex[2].vx = ship->ppivot.vx + (ship->unitVecNose.vx >> 3) - (ship->unitVecNose.vx >> 5) - (ship->unitVecWing.vx >> 6) - (ship->unitVecFin.vx >> 7) + view->relative.t[0];
    ws.vertex[2].vy = ship->ppivot.vy + (ship->unitVecNose.vy >> 3) - (ship->unitVecNose.vy >> 5) - (ship->unitVecWing.vy >> 6) - (ship->unitVecFin.vy >> 7) + view->relative.t[1];
    ws.vertex[2].vz = ship->ppivot.vz + (ship->unitVecNose.vz >> 3) - (ship->unitVecNose.vz >> 5) - (ship->unitVecWing.vz >> 6) - (ship->unitVecFin.vz >> 7) + view->relative.t[2];
    ws.vertex[3].vx = ship->ppivot.vx + (ship->unitVecNose.vx >> 3) - (ship->unitVecNose.vx >> 5) - (ship->unitVecFin.vx >> 5) - (ship->unitVecWing.vx >> 5) + view->relative.t[0];
    ws.vertex[3].vy = ship->ppivot.vy + (ship->unitVecNose.vy >> 3) - (ship->unitVecNose.vy >> 5) - (ship->unitVecFin.vy >> 5) - (ship->unitVecWing.vy >> 5) + view->relative.t[1];
    ws.vertex[3].vz = ship->ppivot.vz + (ship->unitVecNose.vz >> 3) - (ship->unitVecNose.vz >> 5) - (ship->unitVecFin.vz >> 5) - (ship->unitVecWing.vz >> 5) + view->relative.t[2];
    depth = RotTransPers(&ws.vertex[3], (int *)&ws.screen[3], &ws.p, &ws.flag);
    RotTransPers(&ws.vertex[2], (int *)&ws.screen[2], &ws.p, &ws.flag);
    RotTransPers(&ws.vertex[1], (int *)&ws.screen[1], &ws.p, &ws.flag);
    RotTransPers(&ws.vertex[0], (int *)&ws.screen[0], &ws.p, &ws.flag);
    if (!(ws.flag & 0x80460000)) {
        int otDepth;
        muzzletop[CurrentScreen][0].x0 = ws.screen[0].vx;
        muzzletop[CurrentScreen][0].y0 = ws.screen[0].vy + yOffset;
        muzzletop[CurrentScreen][0].x1 = ws.screen[1].vx;
        muzzletop[CurrentScreen][0].y1 = ws.screen[1].vy + yOffset;
        muzzletop[CurrentScreen][0].x2 = ws.screen[3].vx;
        muzzletop[CurrentScreen][0].y2 = ws.screen[3].vy + yOffset;
        muzzletop[CurrentScreen][1].x0 = ws.screen[0].vx;
        muzzletop[CurrentScreen][1].y0 = ws.screen[0].vy + yOffset;
        muzzletop[CurrentScreen][1].x1 = ws.screen[2].vx;
        muzzletop[CurrentScreen][1].y1 = ws.screen[2].vy + yOffset;
        muzzletop[CurrentScreen][1].x2 = ws.screen[3].vx;
        muzzletop[CurrentScreen][1].y2 = ws.screen[3].vy + yOffset;
        otDepth = depth >> 1;
        AddPrim(OT[CurrentScreen] + otDepth, &muzzletop[CurrentScreen][0]);
        AddPrim(OT[CurrentScreen] + otDepth, &muzzletop[CurrentScreen][1]);
    }
    ws.vertex[1].vx = ship->ppivot.vx + (ship->unitVecNose.vx >> 3) - (ship->unitVecNose.vx >> 5) + (ship->unitVecWing.vx >> 7) - (ship->unitVecFin.vx >> 6) + view->relative.t[0];
    ws.vertex[1].vy = ship->ppivot.vy + (ship->unitVecNose.vy >> 3) - (ship->unitVecNose.vy >> 5) + (ship->unitVecWing.vy >> 7) - (ship->unitVecFin.vy >> 6) + view->relative.t[1];
    ws.vertex[1].vz = ship->ppivot.vz + (ship->unitVecNose.vz >> 3) - (ship->unitVecNose.vz >> 5) + (ship->unitVecWing.vz >> 7) - (ship->unitVecFin.vz >> 6) + view->relative.t[2];
    ws.vertex[2].vx = ship->ppivot.vx + (ship->unitVecNose.vx >> 3) - (ship->unitVecNose.vx >> 5) + (ship->unitVecWing.vx >> 6) - (ship->unitVecFin.vx >> 7) + view->relative.t[0];
    ws.vertex[2].vy = ship->ppivot.vy + (ship->unitVecNose.vy >> 3) - (ship->unitVecNose.vy >> 5) + (ship->unitVecWing.vy >> 6) - (ship->unitVecFin.vy >> 7) + view->relative.t[1];
    ws.vertex[2].vz = ship->ppivot.vz + (ship->unitVecNose.vz >> 3) - (ship->unitVecNose.vz >> 5) + (ship->unitVecWing.vz >> 6) - (ship->unitVecFin.vz >> 7) + view->relative.t[2];
    ws.vertex[3].vx = ship->ppivot.vx + (ship->unitVecNose.vx >> 3) - (ship->unitVecNose.vx >> 5) - (ship->unitVecFin.vx >> 5) + (ship->unitVecWing.vx >> 5) + view->relative.t[0];
    ws.vertex[3].vy = ship->ppivot.vy + (ship->unitVecNose.vy >> 3) - (ship->unitVecNose.vy >> 5) - (ship->unitVecFin.vy >> 5) + (ship->unitVecWing.vy >> 5) + view->relative.t[1];
    ws.vertex[3].vz = ship->ppivot.vz + (ship->unitVecNose.vz >> 3) - (ship->unitVecNose.vz >> 5) - (ship->unitVecFin.vz >> 5) + (ship->unitVecWing.vz >> 5) + view->relative.t[2];
    RotTransPers(&ws.vertex[3], (int *)&ws.screen[3], &ws.p, &ws.flag);
    RotTransPers(&ws.vertex[2], (int *)&ws.screen[2], &ws.p, &ws.flag);
    RotTransPers(&ws.vertex[1], (int *)&ws.screen[1], &ws.p, &ws.flag);
    if (!(ws.flag & 0x80460000)) {
        int otDepth;
        muzzlebot[CurrentScreen][0].x0 = ws.screen[0].vx;
        muzzlebot[CurrentScreen][0].y0 = ws.screen[0].vy + yOffset;
        muzzlebot[CurrentScreen][0].x1 = ws.screen[1].vx;
        muzzlebot[CurrentScreen][0].y1 = ws.screen[1].vy + yOffset;
        muzzlebot[CurrentScreen][0].x2 = ws.screen[3].vx;
        muzzlebot[CurrentScreen][0].y2 = ws.screen[3].vy + yOffset;
        muzzlebot[CurrentScreen][1].x0 = ws.screen[0].vx;
        muzzlebot[CurrentScreen][1].y0 = ws.screen[0].vy + yOffset;
        muzzlebot[CurrentScreen][1].x1 = ws.screen[2].vx;
        muzzlebot[CurrentScreen][1].y1 = ws.screen[2].vy + yOffset;
        muzzlebot[CurrentScreen][1].x2 = ws.screen[3].vx;
        muzzlebot[CurrentScreen][1].y2 = ws.screen[3].vy + yOffset;
        otDepth = depth >> 1;
        AddPrim(OT[CurrentScreen] + otDepth, &muzzlebot[CurrentScreen][0]);
        AddPrim(OT[CurrentScreen] + otDepth, &muzzlebot[CurrentScreen][1]);
    }
    ws.vertex[1].vx = ship->ppivot.vx + (ship->unitVecNose.vx >> 3) - (ship->unitVecNose.vx >> 5) - (ship->unitVecWing.vx >> 7) + (ship->unitVecFin.vx >> 6) + view->relative.t[0];
    ws.vertex[1].vy = ship->ppivot.vy + (ship->unitVecNose.vy >> 3) - (ship->unitVecNose.vy >> 5) - (ship->unitVecWing.vy >> 7) + (ship->unitVecFin.vy >> 6) + view->relative.t[1];
    ws.vertex[1].vz = ship->ppivot.vz + (ship->unitVecNose.vz >> 3) - (ship->unitVecNose.vz >> 5) - (ship->unitVecWing.vz >> 7) + (ship->unitVecFin.vz >> 6) + view->relative.t[2];
    ws.vertex[2].vx = ship->ppivot.vx + (ship->unitVecNose.vx >> 3) - (ship->unitVecNose.vx >> 5) - (ship->unitVecWing.vx >> 6) + (ship->unitVecFin.vx >> 7) + view->relative.t[0];
    ws.vertex[2].vy = ship->ppivot.vy + (ship->unitVecNose.vy >> 3) - (ship->unitVecNose.vy >> 5) - (ship->unitVecWing.vy >> 6) + (ship->unitVecFin.vy >> 7) + view->relative.t[1];
    ws.vertex[2].vz = ship->ppivot.vz + (ship->unitVecNose.vz >> 3) - (ship->unitVecNose.vz >> 5) - (ship->unitVecWing.vz >> 6) + (ship->unitVecFin.vz >> 7) + view->relative.t[2];
    ws.vertex[3].vx = ship->ppivot.vx + (ship->unitVecNose.vx >> 3) - (ship->unitVecNose.vx >> 5) + (ship->unitVecFin.vx >> 5) - (ship->unitVecWing.vx >> 5) + view->relative.t[0];
    ws.vertex[3].vy = ship->ppivot.vy + (ship->unitVecNose.vy >> 3) - (ship->unitVecNose.vy >> 5) + (ship->unitVecFin.vy >> 5) - (ship->unitVecWing.vy >> 5) + view->relative.t[1];
    ws.vertex[3].vz = ship->ppivot.vz + (ship->unitVecNose.vz >> 3) - (ship->unitVecNose.vz >> 5) + (ship->unitVecFin.vz >> 5) - (ship->unitVecWing.vz >> 5) + view->relative.t[2];
    RotTransPers(&ws.vertex[3], (int *)&ws.screen[3], &ws.p, &ws.flag);
    RotTransPers(&ws.vertex[2], (int *)&ws.screen[2], &ws.p, &ws.flag);
    RotTransPers(&ws.vertex[1], (int *)&ws.screen[1], &ws.p, &ws.flag);
    if (!(ws.flag & 0x80460000)) {
        int otDepth;
        muzzleleft[CurrentScreen][0].x0 = ws.screen[0].vx;
        muzzleleft[CurrentScreen][0].y0 = ws.screen[0].vy + yOffset;
        muzzleleft[CurrentScreen][0].x1 = ws.screen[1].vx;
        muzzleleft[CurrentScreen][0].y1 = ws.screen[1].vy + yOffset;
        muzzleleft[CurrentScreen][0].x2 = ws.screen[3].vx;
        muzzleleft[CurrentScreen][0].y2 = ws.screen[3].vy + yOffset;
        muzzleleft[CurrentScreen][1].x0 = ws.screen[0].vx;
        muzzleleft[CurrentScreen][1].y0 = ws.screen[0].vy + yOffset;
        muzzleleft[CurrentScreen][1].x1 = ws.screen[2].vx;
        muzzleleft[CurrentScreen][1].y1 = ws.screen[2].vy + yOffset;
        muzzleleft[CurrentScreen][1].x2 = ws.screen[3].vx;
        muzzleleft[CurrentScreen][1].y2 = ws.screen[3].vy + yOffset;
        otDepth = depth >> 1;
        AddPrim(OT[CurrentScreen] + otDepth, &muzzleleft[CurrentScreen][0]);
        AddPrim(OT[CurrentScreen] + otDepth, &muzzleleft[CurrentScreen][1]);
    }
    ws.vertex[1].vx = ship->ppivot.vx + (ship->unitVecNose.vx >> 3) - (ship->unitVecNose.vx >> 5) + (ship->unitVecWing.vx >> 7) + (ship->unitVecFin.vx >> 6) + view->relative.t[0];
    ws.vertex[1].vy = ship->ppivot.vy + (ship->unitVecNose.vy >> 3) - (ship->unitVecNose.vy >> 5) + (ship->unitVecWing.vy >> 7) + (ship->unitVecFin.vy >> 6) + view->relative.t[1];
    ws.vertex[1].vz = ship->ppivot.vz + (ship->unitVecNose.vz >> 3) - (ship->unitVecNose.vz >> 5) + (ship->unitVecWing.vz >> 7) + (ship->unitVecFin.vz >> 6) + view->relative.t[2];
    ws.vertex[2].vx = ship->ppivot.vx + (ship->unitVecNose.vx >> 3) - (ship->unitVecNose.vx >> 5) + (ship->unitVecWing.vx >> 6) + (ship->unitVecFin.vx >> 7) + view->relative.t[0];
    ws.vertex[2].vy = ship->ppivot.vy + (ship->unitVecNose.vy >> 3) - (ship->unitVecNose.vy >> 5) + (ship->unitVecWing.vy >> 6) + (ship->unitVecFin.vy >> 7) + view->relative.t[1];
    ws.vertex[2].vz = ship->ppivot.vz + (ship->unitVecNose.vz >> 3) - (ship->unitVecNose.vz >> 5) + (ship->unitVecWing.vz >> 6) + (ship->unitVecFin.vz >> 7) + view->relative.t[2];
    ws.vertex[3].vx = ship->ppivot.vx + (ship->unitVecNose.vx >> 3) - (ship->unitVecNose.vx >> 5) + (ship->unitVecFin.vx >> 5) + (ship->unitVecWing.vx >> 5) + view->relative.t[0];
    ws.vertex[3].vy = ship->ppivot.vy + (ship->unitVecNose.vy >> 3) - (ship->unitVecNose.vy >> 5) + (ship->unitVecFin.vy >> 5) + (ship->unitVecWing.vy >> 5) + view->relative.t[1];
    ws.vertex[3].vz = ship->ppivot.vz + (ship->unitVecNose.vz >> 3) - (ship->unitVecNose.vz >> 5) + (ship->unitVecFin.vz >> 5) + (ship->unitVecWing.vz >> 5) + view->relative.t[2];
    RotTransPers(&ws.vertex[3], (int *)&ws.screen[3], &ws.p, &ws.flag);
    RotTransPers(&ws.vertex[2], (int *)&ws.screen[2], &ws.p, &ws.flag);
    RotTransPers(&ws.vertex[1], (int *)&ws.screen[1], &ws.p, &ws.flag);
    if (!(ws.flag & 0x80460000)) {
        int otDepth;
        muzzleright[CurrentScreen][0].x0 = ws.screen[0].vx;
        muzzleright[CurrentScreen][0].y0 = ws.screen[0].vy + yOffset;
        muzzleright[CurrentScreen][0].x1 = ws.screen[1].vx;
        muzzleright[CurrentScreen][0].y1 = ws.screen[1].vy + yOffset;
        muzzleright[CurrentScreen][0].x2 = ws.screen[3].vx;
        muzzleright[CurrentScreen][0].y2 = ws.screen[3].vy + yOffset;
        muzzleright[CurrentScreen][1].x0 = ws.screen[0].vx;
        muzzleright[CurrentScreen][1].y0 = ws.screen[0].vy + yOffset;
        muzzleright[CurrentScreen][1].x1 = ws.screen[2].vx;
        muzzleright[CurrentScreen][1].y1 = ws.screen[2].vy + yOffset;
        muzzleright[CurrentScreen][1].x2 = ws.screen[3].vx;
        muzzleright[CurrentScreen][1].y2 = ws.screen[3].vy + yOffset;
        otDepth = depth >> 1;
        AddPrim(OT[CurrentScreen] + otDepth, &muzzleright[CurrentScreen][0]);
        AddPrim(OT[CurrentScreen] + otDepth, &muzzleright[CurrentScreen][1]);
        AddPrim(OT[CurrentScreen] + otDepth, &muzzletrans[CurrentScreen]);
    }
}
