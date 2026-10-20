/* Native side of the record tests (see README): widens and narrows files with the game's own
   conversion code and writes the results for run.py to check against an independent reader of the
   32-bit layout.

     recordtest widen-prm IN OUT      PRM image -> widened image; walks it with the real native types
     recordtest widen-records TABLE IN OUT
                                      IN as an array of 32-bit records of the table's type -> native records
     recordtest roundtrip TABLE IN    narrow(widen(IN)) == IN
     recordtest roundtrip-prm IN      the same for the Object and Skeleton records of a PRM image

   Built with the game's own port_widen.c, port_records.c and malloc.c. */
#include "functions.h"
#include "scene.h"
#include "port_widen.h"
#include <stdint.h>

/* What the game sources expect from the rest of the program. */
char errorString[256];
char msgInsufficientMemory[] = "Insufficient memory\n";
char msgDFreeNotFound[] = "DFree: block not found\n";
char msgAllocRequest[] = "Alloc request %d, free %d\n";
HeapBlock *mainheapcopy;
int lowTideMark;      /* malloc.c keeps it; the data files define it */

void Error(char *text, short level)
{
    fprintf(stderr, "Error: %s\n", text);
    exit(2);
}

static unsigned char *ReadAll(const char *path, int *length)
{
    FILE *file = fopen(path, "rb");
    unsigned char *data;
    long size;

    if (!file) {
        fprintf(stderr, "cannot open %s\n", path);
        exit(2);
    }
    fseek(file, 0, SEEK_END);
    size = ftell(file);
    fseek(file, 0, SEEK_SET);
    data = malloc(size + 32);
    if (fread(data, 1, size, file) != (size_t)size) {
        fprintf(stderr, "cannot read %s\n", path);
        exit(2);
    }
    fclose(file);
    *length = (int)size;
    return data;
}

static void WriteAll(const char *path, const void *data, int length)
{
    FILE *file = fopen(path, "wb");

    if (!file || fwrite(data, 1, length, file) != (size_t)length) {
        fprintf(stderr, "cannot write %s\n", path);
        exit(2);
    }
    fclose(file);
}

static const PortRecordTable *FindTable(const char *name)
{
    unsigned i;

    for (i = 0; i < PortRecordTableCount; i++) {
        if (strcmp(PortRecordTables[i]->name, name) == 0)
            return PortRecordTables[i];
    }
    fprintf(stderr, "no record table %s\n", name);
    exit(2);
}

/* Zeroes the bytes of one file-layout record that no field covers (padding). Returns how many of
   them were not zero. */
static int MaskPadding(const PortRecordTable *table, unsigned char *record)
{
    unsigned char *covered = malloc(table->fileSize);
    unsigned f, i;
    int junk = 0;

    memset(covered, 0, table->fileSize);
    for (f = 0; f < table->fieldCount; f++)
        memset(covered + table->fields[f].fileOffset, 1, table->fields[f].fileSize);
    for (i = 0; i < table->fileSize; i++) {
        if (!covered[i]) {
            junk += record[i] != 0;
            record[i] = 0;
        }
    }
    free(covered);
    return junk;
}

static int Swap16(int value)
{
    return (short)(((value & 0xff) << 8) | ((value >> 8) & 0xff));
}

/* The native size of a primitive of the given file type, 0 if unknown. */
static int PrimitiveSizeOf(int type)
{
    switch (type) {
    case TypeF3: return sizeof(F3);
    case TypeFT3: return sizeof(FT3);
    case TypeF4: return sizeof(F4);
    case TypeFT4: return sizeof(FT4);
    case TypeG3: return sizeof(G3);
    case TypeGT3: return sizeof(GT3);
    case TypeG4: return sizeof(G4);
    case TypeGT4: return sizeof(GT4);
    case TypeTSPR:
    case TypeBSPR: return sizeof(SPR);
    case TypeLSF3: return sizeof(LSF3);
    case TypeLSFT3: return sizeof(LSFT3);
    case TypeLSF4: return sizeof(LSF4);
    case TypeLSFT4: return sizeof(LSFT4);
    case TypeLSG3: return sizeof(LSG3);
    case TypeLSGT3: return sizeof(LSGT3);
    case TypeLSG4: return sizeof(LSG4);
    case TypeLSGT4: return sizeof(LSGT4);
    case TypeSpline: return sizeof(Spline);
    case TypeInfiniteLight: return sizeof(InfiniteLight);
    case TypePointLight: return sizeof(PointLight);
    case TypeSpotLight: return sizeof(SpotLight);
    }
    return 0;
}

