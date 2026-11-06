/* Backend migration must preserve logical resources and pending readbacks, and
   must leave the previous renderer usable when creating a new device fails. */
#define PORT_HAVE_METAL 1
#include "../../platform/render/state.c"
#include <assert.h>
#include <math.h>

typedef struct FakeDevice {
    int api;
} FakeDevice;
typedef struct FakeImage {
    RenderImageDesc desc;
    int api;
    uint8_t *pixels;
} FakeImage;
typedef struct FakeBuffer {
    RenderBufferKind kind;
    int api;
    void *pixels;
} FakeBuffer;
typedef struct FakeRead {
    int api;
    uint8_t *pixels;
} FakeRead;
static int liveImages, liveBuffers, liveDevices, liveReads, failImageAfter, metalAvailable = 1;
static int CreateFake(RenderDevice *d, int api) {
    FakeDevice *f = calloc(1, sizeof(*f));
    assert(f);
    f->api = api;
    d->data = f;
    d->caps.maxTextureSize = 16384;
    d->caps.maxArrayLayers = 2048;
    d->caps.asyncReadback = 1;
    SDL_strlcpy(d->caps.deviceName, "test device", sizeof(d->caps.deviceName));
    liveDevices++;
    return 1;
}
static int CreateGL(RenderDevice *d, void *w) { return CreateFake(d, 1); }
static int CreateMetal(RenderDevice *d, void *w) { return CreateFake(d, 2); }
static int Api(RenderDevice *d) { return ((FakeDevice *)d->data)->api; }
static void DestroyFake(RenderDevice *d) {
    free(d->data);
    d->data = NULL;
    liveDevices--;
}
static uintptr_t ImageFake(RenderDevice *d, RenderImageDesc desc) {
    if (failImageAfter && !--failImageAfter) {
        SDL_SetError("injected image failure");
        return 0;
    }
    FakeImage *im = calloc(1, sizeof(*im));
    assert(im);
    im->desc = desc;
    im->api = Api(d);
    im->pixels = calloc(1, ImageBytes(desc));
    assert(im->pixels);
    liveImages++;
    return (uintptr_t)im;
}
static void ReleaseImageFake(RenderDevice *d, uintptr_t p) {
    FakeImage *im = (FakeImage *)p;
    assert(im->api == Api(d));
    free(im->pixels);
    free(im);
    liveImages--;
}
static int UploadFake(RenderDevice *d, uintptr_t p, RenderRect r, int layer, const void *pixels,
                      size_t row) {
    FakeImage *im = (FakeImage *)p;
    assert(im->api == Api(d));
    size_t bpp = PixelBytes(im->desc.format);
    for (int y = 0; y < r.h; y++)
        memcpy(im->pixels +
                   (((size_t)layer * im->desc.height + r.y + y) * im->desc.width + r.x) * bpp,
               (const uint8_t *)pixels + y * row, (size_t)r.w * bpp);
    return 1;
}
static int ReadFake(RenderDevice *d, RenderTarget t, RenderRect r, RenderFormat format,
                    void *pixels) {
    FakeImage *im = (FakeImage *)(format == RENDER_DEPTH ? t.depth : t.colour);
    assert(im && im->api == Api(d));
    size_t bpp = PixelBytes(format);
    for (int y = 0; y < r.h; y++)
        memcpy((uint8_t *)pixels + (size_t)y * r.w * bpp,
               im->pixels + ((size_t)(r.y + y) * im->desc.width + r.x) * bpp, (size_t)r.w * bpp);
    return 1;
}
static void MipsFake(RenderDevice *d, uintptr_t p) { assert(((FakeImage *)p)->api == Api(d)); }
static uintptr_t BufferFake(RenderDevice *d, const void *p, size_t size, RenderBufferKind kind) {
    FakeBuffer *b = calloc(1, sizeof(*b));
    assert(b);
    b->api = Api(d);
    b->kind = kind;
    b->pixels = malloc(size);
    assert(b->pixels);
    memcpy(b->pixels, p, size);
    liveBuffers++;
    return (uintptr_t)b;
}
static void ReleaseBufferFake(RenderDevice *d, uintptr_t p) {
    FakeBuffer *b = (FakeBuffer *)p;
    assert(b->api == Api(d));
    free(b->pixels);
    free(b);
    liveBuffers--;
}
static int WriteBufferFake(RenderDevice *d, uintptr_t id, const void *pixels, size_t size,
                           RenderBufferKind kind) {
    FakeBuffer *b = (FakeBuffer *)id;
    assert(b->api == Api(d) && b->kind == kind);
    b->pixels = realloc(b->pixels, size);
    assert(b->pixels);
    memcpy(b->pixels, pixels, size);
    return 1;
}
static int ClearFake(RenderDevice *d, const RenderClear *c) {
    FakeImage *im = (FakeImage *)c->target.colour;
    assert(im && im->api == Api(d));
    for (int p = 0; p < im->desc.width * im->desc.height; p++)
        for (int i = 0; i < 4; i++)
            if (c->colourMask & (1 << i))
                im->pixels[p * 4 + i] = (uint8_t)lroundf(c->rgba[i] * 255);
    return 1;
}
static int DrawFake(RenderDevice *d, const RenderDraw *c) {
    assert(((FakeImage *)c->target.colour)->api == Api(d));
    assert(((FakeBuffer *)c->vertices)->api == Api(d));
    assert(c->uniforms.pass == 2);
    return 1;
}
static int CopyFake(RenderDevice *d, const RenderCopy *c) { return 1; }
static void FinishFake(RenderDevice *d) {}
static int PresentFake(RenderDevice *d) { return 1; }
static void ActivateFake(RenderDevice *d, int on) {}
static uintptr_t BeginReadFake(RenderDevice *d, RenderTarget target, RenderRect r) {
    FakeRead *read = calloc(1, sizeof(*read));
    assert(read);
    read->api = Api(d);
    read->pixels = malloc((size_t)r.w * r.h * 4);
    assert(read->pixels);
    assert(ReadFake(d, target, r, RENDER_RGBA8, read->pixels));
    liveReads++;
    return (uintptr_t)read;
}
static int PollReadFake(RenderDevice *d, uintptr_t id, int wait, const uint8_t **pixels) {
    FakeRead *r = (FakeRead *)id;
    assert(r->api == Api(d));
    if (!wait)
        return 0;
    *pixels = r->pixels;
    return 1;
}
static void ReleaseReadFake(RenderDevice *d, uintptr_t id) {
    FakeRead *r = (FakeRead *)id;
    assert(r->api == Api(d));
    free(r->pixels);
    free(r);
    liveReads--;
}
const RenderDeviceOps *RenderOpenGLDevice(void) {
    static const RenderDeviceOps ops = {
        CreateGL,      DestroyFake,  ImageFake,       ReleaseImageFake,  UploadFake,
        ReadFake,      MipsFake,     BufferFake,      ReleaseBufferFake, ClearFake,
        DrawFake,      CopyFake,     FinishFake,      PresentFake,       ActivateFake,
        BeginReadFake, PollReadFake, ReleaseReadFake, WriteBufferFake};
    return &ops;
}
const RenderDeviceOps *RenderMetalDevice(void) {
    static const RenderDeviceOps ops = {
        CreateMetal,   DestroyFake,  ImageFake,      ReleaseImageFake,  UploadFake,
        ReadFake,      MipsFake,     BufferFake,     ReleaseBufferFake, ClearFake,
        DrawFake,      CopyFake,     FinishFake,     PresentFake,       ActivateFake,
        BeginReadFake, PollReadFake, ReleaseReadFake};
    return &ops;
}
int RenderMetalAvailable(void) { return metalAvailable; }
static int usedRead;
static void GotRead(const uint8_t *p, int w, int h, void *user) {
    assert(w == 4 && h == 3 && p[0] == 7 && p[1] == 13 && p[2] == 211 && p[3] == 255);
    usedRead++;
}
static void CheckPicture(void) {
    uint8_t pixels[4 * 3 * 4];
    RgReadPixels(0, 0, 4, 3, RG_RGBA, RG_UNSIGNED_BYTE, pixels);
    GotRead(pixels, 4, 3, NULL);
    usedRead--;
}
int main(void) {
    assert(RenderStart(NULL));
    metalAvailable = 0;
    assert(!RenderBackendAvailable(RENDER_METAL));
    assert(!RenderRequestBackend(RENDER_METAL));
    metalAvailable = 1;
    RgHandle image, target, buffer;
    RgGenTextures(1, &image);
    RgBindTexture(RG_TEXTURE_2D, image);
    RgTexImage2D(RG_TEXTURE_2D, 0, RG_RGBA8, 4, 3, 0, RG_RGBA, RG_UNSIGNED_BYTE, NULL);
    RgGenFramebuffers(1, &target);
    RgBindFramebuffer(RG_FRAMEBUFFER, target);
    RgFramebufferTexture2D(RG_FRAMEBUFFER, RG_COLOR_ATTACHMENT0, RG_TEXTURE_2D, image, 0);
    RgClearColor(7 / 255.0f, 13 / 255.0f, 211 / 255.0f, 1);
    RgClear(RG_COLOR_BUFFER_BIT);
    RgHandle immutable;
    RgGenTextures(1, &immutable);
    RgBindTexture(RG_TEXTURE_2D_ARRAY, immutable);
    uint16_t texels[] = {1, 2, 3, 4, 5, 6, 7, 8};
    RgPixelStorei(RG_UNPACK_ALIGNMENT, 1);
    RgTexImage3D(RG_TEXTURE_2D_ARRAY, 0, RG_R16UI, 2, 2, 2, 0, RG_RED_INTEGER, RG_UNSIGNED_SHORT,
                 texels);
    RgGenBuffers(1, &buffer);
    RgBindBuffer(RG_ARRAY_BUFFER, buffer);
    RenderVertex verts[3] = {{0}};
    RgBufferData(RG_ARRAY_BUFFER, sizeof(verts), verts, RG_STREAM_DRAW);
    uintptr_t initialBuffer = buffers[buffer].native;
    verts[1].x = 17;
    RgBufferData(RG_ARRAY_BUFFER, sizeof(verts), verts, RG_STREAM_DRAW);
    assert(buffers[buffer].native == initialBuffer);
    RgHandle indexBuffer;
    RgGenBuffers(1, &indexBuffer);
    RgBindBuffer(RG_ELEMENT_ARRAY_BUFFER, indexBuffer);
    uint16_t indices[] = {0, 1, 2};
    RgBufferData(RG_ELEMENT_ARRAY_BUFFER, sizeof(indices), indices, RG_STREAM_DRAW);
    RgHandle shader = RgProgram(RENDER_GAME);
    RgUseProgram(shader);
    RgUniform1i(RgGetUniformLocation(shader, "pass"), 2);
    assert(RenderRequestBackend(RENDER_METAL));
    failImageAfter = 2;
    assert(!RenderApplyBackend());
    assert(RenderActiveBackend() == RENDER_OPENGL);
    assert(liveDevices == 1 && liveImages == 2 && liveBuffers == 2);
    CheckPicture();
    assert(RgReadbackBegin(0, 4, 3));
    assert(RgReadbackBusy(0));
    assert(!RgReadbackDone(0, 0, GotRead, NULL));
    assert(RenderRequestBackend(RENDER_METAL));
    assert(RenderApplyBackend());
    assert(RenderActiveBackend() == RENDER_METAL);
    assert(liveDevices == 1 && liveImages == 2 && liveBuffers == 2 && !liveReads);
    assert(RgReadbackBusy(0));
    assert(RgReadbackDone(0, 0, GotRead, NULL));
    assert(usedRead == 1);
    CheckPicture();
    FakeImage *m = (FakeImage *)images[immutable].native;
    assert(!memcmp(m->pixels, texels, sizeof(texels)));
    FakeBuffer *ib = (FakeBuffer *)buffers[indexBuffer].native;
    assert(ib->kind == RENDER_INDEX_BUFFER && !memcmp(ib->pixels, indices, sizeof(indices)));
    FakeBuffer *vb = (FakeBuffer *)buffers[buffer].native;
    assert(!memcmp(vb->pixels, verts, sizeof(verts)));
    RgDrawArrays(RG_TRIANGLES, 0, 3);
    for (int i = 0; i < 8; i++) {
        assert(RenderRequestBackend(i & 1 ? RENDER_METAL : RENDER_OPENGL));
        assert(RenderApplyBackend());
        CheckPicture();
        RgDrawArrays(RG_TRIANGLES, 0, 3);
    }
    assert(!RenderRequestBackend((RenderBackend)99));
    assert(RenderActiveBackend() == RENDER_METAL);
    RenderStop();
    assert(!liveDevices && !liveImages && !liveBuffers && !liveReads);
    puts("backend switches preserve images, arrays, buffers, uniforms and pending reads; failed "
         "switches "
         "roll back without leaks: OK");
}
