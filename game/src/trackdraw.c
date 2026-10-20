/* The track renderer: the three levels of detail in which the faces of a track section are drawn
   (AsmTrackLo, AsmTrackMed, AsmTrackHi).  On the console these were hand-written assembly routines
   (asm/800.s of the decomp); this is their translation to C, running on the GTE of the platform.  Each
   routine projects the corners of the faces of `count` consecutive sections, builds textured quads in the
   pool of the current screen and links them into its ordering table by depth.

   Port: the console's register-level details that no C compiler would produce (the coordinates checked
   with `sra 15`, the scratchpad as the working area, the packets written word by word) are kept as they
   are, because they decide which faces are drawn and how: a corner whose camera-relative coordinate does not
   fit in 16 bits makes the whole face disappear, and the GTE flag bits 0x80460000 (and 0x466 of the
   upper half) reject the face when a corner is off screen or too close. */
#include "functions.h"
#include "track.h"
#include "trackasm.h"

extern short       CurrentScreen;
extern TrackData  *track;
extern Surface     tileSurfaces[][2];
extern short       loTileCount;
extern short       medTileCount;
extern short       hiTileCount;

/* GTE flag bits that reject a face: divide overflow, a screen coordinate out of range, SZ3/OTZ out of range. */
#define GTE_REJECT 0x80460000u

/* A camera-relative coordinate is usable when it fits in a signed 16-bit value: (v >> 15) is 0 or -1. */
static int FitsInHalfword(int value)
{
    return (value >> 15) == 0 || (value >> 15) == -1;
}

/* The tile of a face: the surface of its texture, mirrored when the face is flipped. */
static Surface *FaceSurface(Face *face)
{
    return &tileSurfaces[(u_char)face->texture][(face->flags & FLIP) != 0];
}

/* Puts the tile's texture page, palette and texture coordinates into a textured quad. */
static void SetQuadTile(POLY_FT4 *poly, Tile *tile)
{
    poly->tpage = tile->tsb;
    poly->clut = tile->cba;
    poly->u0 = tile->u0;
    poly->v0 = tile->v0;
    poly->u1 = tile->u1;
    poly->v1 = tile->v1;
    poly->u2 = tile->u2;
    poly->v2 = tile->v2;
    poly->u3 = tile->u3;
    poly->v3 = tile->v3;
}

/* Stores a packed screen position (x in the low half, y in the high half: SXYn of the GTE). */
static void StoreScreenXY(short *x, short *y, int packed)
{
    *x = (short)packed;
    *y = (short)(packed >> 16);
}

/* Loads corner `vertex` of the track, relative to the camera, into the GTE vertex register pair that starts at
   `reg` (xy in `reg`, z in `reg + 1`).  Returns 0, loading nothing, when a coordinate does not fit in 16 bits. */
static int LoadCorner(VECTOR *vertex, int reg, int cameraX, int cameraY, int cameraZ)
{
    int x = vertex->vx + cameraX;
    int y = vertex->vy + cameraY;
    int z = vertex->vz + cameraZ;

    if (!FitsInHalfword(x) || !FitsInHalfword(y) || !FitsInHalfword(z)) {
        return 0;
    }
    GteWriteData(reg, (x & 0xFFFF) | (int)((u_int)y << 16));
    GteWriteData(reg + 1, z);
    return 1;
}

/* Low detail: one textured quad per face.  `loCount` sections starting at firstSection are drawn (the
   console's loops ran their body once before testing the counts, so a count of 0 still draws one face of one
   section). */
