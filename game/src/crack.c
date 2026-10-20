#include "functions.h"
#include "trackasm.h"
#include "crack.h"

extern TrackData   *track;
extern short        CurrentScreen;
extern Surface      tileSurfaces[24][2];
/* One section holds at most 8 faces (the console reserved 8 entries for each table). The console laid
   hiMedCrackPolysB out inside hiMedCrackPolysA's rows (columns 2 and 3 of each row); here they are
   two tables of 8 rows, which behaves the same. */
extern PolyFT3      medLoCrackPolys[8][2];
extern PolyFT3      hiMedCrackPolysA[8][4];
void FillMedLoCrack(TrackSection *section, Skeleton *view)
{
    CrackLocals     locals;
    volatile VECTOR *camera;
    VECTOR          *vertices;
    Face            *face;
    PolyFT3         *poly;
    Tile            *tile;
    short            i;
    int             depth;

    i = 0;
    SetSkeletonTransform(view->super);
    locals.camera.vx = view->relative.t[0];
    locals.camera.vy = view->relative.t[1];
    locals.camera.vz = view->relative.t[2];
    camera = &locals.camera;
    vertices = track->vertices;
    face = track->faces + section->faceStart;
    if (section->faceCount > 0) {
        do {
            locals.v[0] = vertices[face->vertex[1]];
            locals.v[2] = vertices[face->vertex[0]];
            locals.v[6] = vertices[face->vertex[2]];
            locals.v[8] = vertices[face->vertex[3]];
            locals.v[1].vx = (locals.v[0].vx + locals.v[2].vx) >> 1;
            locals.v[1].vy = (locals.v[0].vy + locals.v[2].vy) >> 1;
            locals.v[1].vz = (locals.v[0].vz + locals.v[2].vz) >> 1;
            locals.sv[0].vx = locals.v[0].vx + camera->vx;
            locals.sv[0].vy = locals.v[0].vy + camera->vy;
            locals.sv[0].vz = locals.v[0].vz + camera->vz;
            locals.sv[1].vx = locals.v[1].vx + camera->vx;
            locals.sv[1].vy = locals.v[1].vy + camera->vy;
            locals.sv[1].vz = locals.v[1].vz + camera->vz;
            locals.sv[2].vx = locals.v[2].vx + camera->vx;
            locals.sv[2].vy = locals.v[2].vy + camera->vy;
            locals.sv[2].vz = locals.v[2].vz + camera->vz;
            RotTransPers(&locals.sv[0], (int *)&locals.screen[0][0], &locals.p, &locals.flags[0]);
            depth = RotTransPers(&locals.sv[1], (int *)&locals.screen[1][0], &locals.p, &locals.flags[1]);
            RotTransPers(&locals.sv[2], (int *)&locals.screen[2][0], &locals.p, &locals.flags[2]);
            if (!((locals.flags[0] | locals.flags[1] | locals.flags[2]) & 0x80460000)) {
                poly = &medLoCrackPolys[i][CurrentScreen];
                SetPolyFT3((POLY_FT3 *)poly);
                if (face->flags & 4) {
                    tile = &tileSurfaces[face->texture][1].low[0];
                } else {
                    tile = &tileSurfaces[face->texture][0].low[0];
                }
                poly->cba = tile->cba;
                poly->tsb = tile->tsb;
                poly->u0 = tile->u0;
                poly->v0 = tile->v0;
                poly->u1 = (tile->u0 + tile->u1) >> 1;
                poly->v1 = (tile->v0 + tile->v1) >> 1;
                poly->u2 = tile->u1;
                poly->v2 = tile->v1;
                poly->colour0.r = face->red;
                poly->colour0.g = face->green;
                poly->colour0.b = face->blue;
                poly->xy0 = locals.screen[0][0];
                poly->xy1 = locals.screen[1][0];
                poly->xy2 = locals.screen[2][0];
                AddPrim(OT[CurrentScreen] + (depth & 65535), poly);
            }
            face++;
        } while (++i < section->faceCount);
    }
}


