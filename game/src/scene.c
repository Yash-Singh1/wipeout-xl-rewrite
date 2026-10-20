#include "functions.h"
#include "trackasm.h"

extern char        errorString[];
extern HeapBlock *heap;
extern Texture     *textureTable[];

short IntelPrim(Prm prm)
{
    IntelShort(&prm.f3->type);
    IntelShort(&prm.f3->flag);
    switch (prm.f3->type) {
    case 1:
        IntelShort(&prm.f3->coords[0]);
        IntelShort(&prm.f3->coords[1]);
        IntelShort(&prm.f3->coords[2]);
        break;
    case 2:
        IntelShort(&prm.ft3->coords[0]);
        IntelShort(&prm.ft3->coords[1]);
        IntelShort(&prm.ft3->coords[2]);
        IntelShort(&prm.ft3->texture);
        break;
    case 3:
        IntelShort(&prm.f4->coords[0]);
        IntelShort(&prm.f4->coords[1]);
        IntelShort(&prm.f4->coords[2]);
        IntelShort(&prm.f4->coords[3]);
        break;
    case 4:
        IntelShort(&prm.ft4->coords[0]);
        IntelShort(&prm.ft4->coords[1]);
        IntelShort(&prm.ft4->coords[2]);
        IntelShort(&prm.ft4->coords[3]);
        IntelShort(&prm.ft4->texture);
        break;
    case 5:
        IntelShort(&prm.g3->coords[0]);
        IntelShort(&prm.g3->coords[1]);
        IntelShort(&prm.g3->coords[2]);
        break;
    case 6:
        IntelShort(&prm.gt3->coords[0]);
        IntelShort(&prm.gt3->coords[1]);
        IntelShort(&prm.gt3->coords[2]);
        IntelShort(&prm.gt3->texture);
        break;
    case 7:
        IntelShort(&prm.g4->coords[0]);
        IntelShort(&prm.g4->coords[1]);
        IntelShort(&prm.g4->coords[2]);
        IntelShort(&prm.g4->coords[3]);
        break;
    case 8:
        IntelShort(&prm.gt4->coords[0]);
        IntelShort(&prm.gt4->coords[1]);
        IntelShort(&prm.gt4->coords[2]);
        IntelShort(&prm.gt4->coords[3]);
        IntelShort(&prm.gt4->texture);
        break;
    case 12:
        IntelShort(&prm.lsf3->coords[0]);
        IntelShort(&prm.lsf3->coords[1]);
        IntelShort(&prm.lsf3->coords[2]);
        IntelShort(&prm.lsf3->normal);
        break;
    case 13:
        IntelShort(&prm.lsft3->coords[0]);
        IntelShort(&prm.lsft3->coords[1]);
        IntelShort(&prm.lsft3->coords[2]);
        IntelShort(&prm.lsft3->normal);
        IntelShort(&prm.lsft3->texture);
        break;
    case 14:
        IntelShort(&prm.lsf4->coords[0]);
        IntelShort(&prm.lsf4->coords[1]);
        IntelShort(&prm.lsf4->coords[2]);
        IntelShort(&prm.lsf4->coords[3]);
        IntelShort(&prm.lsf4->normal);
        break;
    case 15:
        IntelShort(&prm.lsft4->coords[0]);
        IntelShort(&prm.lsft4->coords[1]);
        IntelShort(&prm.lsft4->coords[2]);
        IntelShort(&prm.lsft4->coords[3]);
        IntelShort(&prm.lsft4->normal);
        IntelShort(&prm.lsft4->texture);
        break;
    case 16:
        IntelShort(&prm.lsg3->coords[0]);
        IntelShort(&prm.lsg3->coords[1]);
        IntelShort(&prm.lsg3->coords[2]);
        IntelShort(&prm.lsg3->normals[0]);
        IntelShort(&prm.lsg3->normals[1]);
        IntelShort(&prm.lsg3->normals[2]);
        break;
    case 18:
        IntelShort(&prm.lsg4->coords[0]);
        IntelShort(&prm.lsg4->coords[1]);
        IntelShort(&prm.lsg4->coords[2]);
        IntelShort(&prm.lsg4->coords[3]);
        IntelShort(&prm.lsg4->normals[0]);
        IntelShort(&prm.lsg4->normals[1]);
        IntelShort(&prm.lsg4->normals[2]);
        IntelShort(&prm.lsg4->normals[3]);
        break;
    case 17:
        IntelShort(&prm.lsgt3->coords[0]);
        IntelShort(&prm.lsgt3->coords[1]);
        IntelShort(&prm.lsgt3->coords[2]);
        IntelShort(&prm.lsgt3->normals[0]);
        IntelShort(&prm.lsgt3->normals[1]);
        IntelShort(&prm.lsgt3->normals[2]);
        IntelShort(&prm.lsgt3->texture);
        break;
    case 19:
        IntelShort(&prm.lsgt4->coords[0]);
        IntelShort(&prm.lsgt4->coords[1]);
        IntelShort(&prm.lsgt4->coords[2]);
        IntelShort(&prm.lsgt4->coords[3]);
        IntelShort(&prm.lsgt4->normals[0]);
        IntelShort(&prm.lsgt4->normals[1]);
        IntelShort(&prm.lsgt4->normals[2]);
        IntelShort(&prm.lsgt4->normals[3]);
        IntelShort(&prm.lsgt4->texture);
        break;
    case 10:
    case 11:
        IntelShort(&prm.spr->coord);
        IntelShort(&prm.spr->width);
        IntelShort(&prm.spr->height);
        IntelShort(&prm.spr->texture);
        break;
    case 20:
        IntelLong(&prm.spline->control1.vx);
        IntelLong(&prm.spline->control1.vy);
        IntelLong(&prm.spline->control1.vz);
        IntelLong(&prm.spline->position.vx);
        IntelLong(&prm.spline->position.vy);
        IntelLong(&prm.spline->position.vz);
        IntelLong(&prm.spline->control2.vx);
        IntelLong(&prm.spline->control2.vy);
        IntelLong(&prm.spline->control2.vz);
        break;
    case 22:
        IntelLong(&prm.pointLight->position.vx);
        IntelLong(&prm.pointLight->position.vy);
        IntelLong(&prm.pointLight->position.vz);
        IntelShort(&prm.pointLight->startFalloff);
        IntelShort(&prm.pointLight->endFalloff);
        break;
    case 23:
        IntelLong(&prm.spotLight->position.vx);
        IntelLong(&prm.spotLight->position.vy);
        IntelLong(&prm.spotLight->position.vz);
        IntelShort(&prm.spotLight->direction.vx);
        IntelShort(&prm.spotLight->direction.vy);
        IntelShort(&prm.spotLight->direction.vz);
        IntelShort(&prm.spotLight->startFalloff);
        IntelShort(&prm.spotLight->endFalloff);
        IntelShort(&prm.spotLight->coneAngle);
        IntelShort(&prm.spotLight->spreadAngle);
        break;
    case 21:
        IntelShort(&prm.infiniteLight->direction.vx);
        IntelShort(&prm.infiniteLight->direction.vy);
        IntelShort(&prm.infiniteLight->direction.vz);
        break;
    default:
        sprintf(errorString, "Scene:IntelPrim: Bad primitive type $%x \n", prm.f3->type);
        return 1;
    }
    return 0;
}


