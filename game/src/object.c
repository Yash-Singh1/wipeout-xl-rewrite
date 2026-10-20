#include "functions.h"

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
PrimPtr nextPrim;
int envmapr;
int envmapg;
int envmapb;
short CurrentScreen;
unsigned char medalDisplay;

/* DrawEnvMap's stack frame. The gapNNN members stand for stack slots the function never uses; the
   struct is only a local, so its size and the offsets of its pointer members do not matter. */
typedef struct EnvMapWork {
    VECTOR reflect, unusedVector, eye;
    MATRIX matrix;
    VECTOR translation;
    int gap0[4];
    int transformFlag;
    int gap140;
    int savedX;
    int gap148;
    unsigned short *depth;
    int gap156;
    unsigned short *clip;
    int gap164;
    unsigned short i;
    unsigned char gap170[6];
    short v0;
    unsigned char gap178[6];
    short v1;
    unsigned char gap186[6];
    short v2;
    unsigned char gap194[6];
    short primitiveCount;
    unsigned char gap202[6];
    s32 lastVertex;
    int gap212[17];
    F4 *volatile nextPrimitive;
} EnvMapWork;

void DrawEnvMap(Object *object, void *unused, SVECTOR *normals, int xOffsetIn, int yOffset, int scale)
{
    EnvMapWork ws;
    DVECTOR *screen;
    short v3, triangleUnused; int triangleLast, triangle0, triangle1; u16 clip0,clip1,clip2; int quadLast,quad0,quad1,quad3; int initialCount; unsigned int count;
    int dot,u,vertical,z,u0,u1,u2,u3,original0,original1,original2,original3;
    Primitive *primitive;
    F4 *primitiveCursor; F4 *triangleNext;
    SVECTOR *vertices;
    s32 pad[4];

    ws.savedX = xOffsetIn;
    ws.eye.vx = 0; ws.eye.vy = 0; ws.eye.vz = 4096;
    SetSkeletonTransform(object->skeleton);
    initialCount = object->primitiveCount;
    count = (unsigned short)object->vertexCount;
    vertices = object->vertices;
    primsDrawn += initialCount; ws.primitiveCount = initialCount;
    if ((short)count <= 128) { screen = (DVECTOR *)PsxScratchpad; ws.depth = (unsigned short *)(PsxScratchpad + 0x200); ws.clip = (unsigned short *)(PsxScratchpad + 0x300); } /* Port: the console's scratchpad RAM at 0x1F800000 */
    else { screen = StaticScreen; ws.depth = StaticDepth; ws.clip = StaticFlags; }
    RotTransPersN(vertices, screen, ws.depth, ws.clip, (short)count);
    ws.translation.vx = 0; ws.translation.vy = 0; ws.translation.vz = 0;
    TransMatrix(&ws.matrix, &ws.translation);
    SetTransMatrix(&ws.matrix);
    for (ws.i = 0; (short)ws.i < (short)count; ws.i++) {
        s32 transformIndex = (s16)*(volatile u16 *)&ws.i;
        RotTrans(&normals[transformIndex], &StaticRVert[transformIndex], &ws.transformFlag);
    }
    {
        short reflectIndex;
        for (reflectIndex = 0; reflectIndex < (short)count; reflectIndex++) {
            ws.reflect.vx = StaticRVert[reflectIndex].vx;
            ws.reflect.vy = StaticRVert[reflectIndex].vy;
            ws.reflect.vz = StaticRVert[reflectIndex].vz;
            dot = DotProduct(&ws.reflect, &ws.eye);
            ws.reflect.vx = ((s32)((u32)ws.reflect.vx * (u32)dot) >> 11) - ws.eye.vx;
            ws.reflect.vz = ((s32)((u32)ws.reflect.vz * (u32)dot) >> 11) - ws.eye.vz;
            ws.reflect.vy = ((s32)((u32)ws.reflect.vy * (u32)dot) >> 11) - ws.eye.vy;
            envMapU[reflectIndex] = (ratan2(ws.reflect.vx, ws.reflect.vz) >> 6) & 63;
            vertical = 31 - ((s32)((u32)ws.reflect.vy << 6) >> 13);
            if (vertical < 0) vertical = 0;
            if (vertical >= 64) vertical = 63;
            envMapV[reflectIndex] = vertical;
        }
    }
    primitiveCursor = (F4 *)object->primitives;
    ws.i = 0;
    if ((short)*(volatile unsigned short *)&ws.primitiveCount <= 0) goto envDone;
    ws.nextPrimitive = primitiveCursor + 1;
envLoop:
    {
        switch (primitiveCursor->type) {
        case 1:
            triangle0 = ((F3 *)ws.nextPrimitive - 1)->coords[0];
            triangle1 = ((F3 *)ws.nextPrimitive - 1)->coords[1];
            ws.v0 = *(volatile u16 *)&((F3 *)ws.nextPrimitive - 1)->coords[0];
            triangleLast = ((F3 *)ws.nextPrimitive - 1)->coords[2];
            clip1 = ws.clip[triangle1];
            clip0 = ws.clip[triangle0];
            clip2 = ws.clip[triangleLast];
            ws.v1 = *(volatile u16 *)&((F3 *)ws.nextPrimitive - 1)->coords[1];
            v3 = (s16)*(volatile u16 *)&((F3 *)ws.nextPrimitive - 1)->coords[2];
            triangleNext = ws.nextPrimitive;
            if ((clip0 | clip1 | clip2) & 0x460) goto triAdvanceCached;
            if (NormalClip((unsigned short)screen[triangle0].vx | ((u32)(unsigned short)screen[triangle0].vy << 16), (unsigned short)screen[triangle1].vx | ((u32)(unsigned short)screen[triangle1].vy << 16), (unsigned short)screen[triangleLast].vx | ((u32)(unsigned short)screen[triangleLast].vy << 16)) > 0) {
                SetPolyFT3((POLY_FT3 *)nextPrim.ptr);
                ((POLY_FT4 *)nextPrim.ptr)->tpage = GetTPage(1, 0, 640, 256);
                original0 = envMapU[triangle0];
                original1 = envMapU[triangle1];
                original2 = envMapU[triangleLast];
                u0 = original0;
                u1 = original1;
                u2 = original2;
                ws.reflect.vx = (StaticRVert[triangle0].vx + StaticRVert[triangle1].vx) >> 1;
                ws.reflect.vy = (StaticRVert[triangle0].vy + StaticRVert[triangle1].vy) >> 1;
                ws.reflect.vz = (StaticRVert[triangle0].vz + StaticRVert[triangle1].vz) >> 1;
                Normalise(&ws.reflect);
                dot = DotProduct(&ws.reflect, &ws.eye);
                ws.reflect.vx = ((s32)((u32)ws.reflect.vx * (u32)dot) >> 11) - ws.eye.vx;
                ws.reflect.vz = ((s32)((u32)ws.reflect.vz * (u32)dot) >> 11) - ws.eye.vz;
                u = (ratan2(ws.reflect.vx, ws.reflect.vz) >> 6) & 63;
                if (original1 < original0) {
                    if (u < original1 || original0 < u) u1 = original1 + 64;
                } else {
                    if (u < original0 || original1 < u) u0 = original0 + 64;
                }
                triangle1 = (s16)*(volatile u16 *)&ws.v1;
                ws.reflect.vx = (StaticRVert[triangle1].vx + StaticRVert[v3].vx) >> 1;
                ws.reflect.vy = (StaticRVert[triangle1].vy + StaticRVert[v3].vy) >> 1;
                ws.reflect.vz = (StaticRVert[triangle1].vz + StaticRVert[v3].vz) >> 1;
                Normalise(&ws.reflect);
                dot = DotProduct(&ws.reflect, &ws.eye);
                ws.reflect.vx = ((s32)((u32)ws.reflect.vx * (u32)dot) >> 11) - ws.eye.vx;
                ws.reflect.vz = ((s32)((u32)ws.reflect.vz * (u32)dot) >> 11) - ws.eye.vz;
                u = (ratan2(ws.reflect.vx, ws.reflect.vz) >> 6) & 63;
                if (original2 < original1) {
                    if (u < original2 || original1 < u) u2 = original2 + 64;
                } else {
                    if (u < original1 || original2 < u) u1 = original1 + 64;
                }
                triangle0 = (s16)*(volatile u16 *)&ws.v0;
                ws.reflect.vx = (StaticRVert[triangle0].vx + StaticRVert[v3].vx) >> 1;
                ws.reflect.vy = (StaticRVert[triangle0].vy + StaticRVert[v3].vy) >> 1;
                ws.reflect.vz = (StaticRVert[triangle0].vz + StaticRVert[v3].vz) >> 1;
                Normalise(&ws.reflect);
                dot = DotProduct(&ws.reflect, &ws.eye);
                ws.reflect.vx = ((s32)((u32)ws.reflect.vx * (u32)dot) >> 11) - ws.eye.vx;
                ws.reflect.vz = ((s32)((u32)ws.reflect.vz * (u32)dot) >> 11) - ws.eye.vz;
                u = (ratan2(ws.reflect.vx, ws.reflect.vz) >> 6) & 63;
                {
                    int seam2 = original0 < u, seam0 = original2 < u;
                    if (original2 < original0) {
                        if (u < original2 || seam2) u2 = original2 + 64;
                    } else {
                        if (u < original0 || seam0) u0 = original0 + 64;
                    }
                }
                ((POLY_FT4 *)nextPrim.ptr)->u0 = u0;
                ((POLY_FT4 *)nextPrim.ptr)->u1 = u1;
                ((POLY_FT4 *)nextPrim.ptr)->u2 = u2;
                triangle0 = (s16)*(volatile u16 *)&ws.v0;
                ((POLY_FT4 *)nextPrim.ptr)->v0 = envMapV[triangle0];
                triangle1 = (s16)*(volatile u16 *)&ws.v1;
                ((POLY_FT4 *)nextPrim.ptr)->v1 = envMapV[triangle1];
                ((POLY_FT4 *)nextPrim.ptr)->v2 = envMapV[v3];
                *(volatile u_short *)&((POLY_FT4 *)nextPrim.ptr)->clut = GetClut(704, 258);
                ((POLY_FT4 *)nextPrim.ptr)->r0 = envmapr;
                ((POLY_FT4 *)nextPrim.ptr)->g0 = envmapg;
                ((POLY_FT4 *)nextPrim.ptr)->b0 = envmapb;
                *(DVECTOR *)&((POLY_FT4 *)nextPrim.ptr)->x0 = screen[triangle0];
                *(DVECTOR *)&((POLY_FT4 *)nextPrim.ptr)->x1 = screen[triangle1];
                *(DVECTOR *)&((POLY_FT4 *)nextPrim.ptr)->x2 = screen[v3];
                *(volatile short *)&((POLY_FT4 *)nextPrim.ptr)->x0 = ((((POLY_FT4 *)nextPrim.ptr)->x0 * scale) >> 8) + ws.savedX;
                *(volatile short *)&((POLY_FT4 *)nextPrim.ptr)->x1 = ((((POLY_FT4 *)nextPrim.ptr)->x1 * scale) >> 8) + ws.savedX;
                *(volatile short *)&((POLY_FT4 *)nextPrim.ptr)->x2 = ((((POLY_FT4 *)nextPrim.ptr)->x2 * scale) >> 8) + ws.savedX;
                { POLY_FT4 *packet = (POLY_FT4 *)nextPrim.ptr; *(volatile short *)&packet->y0 = ((packet->y0 * scale) >> 8) + yOffset; }
                { POLY_FT4 *packet = (POLY_FT4 *)nextPrim.ptr; *(volatile short *)&packet->y1 = ((packet->y1 * scale) >> 8) + yOffset; }
                { POLY_FT4 *packet = (POLY_FT4 *)nextPrim.ptr; *(volatile short *)&packet->y2 = ((packet->y2 * scale) >> 8) + yOffset; }
                z = AverageZ3(ws.depth[triangle0], ws.depth[triangle1], ws.depth[v3]);
                AddPrim(OT2[CurrentScreen] + z % 1600, nextPrim.ptr);
                nextPrim.ptr += 32;
            }
            triangleNext = ws.nextPrimitive;
triAdvanceCached:
            primitiveCursor++;
            ws.nextPrimitive = triangleNext + 1;
            break;
        case 3:
            ws.v0 = *(volatile u16 *)&((F4 *)ws.nextPrimitive - 1)->coords[0];
            triangle0 = ((F4 *)ws.nextPrimitive - 1)->coords[0];
            ws.v1 = *(volatile u16 *)&((F4 *)ws.nextPrimitive - 1)->coords[1];
            triangle1 = ((F4 *)ws.nextPrimitive - 1)->coords[1];
            v3 = (s16)*(volatile u16 *)&((F4 *)ws.nextPrimitive - 1)->coords[2];
            triangleLast = ((F4 *)ws.nextPrimitive - 1)->coords[2];
            quadLast = ((F4 *)ws.nextPrimitive - 1)->coords[3];
            {
                u16 quadClip3;
                clip0 = ws.clip[triangle0]; clip1 = ws.clip[triangle1]; clip2 = ws.clip[triangleLast]; quadClip3 = ws.clip[quadLast];
                ws.v2 = quadLast;
                if (!((clip0 | clip1 | clip2 | quadClip3) & 0x460) && NormalClip((unsigned short)screen[triangle0].vx | ((u32)(unsigned short)screen[triangle0].vy << 16), (unsigned short)screen[triangle1].vx | ((u32)(unsigned short)screen[triangle1].vy << 16), (unsigned short)screen[triangleLast].vx | ((u32)(unsigned short)screen[triangleLast].vy << 16)) > 0) {
                    SetPolyFT4((POLY_FT4 *)nextPrim.ptr);
                    ((POLY_FT4 *)nextPrim.ptr)->tpage = GetTPage(1, 0, 640, 256);
                    original0 = envMapU[triangle0];
                    original1 = envMapU[triangle1];
                    original2 = envMapU[triangleLast];
                    original3 = envMapU[quadLast];
                    u0 = original0;
                    u1 = original1;
                    u2 = original2;
                    ws.reflect.vx = (StaticRVert[triangle0].vx + StaticRVert[triangle1].vx) >> 1;
                    ws.reflect.vy = (StaticRVert[triangle0].vy + StaticRVert[triangle1].vy) >> 1;
                    ws.reflect.vz = (StaticRVert[triangle0].vz + StaticRVert[triangle1].vz) >> 1;
                    Normalise(&ws.reflect);
                    dot = DotProduct(&ws.reflect, &ws.eye);
                    ws.lastVertex = original3;
                    ws.reflect.vx = ((s32)((u32)ws.reflect.vx * (u32)dot) >> 11) - ws.eye.vx;
                    ws.reflect.vz = ((s32)((u32)ws.reflect.vz * (u32)dot) >> 11) - ws.eye.vz;
                    u = (ratan2(ws.reflect.vx, ws.reflect.vz) >> 6) & 63;
                    if (original1 < original0) {
                        if (u < original1 || original0 < u) u1 = original1 + 64;
                    } else {
                        if (u < original0 || original1 < u) u0 = original0 + 64;
                    }
                    triangle1 = (s16)*(volatile u16 *)&ws.v1;
                    ws.reflect.vx = (StaticRVert[triangle1].vx + StaticRVert[v3].vx) >> 1;
                    ws.reflect.vy = (StaticRVert[triangle1].vy + StaticRVert[v3].vy) >> 1;
                    ws.reflect.vz = (StaticRVert[triangle1].vz + StaticRVert[v3].vz) >> 1;
                    Normalise(&ws.reflect);
                    dot = DotProduct(&ws.reflect, &ws.eye);
                    ws.reflect.vx = ((s32)((u32)ws.reflect.vx * (u32)dot) >> 11) - ws.eye.vx;
                    ws.reflect.vz = ((s32)((u32)ws.reflect.vz * (u32)dot) >> 11) - ws.eye.vz;
                    u = (ratan2(ws.reflect.vx, ws.reflect.vz) >> 6) & 63;
                    if (original2 < original1) {
                        if (u < original2 || original1 < u) u2 = original2 + 64;
                    } else {
                        if (u < original1 || original2 < u) u1 = original1 + 64;
                    }
                    {
                        int lastIndex = (short)*(volatile unsigned short *)&ws.v2;
                        ws.reflect.vx = (StaticRVert[v3].vx + StaticRVert[lastIndex].vx) >> 1;
                        ws.reflect.vy = (StaticRVert[v3].vy + StaticRVert[lastIndex].vy) >> 1;
                        ws.reflect.vz = (StaticRVert[v3].vz + StaticRVert[lastIndex].vz) >> 1;
                    }
                    Normalise(&ws.reflect);
                    dot = DotProduct(&ws.reflect, &ws.eye);
                    ws.reflect.vx = ((s32)((u32)ws.reflect.vx * (u32)dot) >> 11) - ws.eye.vx;
                    ws.reflect.vz = ((s32)((u32)ws.reflect.vz * (u32)dot) >> 11) - ws.eye.vz;
                    u = (ratan2(ws.reflect.vx, ws.reflect.vz) >> 6) & 63;
                    if (original3 < original2) {
                        if (u < original3 || original2 < u) ws.lastVertex = original3 + 64;
                    } else {
                        if (u < original2 || original3 < u) u2 = original2 + 64;
                    }
                    {
                        int firstIndex = (short)*(volatile unsigned short *)&ws.v0;
                        int lastIndex = (short)*(volatile unsigned short *)&ws.v2;
                        ws.reflect.vx = (StaticRVert[firstIndex].vx + StaticRVert[lastIndex].vx) >> 1;
                        ws.reflect.vy = (StaticRVert[firstIndex].vy + StaticRVert[lastIndex].vy) >> 1;
                        ws.reflect.vz = (StaticRVert[firstIndex].vz + StaticRVert[lastIndex].vz) >> 1;
                    }
                    Normalise(&ws.reflect);
                    dot = DotProduct(&ws.reflect, &ws.eye);
                    ws.reflect.vx = ((s32)((u32)ws.reflect.vx * (u32)dot) >> 11) - ws.eye.vx;
                    ws.reflect.vz = ((s32)((u32)ws.reflect.vz * (u32)dot) >> 11) - ws.eye.vz;
                    u = (ratan2(ws.reflect.vx, ws.reflect.vz) >> 6) & 63;
                    {
                        int seam0 = original0 < u, seam3 = original3 < u;
                        if (original3 < original0) {
                            if (u < original3 || seam0) ws.lastVertex = original3 + 64;
                        } else {
                            if (u < original0 || seam3) u0 = original0 + 64;
                        }
                    }
                    ((POLY_FT4 *)nextPrim.ptr)->u0 = u0;
                    ((POLY_FT4 *)nextPrim.ptr)->u1 = u1;
                    ((POLY_FT4 *)nextPrim.ptr)->u2 = u2;
                    ((POLY_FT4 *)nextPrim.ptr)->u3 = *(volatile unsigned char *)&ws.lastVertex;
                    quad0 = (short)*(volatile unsigned short *)&ws.v0;
                    ((POLY_FT4 *)nextPrim.ptr)->v0 = envMapV[quad0];
                    quad1 = (short)*(volatile unsigned short *)&ws.v1;
                    ((POLY_FT4 *)nextPrim.ptr)->v1 = envMapV[quad1];
                    ((POLY_FT4 *)nextPrim.ptr)->v2 = envMapV[v3];
                    quad3 = (short)*(volatile unsigned short *)&ws.v2;
                    ((POLY_FT4 *)nextPrim.ptr)->v3 = envMapV[quad3];
                    *(volatile u_short *)&((POLY_FT4 *)nextPrim.ptr)->clut = GetClut(704, 258);
                    ((POLY_FT4 *)nextPrim.ptr)->r0 = envmapr;
                    ((POLY_FT4 *)nextPrim.ptr)->g0 = envmapg;
                    ((POLY_FT4 *)nextPrim.ptr)->b0 = envmapb;
                    *(DVECTOR *)&((POLY_FT4 *)nextPrim.ptr)->x0 = screen[quad0];
                    *(DVECTOR *)&((POLY_FT4 *)nextPrim.ptr)->x1 = screen[quad1];
                    *(DVECTOR *)&((POLY_FT4 *)nextPrim.ptr)->x2 = screen[v3];
                    *(DVECTOR *)&((POLY_FT4 *)nextPrim.ptr)->x3 = screen[quad3];
                    *(volatile short *)&((POLY_FT4 *)nextPrim.ptr)->x0 = ((((POLY_FT4 *)nextPrim.ptr)->x0 * scale) >> 8) + ws.savedX;
                    *(volatile short *)&((POLY_FT4 *)nextPrim.ptr)->x1 = ((((POLY_FT4 *)nextPrim.ptr)->x1 * scale) >> 8) + ws.savedX;
                    *(volatile short *)&((POLY_FT4 *)nextPrim.ptr)->x2 = ((((POLY_FT4 *)nextPrim.ptr)->x2 * scale) >> 8) + ws.savedX;
                    *(volatile short *)&((POLY_FT4 *)nextPrim.ptr)->x3 = ((((POLY_FT4 *)nextPrim.ptr)->x3 * scale) >> 8) + ws.savedX;
                    *(volatile short *)&((POLY_FT4 *)nextPrim.ptr)->y0 = ((((POLY_FT4 *)nextPrim.ptr)->y0 * scale) >> 8) + yOffset;
                    *(volatile short *)&((POLY_FT4 *)nextPrim.ptr)->y1 = ((((POLY_FT4 *)nextPrim.ptr)->y1 * scale) >> 8) + yOffset;
                    *(volatile short *)&((POLY_FT4 *)nextPrim.ptr)->y2 = ((((POLY_FT4 *)nextPrim.ptr)->y2 * scale) >> 8) + yOffset;
                    *(volatile short *)&((POLY_FT4 *)nextPrim.ptr)->y3 = ((((POLY_FT4 *)nextPrim.ptr)->y3 * scale) >> 8) + yOffset;
                    z = AverageZ4(ws.depth[quad0], ws.depth[quad1], ws.depth[v3], ws.depth[quad3]);
                    AddPrim(OT2[CurrentScreen] + z % 1600, nextPrim.ptr);
                    nextPrim.ptr += 40;
                }
            }
            primitiveCursor = ws.nextPrimitive;
            ws.nextPrimitive++;
            break;
        }
        ws.i++;
        if ((short)ws.i < (short)*(volatile unsigned short *)&ws.primitiveCount) goto envLoop;
    }
envDone:
    medalDisplay = 1;
}


void InitFonts(void)
{
    FntLoad(0x280, 0x100);
    SetDumpFnt(FntOpen(0xA, 0x46, 0x136, 0xE6, 0, 0x100));
}
