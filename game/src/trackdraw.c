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
#include <string.h>

#include "functions.h"
#include "track.h"
#include "trackasm.h"
#include "drawasm.h"
#include "portfar.h"
#include "../../platform/psx/gte.h" /* Port: precise projections bound to the packets */

extern short       CurrentScreen;
extern TrackData  *track;
extern Surface     tileSurfaces[][2];
extern short       loTileCount;
extern short       medTileCount;
extern short       hiTileCount;

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

/* Port: frame interpolation (gte.h): a point of the track is known by where it is in the world, so that a point two
   faces share (a corner, a point halfway along an edge) has the one key whichever face projects it and at whichever
   detail, and moves alike in both between frames (each face's own keys moved them apart where one face changed its
   detail: a crack the background showed through). The game's halving of camera-relative coordinates gives a point's
   world position exactly (the camera's offset is whole), so the key is the same from frame to frame too. */
static int gridCamera[3]; /* the camera's offset of the grids being projected */

static uint64_t WorldKey(int x, int y, int z)
{
    return GteKeyMix(GteKeyMix(GteKeyMix(0x5452414B, (uint64_t)(int64_t)x), (uint64_t)(int64_t)y), (uint64_t)(int64_t)z) |
           GTE_KEY_WORLD;
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
    GteKeyNext(reg / 2, WorldKey(vertex->vx, vertex->vy, vertex->vz)); /* Port: frame interpolation */
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
            GteBindPrecise(&poly->x3, GtePreciseHandle(14));
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
            GteBindPrecise(&poly->x1, GtePreciseHandle(12));
            GteBindPrecise(&poly->x0, GtePreciseHandle(13));
            GteBindPrecise(&poly->x2, GtePreciseHandle(14));
            depth = GteReadData(19);
            if ((u_int)GteReadControl(31) & GTE_REJECT) {
                goto next_face;
            }
            SetQuadTile(poly, &FaceSurface(face)->low[0]);
            /* Port: the console read the texture page and palette (one word) in the delay slot of the test for
               a mirrored face, before moving on to the mirrored surface, so they always come from the plain one. */
            poly->tpage = tileSurfaces[(u_char)face->texture][0].low[0].tsb;
            poly->clut = tileSurfaces[(u_char)face->texture][0].low[0].cba;
            poly->r0 = face->red;
            poly->g0 = face->green;
            poly->b0 = face->blue;
            AddPrim((u_long *)((char *)ordering + (depth & 0xFFFC)), poly);
            PortFarMarkFace(face); /* Port: the far view draws the faces the game did not */
            PortTagPrims(poly, poly + 1, "track lo face", NULL, (int)(face - track->faces)); /* Port: the frame recorder */
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
    uint32_t handle; /* Port: the precise projection (gte.h) */
} GridPoint;

/* Projects a grid point through the GTE: fills in its screen position, depth and the flag bits that matter
   (the upper half of the GTE flag word, masked with 0x466).  Returns whether the point is rejected. */
static int ProjectGridPoint(GridPoint *point)
{
    GteWriteData(0, (point->x & 0xFFFF) | (int)((u_int)(int)point->y << 16));
    GteWriteData(1, point->z);
    GteKeyNext(0, WorldKey(point->x - gridCamera[0], point->y - gridCamera[1], point->z - gridCamera[2])); /* Port */
    GteExecute(GTE_RTPS);
    StoreScreenXY(&point->sx, &point->sy, GteReadData(14));
    point->handle = GtePreciseHandle(14);
    point->depth = (u_short)GteReadData(19);
    point->flag = (u_short)(((u_int)GteReadControl(31) >> 12) & 0x466);
    return point->flag != 0;
}