void AsmTrackLo(TrackSection *firstSection, Skeleton *camera, short loCount)
{
    TrackSection *section;
    Face         *face;
    POLY_FT4     *poly;
    u_long       *ordering;
    VECTOR       *vertices;
    int           cameraX, cameraY, cameraZ;
    int           faceCount;
    int           depth;

    section = firstSection;
    ordering = OT[CurrentScreen];
    SetSkeletonTransform(camera->super);
    cameraX = camera->relative.t[0];
    cameraY = camera->relative.t[1];
    cameraZ = camera->relative.t[2];
    vertices = track->vertices;
    poly = TrackLoPolys[CurrentScreen] + loTileCount;
    do {
        faceCount = (u_short)section->faceCount;
        face = track->faces + (u_short)section->faceStart;
        do {
            /* Corner 3 on its own, then corners 0, 1 and 2 together. */
            if (!LoadCorner(&vertices[face->vertex[3]], 0, cameraX, cameraY, cameraZ)) {
                goto next_face;
            }
            GteExecute(GTE_RTPS);
            StoreScreenXY(&poly->x3, &poly->y3, GteReadData(14));
            if ((u_int)GteReadControl(31) & GTE_REJECT) {
                goto next_face;
            }
            if (!LoadCorner(&vertices[face->vertex[0]], 0, cameraX, cameraY, cameraZ) ||
                !LoadCorner(&vertices[face->vertex[1]], 2, cameraX, cameraY, cameraZ) ||
                !LoadCorner(&vertices[face->vertex[2]], 4, cameraX, cameraY, cameraZ)) {
                goto next_face;
            }
            GteExecute(GTE_RTPT);
            StoreScreenXY(&poly->x1, &poly->y1, GteReadData(12));
            StoreScreenXY(&poly->x0, &poly->y0, GteReadData(13));
            StoreScreenXY(&poly->x2, &poly->y2, GteReadData(14));
            depth = GteReadData(19);
            if ((u_int)GteReadControl(31) & GTE_REJECT) {
                goto next_face;
            }
            SetQuadTile(poly, &FaceSurface(face)->low[0]);
            poly->r0 = face->red;
            poly->g0 = face->green;
            poly->b0 = face->blue;
            AddPrim((u_long *)((char *)ordering + (depth & 0xFFFC)), poly);
            poly++;
            loTileCount++;
        next_face:
            face++;
        } while (--faceCount > 0);
        section++;
    } while (--loCount > 0);
}


/* ---- medium detail ---- */

/* One of the nine points of the 3x3 grid a face is cut into: the camera-relative position (a halfword each),
   and what the projection returned: the screen position, the depth, and the upper bits of the GTE flags. */
typedef struct GridPoint {
    short   x, y, z;
    short   unused;
    short   sx, sy;
    u_short depth;
    u_short flag;
} GridPoint;

/* Draws the quad of four grid points as one textured quad of the pool and links it by the average of their depths. */
static POLY_FT4 *DrawGridQuad(POLY_FT4 *poly, u_long *ordering, Face *face, Tile *tile,
                              GridPoint *p0, GridPoint *p1, GridPoint *p2, GridPoint *p3)
{
    poly->x0 = p0->sx; poly->y0 = p0->sy;
    poly->x1 = p1->sx; poly->y1 = p1->sy;
    poly->x2 = p2->sx; poly->y2 = p2->sy;
    poly->x3 = p3->sx; poly->y3 = p3->sy;
    SetQuadTile(poly, tile);
    poly->r0 = face->red;
    poly->g0 = face->green;
    poly->b0 = face->blue;
    GteWriteData(16, p0->depth);
    GteWriteData(17, p1->depth);
    GteWriteData(18, p2->depth);
    GteWriteData(19, p3->depth);
    GteExecute(GTE_AVSZ4);
    AddPrim(ordering + GteReadData(7), poly);
    return poly + 1;
}

/* Medium detail: every face is cut into 2x2 quads. The nine points are the four corners, the midpoints of the
   four edges and the centre; a quad is drawn when all its points are on screen, and all four at once when
   no point of the face is rejected.  `medCount` sections are drawn (the loops run their body once before
   testing the counts). */