/* The walk of LoadPrmCommon, with the real native types, over a widened image whose fields are
   still in file byte order (but with bounds checks, which the game's loop does not have). Returns
   the number of objects, or a negative code. */
static int WalkNative(char *image, int length, int *misaligned)
{
    Prm prm;
    char *finish = image + length;
    int objects = 0;
    int i;

    prm.ptr = image;
    *misaligned = 0;
    while (finish > prm.ptr) {
        int vertexCount, primitiveCount;

        prm.ptr = PortAlignPrm(image, prm.ptr);
        if (finish - prm.ptr < (int)(sizeof(Object) + sizeof(Skeleton)))
            return -1;
        if ((uintptr_t)prm.ptr % _Alignof(Object))
            (*misaligned)++;
        vertexCount = Swap16(prm.object->vertexCount);
        primitiveCount = Swap16(prm.object->primitiveCount);
        prm.object++;
        if ((uintptr_t)prm.ptr % _Alignof(Skeleton))
            (*misaligned)++;
        prm.skeleton++;
        if (vertexCount < 0 || vertexCount > (finish - prm.ptr) / (int)sizeof(SVECTOR))
            return -1;
        prm.svector += vertexCount;
        for (i = 0; i < primitiveCount; i++) {
            int size;

            if (finish - prm.ptr < 2)
                return -2;
            size = PrimitiveSizeOf(Swap16(prm.f3->type));
            if (!size)
                return -3;
            if (size > finish - prm.ptr)
                return -2;
            prm.ptr += size;
        }
        objects++;
    }
    return prm.ptr == finish ? objects : -2;
}

static int CmdWidenPrm(const char *in, const char *out)
{
    int length, widened, objects, misaligned;
    unsigned char *file = ReadAll(in, &length);
    char *image;

    widened = PortWidenPrm((char *)file, length, 0);
    image = aligned_alloc(16, (widened + 32 + 15) & ~15);
    if (PortWidenPrm((char *)file, length, image) != widened) {
        printf("FAIL measured and written lengths differ\n");
        return 1;
    }
    WriteAll(out, image, widened);
    if (widened == length && !memcmp(image, file, length) && WalkNative(image, widened, &misaligned) < 0) {
        printf("prm file %d is not a PRM image: copied unchanged\n", length);
        return 0;
    }
    objects = WalkNative(image, widened, &misaligned);
    printf("prm file %d widened %d objects %d misaligned %d\n", length, widened, objects, misaligned);
    return objects < 0 || misaligned ? 1 : 0;
}

static int CmdWidenRecords(const char *tableName, const char *in, const char *out)
{
    const PortRecordTable *table = FindTable(tableName);
    int length, count;
    unsigned char *file = ReadAll(in, &length);
    unsigned char *widened;

    if (length % table->fileSize) {
        printf("FAIL %s: length %d is not a multiple of %u\n", in, length, table->fileSize);
        return 1;
    }
    count = length / table->fileSize;
    widened = malloc(count * table->nativeSize + 1);
    PortWidenRecords(table, file, count, widened);
    WriteAll(out, widened, count * table->nativeSize);
    printf("%s records %d file %u native %u\n", table->name, count, table->fileSize, table->nativeSize);
    return 0;
}

