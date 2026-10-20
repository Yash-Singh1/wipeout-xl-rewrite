#include "functions.h"
#include "track.h"
#include "port_widen.h"
#define TrackVersion 8



extern TrackData   *track;
extern char         msgNewline[];

extern short        drawCracks;
void IntelVertex(void)
{
    VECTOR *vert;
    short   i;

    vert = track->vertices;
    for (i = 0; i < track->vertexCount; i++) {
        IntelLong(&vert->vx);
        IntelLong(&vert->vy);
        IntelLong(&vert->vz);
        vert++;
    }
}


void IntelFace(void)
{
    Face *face;
    short i;

    face = track->faces;
    for (i = 0; i < track->faceCount; i++) {
        IntelShort(&face->vertex[0]);
        IntelShort(&face->vertex[1]);
        IntelShort(&face->vertex[2]);
        IntelShort(&face->vertex[3]);

        IntelShort(&face->normal.vx);
        IntelShort(&face->normal.vy);
        IntelShort(&face->normal.vz);
        face++;
    }
}


void IntelSection(void)
{
    TrackSection *section;
    short         i;
    short         j;

    section = track->sections;
    for (i = 0; i < track->sectionCount; i++) {
        IntelLong(&section->junction.index);
        IntelLong(&section->prevSection.index);
        IntelLong(&section->nextSection.index);

        IntelLong(&section->centre.vx);
        IntelLong(&section->centre.vy);
        IntelLong(&section->centre.vz);

        IntelShort(&section->version);
        IntelShort(&section->pad);

        for (j = 0; j < 3; j++) {
            IntelShort(&section->northViewCount[j]);
            IntelShort(&section->southViewCount[j]);
            IntelShort(&section->eastViewCount[j]);
            IntelShort(&section->westViewCount[j]);
            IntelShort(&section->allViewCount[j]);
        }

        for (j = 0; j < 4; j++) {
            IntelShort(&section->high[j]);
            IntelShort(&section->med[j]);
        }

        IntelShort(&section->faceStart);
        IntelShort(&section->faceCount);

        IntelShort(&section->globalRadius);
        IntelShort(&section->localRadius);

        IntelShort(&section->flags);
        IntelShort(&section->secNo);

        section++;
    }
}


void CheckVersion(void)
{
    TrackSection *section;
    short         i;

    section = track->sections;
    for (i = 0; i < track->sectionCount; i++) {
        if (section->version != TrackVersion) {
            Error("Convert track with track10", Fatal);
        }
        section++;
    }
}


void IndexToPointers(void)
{
    TrackSection *section;
    short         i;

    section = track->sections;
    for (i = 0; i < track->sectionCount; i++) {
        section->prevSection.ptr = track->sections + section->prevSection.index;
        section->nextSection.ptr = track->sections + section->nextSection.index;

        if (section->junction.index != -1) {
            section->junction.ptr = track->sections + section->junction.index;
        } else {
            /* Port: the slot keeps -1 as a pointer too (a zero-extended file value would not
               compare equal to (TrackSection *)-1 natively). */
            section->junction.ptr = (TrackSection *)-1;
        }

        section++;
    }
}


void PointersToIndex(void)
{
    TrackSection *section;
    short         i;

    section = track->sections;
    for (i = 0; i < track->sectionCount; i++) {
        section->prevSection.index = (int)(section->prevSection.ptr - track->sections);
        section->nextSection.index = (int)(section->nextSection.ptr - track->sections);

        if (section->junction.index != -1) {
            section->junction.index = (int)(section->junction.ptr - track->sections);
        }

        section++;
    }
}


void WADInitVertices(WADFILE *wad)
{
    int length;

    length = wad->sizes[wad->current];

    track->vertices = (VECTOR *)DAlloc(heap, length + 32);
    printf("verts at : %p\n", (void *)track->vertices); /* Port: %p (was %x of the address) */
    if (!track->vertices) {
        Error("Track.c:InitVertices: Failed to allocate memory for track vertices", Fatal);
    }

    readnextfile(wad, (char *)track->vertices);
    track->vertexCount = length / sizeof(VECTOR);

    IntelVertex();
}


void WADInitFaces(WADFILE *wad)
{
    int  length;
    short i;

    length = wad->sizes[wad->current];

    track->faces = (Face *)DAlloc(heap, length + 32);
    if (!track->faces) {
        Error("Track.c:InitFaces: Failed to allocate memory for track faces", Fatal);
    }

    readnextfile(wad, (char *)track->faces);
    track->faceCount = length / sizeof(Face);

    IntelFace();

    /* keep a copy of the face colours */
    track->colCopy = DAlloc(heap, track->faceCount * sizeof(Colors));
    if (!track->faces) { /* checks the faces, not the colour copy */
        Error("Track.c:InitFaces: Failed to allocate memory for copy of colors", Fatal);
    }

    for (i = 0; i < track->faceCount; i++) {
        ((Colors *)track->colCopy)[i].red = track->faces[i].red;
        ((Colors *)track->colCopy)[i].green = track->faces[i].green;
        ((Colors *)track->colCopy)[i].blue = track->faces[i].blue;
    }
}


