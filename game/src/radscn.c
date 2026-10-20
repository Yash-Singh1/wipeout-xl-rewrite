#include "functions.h"

/* Debug pad state for the radius controls: the current pad word, and the raw input. */
u_short debugPadState;
/* Port: debugPadInput is part of padBuf1; the uses read it directly. */
#include "pad.h"


extern TrackData   *track;
extern Object     **trackObjects;
void CalcNewRadius(TrackSection *section, Object *objects)
{
    TrackSection *visSection;
    Object       *object;
    MATRIX       *matrix;
    Vector       *sectionCentre;
    VECTOR        vector;
    long long     distance;
    short         j;
    short         objectCount;
    short         withinLocal;
    short         withinGlobal;
    short         allCount;
    short         objectList[128];

    DFree(heap, (char *)section->objects);
    section->objectCount = 0;
    object = objects;
    objectCount = 0;
    allCount = 0;
    while (object) {
        withinLocal = 0;
        withinGlobal = 0;
        matrix = &object->skeleton->relative;
        sectionCentre = &section->centre;
        vector.vx = matrix->t[0] - sectionCentre->vx;
        vector.vy = matrix->t[1] - sectionCentre->vy;
        vector.vz = matrix->t[2] - sectionCentre->vz;
        if (section->flags & 1) {
            if (((abs(vector.vx) < (section->globalRadius * 2)) && (abs(vector.vy) < (section->globalRadius * 2))) && (abs(vector.vz) < (section->globalRadius * 2))) {
                distance = SquareRoot0(((vector.vx * vector.vx) + (vector.vy * vector.vy)) + (vector.vz * vector.vz));
                distance = distance - object->extent;
                distance -= 1024;
                if (distance < section->globalRadius) {
                    withinGlobal = 1;
                }
            }
        } else {
            if (((abs(vector.vx) < section->globalRadius) && (abs(vector.vy) < section->globalRadius)) && (abs(vector.vz) < section->globalRadius)) {
                distance = SquareRoot0(((vector.vx * vector.vx) + (vector.vy * vector.vy)) + (vector.vz * vector.vz));
                distance = distance - object->extent;
                distance -= 1024;
                if (distance < section->globalRadius) {
                    withinGlobal = 1;
                }
            }
        }
        if (withinGlobal) {
            visSection = section;
            for (j = 0; (!withinLocal) && (j < 30); j++) {
                matrix = &object->skeleton->relative;
                sectionCentre = &visSection->centre;
                vector.vx = matrix->t[0] - sectionCentre->vx;
                vector.vy = matrix->t[1] - sectionCentre->vy;
                vector.vz = matrix->t[2] - sectionCentre->vz;
                if (((abs(vector.vx) < section->localRadius) && (abs(vector.vy) < section->localRadius)) && (abs(vector.vz) < section->localRadius)) {
                    distance = SquareRoot0(((vector.vx * vector.vx) + (vector.vy * vector.vy)) + (vector.vz * vector.vz));
                    distance = distance - object->extent;
                    distance -= 1024;
                    if (distance < section->localRadius) {
                        withinLocal = 1;
                    }
                }
                visSection = visSection->nextSection.ptr;
            }

            visSection = section;
            for (j = 0; (!withinLocal) && (j < 30); j++) {
                matrix = &object->skeleton->relative;
                sectionCentre = &visSection->centre;
                vector.vx = matrix->t[0] - sectionCentre->vx;
                vector.vy = matrix->t[1] - sectionCentre->vy;
                vector.vz = matrix->t[2] - sectionCentre->vz;
                if (((abs(vector.vx) < section->localRadius) && (abs(vector.vy) < section->localRadius)) && (abs(vector.vz) < section->localRadius)) {
                    distance = SquareRoot0(((vector.vx * vector.vx) + (vector.vy * vector.vy)) + (vector.vz * vector.vz));
                    distance = distance - object->extent;
                    distance -= 1024;
                    if (distance < section->localRadius) {
                        withinLocal = 1;
                    }
                }
                visSection = visSection->prevSection.ptr;
            }
        }
        if (withinLocal) {
            if (objectCount < 128) {
                objectList[objectCount++] = allCount;
            } else {
                Error("Radius.c:SceneRadiusCheck: Maximum number of objects exceeded", Fatal);
            }
        }
        object = object->next;
        allCount++;
    }

    section->objects = DAlloc(heap, objectCount * sizeof(short));
    if (!section->objects) {
        Error("Error:: radius.c: Not enough memory for section object list", Fatal);
    }
    section->objectCount = objectCount;
    for (j = 0; j < objectCount; j++) {
        section->objects[j] = objectList[j];
    }
}


