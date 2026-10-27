#include "functions.h"
#include "../../platform/psx/gte.h" /* Port: precise projections */

/* Port: scratchpad storage (see port_platform.h) */
unsigned char PsxScratchpad[1024] __attribute__((aligned(8)));
#include "include_asm_g8.h"

extern MATRIX cameraMatrix;
extern VECTOR cameraOffset;

void SetCameraPosition(int x, int y, int z)
{
    cameraOffset.vx = 0;
    cameraOffset.vy = 0;
    cameraOffset.vz = 0;
    cameraMatrix.t[0] = x;
    cameraMatrix.t[1] = y;
    cameraMatrix.t[2] = z;
    SetTransMatrix(&cameraMatrix);
}


void SetCameraDirection(short x, short y, short z)
{
    short sx;
    short sy;
    short sz;
    short cx;
    short cy;
    short cz;

    sx = rsin(x);
    sy = rsin(y);
    sz = rsin(z);
    cx = rcos(x);
    cy = rcos(y);
    cz = rcos(z);

    cameraMatrix.m[0][0] = (cy * cz) >> 12;
    cameraMatrix.m[0][1] = ((-cy) * sz) >> 12;
    cameraMatrix.m[0][2] = -sy;

    cameraMatrix.m[1][0] = ((cx * sz) >> 12) - ((((sx * sy) >> 12) * cz) >> 12);
    cameraMatrix.m[1][1] = ((cx * cz) >> 12) + ((((sx * sy) >> 12) * sz) >> 12);
    cameraMatrix.m[1][2] = ((-sx) * cy) >> 12;

    cameraMatrix.m[2][0] = ((sx * sz) >> 12) + ((((cx * sy) >> 12) * cz) >> 12);
    cameraMatrix.m[2][1] = ((sx * cz) >> 12) - ((((cx * sy) >> 12) * sz) >> 12);
    cameraMatrix.m[2][2] = (cx * cy) >> 12;

    SetRotMatrix(&cameraMatrix);
}


void MatrixMultiply(MATRIX *m1, MATRIX *m2, MATRIX *m3)
{
    m3->t[0] = m2->t[0] + ((m1->t[0] * m2->m[0][0] + m1->t[1] * m2->m[0][1] + m1->t[2] * m2->m[0][2]) >> 12);
    m3->t[1] = m2->t[1] + ((m1->t[0] * m2->m[1][0] + m1->t[1] * m2->m[1][1] + m1->t[2] * m2->m[1][2]) >> 12);
    m3->t[2] = m2->t[2] + ((m1->t[0] * m2->m[2][0] + m1->t[1] * m2->m[2][1] + m1->t[2] * m2->m[2][2]) >> 12);
    MulMatrix0(m1, m2, m3);
}


void InitialiseAmbientColour(void)
{
    SetBackColor(0x00, 0x00, 0xff);
}


void InitialiseColourMatrix(void)
{
    MATRIX m;

    m.m[0][0] = 0x1000;
    m.m[1][0] = 0;
    m.m[2][0] = 0;
    m.m[0][1] = 0;
    m.m[1][1] = 0x1000;
    m.m[2][1] = 0;
    m.m[0][2] = 0;
    m.m[1][2] = 0;
    m.m[2][2] = 0x1000;
    SetColorMatrix(&m);
}


void InitMatrix(MATRIX *m)
{
    m->m[0][0] = 0x1000;
    m->m[1][0] = 0;
    m->m[2][0] = 0;
    m->m[0][1] = 0;
    m->m[1][1] = 0x1000;
    m->m[2][1] = 0;
    m->m[0][2] = 0;
    m->m[1][2] = 0;
    m->m[2][2] = 0x1000;
    m->t[0] = 0;
    m->t[1] = 0;
    m->t[2] = 0;
}