void WADInitSections(WADFILE *wad)
{
    int length;

    length = wad->sizes[wad->current];

    track->sections = (TrackSection *)DAlloc(heap, length + 32);
    if (!track->sections) {
        Error("Track.c:InitSections: Failed to allocate memory for track sections", Fatal);
    }

    readnextfile(wad, (char *)track->sections);
    track->sectionCount = length / PORT_FILE_SIZEOF_TrackSection; /* Port: the file holds 156-byte records */
    /* Port: widen the file's 32-bit TrackSection records to native ones (port_widen.h). */
    track->sections = PortWidenHeapRecords(heap, &PortRecord_TrackSection, (char *)track->sections, track->sectionCount);

    IntelSection();
    IndexToPointers();
    CheckVersion();
}


void WADInitViewList(WADFILE *wad)
{
    int          length;
    TrackSection *section;
    int          total;
    short         i;
    short         j;

    length = wad->sizes[wad->current];

    track->viewList = (short *)DAlloc(heap, length + 32);
    if (!track->viewList) {
        Error("Track.c:InitViewList: Failed to allocate memory for view list", Fatal);
    }

    readnextfile(wad, (char *)track->viewList);

    for (i = 0; i < length / 2; i++) {
        IntelShort(&track->viewList[i]);
    }

    section = track->sections;
    total = 0;
    for (i = 0; i < track->sectionCount; i++) {
        for (j = 0; j < 3; j++) {
            section->northViewSection[j] = track->viewList + total;
            total += section->northViewCount[j];

            section->southViewSection[j] = track->viewList + total;
            total += section->southViewCount[j];

            section->eastViewSection[j] = track->viewList + total;
            total += section->eastViewCount[j];

            section->westViewSection[j] = track->viewList + total;
            total += section->westViewCount[j];

            section->allViewSection[j] = track->viewList + total;
            total += section->allViewCount[j];
        }

        section++;
    }

    if ((total * 2) != length) {
        printf("%s", msgNewline); /* Port: not a literal format */
        printf("total  %d\n", total);
        printf("length %d\n", length);
        Error("track.c::InitViewList():problem with view list", Fatal);
    }
}


TrackSection *NextIndexSection(TrackSection *section)
{
    section += 1;

    if (section > track->sections + track->sectionCount - 1) {
        section = track->sections;
    }

    return section;
}


TrackSection *PrevIndexSection(TrackSection *section)
{
    section -= 1;

    if (section < track->sections) {
        section = track->sections + track->sectionCount - 1;
    }

    return section;
}