void SceneRadiusCheck(Object *objects)
{
    TrackSection *section;
    TrackSection *visSection;
    short         i;
    short         j;
    Object       *object;
    MATRIX       *matrix;
    Vector       *sectionCentre;
    short         objectCount;
    VECTOR        vector;
    long long     distance;
    short         objectList[128];
    short         withinLocal;
    short         withinGlobal;
    short         allCount;

    object = objects;
    objectCount = 0;
    while (object) {
        object = object->next;
        objectCount++;
    }

    trackObjects = DAlloc(heap, (objectCount + 1) * sizeof(Object *));
    if (!trackObjects) {
        Error("radscn.c::SceneRadiusCheck(): No memory for global object table", Fatal);
    }
    object = objects;
    for (i = 0; i < objectCount; i++) {
        trackObjects[i] = object;
        object = object->next;
    }

    section = track->sections;
    for (i = 0; i < track->sectionCount; i++) {
        object = objects;
        objectCount = 0;
        allCount = 0;
        while (object) {
            withinLocal = 0;
            withinGlobal = 0;
            matrix = &object->skeleton->relative;
            sectionCentre = &section->centre;
            vector.vx = matrix->t[0] - sectionCentre->vx;
            vector.vy = matrix->t[1] - sectionCentre->vy;
            vector.vz = matrix->t[2] - sectionCentre->vz;
            if (section->flags & 1) {
                if (((abs(vector.vx) < (section->globalRadius * 2)) && (abs(vector.vy) < (section->globalRadius * 2))) && (abs(vector.vz) < (section->globalRadius * 2))) {
                    distance = SquareRoot0(((vector.vx * vector.vx) + (vector.vy * vector.vy)) + (vector.vz * vector.vz));
                    distance = distance - object->extent;
                    distance -= 1024;
                    if (distance < section->globalRadius) {
                        withinGlobal = 1;
                    }
                }
            } else {
                if (((abs(vector.vx) < section->globalRadius) && (abs(vector.vy) < section->globalRadius)) && (abs(vector.vz) < section->globalRadius)) {
                    distance = SquareRoot0(((vector.vx * vector.vx) + (vector.vy * vector.vy)) + (vector.vz * vector.vz));
                    distance = distance - object->extent;
                    distance -= 1024;
                    if (distance < section->globalRadius) {
                        withinGlobal = 1;
                    }
                }
            }
            if (withinGlobal) {
                visSection = section;
                for (j = 0; (!withinLocal) && (j < 30); j++) {
                    matrix = &object->skeleton->relative;
                    sectionCentre = &visSection->centre;
                    vector.vx = matrix->t[0] - sectionCentre->vx;
                    vector.vy = matrix->t[1] - sectionCentre->vy;
                    vector.vz = matrix->t[2] - sectionCentre->vz;
                    if (((abs(vector.vx) < section->localRadius) && (abs(vector.vy) < section->localRadius)) && (abs(vector.vz) < section->localRadius)) {
                        distance = SquareRoot0(((vector.vx * vector.vx) + (vector.vy * vector.vy)) + (vector.vz * vector.vz));
                        distance = distance - object->extent;
                        distance -= 1024;
                        if (distance < section->localRadius) {
                            withinLocal = 1;
                        }
                    }
                    visSection = visSection->nextSection.ptr;
                }

                visSection = section;
                for (j = 0; (!withinLocal) && (j < 30); j++) {
                    matrix = &object->skeleton->relative;
                    sectionCentre = &visSection->centre;
                    vector.vx = matrix->t[0] - sectionCentre->vx;
                    vector.vy = matrix->t[1] - sectionCentre->vy;
                    vector.vz = matrix->t[2] - sectionCentre->vz;
                    if (((abs(vector.vx) < section->localRadius) && (abs(vector.vy) < section->localRadius)) && (abs(vector.vz) < section->localRadius)) {
                        distance = SquareRoot0(((vector.vx * vector.vx) + (vector.vy * vector.vy)) + (vector.vz * vector.vz));
                        distance = distance - object->extent;
                        distance -= 1024;
                        if (distance < section->localRadius) {
                            withinLocal = 1;
                        }
                    }
                    visSection = visSection->prevSection.ptr;
                }
            }
            if (withinLocal) {
                if (objectCount < 128) {
                    objectList[objectCount++] = allCount;
                } else {
                    Error("Radius.c:SceneRadiusCheck: Maximum number of objects exceeded", Fatal);
                }
            }
            object = object->next;
            allCount++;
        }

        section->objects = DAlloc(heap, objectCount * sizeof(short));
        if (!section->objects) {
            Error("Error:: radius.c: Not enough memory for section object list", Fatal);
        }
        section->objectCount = objectCount;
        for (j = 0; j < objectCount; j++) {
            section->objects[j] = objectList[j];
        }

        section = NextIndexSection(section);
    }
}