void InitialiseLightMatrix(void)
{
    MATRIX lm;

    lm.m[0][0] = 4096;
    lm.m[0][1] = 0;
    lm.m[0][2] = 0;
    lm.m[1][0] = 0;
    lm.m[1][1] = 4096;
    lm.m[1][2] = 0;
    lm.m[2][0] = 0;
    lm.m[2][1] = 0;
    lm.m[2][2] = 4096;
    SetLightMatrix(&lm);
}


void SetupLights(Object *lights)
{
    Poly        prm;
    short       ambient = 0;
    short       diffuse = 0;
    short       i;
    char        scratch[256]; /* unused: keeps the stack frame size */
    CVECTOR     colour;
    SVECTOR     direction;
    MATRIX      cm;
    MATRIX      lm;

    prm.prim = lights->primitives;

    for (i = 0; i < lights->primitiveCount; i++) {
        switch (prm.prim->type) {
        case TypePointLight:
            if (!ambient) {
                colour = prm.pointLight->colour;
                colour.r = colour.r >> 1;
                colour.g = colour.g >> 1;
                colour.b = colour.b >> 1;
                SetBackColor(colour.r, colour.g, colour.b);
            }
            ambient += 1;
            prm.pointLight += 1;
            break;
        case TypeInfiniteLight:
            colour = prm.infiniteLight->colour;
            direction = prm.infiniteLight->direction;
            if (direction.vx > 4098) {
                direction.vx = 4098;
            }
            if (direction.vx < -4098) {
                direction.vx = -4098;
            }
            if (direction.vy > 4098) {
                direction.vy = 4098;
            }
            if (direction.vy < -4098) {
                direction.vy = -4098;
            }
            if (direction.vz > 4098) {
                direction.vz = 4098;
            }
            if (direction.vz < -4098) {
                direction.vz = -4098;
            }
            if (diffuse == 0) {
                lm.m[0][0] = direction.vx;
                lm.m[0][1] = direction.vy;
                lm.m[0][2] = direction.vz;
                cm.m[0][0] = colour.r << 4;
                cm.m[1][0] = colour.g << 4;
                cm.m[2][0] = colour.b << 4;
            } else if (diffuse == 1) {
                lm.m[1][0] = direction.vx;
                lm.m[1][1] = direction.vy;
                lm.m[1][2] = direction.vz;
                cm.m[0][1] = colour.r << 4;
                cm.m[1][1] = colour.g << 4;
                cm.m[2][1] = colour.b << 4;
            } else if (diffuse == 2) {
                lm.m[2][0] = direction.vx;
                lm.m[2][1] = direction.vy;
                lm.m[2][2] = direction.vz;
                cm.m[0][2] = colour.r << 4;
                cm.m[1][2] = colour.g << 4;
                cm.m[2][2] = colour.b << 4;
            }
            diffuse += 1;
            prm.infiniteLight += 1;
            break;
        default:
            printf("WARNING: Bad primitive $%x in lights\n", prm.prim->type);
            break;
        }
    }
    if (diffuse >= 3) {
        SetColorMatrix(&cm);
        SetLightMatrix(&lm);
    }
}


void Normalise(VECTOR *vector)
{
    s32 normalizedX;
    s32 zSquared;
    s32 scaledX;
    s32 scaledY;
    s32 scaledZ;
    s32 normalizedZ;
    s32 normalizedY;
    s32 magnitude;

    scaledX = vector->vx >> 6;
    scaledY = vector->vy >> 6;
    scaledZ = vector->vz >> 6;
    zSquared = scaledZ * scaledZ;
    magnitude = SquareRoot0((scaledX * scaledX) + (scaledY * scaledY) + zSquared);
    if (magnitude == 0) {
        magnitude = 1;
    }
    normalizedX = vector->vx << 6;
    normalizedY = vector->vy << 6;
    normalizedZ = vector->vz << 6;
    vector->vx = normalizedX / magnitude;
    vector->vy = normalizedY / magnitude;
    vector->vz = normalizedZ / magnitude;
}