void IntelLong(int *lvalue)
{
    char *cvalue;
    char  temp;

    cvalue = (char *)lvalue;
    temp = cvalue[0];
    cvalue[0] = cvalue[3];
    cvalue[3] = temp;
    temp = cvalue[1];
    cvalue[1] = cvalue[2];
    cvalue[2] = temp;
}


void IntelShort(short *svalue)
{
    char *cvalue;
    char  temp;

    cvalue = (char *)svalue;
    temp = cvalue[0];
    cvalue[0] = cvalue[1];
    cvalue[1] = temp;
}


Object *LoadPrmCommon(WADFILE *wad, char *name, short textureStart)
{
    Object  *object;
    Object  *lastObject;
    Object  *firstObject;
    Prm      prm;
    int     length;
    short    retry = 0;
    short    i;
    short    j;
    char    *start;
    char    *finish;
    short    uOffset;
    short    vOffset;
    Texture *texture;

retry_load:
    if (!wad) {
        length = FileLength(name);
        if (length <= 0) {
            sprintf(errorString, "scene.c:LoadPrm: File %s not found\n", name);
            Error(errorString, Fatal);
        }

        start = DAlloc(heap, length + 32);
        finish = start + length;
        if (!start) {
            sprintf(errorString, "scene.c:LoadPrm(): Failed to allocate memory for file %s\n", name);
            Error(errorString, Fatal);
        }

        if (retry) {
            printf("scene.c:LoadPrm(): Re-reading scene data - %s \n", name);
        }
        if (LoadFile(name, start) != length) {
            sprintf(errorString, "scene.c:LoadPrm(): Failed to load file %s\n", name);
            Error(errorString, Fatal);
        }
    } else {
        length = wad->sizes[wad->current];
        start = DAlloc(heap, length + 32);
        finish = start + length;
        readnextfile(wad, start);
    }

    prm.ptr = start;
    firstObject = prm.object;
    lastObject = 0;
    while (prm.ptr < finish) {
        object = prm.object++;
        IntelShort(&object->vertexCount);
        IntelShort(&object->normalCount);
        IntelShort(&object->primitiveCount);
        IntelLong(&object->extent);
        IntelShort(&object->flags);
        object->skeleton = prm.skeleton++;
        for (i = 0; i < 3; i++) {
            for (j = 0; j < 3; j++) {
                IntelShort(&object->skeleton->relative.m[i][j]);
            }

            IntelLong(&object->skeleton->relative.t[i]);
            IntelLong(&object->skeleton->absolute.t[i]);
        }

        IntelShort(&object->skeleton->update);
        object->bspTree = 0;
        object->next = 0;
        object->vertices = prm.svector;
        prm.svector += object->vertexCount;
        for (i = 0; i < object->vertexCount; i++) {
            IntelShort(&object->vertices[i].vx);
            IntelShort(&object->vertices[i].vy);
            IntelShort(&object->vertices[i].vz);
        }

        object->normals = prm.svector;
        prm.svector += object->normalCount;
        for (i = 0; i < object->normalCount; i++) {
            IntelShort(&object->normals[i].vx);
            IntelShort(&object->normals[i].vy);
            IntelShort(&object->normals[i].vz);
        }

        object->primitives = prm.primitive;
        for (i = 0; i < object->primitiveCount; i++) {
            retry = IntelPrim(prm);
            if (retry == 1) {
                DFree(heap, start);
                printf("error with prm file\n");
                goto retry_load;
            }
            switch (prm.f3->type) {
            case 1:
                prm.f3 += 1;
                break;

            case 3:
                prm.f4 += 1;
                break;

            case 2:
                prm.ft3->texture += textureStart;
                texture = textureTable[prm.ft3->texture];
                uOffset = texture->u0;
                vOffset = texture->v0;
                prm.ft3->cba = texture->cba;
                prm.ft3->tsb = texture->tsb;
                prm.ft3->u0 += uOffset;
                prm.ft3->v0 += vOffset;
                prm.ft3->u1 += uOffset;
                prm.ft3->v1 += vOffset;
                prm.ft3->u2 += uOffset;
                prm.ft3->v2 += vOffset;
                prm.ft3 += 1;
                break;

            case 4:
                prm.ft4->texture += textureStart;
                texture = textureTable[prm.ft4->texture];
                uOffset = texture->u0;
                vOffset = texture->v0;
                prm.ft4->cba = texture->cba;
                prm.ft4->tsb = texture->tsb;
                prm.ft4->u0 += uOffset;
                prm.ft4->v0 += vOffset;
                prm.ft4->u1 += uOffset;
                prm.ft4->v1 += vOffset;
                prm.ft4->u2 += uOffset;
                prm.ft4->v2 += vOffset;
                prm.ft4->u3 += uOffset;
                prm.ft4->v3 += vOffset;
                prm.ft4 += 1;
                break;

            case 5:
                prm.g3 += 1;
                break;

            case 7:
                prm.g4 += 1;
                break;

            case 6:
                prm.gt3->texture += textureStart;
                texture = textureTable[prm.gt3->texture];
                uOffset = texture->u0;
                vOffset = texture->v0;
                prm.gt3->cba = texture->cba;
                prm.gt3->tsb = texture->tsb;
                prm.gt3->u0 += uOffset;
                prm.gt3->v0 += vOffset;
                prm.gt3->u1 += uOffset;
                prm.gt3->v1 += vOffset;
                prm.gt3->u2 += uOffset;
                prm.gt3->v2 += vOffset;
                prm.gt3 += 1;
                break;

            case 8:
                prm.gt4->texture += textureStart;
                texture = textureTable[prm.gt4->texture];
                uOffset = texture->u0;
                vOffset = texture->v0;
                prm.gt4->cba = texture->cba;
                prm.gt4->tsb = texture->tsb;
                prm.gt4->u0 += uOffset;
                prm.gt4->v0 += vOffset;
                prm.gt4->u1 += uOffset;
                prm.gt4->v1 += vOffset;
                prm.gt4->u2 += uOffset;
                prm.gt4->v2 += vOffset;
                prm.gt4->u3 += uOffset;
                prm.gt4->v3 += vOffset;
                prm.gt4 += 1;
                break;

            case 12:
                prm.lsf3 += 1;
                break;

            case 14:
                prm.lsf4 += 1;
                break;

            case 13:
                prm.lsft3->texture += textureStart;
                texture = textureTable[prm.lsft3->texture];
                uOffset = texture->u0;
                vOffset = texture->v0;
                prm.lsft3->cba = texture->cba;
                prm.lsft3->tsb = texture->tsb;
                prm.lsft3->u0 += uOffset;
                prm.lsft3->v0 += vOffset;
                prm.lsft3->u1 += uOffset;
                prm.lsft3->v1 += vOffset;
                prm.lsft3->u2 += uOffset;
                prm.lsft3->v2 += vOffset;
                prm.lsft3 += 1;
                break;

            case 15:
                prm.lsft4->texture += textureStart;
                texture = textureTable[prm.lsft4->texture];
                uOffset = texture->u0;
                vOffset = texture->v0;
                prm.lsft4->cba = texture->cba;
                prm.lsft4->tsb = texture->tsb;
                prm.lsft4->u0 += uOffset;
                prm.lsft4->v0 += vOffset;
                prm.lsft4->u1 += uOffset;
                prm.lsft4->v1 += vOffset;
                prm.lsft4->u2 += uOffset;
                prm.lsft4->v2 += vOffset;
                prm.lsft4->u3 += uOffset;
                prm.lsft4->v3 += vOffset;
                prm.lsft4 += 1;
                break;

            case 16:
                prm.lsg3 += 1;
                break;

            case 18:
                prm.lsg4 += 1;
                break;

            case 17:
                prm.lsgt3->texture += textureStart;
                texture = textureTable[prm.lsgt3->texture];
                uOffset = texture->u0;
                vOffset = texture->v0;
                prm.lsgt3->cba = texture->cba;
                prm.lsgt3->tsb = texture->tsb;
                prm.lsgt3->u0 += uOffset;
                prm.lsgt3->v0 += vOffset;
                prm.lsgt3->u1 += uOffset;
                prm.lsgt3->v1 += vOffset;
                prm.lsgt3->u2 += uOffset;
                prm.lsgt3->v2 += vOffset;
                prm.lsgt3 += 1;
                break;

            case 19:
                prm.lsgt4->texture += textureStart;
                texture = textureTable[prm.lsgt4->texture];
                uOffset = texture->u0;
                vOffset = texture->v0;
                prm.lsgt4->cba = texture->cba;
                prm.lsgt4->tsb = texture->tsb;
                prm.lsgt4->u0 += uOffset;
                prm.lsgt4->v0 += vOffset;
                prm.lsgt4->u1 += uOffset;
                prm.lsgt4->v1 += vOffset;
                prm.lsgt4->u2 += uOffset;
                prm.lsgt4->v2 += vOffset;
                prm.lsgt4->u3 += uOffset;
                prm.lsgt4->v3 += vOffset;
                prm.lsgt4 += 1;
                break;

            case 10:

            case 11:
                prm.spr->texture += textureStart;
                prm.spr += 1;
                break;

            case 20:
                prm.spline += 1;
                break;

            case 22:
                prm.pointLight += 1;
                break;

            case 23:
                prm.spotLight += 1;
                break;

            case 21:
                prm.infiniteLight += 1;
                break;

            default:
                sprintf(errorString, "Bad primitive type %x in $%x \n", prm.f3->type, name);
                Error(errorString, Fatal);
                break;
            }
        }

        if (lastObject) {
            lastObject->next = object;
        }
        lastObject = object;
    }

    return firstObject;
}
void *WADLoadPrm(WADFILE *wad, short textureStart)
{
    return LoadPrmCommon(wad, 0, textureStart);
}


Object *LoadPrm(char *arg0, short arg1)
{
    return LoadPrmCommon(0, arg0, arg1);
}


Object *NewInstance(Object *source)
{
    Object *copy;
    char   *name;
    char   *sourceName;

    copy = DAlloc(heap, 0x40);
    copy->vertexCount = source->vertexCount;
    copy->vertices = source->vertices;
    copy->normalCount = source->normalCount;
    copy->normals = source->normals;
    copy->primitiveCount = source->primitiveCount;
    copy->primitives = source->primitives;
    copy->scratch = source->scratch;
    copy->bspTree = source->bspTree;
    copy->skeleton = DAlloc(heap, 0x50);
    copy->skeleton->super = source->skeleton->super;
    copy->skeleton->sub = source->skeleton->sub;
    copy->skeleton->next = source->skeleton->next;
    copy->extent = source->extent;
    copy->flags = source->flags;
    copy->next = 0;
    name = copy->name;
    sourceName = source->name;
    do {
        *name++ = *sourceName++;
    } while ((int)name < (int)(copy->name + 16));
    return copy;
}