/* Draws the quad of four grid points as one textured quad of the pool and links it by the average of their depths. */
static POLY_FT4 *DrawGridQuad(POLY_FT4 *poly, u_long *ordering, Face *face, Tile *tile,
                              GridPoint *p0, GridPoint *p1, GridPoint *p2, GridPoint *p3)
{
    poly->x0 = p0->sx; poly->y0 = p0->sy;
    poly->x1 = p1->sx; poly->y1 = p1->sy;
    poly->x2 = p2->sx; poly->y2 = p2->sy;
    poly->x3 = p3->sx; poly->y3 = p3->sy;
    GteBindPrecise(&poly->x0, p0->handle);
    GteBindPrecise(&poly->x1, p1->handle);
    GteBindPrecise(&poly->x2, p2->handle);
    GteBindPrecise(&poly->x3, p3->handle);
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
    PortTagPrims(poly, poly + 1, "track face", NULL, (int)(face - track->faces)); /* Port: the frame recorder */
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
    gridCamera[0] = cameraX; /* Port: frame interpolation (WorldKey) */
    gridCamera[1] = cameraY;
    gridCamera[2] = cameraZ;
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
                int rejected = ProjectGridPoint(&grid[i]);

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
                    PortFarMarkFace(face); /* Port: whole; the far view draws faces the game left (partly) out */
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


/* ---- high detail ---- */

/* The 25 points of the 5x5 grid a face is cut into are numbered row by row from the corner
   made by vertex 1; vertex 0 is point 4, vertex 2 is point 20 and vertex 3 is point 24. */
#define HI_POINTS 25

/* Splits the four corner values of one coordinate into the 25 grid values (the halving of the console's
   code: integer averages of the unrounded 32-bit values, not of the stored halfwords). */
static void SubdivideHigh(const int corner[4], int out[HI_POINTS])
{
    int top = (corner[0] + corner[1]) >> 1;
    int bottom = (corner[2] + corner[3]) >> 1;
    int left = (corner[1] + corner[2]) >> 1;
    int right = (corner[0] + corner[3]) >> 1;
    int centre = (left + right) >> 1;

    out[4] = corner[0];
    out[0] = corner[1];
    out[20] = corner[2];
    out[24] = corner[3];
    out[2] = top;
    out[22] = bottom;
    out[10] = left;
    out[14] = right;
    out[12] = centre;
    out[1] = (corner[1] + top) >> 1;
    out[5] = (corner[1] + left) >> 1;
    out[7] = (centre + top) >> 1;
    out[6] = (out[5] + out[7]) >> 1;
    out[9] = (corner[0] + right) >> 1;
    out[3] = (corner[0] + top) >> 1;
    out[13] = (centre + right) >> 1;
    out[8] = (out[3] + out[13]) >> 1;
    out[15] = (corner[2] + left) >> 1;
    out[21] = (corner[2] + bottom) >> 1;
    out[11] = (centre + left) >> 1;
    out[16] = (out[21] + out[11]) >> 1;
    out[23] = (corner[3] + bottom) >> 1;
    out[19] = (corner[3] + right) >> 1;
    out[17] = (centre + bottom) >> 1;
    out[18] = (out[19] + out[17]) >> 1;
}

/* Points of the 2x2 quads of a 3x3 grid, as indices into it. */
static const int gridQuad[4][4] = { { 0, 1, 3, 4 }, { 1, 2, 4, 5 }, { 3, 4, 6, 7 }, { 4, 5, 7, 8 } };

/* A cell of the high detail grid that is partly off screen: it is cut once more into four quads, each drawn
   only if all its points are on screen.  The quads come from the general primitive buffer (nextPrim), not
   from the track's pool, and are not counted in hiTileCount.  The texture coordinates are averaged along. */
static void DrawPartialCell(GridPoint *cell[4], Tile *tile, Face *face, u_long *ordering)
{
    GridPoint sub[9];
    u_short   uv[9];
    int       i;
    int       u[4], v[4];

    u[0] = tile->u0; u[1] = tile->u1; u[2] = tile->u2; u[3] = tile->u3;
    v[0] = tile->v0; v[1] = tile->v1; v[2] = tile->v2; v[3] = tile->v3;

    sub[0] = *cell[0];
    sub[2] = *cell[1];
    sub[6] = *cell[2];
    sub[8] = *cell[3];
    sub[1].x = (cell[0]->x + cell[1]->x) >> 1;  sub[1].y = (cell[0]->y + cell[1]->y) >> 1;  sub[1].z = (cell[0]->z + cell[1]->z) >> 1;
    sub[7].x = (cell[2]->x + cell[3]->x) >> 1;  sub[7].y = (cell[2]->y + cell[3]->y) >> 1;  sub[7].z = (cell[2]->z + cell[3]->z) >> 1;
    sub[3].x = (cell[0]->x + cell[2]->x) >> 1;  sub[3].y = (cell[0]->y + cell[2]->y) >> 1;  sub[3].z = (cell[0]->z + cell[2]->z) >> 1;
    sub[5].x = (cell[1]->x + cell[3]->x) >> 1;  sub[5].y = (cell[1]->y + cell[3]->y) >> 1;  sub[5].z = (cell[1]->z + cell[3]->z) >> 1;
    sub[4].x = (sub[3].x + sub[5].x) >> 1;      sub[4].y = (sub[3].y + sub[5].y) >> 1;      sub[4].z = (sub[3].z + sub[5].z) >> 1;
    ProjectGridPoint(&sub[1]);
    ProjectGridPoint(&sub[3]);
    ProjectGridPoint(&sub[4]);
    ProjectGridPoint(&sub[5]);
    ProjectGridPoint(&sub[7]);

    uv[0] = (u_short)(u[0] | v[0] << 8);
    uv[2] = (u_short)(u[1] | v[1] << 8);
    uv[6] = (u_short)(u[2] | v[2] << 8);
    uv[8] = (u_short)(u[3] | v[3] << 8);
    uv[1] = (u_short)(((u[0] + u[1]) >> 1) | ((v[0] + v[1]) >> 1) << 8);
    uv[7] = (u_short)(((u[2] + u[3]) >> 1) | ((v[2] + v[3]) >> 1) << 8);
    uv[3] = (u_short)(((u[0] + u[2]) >> 1) | ((v[0] + v[2]) >> 1) << 8);
    uv[5] = (u_short)(((u[1] + u[3]) >> 1) | ((v[1] + v[3]) >> 1) << 8);
    uv[4] = (u_short)((((u[0] + u[2]) >> 1) + ((u[1] + u[3]) >> 1)) >> 1 | ((((v[0] + v[2]) >> 1) + ((v[1] + v[3]) >> 1)) >> 1) << 8);

    for (i = 0; i < 4; i++) {
        GridPoint *a = &sub[gridQuad[i][0]], *b = &sub[gridQuad[i][1]], *c = &sub[gridQuad[i][2]], *d = &sub[gridQuad[i][3]];
        POLY_FT4  *poly;

        if (a->flag | b->flag | c->flag | d->flag) {
            continue;
        }
        poly = (POLY_FT4 *)nextPrim.ptr;
        SetPolyFT4(poly);
        poly->x0 = a->sx; poly->y0 = a->sy;
        poly->x1 = b->sx; poly->y1 = b->sy;
        poly->x2 = c->sx; poly->y2 = c->sy;
        poly->x3 = d->sx; poly->y3 = d->sy;
        GteBindPrecise(&poly->x0, a->handle);
        GteBindPrecise(&poly->x1, b->handle);
        GteBindPrecise(&poly->x2, c->handle);
        GteBindPrecise(&poly->x3, d->handle);
        poly->u0 = (u_char)uv[gridQuad[i][0]]; poly->v0 = (u_char)(uv[gridQuad[i][0]] >> 8);
        poly->u1 = (u_char)uv[gridQuad[i][1]]; poly->v1 = (u_char)(uv[gridQuad[i][1]] >> 8);
        poly->u2 = (u_char)uv[gridQuad[i][2]]; poly->v2 = (u_char)(uv[gridQuad[i][2]] >> 8);
        poly->u3 = (u_char)uv[gridQuad[i][3]]; poly->v3 = (u_char)(uv[gridQuad[i][3]] >> 8);
        poly->tpage = tile->tsb;
        poly->clut = tile->cba;
        poly->r0 = face->red;
        poly->g0 = face->green;
        poly->b0 = face->blue;
        GteWriteData(16, a->depth);
        GteWriteData(17, b->depth);
        GteWriteData(18, c->depth);
        GteWriteData(19, d->depth);
        GteExecute(GTE_AVSZ4);
        AddPrim(ordering + GteReadData(7), poly);
        nextPrim.ptr = (char *)(poly + 1);
        PortTagPrims(poly, poly + 1, "track cell", NULL, (int)(face - track->faces)); /* Port: the frame recorder */
    }
}

/* High detail: every face is cut into 4x4 quads (a 5x5 grid of points).  A quad is drawn when its four points
   are on screen; where only some are, the quad is cut in four again (DrawPartialCell) unless all four are
   rejected.  A face with every point rejected is skipped.  `hiCount` sections are drawn.  `autoMesh` is
   passed by the callers but not used. */
void AsmTrackHi(TrackSection *firstSection, Skeleton *camera, AutoMeshData *autoMesh, short hiCount)
{
    TrackSection *section;
    Face         *face;
    POLY_FT4     *poly;
    u_long       *ordering;
    VECTOR       *vertices;
    int           cameraX, cameraY, cameraZ;
    int           faceCount;
    int           count;

    section = firstSection;
    SetSkeletonTransform(camera->super);
    cameraX = camera->relative.t[0];
    cameraY = camera->relative.t[1];
    cameraZ = camera->relative.t[2];
    gridCamera[0] = cameraX; /* Port: frame interpolation (WorldKey) */
    gridCamera[1] = cameraY;
    gridCamera[2] = cameraZ;
    ordering = OT[CurrentScreen];
    count = hiTileCount;
    poly = TrackHiPolys[CurrentScreen] + count;
    vertices = track->vertices;
    do {
        faceCount = (u_short)section->faceCount;
        face = track->faces + (u_short)section->faceStart;
        do {
            GridPoint grid[HI_POINTS];
            int       x[4], y[4], z[4];
            int       gx[HI_POINTS], gy[HI_POINTS], gz[HI_POINTS];
            int       i, row, col;
            int       allRejected = 1, anyRejected = 0;
            Tile     *tile;

            for (i = 0; i < 4; i++) {
                VECTOR *corner = &vertices[face->vertex[i]];

                x[i] = corner->vx + cameraX;
                y[i] = corner->vy + cameraY;
                z[i] = corner->vz + cameraZ;
            }
            SubdivideHigh(x, gx);
            SubdivideHigh(y, gy);
            SubdivideHigh(z, gz);
            for (i = 0; i < HI_POINTS; i++) {
                int rejected;

                grid[i].x = (short)gx[i];
                grid[i].y = (short)gy[i];
                grid[i].z = (short)gz[i];
                rejected = ProjectGridPoint(&grid[i]);
                allRejected &= rejected;
                anyRejected |= rejected;
            }
            if (!anyRejected) {
                PortFarMarkFace(face); /* Port: whole; the far view draws faces the game left (partly) out */
            }
            if (!allRejected) {
                tile = &FaceSurface(face)->high[0];
                for (row = 0; row < 4; row++) {
                    for (col = 0; col < 4; col++) {
                        GridPoint *cell[4];

                        cell[0] = &grid[5 * row + col];
                        cell[1] = &grid[5 * row + col + 1];
                        cell[2] = &grid[5 * row + col + 5];
                        cell[3] = &grid[5 * row + col + 6];
                        if (!anyRejected || !(cell[0]->flag | cell[1]->flag | cell[2]->flag | cell[3]->flag)) {
                            poly = DrawGridQuad(poly, ordering, face, tile, cell[0], cell[1], cell[2], cell[3]);
                            count++;
                        } else if (!(cell[0]->flag && cell[1]->flag && cell[2]->flag && cell[3]->flag)) {
                            DrawPartialCell(cell, tile, face, ordering);
                        }
                        tile++;
                    }
                }
            }
            face++;
        } while (--faceCount > 0);
        section++;
    } while (--hiCount > 0);
    hiTileCount = (short)count;
}


/* Port: a face with a corner too near the camera or beyond the GPU's coordinates. The game leaves out each
   cell of it with such corners (AsmTrackMed, AsmTrackHi), so a face right beside the camera left a hole in the
   track in front of the ship for a frame or two. It is drawn here whole, its corners' view-space positions given
   (PortFarProjectAny): libgpu cuts it just in front of the camera in each picture. Faces wholly behind the camera
   are drawn too, as far as the pictures between two frames can see them (PORT_FAR_BEHIND). */
static void DrawNearFace(Face *face, const PortView *view, u_short tpage, u_short clut)
{
    PortFarVertex corner[4];
    POLY_FT4     *poly;
    double        deepest = 0.0;
    int           inFront = 0, reach = 0, left = 0, right = 0, above = 0, below = 0;
    int           margin = PortScreenMargin() + PORT_FAR_AROUND; /* widescreen's sides, and around the screen */
    int           k;

    for (k = 0; k < 4; k++) {
        VECTOR *vertex = &track->vertices[face->vertex[k]];
        double  c[3];
        int     r;

        for (r = 0; r < 3; r++) {
            c[r] = view->m[r][0] * vertex->vx + view->m[r][1] * vertex->vy + view->m[r][2] * vertex->vz + view->t[r];
        }
        GteKeyNext(0, WorldKey(vertex->vx, vertex->vy, vertex->vz)); /* frame interpolation: as the game's (WorldKey) */
        PortFarProjectAny(c[0], c[1], c[2], &corner[k]);
        inFront += c[2] >= PORT_FAR_NEAR;
        reach += c[2] >= -PORT_FAR_BEHIND;
        left += corner[k].x < -margin;
        right += corner[k].x > 320 + margin;
        above += corner[k].y < -PORT_FAR_AROUND;
        below += corner[k].y > SCREEN_HEIGHT + PORT_FAR_AROUND;
        deepest = c[2] > deepest ? c[2] : deepest;
    }
    if (!reach || (inFront == 4 && (left == 4 || right == 4 || above == 4 || below == 4))) {
        return; /* too far behind, or wholly off the screen */
    }
    poly = (POLY_FT4 *)PortFarAlloc(sizeof(POLY_FT4));
    if (!poly) {
        return;
    }
    SetPolyFT4(poly);
    /* the corners as AsmTrackLo stores them: RTPT's vertices 0, 1, 2 go to x1, x0, x2 */
    StoreScreenXY(&poly->x1, &poly->y1, corner[0].screen);
    StoreScreenXY(&poly->x0, &poly->y0, corner[1].screen);
    StoreScreenXY(&poly->x2, &poly->y2, corner[2].screen);
    StoreScreenXY(&poly->x3, &poly->y3, corner[3].screen);
    GteBindPrecise(&poly->x1, corner[0].handle);
    GteBindPrecise(&poly->x0, corner[1].handle);
    GteBindPrecise(&poly->x2, corner[2].handle);
    GteBindPrecise(&poly->x3, corner[3].handle);
    SetQuadTile(poly, &FaceSurface(face)->low[0]);
    poly->tpage = tpage;
    poly->clut = clut;
    poly->r0 = face->red;
    poly->g0 = face->green;
    poly->b0 = face->blue;
    /* behind whatever the game drew of the face, and nearer things */
    AddPrim(PortFarSlot((int)deepest >> 2, deepest), poly);
    PortTagPrims(poly, poly + 1, "near fill face", NULL, (int)(face - track->faces));
}

/* Port: the faces of a section the game did not draw (beyond its view lists, or dropped: a corner farther
   than 16 bits or deeper than the GTE's depth range), in the low detail of AsmTrackLo, projected in full precision
   (portfar.h). Faces with a corner too near, behind the camera or beyond the GPU's coordinates are drawn by
   DrawNearFace; faces wholly off screen are left out. */
void PortTrackFar(TrackSection *section, const PortView *view)
{
    Face *face = track->faces + (u_short)section->faceStart;
    int   faceCount = (u_short)section->faceCount;
    int   margin = PortScreenMargin() + PORT_FAR_AROUND; /* widescreen's sides, and around the screen */
    int   n, k;

    /* a face the game drew in part is drawn whole here, a hair deeper, so that the game's cells cover it */
    PortFarDepthBias(1.002);

    for (n = 0; n < faceCount; n++, face++) {
        PortFarVertex corner[4];
        POLY_FT4     *poly;
        int           left = 0, right = 0, above = 0, below = 0;

        if (PortFarFaceDrawn(face)) {
            continue; /* the game drew it */
        }
        for (k = 0; k < 4; k++) {
            VECTOR *vertex = &track->vertices[face->vertex[k]];

            GteKeyNext(0, WorldKey(vertex->vx, vertex->vy, vertex->vz)); /* frame interpolation: as the game's (WorldKey) */
            PortFarProject(view, vertex->vx, vertex->vy, vertex->vz, &corner[k]);
            if (corner[k].rejected) {
                break;
            }
            left += corner[k].x < -margin;
            right += corner[k].x > 320 + margin;
            above += corner[k].y < -PORT_FAR_AROUND;
            below += corner[k].y > SCREEN_HEIGHT + PORT_FAR_AROUND;
        }
        if (k < 4) {
            DrawNearFace(face, view, tileSurfaces[(u_char)face->texture][0].low[0].tsb,
                         tileSurfaces[(u_char)face->texture][0].low[0].cba);
            continue;
        }
        if (left == 4 || right == 4 || above == 4 || below == 4) {
            continue;
        }
        poly = (POLY_FT4 *)PortFarAlloc(sizeof(POLY_FT4));
        if (!poly) {
            break;
        }
        SetPolyFT4(poly);
        /* the corners as AsmTrackLo stores them: RTPT's vertices 0, 1, 2 go to x1, x0, x2 */
        StoreScreenXY(&poly->x1, &poly->y1, corner[0].screen);
        StoreScreenXY(&poly->x0, &poly->y0, corner[1].screen);
        StoreScreenXY(&poly->x2, &poly->y2, corner[2].screen);
        StoreScreenXY(&poly->x3, &poly->y3, corner[3].screen);
        GteBindPrecise(&poly->x1, corner[0].handle);
        GteBindPrecise(&poly->x0, corner[1].handle);
        GteBindPrecise(&poly->x2, corner[2].handle);
        GteBindPrecise(&poly->x3, corner[3].handle);
        SetQuadTile(poly, &FaceSurface(face)->low[0]);
        poly->tpage = tileSurfaces[(u_char)face->texture][0].low[0].tsb;
        poly->clut = tileSurfaces[(u_char)face->texture][0].low[0].cba;
        poly->r0 = face->red;
        poly->g0 = face->green;
        poly->b0 = face->blue;
        /* AsmTrackLo's slot: the depth of the last corner of RTPT (vertex 2), a quarter per entry */
        AddPrim(PortFarSlot((int)corner[2].z >> 2, corner[2].z), poly);
        PortTagPrims(poly, poly + 1, "far track face", NULL, (int)(face - track->faces));
    }
    PortFarDepthBias(1.0);
}