int DotProduct(VECTOR *a, VECTOR *b)
{
    return (((a->vx >> 6) * (b->vx >> 6)) + ((a->vy >> 6) * (b->vy >> 6)) + ((a->vz >> 6) * (b->vz >> 6)));
}


void MyCrossProduct(VECTOR *a, VECTOR *b)
{
    OuterProduct12(a, b, a);
}


void PreCalcNormals(Object *object, SVECTOR *normals)
{
    VECTOR   edge1, edge2;
    VECTOR   faceNormals[256];
    Poly     prm;
    SVECTOR *vertices;
    short    primitiveCount, vertexCount;
    int      face, vertex, count;

    prm.prim = object->primitives;
    vertices = object->vertices;
    primitiveCount = object->primitiveCount;
    vertexCount = object->vertexCount;
    for (face = 0; face < primitiveCount; face++) {
        edge1.vx = (vertices[prm.f3->coords[1]].vx - vertices[prm.f3->coords[0]].vx) << 12;
        edge1.vy = (vertices[prm.f3->coords[1]].vy - vertices[prm.f3->coords[0]].vy) << 12;
        edge1.vz = (vertices[prm.f3->coords[1]].vz - vertices[prm.f3->coords[0]].vz) << 12;
        edge2.vx = (vertices[prm.f3->coords[2]].vx - vertices[prm.f3->coords[0]].vx) << 12;
        edge2.vy = (vertices[prm.f3->coords[2]].vy - vertices[prm.f3->coords[0]].vy) << 12;
        edge2.vz = (vertices[prm.f3->coords[2]].vz - vertices[prm.f3->coords[0]].vz) << 12;
        Normalise(&edge1);
        Normalise(&edge2);
        MyCrossProduct(&edge1, &edge2);
        Normalise(&edge1);
        faceNormals[face].vx = edge1.vx;
        faceNormals[face].vy = edge1.vy;
        faceNormals[face].vz = edge1.vz;
        switch (prm.f3->type) {
        case TypeG3: prm.g3++; break;
        case TypeG4: prm.g4++; break;
        case TypeF3:
        case TypeF4: prm.f3++; break;
        }
    }
    for (vertex = 0; vertex < vertexCount; vertex++) {
        edge1.vx = 0;
        edge1.vy = 0;
        edge1.vz = 0;
        prm.prim = object->primitives;
        count = 0;
        for (face = 0; face < primitiveCount; face++) {
            switch (prm.f3->type) {
            case TypeF3:
                if (prm.f3->coords[0] == vertex || prm.f3->coords[1] == vertex || prm.f3->coords[2] == vertex) {
                    edge1.vx += faceNormals[face].vx;
                    edge1.vy += faceNormals[face].vy;
                    edge1.vz += faceNormals[face].vz;
                    count++;
                }
                prm.f3++;
                break;
            case TypeF4:
                if (prm.f4->coords[0] == vertex || prm.f4->coords[1] == vertex || prm.f4->coords[2] == vertex || prm.f4->coords[3] == vertex) {
                    edge1.vx += faceNormals[face].vx;
                    edge1.vy += faceNormals[face].vy;
                    edge1.vz += faceNormals[face].vz;
                    count++;
                }
                prm.f4 = (F4 *)(prm.f3 + 1);
                break;
            }
        }
        if (count > 0) {
            edge1.vx /= count;
            edge1.vy /= count;
            edge1.vz /= count;
            Normalise(&edge1);
            normals[vertex].vx = edge1.vx;
            normals[vertex].vy = edge1.vy;
            normals[vertex].vz = edge1.vz;
        }
    }
}