static int CmdRoundTrip(const char *tableName, const char *in)
{
    const PortRecordTable *table = FindTable(tableName);
    int length, count;
    unsigned char *file = ReadAll(in, &length);
    unsigned char *widened, *narrowed;
    int i, junk = 0;

    if (length % table->fileSize) {
        printf("FAIL %s: length %d is not a multiple of %u\n", in, length, table->fileSize);
        return 1;
    }
    count = length / table->fileSize;
    widened = malloc(count * table->nativeSize + 1);
    narrowed = malloc(length + 1);
    PortWidenRecords(table, file, count, widened);
    PortNarrowRecords(table, widened, count, narrowed);
    for (i = 0; i < count; i++)
        junk += MaskPadding(table, file + i * table->fileSize);
    if (memcmp(file, narrowed, length) != 0) {
        printf("FAIL narrow(widen(%s)) differs from the file\n", in);
        return 1;
    }
    printf("roundtrip %s %d records ok (padding bytes that were not zero: %d)\n", table->name, count, junk);
    return 0;
}

/* narrow(widen) for the Object and Skeleton records of a PRM image, walking file and image together. */
static int CmdRoundTripPrm(const char *in)
{
    int length, widened;
    unsigned char *file = ReadAll(in, &length);
    char *image;
    int fp = 0, ip = 0, objects = 0, junk = 0;
    unsigned char narrowed[256];
    const PortRecordTable *objectTable = &PortRecord_Object;
    const PortRecordTable *skeletonTable = &PortRecord_Skeleton;

    widened = PortWidenPrm((char *)file, length, 0);
    image = aligned_alloc(16, (widened + 32 + 15) & ~15);
    PortWidenPrm((char *)file, length, image);
    if (WalkNative(image, widened, &junk) < 0) {
        printf("roundtrip-prm: not a PRM image, skipped\n");
        return 0;
    }
    junk = 0;
    while (fp < length) {
        int vertexCount, primitiveCount, rest;
        Prm prm;

        ip = PortAlignPrm(image, image + ip) - image;
        PortNarrowRecords(objectTable, image + ip, 1, narrowed);
        junk += MaskPadding(objectTable, file + fp);
        if (memcmp(narrowed, file + fp, objectTable->fileSize)) {
            printf("FAIL object %d: narrow(widen) differs\n", objects);
            return 1;
        }
        PortNarrowRecords(skeletonTable, image + ip + objectTable->nativeSize, 1, narrowed);
        junk += MaskPadding(skeletonTable, file + fp + objectTable->fileSize);
        if (memcmp(narrowed, file + fp + objectTable->fileSize, skeletonTable->fileSize)) {
            printf("FAIL skeleton %d: narrow(widen) differs\n", objects);
            return 1;
        }
        vertexCount = (short)((file[fp + 16] << 8) | file[fp + 17]);
        primitiveCount = (short)((file[fp + 32] << 8) | file[fp + 33]);
        /* Everything after the two records is copied verbatim up to the next object. */
        fp += objectTable->fileSize + skeletonTable->fileSize;
        ip += objectTable->nativeSize + skeletonTable->nativeSize;
        rest = vertexCount * 8;
        prm.ptr = image + ip + rest;
        {
            int i;
            for (i = 0; i < primitiveCount; i++) {
                int size = PrimitiveSizeOf(Swap16(prm.f3->type));
                if (!size) {
                    printf("FAIL object %d: unknown primitive\n", objects);
                    return 1;
                }
                rest += size;
                prm.ptr += size;
            }
        }
        if (memcmp(image + ip, file + fp, rest)) {
            printf("FAIL object %d: vertices and primitives are not verbatim\n", objects);
            return 1;
        }
        fp += rest;
        ip += rest;
        objects++;
    }
    printf("roundtrip-prm %d objects ok (padding bytes that were not zero: %d)\n", objects, junk);
    return 0;
}

int main(int argc, char **argv)
{
    if (argc == 4 && !strcmp(argv[1], "widen-prm"))
        return CmdWidenPrm(argv[2], argv[3]);
    if (argc == 5 && !strcmp(argv[1], "widen-records"))
        return CmdWidenRecords(argv[2], argv[3], argv[4]);
    if (argc == 4 && !strcmp(argv[1], "roundtrip"))
        return CmdRoundTrip(argv[2], argv[3]);
    if (argc == 3 && !strcmp(argv[1], "roundtrip-prm"))
        return CmdRoundTripPrm(argv[2]);
    fprintf(stderr, "usage: see tools/recordtest/README\n");
    return 2;
}