void RadiusControl(TrackCamera *camera, Object **prm)
{
    u_short opad;
    u_short xpad;
    int    junctionIndex;
    int     noJunction = -1;

    opad = debugPadState;
    debugPadState = padBuf1[0].buttons;
    debugPadState = ~debugPadState;
    xpad = (debugPadState ^ opad) & debugPadState;

    junctionIndex = camera->section->junction.index;
    if ((xpad & 0x08) && (junctionIndex != noJunction)) {
        camera->section = camera->section->junction.ptr;
    } else if (xpad & 0x1000) {
        camera->section = camera->section->nextSection.ptr;
    } else if (xpad & 0x4000) {
        camera->section = camera->section->prevSection.ptr;
    } else if (debugPadState & 0x2000) {
        camera->section = camera->section->nextSection.ptr;
    } else if (debugPadState & 0x8000) {
        camera->section = camera->section->prevSection.ptr;
    }

    if (debugPadState & 0x0400) {
        camera->section->localRadius -= 1024;
        if (camera->section->localRadius < 0) {
            camera->section->localRadius = 0;
        }
        CalcNewRadius(camera->section, prm[0]);
    } else if (debugPadState & 0x0800) {
        camera->section->localRadius += 1024;
        if (camera->section->localRadius < 0) {
            camera->section->localRadius = 32767;
        }
        CalcNewRadius(camera->section, prm[0]);
    } else if (debugPadState & 0x0100) {
        camera->section->globalRadius -= 1024;
        if (camera->section->globalRadius < 0) {
            camera->section->globalRadius = 0;
        }
        CalcNewRadius(camera->section, prm[0]);
    } else if (debugPadState & 0x0200) {
        camera->section->globalRadius += 1024;
        if (camera->section->globalRadius < 0) {
            camera->section->globalRadius = 32767;
        }
        CalcNewRadius(camera->section, prm[0]);
    }

    if (debugPadState & 0x80) {
        camera->hdg -= 64;
    }
    if (debugPadState & 0x20) {
        camera->hdg += 64;
    }
    if (debugPadState & 0x40) {
        camera->pitch -= 64;
    }
    if (debugPadState & 0x10) {
        camera->pitch += 64;
    }
}