extern int primsDrawn;
extern DVECTOR StaticScreen[];
extern unsigned short StaticDepth[];
extern unsigned short StaticFlags[];
extern VECTOR StaticRVert[];
extern unsigned char envMapU[];
extern unsigned char envMapV[];
extern PrimPtr nextPrim;
extern int envmapr;
extern int envmapg;
extern int envmapb;
extern short CurrentScreen;
extern unsigned char medalDisplay;

/* Port: NormalClip takes each screen point packed in one word (x in the low half, y in the high half),
   as the console passed a DVECTOR by value. */
static int PackScreenPoint(DVECTOR point)
{
    return (int)((u_short)point.vx | ((u32)(u_short)point.vy << 16));
}

/* Port: a packet vertex the game scaled about the screen from a projected one (DrawEnvMap): bound to the
   projection's precise position scaled the same way, so the scaled model does not snap to whole pixels. Known by the
   projection's key, at a view-space position that projects to the scaled one, so that the pictures between two frames
   turn the model smoothly (a menu's spinning ship stepped at the game's rate); as what moves (GTE_KEY_MOVING): a
   vertex of a face just turned into view, not drawn in the frame before, stays where the frame has it (moved as if it
   stood still in the world, it came from far off the model). */
static void BindScaled(short *x, const DVECTOR *source, int scale, int xOffset, int yOffset)
{
    GtePrecise precise;
    uint32_t handle = GteBoundHandle(source, (u_short)source->vx | (uint32_t)(u_short)source->vy << 16);

    if (handle && GtePreciseOf(handle, &precise)) {
        uint32_t key = (u_short)x[0] | (uint32_t)(u_short)x[1] << 16;
        float sx = precise.x * scale / 256.0f + xOffset, sy = precise.y * scale / 256.0f + yOffset;
        double depth = precise.view[2], h = (double)(u_short)GteReadControl(26);

        if (precise.key && depth > 0.0) {
            GteKeyNext(0, GteKeyMix(precise.key, 0x5343414C4544ull) | GTE_KEY_MOVING);
        }
        handle = GteRecordProjection(key, sx, sy, precise.z, precise.fade);
        if (precise.key && depth > 0.0) {
            GteRecordView(handle, (sx - GteReadControl(24) / 65536.0) * depth / h,
                          (sy - GteReadControl(25) / 65536.0) * depth / h, depth);
        }
        GteBindPrecise(x, handle);
    }
}

/* The clip flags of a vertex that keep its face out of the environment map. */
#define ENV_CLIP_FLAGS 0x460