void FillHiMedCrack(TrackSection *hiSection, Skeleton *camera)
{
    Face       *hiFace;
    short       i;
    VECTOR      camposData;
    VECTOR     *campos = &camposData;
    VECTOR     *vertices;
    VECTOR      vector[25];
    SVECTOR     svector[25];
    unsigned short depthA, depthB, depthC, depthD, depthE;
    int        flagA, flagB, flagC, flagD, flagE;
    DVECTOR     sxyA, sxyB, sxyC, sxyD, sxyE;
    int        p;
    PolyFT3    *ft3;
    Surface    *surface;
    Tile       *tile;

    SetSkeletonTransform(camera->super);

    campos->vx = camera->relative.t[0];
    campos->vy = camera->relative.t[1];
    campos->vz = camera->relative.t[2];

    vertices = track->vertices;
    hiFace = track->faces + hiSection->faceStart;

    for (i = 0; i < hiSection->faceCount; i++) {
        vector[0] = *(vertices + hiFace->vertex[1]);
        vector[4] = *(vertices + hiFace->vertex[0]);

        vector[2].vx = (vector[0].vx + vector[4].vx) >> 1;
        vector[2].vy = (vector[0].vy + vector[4].vy) >> 1;
        vector[2].vz = (vector[0].vz + vector[4].vz) >> 1;

        vector[1].vx = (vector[0].vx + vector[2].vx) >> 1;
        vector[1].vy = (vector[0].vy + vector[2].vy) >> 1;
        vector[1].vz = (vector[0].vz + vector[2].vz) >> 1;

        vector[3].vx = (vector[2].vx + vector[4].vx) >> 1;
        vector[3].vy = (vector[2].vy + vector[4].vy) >> 1;
        vector[3].vz = (vector[2].vz + vector[4].vz) >> 1;

        svector[0].vx = vector[0].vx + campos->vx;
        svector[0].vy = vector[0].vy + campos->vy;
        svector[0].vz = vector[0].vz + campos->vz;

        svector[1].vx = vector[1].vx + campos->vx;
        svector[1].vy = vector[1].vy + campos->vy;
        svector[1].vz = vector[1].vz + campos->vz;

        svector[2].vx = vector[2].vx + campos->vx;
        svector[2].vy = vector[2].vy + campos->vy;
        svector[2].vz = vector[2].vz + campos->vz;

        svector[3].vx = vector[3].vx + campos->vx;
        svector[3].vy = vector[3].vy + campos->vy;
        svector[3].vz = vector[3].vz + campos->vz;

        svector[4].vx = vector[4].vx + campos->vx;
        svector[4].vy = vector[4].vy + campos->vy;
        svector[4].vz = vector[4].vz + campos->vz;

        depthA = RotTransPers(&svector[0], (int *)&sxyA, &p, &flagA);
        depthB = RotTransPers(&svector[1], (int *)&sxyB, &p, &flagB);
        depthC = RotTransPers(&svector[2], (int *)&sxyC, &p, &flagC);
        depthD = RotTransPers(&svector[3], (int *)&sxyD, &p, &flagD);
        depthE = RotTransPers(&svector[4], (int *)&sxyE, &p, &flagE);

        if (!((flagA | flagB | flagC) & 0x80460000)) {
            ft3 = &hiMedCrackPolysA[i][CurrentScreen];
            SetPolyFT3((POLY_FT3 *)ft3);
            if (hiFace->flags & 4) {
                surface = &tileSurfaces[hiFace->texture][1];
            } else {
                surface = &tileSurfaces[hiFace->texture][0];
            }
            tile = &surface->med[0];
            ft3->cba = tile->cba;
            ft3->tsb = tile->tsb;
            ft3->u0 = tile->u0;
            ft3->v0 = tile->v0;
            ft3->u1 = (tile->u0 + tile->u1) >> 1;
            ft3->v1 = (tile->v0 + tile->v1) >> 1;
            ft3->u2 = tile->u1;
            ft3->v2 = tile->v1;
            ft3->colour0.r = hiFace->red;
            ft3->colour0.g = hiFace->green;
            ft3->colour0.b = hiFace->blue;
            ft3->xy0 = sxyA;
            ft3->xy1 = sxyB;
            ft3->xy2 = sxyC;
            AddPrim(OT[CurrentScreen] + depthB, ft3);
        }

        if (!((flagC | flagD | flagE) & 0x80460000)) {
            ft3 = &hiMedCrackPolysB[i][CurrentScreen];
            SetPolyFT3((POLY_FT3 *)ft3);
            if (hiFace->flags & 4) {
                surface = &tileSurfaces[hiFace->texture][1];
            } else {
                surface = &tileSurfaces[hiFace->texture][0];
            }
            tile = &surface->med[1];
            ft3->cba = tile->cba;
            ft3->tsb = tile->tsb;
            ft3->u0 = tile->u0;
            ft3->v0 = tile->v0;
            ft3->u1 = (tile->u0 + tile->u1) >> 1;
            ft3->v1 = (tile->v0 + tile->v1) >> 1;
            ft3->u2 = tile->u1;
            ft3->v2 = tile->v1;
            ft3->colour0.r = hiFace->red;
            ft3->colour0.g = hiFace->green;
            ft3->colour0.b = hiFace->blue;
            ft3->xy0 = sxyC;
            ft3->xy1 = sxyD;
            ft3->xy2 = sxyE;
            AddPrim(OT[CurrentScreen] + depthD, ft3);
        }

        hiFace++;
    }
}