void DrawNewTrack(TrackCamera *camera, AutoMeshData *autoMesh)
{
    short i;
    short count;
    short dir;
    short loCount = 0;
    short medCount = 0;
    short hiCount = 0;

    autoMesh->triCount = 0;
    autoMesh->quadCount = 0;
    autoMesh->attemptedAutos = 0;
    GsGetVcount();
    AsmNextFrame();
    dir = TrackDirection(camera);
    if (dir == 1) {
        for (i = 0; i < camera->section->northViewCount[2]; i += 2) {
            count = camera->section->northViewSection[2][i + 1];
            if (count > 100) {
                count = 100;
            }
            AsmTrackLo(track->sections + camera->section->northViewSection[2][i], camera->camPos, count);
            loCount += camera->section->northViewSection[2][i + 1];
        }

        for (i = 0; i < camera->section->northViewCount[1]; i += 2) {
            AsmTrackMed(track->sections + camera->section->northViewSection[1][i], camera->camPos, camera->section->northViewSection[1][i + 1]);
            medCount += camera->section->northViewSection[1][i + 1];
        }

        for (i = 0; i < camera->section->northViewCount[0]; i += 2) {
            AsmTrackHi(track->sections + camera->section->northViewSection[0][i], camera->camPos, autoMesh, camera->section->northViewSection[0][i + 1]);
            hiCount += camera->section->northViewSection[0][i + 1];
        }

        if (drawCracks) {
            GsGetVcount();
            if (camera->section->med[0] != (-1)) {
                FillMedLoCrack(track->sections + camera->section->med[0], camera->camPos);
            }
            if (camera->section->high[0] != (-1)) {
                FillHiMedCrack(track->sections + camera->section->high[0], camera->camPos);
            }
        }
    } else if (dir == 2) {
        for (i = 0; i < camera->section->southViewCount[2]; i += 2) {
            count = camera->section->southViewSection[2][i + 1];
            if (count > 100) {
                count = 100;
            }
            AsmTrackLo(track->sections + camera->section->southViewSection[2][i], camera->camPos, count);
            loCount += camera->section->southViewSection[2][i + 1];
        }

        for (i = 0; i < camera->section->southViewCount[1]; i += 2) {
            AsmTrackMed(track->sections + camera->section->southViewSection[1][i], camera->camPos, camera->section->southViewSection[1][i + 1]);
            medCount += camera->section->southViewSection[1][i + 1];
        }

        for (i = 0; i < camera->section->southViewCount[0]; i += 2) {
            AsmTrackHi(track->sections + camera->section->southViewSection[0][i], camera->camPos, autoMesh, camera->section->southViewSection[0][i + 1]);
            hiCount += camera->section->southViewSection[0][i + 1];
        }

        if (drawCracks) {
            GsGetVcount();
            if (camera->section->med[1] != (-1)) {
                FillMedLoCrack(track->sections + camera->section->med[1], camera->camPos);
            }
            if (camera->section->high[1] != (-1)) {
                FillHiMedCrack(track->sections + camera->section->high[1], camera->camPos);
            }
        }
    } else if (dir == 4) {
        for (i = 0; i < camera->section->westViewCount[2]; i += 2) {
            count = camera->section->westViewSection[2][i + 1];
            if (count > 100) {
                count = 100;
            }
            AsmTrackLo(track->sections + camera->section->westViewSection[2][i], camera->camPos, count);
            loCount += camera->section->westViewSection[2][i + 1];
        }

        for (i = 0; i < camera->section->westViewCount[1]; i += 2) {
            AsmTrackMed(track->sections + camera->section->westViewSection[1][i], camera->camPos, camera->section->westViewSection[1][i + 1]);
            medCount += camera->section->westViewSection[1][i + 1];
        }

        for (i = 0; i < camera->section->westViewCount[0]; i += 2) {
            AsmTrackHi(track->sections + camera->section->westViewSection[0][i], camera->camPos, autoMesh, camera->section->westViewSection[0][i + 1]);
            hiCount += camera->section->westViewSection[0][i + 1];
        }
    } else if (dir == 8) {
        for (i = 0; i < camera->section->eastViewCount[2]; i += 2) {
            count = camera->section->eastViewSection[2][i + 1];
            if (count > 100) {
                count = 100;
            }
            AsmTrackLo(track->sections + camera->section->eastViewSection[2][i], camera->camPos, count);
            loCount += camera->section->eastViewSection[2][i + 1];
        }

        for (i = 0; i < camera->section->eastViewCount[1]; i += 2) {
            AsmTrackMed(track->sections + camera->section->eastViewSection[1][i], camera->camPos, camera->section->eastViewSection[1][i + 1]);
            medCount += camera->section->eastViewSection[1][i + 1];
        }

        for (i = 0; i < camera->section->eastViewCount[0]; i += 2) {
            AsmTrackHi(track->sections + camera->section->eastViewSection[0][i], camera->camPos, autoMesh, camera->section->eastViewSection[0][i + 1]);
            hiCount += camera->section->eastViewSection[0][i + 1];
        }
    }
}


void DrawAllTrack(TrackCamera *camera, AutoMeshData *autoMesh)
{
    short i;
    short loCount = 0;
    short medCount = 0;
    short hiCount = 0;

    autoMesh->triCount = 0;
    autoMesh->quadCount = 0;
    autoMesh->attemptedAutos = 0;
    AsmNextFrame();
    for (i = 0; i < camera->section->allViewCount[2]; i += 2) {
        AsmTrackLo(track->sections + camera->section->allViewSection[2][i], camera->camPos, camera->section->allViewSection[2][i + 1]);
        loCount += camera->section->allViewSection[2][i + 1];
    }

    for (i = 0; i < camera->section->allViewCount[1]; i += 2) {
        AsmTrackMed(track->sections + camera->section->allViewSection[1][i], camera->camPos, camera->section->allViewSection[1][i + 1]);
        medCount += camera->section->allViewSection[1][i + 1];
    }

    for (i = 0; i < camera->section->allViewCount[0]; i += 2) {
        AsmTrackHi(track->sections + camera->section->allViewSection[0][i], camera->camPos, autoMesh, camera->section->allViewSection[0][i + 1]);
        hiCount += camera->section->allViewSection[0][i + 1];
    }
}


void CheckCracks(void)
{
    TrackSection *section;
    short         i;

    section = track->sections;
    for (i = 0; i < track->sectionCount; i++) {
        if ((section->high[0] < 0) || (section->high[0] >= track->sectionCount)) {
            printf("section %3d - forward high of %d incorrect\n", i, section->high[0]);
        }
        if ((section->med[0] < 0) || (section->med[0] >= track->sectionCount)) {
            printf("section %3d - forward med of %d incorrect\n", i, section->med[0]);
        }
        if ((section->high[1] < 0) || (section->high[1] >= track->sectionCount)) {
            printf("section %3d - backward high of %d incorrect\n", i, section->high[1]);
        }
        if ((section->med[1] < 0) || (section->med[1] >= track->sectionCount)) {
            printf("section %3d - backward med of %d incorrect\n", i, section->med[1]);
        }
        section++;
    }
}