void AsmTrackMed(TrackSection *firstSection, Skeleton *camera, short medCount)
{
    TrackSection *section;
    Face         *face;
    POLY_FT4     *poly;
    u_long       *ordering;
    VECTOR       *vertices;
    int           cameraX, cameraY, cameraZ;
    int           faceCount;

    section = firstSection;
    SetSkeletonTransform(camera->super);
    cameraX = camera->relative.t[0];
    cameraY = camera->relative.t[1];
    cameraZ = camera->relative.t[2];
    ordering = OT[CurrentScreen];
    poly = TrackMedPolys[CurrentScreen] + medTileCount;
    vertices = track->vertices;
    do {
        faceCount = (u_short)section->faceCount;
        face = track->faces + (u_short)section->faceStart;
        do {
            GridPoint grid[9];
            VECTOR   *corner[4];
            int       cx[4], cy[4], cz[4];
            int       i;
            int       allRejected = 1, anyRejected = 0;
            Tile     *tile;

            for (i = 0; i < 4; i++) {
                corner[i] = &vertices[face->vertex[i]];
                cx[i] = corner[i]->vx + cameraX;
                cy[i] = corner[i]->vy + cameraY;
                cz[i] = corner[i]->vz + cameraZ;
            }
            /* The corners are grid points 2, 0, 6 and 8 (vertex 1 is in the first corner of the grid). */
            grid[2].x = (short)cx[0]; grid[0].x = (short)cx[1]; grid[6].x = (short)cx[2]; grid[8].x = (short)cx[3];
            grid[2].y = (short)cy[0]; grid[0].y = (short)cy[1]; grid[6].y = (short)cy[2]; grid[8].y = (short)cy[3];
            grid[2].z = (short)cz[0]; grid[0].z = (short)cz[1]; grid[6].z = (short)cz[2]; grid[8].z = (short)cz[3];
            grid[1].x = (short)((cx[0] + cx[1]) >> 1); grid[7].x = (short)((cx[2] + cx[3]) >> 1);
            grid[3].x = (short)((cx[1] + cx[2]) >> 1); grid[5].x = (short)((cx[0] + cx[3]) >> 1);
            grid[4].x = (short)((((cx[1] + cx[2]) >> 1) + ((cx[0] + cx[3]) >> 1)) >> 1);
            grid[1].y = (short)((cy[0] + cy[1]) >> 1); grid[7].y = (short)((cy[2] + cy[3]) >> 1);
            grid[3].y = (short)((cy[1] + cy[2]) >> 1); grid[5].y = (short)((cy[0] + cy[3]) >> 1);
            grid[4].y = (short)((((cy[1] + cy[2]) >> 1) + ((cy[0] + cy[3]) >> 1)) >> 1);
            grid[1].z = (short)((cz[0] + cz[1]) >> 1); grid[7].z = (short)((cz[2] + cz[3]) >> 1);
            grid[3].z = (short)((cz[1] + cz[2]) >> 1); grid[5].z = (short)((cz[0] + cz[3]) >> 1);
            grid[4].z = (short)((((cz[1] + cz[2]) >> 1) + ((cz[0] + cz[3]) >> 1)) >> 1);

            for (i = 0; i < 9; i++) {
                int flag, rejected;

                GteWriteData(0, (grid[i].x & 0xFFFF) | (int)((u_int)(int)grid[i].y << 16));
                GteWriteData(1, grid[i].z);
                GteExecute(GTE_RTPS);
                flag = GteReadControl(31);
                grid[i].sx = (short)GteReadData(14);
                grid[i].sy = (short)(GteReadData(14) >> 16);
                grid[i].depth = (u_short)GteReadData(19);
                grid[i].flag = (u_short)(((u_int)flag >> 12) & 0x466);
                rejected = grid[i].flag != 0;
                allRejected &= rejected;
                anyRejected |= rejected;
            }
            if (!allRejected) {
                tile = &FaceSurface(face)->med[0];
                if (!anyRejected) {
                    poly = DrawGridQuad(poly, ordering, face, tile + 0, &grid[0], &grid[1], &grid[3], &grid[4]);
                    poly = DrawGridQuad(poly, ordering, face, tile + 1, &grid[1], &grid[2], &grid[4], &grid[5]);
                    poly = DrawGridQuad(poly, ordering, face, tile + 2, &grid[3], &grid[4], &grid[6], &grid[7]);
                    poly = DrawGridQuad(poly, ordering, face, tile + 3, &grid[4], &grid[5], &grid[7], &grid[8]);
                    medTileCount += 4;
                } else {
                    static const int quad[4][4] = { { 0, 1, 3, 4 }, { 1, 2, 4, 5 }, { 3, 4, 6, 7 }, { 4, 5, 7, 8 } };

                    for (i = 0; i < 4; i++) {
                        GridPoint *a = &grid[quad[i][0]], *b = &grid[quad[i][1]], *c = &grid[quad[i][2]], *d = &grid[quad[i][3]];

                        if (!(a->flag | b->flag | c->flag | d->flag)) {
                            poly = DrawGridQuad(poly, ordering, face, tile + i, a, b, c, d);
                            medTileCount++;
                        }
                    }
                }
            }
            face++;
        } while (--faceCount > 0);
        section++;
    } while (--medCount > 0);
}