void DrawEnvMap(Object *object, void *unused, SVECTOR *normals, int xOffset, int yOffset, int scale)
{
    VECTOR    reflect, eye;
    MATRIX    matrix;
    VECTOR    translation;
    int       transformFlag;
    u_short  *depth, *clip;
    short     i;
    short     vertex0, vertex1, vertex2, vertex3;
    short     primitiveCount;
    short     vertexCount;
    DVECTOR  *screen;
    /* A face's U at each vertex as the reflection gives it (baseU), and as drawn: where an edge crosses the
       texture's seam (U wraps from 63 to 0), the vertex on the low side moves one texture width (64) up. */
    int       reflectionValue, v, z, u0, u1, u2, lastU, baseU0, baseU1, baseU2, baseU3;
    Poly      currentPrimitive;
    SVECTOR  *vertices;
    char     *start = nextPrim.ptr; /* Port: the frame recorder */

    eye.vx = 0;
    eye.vy = 0;
    eye.vz = 4096;
    SetSkeletonTransform(object->skeleton);
    primitiveCount = object->primitiveCount;
    vertexCount = object->vertexCount;
    vertices = object->vertices;
    primsDrawn += primitiveCount;
    if (vertexCount <= 128) {
        /* Port: the console's scratchpad RAM at 0x1F800000 */
        screen = (DVECTOR *)PsxScratchpad;
        depth = (u_short *)(PsxScratchpad + 0x200);
        clip = (u_short *)(PsxScratchpad + 0x300);
    } else {
        screen = StaticScreen;
        depth = StaticDepth;
        clip = StaticFlags;
    }
    RotTransPersN(vertices, screen, depth, clip, vertexCount);
    translation.vx = 0;
    translation.vy = 0;
    translation.vz = 0;
    TransMatrix(&matrix, &translation);
    SetTransMatrix(&matrix);
    for (i = 0; i < vertexCount; i++) {
        RotTrans(&normals[i], &StaticRVert[i], &transformFlag);
    }
    /* Each vertex's texture coordinates: U from the reflected direction's heading, V from its height. */
    for (i = 0; i < vertexCount; i++) {
        reflect.vx = StaticRVert[i].vx;
        reflect.vy = StaticRVert[i].vy;
        reflect.vz = StaticRVert[i].vz;
        reflectionValue = DotProduct(&reflect, &eye);
        reflect.vx = ((s32)((u32)reflect.vx * (u32)reflectionValue) >> 11) - eye.vx;
        reflect.vz = ((s32)((u32)reflect.vz * (u32)reflectionValue) >> 11) - eye.vz;
        reflect.vy = ((s32)((u32)reflect.vy * (u32)reflectionValue) >> 11) - eye.vy;
        envMapU[i] = (ratan2(reflect.vx, reflect.vz) >> 6) & 63;
        v = 31 - ((s32)((u32)reflect.vy << 6) >> 13);
        if (v < 0) {
            v = 0;
        }
        if (v >= 64) {
            v = 63;
        }
        envMapV[i] = v;
    }
    /* Each edge's midpoint normal tells whether the edge crosses the seam: its U then lies outside the
       range between the two ends' U. */
    currentPrimitive.prim = object->primitives;
    for (i = 0; i < primitiveCount; i++) {
        switch (currentPrimitive.prim->type) {
        case TypeF3:
            vertex0 = currentPrimitive.f3->coords[0];
            vertex1 = currentPrimitive.f3->coords[1];
            vertex2 = currentPrimitive.f3->coords[2];
            if (!((clip[vertex0] | clip[vertex1] | clip[vertex2]) & ENV_CLIP_FLAGS)
                && NormalClip(PackScreenPoint(screen[vertex0]), PackScreenPoint(screen[vertex1]),
                              PackScreenPoint(screen[vertex2])) > 0) {
                POLY_FT3 *packet = (POLY_FT3 *)nextPrim.ptr;

                SetPolyFT3(packet);
                packet->tpage = GetTPage(1, 0, 640, 256);
                baseU0 = envMapU[vertex0];
                baseU1 = envMapU[vertex1];
                baseU2 = envMapU[vertex2];
                u0 = baseU0;
                u1 = baseU1;
                u2 = baseU2;
                reflect.vx = (StaticRVert[vertex0].vx + StaticRVert[vertex1].vx) >> 1;
                reflect.vy = (StaticRVert[vertex0].vy + StaticRVert[vertex1].vy) >> 1;
                reflect.vz = (StaticRVert[vertex0].vz + StaticRVert[vertex1].vz) >> 1;
                Normalise(&reflect);
                reflectionValue = DotProduct(&reflect, &eye);
                reflect.vx = ((s32)((u32)reflect.vx * (u32)reflectionValue) >> 11) - eye.vx;
                reflect.vz = ((s32)((u32)reflect.vz * (u32)reflectionValue) >> 11) - eye.vz;
                reflectionValue = (ratan2(reflect.vx, reflect.vz) >> 6) & 63;
                if (baseU1 < baseU0) {
                    if (reflectionValue < baseU1 || baseU0 < reflectionValue) {
                        u1 = baseU1 + 64;
                    }
                } else {
                    if (reflectionValue < baseU0 || baseU1 < reflectionValue) {
                        u0 = baseU0 + 64;
                    }
                }
                reflect.vx = (StaticRVert[vertex1].vx + StaticRVert[vertex2].vx) >> 1;
                reflect.vy = (StaticRVert[vertex1].vy + StaticRVert[vertex2].vy) >> 1;
                reflect.vz = (StaticRVert[vertex1].vz + StaticRVert[vertex2].vz) >> 1;
                Normalise(&reflect);
                reflectionValue = DotProduct(&reflect, &eye);
                reflect.vx = ((s32)((u32)reflect.vx * (u32)reflectionValue) >> 11) - eye.vx;
                reflect.vz = ((s32)((u32)reflect.vz * (u32)reflectionValue) >> 11) - eye.vz;
                reflectionValue = (ratan2(reflect.vx, reflect.vz) >> 6) & 63;
                if (baseU2 < baseU1) {
                    if (reflectionValue < baseU2 || baseU1 < reflectionValue) {
                        u2 = baseU2 + 64;
                    }
                } else {
                    if (reflectionValue < baseU1 || baseU2 < reflectionValue) {
                        u1 = baseU1 + 64;
                    }
                }
                reflect.vx = (StaticRVert[vertex0].vx + StaticRVert[vertex2].vx) >> 1;
                reflect.vy = (StaticRVert[vertex0].vy + StaticRVert[vertex2].vy) >> 1;
                reflect.vz = (StaticRVert[vertex0].vz + StaticRVert[vertex2].vz) >> 1;
                Normalise(&reflect);
                reflectionValue = DotProduct(&reflect, &eye);
                reflect.vx = ((s32)((u32)reflect.vx * (u32)reflectionValue) >> 11) - eye.vx;
                reflect.vz = ((s32)((u32)reflect.vz * (u32)reflectionValue) >> 11) - eye.vz;
                reflectionValue = (ratan2(reflect.vx, reflect.vz) >> 6) & 63;
                if (baseU2 < baseU0) {
                    if (reflectionValue < baseU2 || baseU0 < reflectionValue) {
                        u2 = baseU2 + 64;
                    }
                } else {
                    if (reflectionValue < baseU0 || baseU2 < reflectionValue) {
                        u0 = baseU0 + 64;
                    }
                }
                packet->u0 = u0;
                packet->u1 = u1;
                packet->u2 = u2;
                packet->v0 = envMapV[vertex0];
                packet->v1 = envMapV[vertex1];
                packet->v2 = envMapV[vertex2];
                packet->clut = GetClut(704, 258);
                packet->r0 = envmapr;
                packet->g0 = envmapg;
                packet->b0 = envmapb;
                *(DVECTOR *)&packet->x0 = screen[vertex0];
                *(DVECTOR *)&packet->x1 = screen[vertex1];
                *(DVECTOR *)&packet->x2 = screen[vertex2];
                packet->x0 = ((packet->x0 * scale) >> 8) + xOffset;
                packet->x1 = ((packet->x1 * scale) >> 8) + xOffset;
                packet->x2 = ((packet->x2 * scale) >> 8) + xOffset;
                packet->y0 = ((packet->y0 * scale) >> 8) + yOffset;
                packet->y1 = ((packet->y1 * scale) >> 8) + yOffset;
                packet->y2 = ((packet->y2 * scale) >> 8) + yOffset;
                BindScaled(&packet->x0, &screen[vertex0], scale, xOffset, yOffset);
                BindScaled(&packet->x1, &screen[vertex1], scale, xOffset, yOffset);
                BindScaled(&packet->x2, &screen[vertex2], scale, xOffset, yOffset);
                z = AverageZ3(depth[vertex0], depth[vertex1], depth[vertex2]);
                z %= 1600;
                AddPrim(OT2[CurrentScreen] + z, nextPrim.ptr);
                nextPrim.ptr += sizeof(POLY_FT3);
            }
            currentPrimitive.f3++;
            break;

        case TypeF4:
            vertex0 = currentPrimitive.f4->coords[0];
            vertex1 = currentPrimitive.f4->coords[1];
            vertex2 = currentPrimitive.f4->coords[2];
            vertex3 = currentPrimitive.f4->coords[3];
            if (!((clip[vertex0] | clip[vertex1] | clip[vertex2] | clip[vertex3]) & ENV_CLIP_FLAGS)
                && NormalClip(PackScreenPoint(screen[vertex0]), PackScreenPoint(screen[vertex1]),
                              PackScreenPoint(screen[vertex2])) > 0) {
                POLY_FT4 *packet = (POLY_FT4 *)nextPrim.ptr;

                SetPolyFT4(packet);
                packet->tpage = GetTPage(1, 0, 640, 256);
                baseU0 = envMapU[vertex0];
                baseU1 = envMapU[vertex1];
                baseU2 = envMapU[vertex2];
                baseU3 = envMapU[vertex3];
                u0 = baseU0;
                u1 = baseU1;
                u2 = baseU2;
                lastU = baseU3;
                reflect.vx = (StaticRVert[vertex0].vx + StaticRVert[vertex1].vx) >> 1;
                reflect.vy = (StaticRVert[vertex0].vy + StaticRVert[vertex1].vy) >> 1;
                reflect.vz = (StaticRVert[vertex0].vz + StaticRVert[vertex1].vz) >> 1;
                Normalise(&reflect);
                reflectionValue = DotProduct(&reflect, &eye);
                reflect.vx = ((s32)((u32)reflect.vx * (u32)reflectionValue) >> 11) - eye.vx;
                reflect.vz = ((s32)((u32)reflect.vz * (u32)reflectionValue) >> 11) - eye.vz;
                reflectionValue = (ratan2(reflect.vx, reflect.vz) >> 6) & 63;
                if (baseU1 < baseU0) {
                    if (reflectionValue < baseU1 || baseU0 < reflectionValue) {
                        u1 = baseU1 + 64;
                    }
                } else {
                    if (reflectionValue < baseU0 || baseU1 < reflectionValue) {
                        u0 = baseU0 + 64;
                    }
                }
                reflect.vx = (StaticRVert[vertex1].vx + StaticRVert[vertex2].vx) >> 1;
                reflect.vy = (StaticRVert[vertex1].vy + StaticRVert[vertex2].vy) >> 1;
                reflect.vz = (StaticRVert[vertex1].vz + StaticRVert[vertex2].vz) >> 1;
                Normalise(&reflect);
                reflectionValue = DotProduct(&reflect, &eye);
                reflect.vx = ((s32)((u32)reflect.vx * (u32)reflectionValue) >> 11) - eye.vx;
                reflect.vz = ((s32)((u32)reflect.vz * (u32)reflectionValue) >> 11) - eye.vz;
                reflectionValue = (ratan2(reflect.vx, reflect.vz) >> 6) & 63;
                if (baseU2 < baseU1) {
                    if (reflectionValue < baseU2 || baseU1 < reflectionValue) {
                        u2 = baseU2 + 64;
                    }
                } else {
                    if (reflectionValue < baseU1 || baseU2 < reflectionValue) {
                        u1 = baseU1 + 64;
                    }
                }
                reflect.vx = (StaticRVert[vertex2].vx + StaticRVert[vertex3].vx) >> 1;
                reflect.vy = (StaticRVert[vertex2].vy + StaticRVert[vertex3].vy) >> 1;
                reflect.vz = (StaticRVert[vertex2].vz + StaticRVert[vertex3].vz) >> 1;
                Normalise(&reflect);
                reflectionValue = DotProduct(&reflect, &eye);
                reflect.vx = ((s32)((u32)reflect.vx * (u32)reflectionValue) >> 11) - eye.vx;
                reflect.vz = ((s32)((u32)reflect.vz * (u32)reflectionValue) >> 11) - eye.vz;
                reflectionValue = (ratan2(reflect.vx, reflect.vz) >> 6) & 63;
                if (baseU3 < baseU2) {
                    if (reflectionValue < baseU3 || baseU2 < reflectionValue) {
                        lastU = baseU3 + 64;
                    }
                } else {
                    if (reflectionValue < baseU2 || baseU3 < reflectionValue) {
                        u2 = baseU2 + 64;
                    }
                }
                reflect.vx = (StaticRVert[vertex0].vx + StaticRVert[vertex3].vx) >> 1;
                reflect.vy = (StaticRVert[vertex0].vy + StaticRVert[vertex3].vy) >> 1;
                reflect.vz = (StaticRVert[vertex0].vz + StaticRVert[vertex3].vz) >> 1;
                Normalise(&reflect);
                reflectionValue = DotProduct(&reflect, &eye);
                reflect.vx = ((s32)((u32)reflect.vx * (u32)reflectionValue) >> 11) - eye.vx;
                reflect.vz = ((s32)((u32)reflect.vz * (u32)reflectionValue) >> 11) - eye.vz;
                reflectionValue = (ratan2(reflect.vx, reflect.vz) >> 6) & 63;
                if (baseU3 < baseU0) {
                    if (reflectionValue < baseU3 || baseU0 < reflectionValue) {
                        lastU = baseU3 + 64;
                    }
                } else {
                    if (reflectionValue < baseU0 || baseU3 < reflectionValue) {
                        u0 = baseU0 + 64;
                    }
                }
                packet->u0 = u0;
                packet->u1 = u1;
                packet->u2 = u2;
                packet->u3 = lastU;
                packet->v0 = envMapV[vertex0];
                packet->v1 = envMapV[vertex1];
                packet->v2 = envMapV[vertex2];
                packet->v3 = envMapV[vertex3];
                packet->clut = GetClut(704, 258);
                packet->r0 = envmapr;
                packet->g0 = envmapg;
                packet->b0 = envmapb;
                *(DVECTOR *)&packet->x0 = screen[vertex0];
                *(DVECTOR *)&packet->x1 = screen[vertex1];
                *(DVECTOR *)&packet->x2 = screen[vertex2];
                *(DVECTOR *)&packet->x3 = screen[vertex3];
                packet->x0 = ((packet->x0 * scale) >> 8) + xOffset;
                packet->x1 = ((packet->x1 * scale) >> 8) + xOffset;
                packet->x2 = ((packet->x2 * scale) >> 8) + xOffset;
                packet->x3 = ((packet->x3 * scale) >> 8) + xOffset;
                packet->y0 = ((packet->y0 * scale) >> 8) + yOffset;
                packet->y1 = ((packet->y1 * scale) >> 8) + yOffset;
                packet->y2 = ((packet->y2 * scale) >> 8) + yOffset;
                packet->y3 = ((packet->y3 * scale) >> 8) + yOffset;
                BindScaled(&packet->x0, &screen[vertex0], scale, xOffset, yOffset);
                BindScaled(&packet->x1, &screen[vertex1], scale, xOffset, yOffset);
                BindScaled(&packet->x2, &screen[vertex2], scale, xOffset, yOffset);
                BindScaled(&packet->x3, &screen[vertex3], scale, xOffset, yOffset);
                z = AverageZ4(depth[vertex0], depth[vertex1], depth[vertex2], depth[vertex3]);
                z %= 1600;
                AddPrim(OT2[CurrentScreen] + z, nextPrim.ptr);
                nextPrim.ptr += sizeof(POLY_FT4);
            }
            currentPrimitive.f4++;
            break;
        }
    }
    medalDisplay = 1;
    PortTagPrims(start, nextPrim.ptr, "env map", object->name, 0);
}


void InitFonts(void)
{
    FntLoad(0x280, 0x100);
    SetDumpFnt(FntOpen(0xA, 0x46, 0x136, 0xE6, 0, 0x100));
}
