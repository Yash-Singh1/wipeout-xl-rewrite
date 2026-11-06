/* Shared renderer resource ownership and draw-state assembly. Backend switches
   rebuild native resources transactionally while preserving logical handles. */
#include "state.h"
#include <SDL3/SDL.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>

#define RESOURCES 4096
#define TARGETS 512
typedef struct Image {
    int used, dirty;
    RenderImageDesc desc;
    RenderSampler sampler;
    uintptr_t native;
    uint8_t *shadow;
    uint8_t **layerShadows;
} Image;
typedef struct Buffer {
    int used;
    uintptr_t native;
    void *shadow;
    size_t size;
    RenderBufferKind kind;
} Buffer;
typedef struct Target {
    int used;
    RgHandle colour, depth;
} Target;
static Image images[RESOURCES];
static Buffer buffers[RESOURCES];
static Target targets[TARGETS];
typedef struct Readback {
    int busy, width, height;
    uintptr_t native;
    uint8_t *cpu;
} Readback;
static Readback readbacks[2];
static int DrainReadbacks(void);
static RenderDevice device;
static void *renderWindow;
static RenderBackend backend, requested;
static int started;
static char error[256];
static RgHandle textureUnits[5][2], readTarget, drawTarget, vertexBuffer, packBuffer, depthImage,
    indexBuffer;
static int activeUnit, unpackAlignment = 4, unpackRowLength, program;
static RenderDraw state;
static RenderUniforms uniforms[RENDER_SHADER_COUNT];
static float clearColour[4];
static void Failed(const char *what) {
    SDL_snprintf(error, sizeof(error), "%s: %s", what, SDL_GetError());
    SDL_Log("renderer: %s", error);
}
static size_t PixelBytes(RenderFormat f) { return f == RENDER_R16_UINT ? 2 : 4; }
static size_t ImageBytes(RenderImageDesc d) {
    return (size_t)d.width * d.height * d.layers * PixelBytes(d.format);
}
static void FreeShadow(Image *im) {
    SDL_free(im->shadow);
    im->shadow = NULL;
    if (im->layerShadows) {
        for (int i = 0; i < im->desc.layers; i++) SDL_free(im->layerShadows[i]);
        SDL_free(im->layerShadows);
        im->layerShadows = NULL;
    }
}
static RenderTarget TargetOf(RgHandle id) {
    if (!id)
        return (RenderTarget){0, 0, 1};
    Target *t = &targets[id];
    return (RenderTarget){images[t->colour].native, images[t->depth].native, 0};
}
static void Dirty(RgHandle id, int depth) {
    if (id) {
        RgHandle im = depth ? targets[id].depth : targets[id].colour;
        if (im)
            images[im].dirty = 1;
    }
}
static int Slot(const void *data, size_t stride, int count) {
    for (int i = 1; i < count; i++)
        if (!*(const int *)((const uint8_t *)data + i * stride))
            return i;
    SDL_Log("renderer resource table exhausted");
    abort();
}
const RenderDevice *RenderActiveDevice(void) { return &device; }
RenderBackend RenderActiveBackend(void) { return backend; }
typedef struct BackendEntry {
    RenderBackend id;
    const char *name;
    const RenderDeviceOps *(*factory)(void);
    int (*available)(void);
} BackendEntry;
static const BackendEntry backends[] = {
#ifdef __EMSCRIPTEN__
    {RENDER_WEBGL, "WebGL 2", RenderOpenGLDevice, NULL},
#else
    {RENDER_OPENGL, "OpenGL", RenderOpenGLDevice, NULL},
#ifdef PORT_HAVE_METAL
    {RENDER_METAL, "Metal", RenderMetalDevice, RenderMetalAvailable},
#endif
#endif
};
static const BackendEntry *Backend(RenderBackend id) {
    for (size_t i = 0; i < sizeof(backends) / sizeof(*backends); i++)
        if (backends[i].id == id)
            return &backends[i];
    return NULL;
}
const char *RenderBackendName(RenderBackend id) {
    const BackendEntry *b = Backend(id);
    return b ? b->name : "Unavailable";
}
int RenderBackendAvailable(RenderBackend id) {
    const BackendEntry *b = Backend(id);
    return b && (!b->available || b->available());
}
static const RenderDeviceOps *Ops(RenderBackend id) {
    const BackendEntry *b = Backend(id);
    return b ? b->factory() : NULL;
}
RenderBackend RenderDefaultBackend(void) { return backends[0].id; }
int RenderStart(void *window) { return RenderStartBackend(window, RenderDefaultBackend()); }
int RenderStartBackend(void *window, RenderBackend selected) {
    if (started)
        return 1;
    renderWindow = window;
    if (!RenderBackendAvailable(selected)) {
        SDL_strlcpy(error, "That renderer is unavailable on this system", sizeof(error));
        return 0;
    }
    backend = selected;
    requested = backend;
    device.ops = Ops(backend);
    device.vsync = 1;
    if (!device.ops->create(&device, window)) {
        Failed("initialization failed");
        if (device.data)
            device.ops->destroy(&device);
        return 0;
    }
    state.pipeline.colourMask = 15;
    state.uniforms.depthPull = 1;
    device.ops->activate(&device, 1);
    started = 1;
    SDL_Log("renderer: %s on %s", RenderBackendName(backend), device.caps.deviceName);
    return 1;
}
int RgInit(void) { return started || RenderStart(NULL); }
const char *RenderBackendError(void) { return error; }
int RenderRequestBackend(RenderBackend b) {
    if (!RenderBackendAvailable(b)) {
        SDL_strlcpy(error, "That renderer is unavailable on this system", sizeof(error));
        return 0;
    }
    requested = b;
    error[0] = 0;
    return 1;
}
int RenderFindBackend(const char *name, RenderBackend *backend) {
    for (size_t i = 0; i < sizeof(backends) / sizeof(*backends); i++) {
        if (!SDL_strcasecmp(name, backends[i].name) ||
            (backends[i].id == RENDER_WEBGL && !SDL_strcasecmp(name, "webgl"))) {
            *backend = backends[i].id;
            return 1;
        }
    }
    return 0;
}
int RenderRequestBackendName(const char *name) {
    RenderBackend backend;
    if (RenderFindBackend(name, &backend))
        return RenderRequestBackend(backend);
    SDL_strlcpy(error, "That renderer is unavailable on this system", sizeof(error));
    return 0;
}

/* Resources remain owned by the old device until every new allocation and
   upload succeeds. Failed switches leave the running renderer intact. */
int RenderApplyBackend(void) {
    if (requested == backend)
        return 1;
    RenderDevice next = {0};
    next.ops = Ops(requested);
    next.vsync = device.vsync;
    uintptr_t *newImages = SDL_calloc(RESOURCES, sizeof(uintptr_t));
    uintptr_t *newBuffers = SDL_calloc(RESOURCES, sizeof(uintptr_t));
    int ok = 0;
    if (!newImages || !newBuffers)
        goto done;
    if (!DrainReadbacks())
        goto done;
    device.ops->finish(&device);
    if (!next.ops->create(&next, renderWindow))
        goto done;
    for (int i = 1; i < RESOURCES; i++) {
        Image *im = &images[i];
        if (!im->used || !im->native)
            continue;
        if (im->desc.width > next.caps.maxTextureSize ||
            im->desc.height > next.caps.maxTextureSize ||
            im->desc.layers > next.caps.maxArrayLayers) {
            SDL_SetError("Current resolution exceeds the new device's limits");
            goto done;
        }
        newImages[i] = next.ops->image(&next, im->desc);
        if (!newImages[i])
            goto done;
        uint8_t *pixels = im->shadow;
        int temporary = 0;
        if (im->dirty) {
            /* Only rendered images need a GPU snapshot; immutable resources
               retain their original CPU uploads. Depth transfers use floats. */
            pixels = SDL_malloc(ImageBytes(im->desc));
            temporary = 1;
            if (!pixels)
                goto done;
            RenderTarget t = {0};
            if (im->desc.format == RENDER_DEPTH)
                t.depth = im->native;
            else
                t.colour = im->native;
            if (!device.ops->read(&device, t, (RenderRect){0, 0, im->desc.width, im->desc.height},
                                  im->desc.format, pixels)) {
                SDL_free(pixels);
                goto done;
            }
        }
        if (pixels) {
            size_t row = (size_t)im->desc.width * PixelBytes(im->desc.format),
                   layer = row * im->desc.height;
            for (int z = 0; z < im->desc.layers; z++)
                if (!next.ops->upload(&next, newImages[i],
                                      (RenderRect){0, 0, im->desc.width, im->desc.height}, z,
                                      pixels + layer * z, row)) {
                    if (temporary)
                        SDL_free(pixels);
                    goto done;
                }
        }
        if (!pixels && im->layerShadows) {
            size_t row = (size_t)im->desc.width * PixelBytes(im->desc.format);
            for (int z = 0; z < im->desc.layers; z++)
                if (im->layerShadows[z] && !next.ops->upload(&next, newImages[i],
                    (RenderRect){0, 0, im->desc.width, im->desc.height}, z, im->layerShadows[z], row))
                    goto done;
        }
        if (temporary)
            SDL_free(pixels);
        if (im->desc.mipmaps)
            next.ops->mipmaps(&next, newImages[i]);
    }
    for (int i = 1; i < RESOURCES; i++)
        if (buffers[i].used && buffers[i].size) {
            newBuffers[i] =
                next.ops->buffer(&next, buffers[i].shadow, buffers[i].size, buffers[i].kind);
            if (!newBuffers[i])
                goto done;
        }
    for (int i = 1; i < RESOURCES; i++) {
        if (images[i].native)
            device.ops->releaseImage(&device, images[i].native);
        if (buffers[i].native)
            device.ops->releaseBuffer(&device, buffers[i].native);
        images[i].native = newImages[i];
        buffers[i].native = newBuffers[i];
    }
    device.ops->activate(&device, 0);
    device.ops->destroy(&device);
    device = next;
    device.ops->activate(&device, 1);
    backend = requested;
    SDL_Log("renderer: switched to %s on %s", RenderBackendName(backend), device.caps.deviceName);
    ok = 1;
done:
    if (!ok) {
        Failed("backend switch failed");
        if (next.data) {
            for (int i = 1; i < RESOURCES; i++) {
                if (newImages && newImages[i])
                    next.ops->releaseImage(&next, newImages[i]);
                if (newBuffers && newBuffers[i])
                    next.ops->releaseBuffer(&next, newBuffers[i]);
            }
            next.ops->destroy(&next);
        }
        device.ops->activate(&device, 1);
        requested = backend;
    }
    SDL_free(newImages);
    SDL_free(newBuffers);
    return ok;
}
void RenderStop(void) {
    if (!started)
        return;
    device.ops->finish(&device);
    for (int i = 0; i < 2; i++) {
        if (readbacks[i].native)
            device.ops->releaseRead(&device, readbacks[i].native);
        SDL_free(readbacks[i].cpu);
    }
    memset(readbacks, 0, sizeof(readbacks));
    for (int i = 1; i < RESOURCES; i++) {
        if (images[i].native)
            device.ops->releaseImage(&device, images[i].native);
        if (buffers[i].native)
            device.ops->releaseBuffer(&device, buffers[i].native);
        FreeShadow(&images[i]);
        SDL_free(buffers[i].shadow);
    }
    device.ops->destroy(&device);
    memset(images, 0, sizeof(images));
    memset(buffers, 0, sizeof(buffers));
    memset(targets, 0, sizeof(targets));
    started = 0;
}
void RenderTrimMemory(void) {
    if (started && device.ops->trim) device.ops->trim(&device);
}
void RenderPresent(void) {
    if (started)
        device.ops->present(&device);
}
RgHandle RgProgram(RenderShader kind) { return (RgHandle)kind + 1; }
void RgUseProgram(RgHandle p) { program = (int)p - 1; }
enum {
    U_TARGET = 1,
    U_ORIGIN,
    U_PULL,
    U_PASS,
    U_DEPTH_PASS,
    U_RECT,
    U_FACTOR,
    U_SCALE,
    U_ADAPTIVE,
    U_STEP,
    U_RASTER_SCALE,
    U_UNUSED
};
int RgGetUniformLocation(RgHandle p, const char *name) {
    const char *names[] = {"target", "origin",      "depthPull", "pass",     "depthPass",  "rect",
                           "factor", "targetScale", "adaptive",  "stepSize", "rasterScale"};
    for (int i = 0; i < 11; i++)
        if (!strcmp(name, names[i]))
            return (int)p * 32 + i + 1;
    return (int)p * 32 + U_UNUSED;
}
static RenderUniforms *Uniform(int id) { return &uniforms[id / 32 - 1]; }
void RgUniform1i(int id, int value) {
    RenderUniforms *u = Uniform(id);
    switch (id % 32) {
    case U_PASS:
        u->pass = value;
        break;
    case U_DEPTH_PASS:
        u->depthPass = value;
        break;
    case U_FACTOR:
        u->factor = value;
        break;
    case U_SCALE:
        u->targetScale = value;
        break;
    case U_ADAPTIVE:
        u->adaptive = value;
        break;
    }
}
void RgUniform1f(int id, float v) {
    RenderUniforms *u = Uniform(id);
    if (id % 32 == U_PULL)
        u->depthPull = v;
    else if (id % 32 == U_STEP)
        u->stepSize = v;
    else if (id % 32 == U_RASTER_SCALE)
        u->rasterScale = v;
}
void RgUniform2f(int id, float x, float y) {
    RenderUniforms *u = Uniform(id);
    if (id % 32 == U_TARGET) {
        u->targetX = x;
        u->targetY = y;
    } else if (id % 32 == U_ORIGIN) {
        u->originX = x;
        u->originY = y;
    }
}
void RgUniform4i(int id, int x, int y, int z, int w) {
    RenderUniforms *u = Uniform(id);
    if (id % 32 == U_RECT) {
        u->left = x;
        u->top = y;
        u->right = z;
        u->bottom = w;
    }
}
void RgGenTextures(int n, RgHandle *out) {
    while (n--) {
        int id = Slot(images, sizeof(*images), RESOURCES);
        images[id].used = 1;
        *out++ = id;
    }
}
void RgDeleteTextures(int n, const RgHandle *ids) {
    while (n--) {
        Image *im = &images[*ids++];
        if (im->native)
            device.ops->releaseImage(&device, im->native);
        FreeShadow(im);
        memset(im, 0, sizeof(*im));
    }
}
void RgActiveTexture(int unit) { activeUnit = unit - RG_TEXTURE0; }
void RgBindTexture(int kind, RgHandle im) {
    textureUnits[activeUnit][kind == RG_TEXTURE_2D_ARRAY] = im;
}
static Image *Bound(int kind) {
    return &images[textureUnits[activeUnit][kind == RG_TEXTURE_2D_ARRAY]];
}
void RgTexParameteri(int kind, int parameter, int value) {
    RenderSampler *s = &Bound(kind)->sampler;
    if (parameter == RG_TEXTURE_MIN_FILTER) {
        s->linear = value != RG_NEAREST;
        s->mipmaps = value == RG_LINEAR_MIPMAP_LINEAR;
    } else if (parameter == RG_TEXTURE_MAG_FILTER)
        s->linear = value != RG_NEAREST;
    else if (parameter == RG_TEXTURE_WRAP_S || parameter == RG_TEXTURE_WRAP_T)
        s->clamp = value == RG_CLAMP_TO_EDGE;
}
void RgTexSubImage3D(int kind, int level, int x, int y, int layer, int w, int h, int layers,
                     int format, int type, const void *data) {
    Image *im = Bound(kind);
    size_t bpp = PixelBytes(im->desc.format);
    int inputBpp = format == RG_RGB ? 3 : (int)bpp;
    size_t inputRow = (size_t)(unpackRowLength ? unpackRowLength : w) * inputBpp;
    inputRow = (inputRow + unpackAlignment - 1) / unpackAlignment * unpackAlignment;
    /* Large color textures already have authoritative GPU storage. Keep only
       the upload rectangle on the CPU, and read the texture if switching APIs.
       A recorder preview/HD atlas must not pin another full-resolution copy. */
    if (im->desc.layers == 1 && im->desc.format == RENDER_RGBA8 && ImageBytes(im->desc) >= 4 * 1024 * 1024) {
        size_t packedRow = (size_t)w * bpp;
        uint8_t *packed = SDL_malloc(packedRow * h);
        if (!packed) abort();
        for (int j = 0; j < h; j++) {
            uint8_t *dst = packed + j * packedRow;
            const uint8_t *src = (const uint8_t *)data + j * inputRow;
            if (format == RG_RGB) {
                for (int i = 0; i < w; i++) {
                    memcpy(dst + i * 4, src + i * 3, 3);
                    dst[i * 4 + 3] = 255;
                }
            } else memcpy(dst, src, packedRow);
        }
        int ok = device.ops->upload(&device, im->native, (RenderRect){x, y, w, h}, layer, packed, packedRow);
        SDL_free(packed);
        if (!ok) { Failed("texture upload"); abort(); }
        im->dirty = 1;
        return;
    }
    size_t row = (size_t)im->desc.width * bpp, plane = row * im->desc.height;
    if (im->desc.layers > 1) {
        if (!im->layerShadows) im->layerShadows = SDL_calloc(im->desc.layers, sizeof(uint8_t *));
        if (!im->layerShadows) abort();
    } else if (!im->shadow) im->shadow = SDL_calloc(1, plane);
    if (im->desc.layers == 1 && !im->shadow) abort();
    for (int z = 0; z < layers; z++) {
        uint8_t *pixels = im->shadow;
        if (im->layerShadows) {
            if (!im->layerShadows[layer + z]) im->layerShadows[layer + z] = SDL_calloc(1, plane);
            pixels = im->layerShadows[layer + z];
            if (!pixels) abort();
        }
        for (int j = 0; j < h; j++) {
            uint8_t *dst = pixels + row * (y + j) + (size_t)x * bpp;
            const uint8_t *src = (const uint8_t *)data + inputRow * (z * h + j);
            if (format == RG_RGB)
                for (int i = 0; i < w; i++) {
                    memcpy(dst + i * 4, src + i * 3, 3);
                    dst[i * 4 + 3] = 255;
                }
            else
                memcpy(dst, src, (size_t)w * bpp);
        }
        if (!device.ops->upload(&device, im->native, (RenderRect){x, y, w, h}, layer + z,
                                pixels + row * y + (size_t)x * bpp,
                                row)) {
            Failed("texture upload");
            abort();
        }
    }
}
void RgTexSubImage2D(int kind, int level, int x, int y, int w, int h, int format, int type,
                     const void *data) {
    RgTexSubImage3D(kind, level, x, y, 0, w, h, 1, format, type, data);
}
void RgTexImage3D(int kind, int level, int format, int w, int h, int layers, int border, int input,
                  int type, const void *data) {
    Image *im = Bound(kind);
    int mipmaps = im->desc.mipmaps;
    RenderFormat f = format == RG_R16UI               ? RENDER_R16_UINT
                     : format == RG_DEPTH_COMPONENT24 ? RENDER_DEPTH
                                                      : RENDER_RGBA8;
    if (im->native)
        device.ops->releaseImage(&device, im->native);
    FreeShadow(im);
    im->dirty = 0;
    im->desc = (RenderImageDesc){w, h, layers, mipmaps, f};
    im->native = device.ops->image(&device, im->desc);
    if (!im->native) {
        Failed("texture allocation");
        abort();
    }
    if (data)
        RgTexSubImage3D(kind, level, 0, 0, 0, w, h, layers, input, type, data);
}
void RgTexImage2D(int kind, int level, int format, int w, int h, int border, int input, int type,
                  const void *data) {
    RgTexImage3D(kind, level, format, w, h, 1, border, input, type, data);
}
void RgGenerateMipmap(int kind) {
    Image *im = Bound(kind);
    if (!im->desc.mipmaps) {
        im->desc.mipmaps = 1;
        uintptr_t next = device.ops->image(&device, im->desc);
        if (!next) {
            Failed("mipmap allocation");
            abort();
        }
        if (im->dirty) {
            RenderCopy copy = {{im->native, 0, 0},
                               {next, 0, 0},
                               {0, 0, im->desc.width, im->desc.height},
                               {0, 0, im->desc.width, im->desc.height},
                               0,
                               0};
            device.ops->copy(&device, &copy);
        } else if (im->shadow)
            device.ops->upload(&device, next, (RenderRect){0, 0, im->desc.width, im->desc.height},
                               0, im->shadow, (size_t)im->desc.width * PixelBytes(im->desc.format));
        if (!im->dirty && im->layerShadows)
            for (int z = 0; z < im->desc.layers; z++)
                if (im->layerShadows[z]) device.ops->upload(&device, next,
                    (RenderRect){0, 0, im->desc.width, im->desc.height}, z, im->layerShadows[z],
                    (size_t)im->desc.width * PixelBytes(im->desc.format));
        device.ops->releaseImage(&device, im->native);
        im->native = next;
    }
    device.ops->mipmaps(&device, im->native);
}
void RgGenFramebuffers(int n, RgHandle *out) {
    while (n--) {
        int id = Slot(targets, sizeof(*targets), TARGETS);
        targets[id].used = 1;
        *out++ = id;
    }
}
void RgDeleteFramebuffers(int n, const RgHandle *ids) {
    while (n--)
        memset(&targets[*ids++], 0, sizeof(Target));
}
void RgBindFramebuffer(int kind, RgHandle target) {
    if (kind != RG_DRAW_FRAMEBUFFER)
        readTarget = target;
    if (kind != RG_READ_FRAMEBUFFER)
        drawTarget = target;
}
void RgFramebufferTexture2D(int kind, int attachment, int texKind, RgHandle image, int level) {
    Target *t = &targets[kind == RG_READ_FRAMEBUFFER ? readTarget : drawTarget];
    if (attachment == RG_DEPTH_ATTACHMENT)
        t->depth = image;
    else
        t->colour = image;
}
int RgCheckFramebufferStatus(int kind) { return RG_FRAMEBUFFER_COMPLETE; }
void RgGenRenderbuffers(int n, RgHandle *out) { RgGenTextures(n, out); }
void RgDeleteRenderbuffers(int n, const RgHandle *ids) { RgDeleteTextures(n, ids); }
void RgBindRenderbuffer(int kind, RgHandle image) { depthImage = image; }
void RgRenderbufferStorage(int kind, int format, int w, int h) {
    RgHandle saved = textureUnits[activeUnit][0];
    textureUnits[activeUnit][0] = depthImage;
    RgTexImage2D(RG_TEXTURE_2D, 0, format, w, h, 0, RG_RGBA, RG_FLOAT, NULL);
    textureUnits[activeUnit][0] = saved;
}
void RgFramebufferRenderbuffer(int kind, int attachment, int rbKind, RgHandle image) {
    RgFramebufferTexture2D(kind, attachment, RG_TEXTURE_2D, image, 0);
}
void RgGenBuffers(int n, RgHandle *out) {
    while (n--) {
        int id = Slot(buffers, sizeof(*buffers), RESOURCES);
        buffers[id].used = 1;
        *out++ = id;
    }
}
void RgBindBuffer(int kind, RgHandle b) {
    if (kind == RG_PIXEL_PACK_BUFFER)
        packBuffer = b;
    else if (kind == RG_ELEMENT_ARRAY_BUFFER)
        indexBuffer = b;
    else
        vertexBuffer = b;
}
void RgBufferData(int kind, size_t size, const void *data, int usage) {
    Buffer *b = &buffers[kind == RG_PIXEL_PACK_BUFFER      ? packBuffer
                         : kind == RG_ELEMENT_ARRAY_BUFFER ? indexBuffer
                                                           : vertexBuffer];
    b->kind = kind == RG_ELEMENT_ARRAY_BUFFER ? RENDER_INDEX_BUFFER : RENDER_VERTEX_BUFFER;
    void *copy = SDL_realloc(b->shadow, size);
    if (!copy && size) {
        Failed("vertex allocation");
        abort();
    }
    b->shadow = copy;
    b->size = size;
    if (data)
        memcpy(copy, data, size);
    if (b->native && device.ops->writeBuffer) {
        if (!device.ops->writeBuffer(&device, b->native, copy, size, b->kind)) {
            Failed("buffer upload");
            abort();
        }
    } else {
        if (b->native)
            device.ops->releaseBuffer(&device, b->native);
        b->native = device.ops->buffer(&device, copy, size, b->kind);
    }
}
void RgGenVertexArrays(int n, RgHandle *out) {
    while (n--)
        *out++ = 1;
}
void RgBindVertexArray(RgHandle a) { (void)a; }
void RgEnableVertexAttribArray(int i) { (void)i; }
void RgVertexAttribPointer(int i, int n, int type, int norm, int stride, const void *offset) {
    (void)i;
    (void)n;
    (void)type;
    (void)norm;
    (void)stride;
    (void)offset;
}
void RgVertexAttribIPointer(int i, int n, int type, int stride, const void *offset) {
    (void)i;
    (void)n;
    (void)type;
    (void)stride;
    (void)offset;
}
void RgViewport(int x, int y, int w, int h) { state.viewport = (RenderRect){x, y, w, h}; }
void RgScissor(int x, int y, int w, int h) { state.scissor = (RenderRect){x, y, w, h}; }
void RgEnable(int s) {
    if (s == RG_SCISSOR_TEST)
        state.scissorEnabled = 1;
    else if (s == RG_DEPTH_TEST)
        state.pipeline.depthTest = 1;
    else if (s == RG_BLEND)
        state.pipeline.blend = 1;
}
void RgDisable(int s) {
    if (s == RG_SCISSOR_TEST)
        state.scissorEnabled = 0;
    else if (s == RG_DEPTH_TEST)
        state.pipeline.depthTest = 0;
    else if (s == RG_BLEND)
        state.pipeline.blend = 0;
}
int RgIsEnabled(int s) {
    return s == RG_SCISSOR_TEST ? state.scissorEnabled
           : s == RG_DEPTH_TEST ? state.pipeline.depthTest
                                : state.pipeline.blend != 0;
}
void RgColorMask(int r, int g, int b, int a) {
    state.pipeline.colourMask = !!r | !!g << 1 | !!b << 2 | !!a << 3;
}
void RgDepthMask(int on) { state.pipeline.depthWrite = on; }
void RgDepthFunc(int comp) { state.pipeline.depthLess = comp == RG_LESS; }
void RgBlendEquation(int eq) {
    if (state.pipeline.blend)
        state.pipeline.blend = eq == RG_FUNC_REVERSE_SUBTRACT ? 2 : 1;
}
void RgBlendFuncSeparate(int r, int g, int b, int a) {
    (void)r;
    (void)g;
    (void)b;
    (void)a;
}
void RgBlendColor(float r, float g, float b, float a) {
    (void)r;
    (void)g;
    (void)b;
    (void)a;
}
void RgClearColor(float r, float g, float b, float a) {
    clearColour[0] = r;
    clearColour[1] = g;
    clearColour[2] = b;
    clearColour[3] = a;
}
void RgClear(RgBits flags) {
    RenderClear c = {.target = TargetOf(drawTarget),
                     .scissor = state.scissor,
                     .scissorEnabled = state.scissorEnabled,
                     .colour = !!(flags & RG_COLOR_BUFFER_BIT),
                     .depth = !!(flags & RG_DEPTH_BUFFER_BIT),
                     .colourMask = state.pipeline.colourMask};
    memcpy(c.rgba, clearColour, sizeof(clearColour));
    if (!device.ops->clear(&device, &c)) {
        Failed("clear");
        abort();
    }
    if (c.colour)
        Dirty(drawTarget, 0);
    if (c.depth)
        Dirty(drawTarget, 1);
}
void RgGetIntegerv(int f, int *v) {
    if (f == RG_VIEWPORT)
        memcpy(v, &state.viewport, sizeof(state.viewport));
    else if (f == RG_MAX_TEXTURE_SIZE)
        *v = device.caps.maxTextureSize;
}
void RgPixelStorei(int f, int v) {
    if (f == RG_UNPACK_ALIGNMENT)
        unpackAlignment = v;
    else if (f == RG_UNPACK_ROW_LENGTH)
        unpackRowLength = v;
}
void RgDrawArrays(int kind, int first, int count) {
    state.target = TargetOf(drawTarget);
    state.pipeline.shader = program;
    state.uniforms = uniforms[program];
    state.vertices = buffers[vertexBuffer].native;
    state.first = first;
    state.count = count;
    for (int i = 0; i < 5; i++) {
        Image *im = &images[textureUnits[i][program == RENDER_GAME && i == 4]];
        state.images[i] = im->native;
        state.samplers[i] = im->sampler;
    }
    if (!device.ops->draw(&device, &state)) {
        Failed("draw");
        abort();
    }
    Dirty(drawTarget, 0);
    if (state.pipeline.depthWrite)
        Dirty(drawTarget, 1);
}
void RgBlitFramebuffer(int x, int y, int r, int b, int tx, int ty, int tr, int tb, RgBits flags,
                       int filter) {
    RenderCopy c = {TargetOf(readTarget), TargetOf(drawTarget),
                    {x, y, r - x, b - y}, {tx, ty, tr - tx, tb - ty},
                    filter == RG_LINEAR,  !!(flags & RG_DEPTH_BUFFER_BIT)};
    if (!device.ops->copy(&device, &c)) {
        Failed("copy");
        abort();
    }
    Dirty(drawTarget, c.depth);
}
void RgCopyTexSubImage2D(int kind, int level, int x, int y, int fx, int fy, int w, int h) {
    Image *im = Bound(kind);
    RenderCopy c = {TargetOf(readTarget), {im->native, 0, 0}, {fx, fy, w, h}, {x, y, w, h}, 0, 0};
    if (!device.ops->copy(&device, &c)) {
        Failed("texture copy");
        abort();
    }
    im->dirty = 1;
}
void RgReadPixels(int x, int y, int w, int h, int format, int type, void *pixels) {
    if (!device.ops->read(&device, TargetOf(readTarget), (RenderRect){x, y, w, h},
                          format == RG_DEPTH_COMPONENT ? RENDER_DEPTH : RENDER_RGBA8, pixels)) {
        Failed("readback");
        abort();
    }
}
void RgFinish(void) { device.ops->finish(&device); }
void RgFlush(void) { device.ops->finish(&device); }

void RgDrawUI(RgHandle vertices, RgHandle indices, RgHandle image, RenderRect clip, int width,
              int height, int first, int count, int indexBytes, size_t vertexOffset, float sx,
              float sy, float ox, float oy, int linear) {
    RenderDraw draw = {0};
    draw.target = (RenderTarget){0, 0, 1};
    draw.viewport = (RenderRect){0, 0, width, height};
    draw.scissor = clip;
    draw.scissorEnabled = 1;
    draw.pipeline = (RenderPipeline){.shader = RENDER_UI, .blend = 1, .colourMask = 15};
    draw.uniforms.uiScaleX = sx;
    draw.uniforms.uiScaleY = sy;
    draw.uniforms.uiOffsetX = ox;
    draw.uniforms.uiOffsetY = oy;
    draw.vertices = buffers[vertices].native;
    draw.indices = buffers[indices].native;
    draw.vertexOffset = vertexOffset;
    draw.first = first;
    draw.count = count;
    draw.indexBytes = indexBytes;
    draw.images[0] = images[image].native;
    draw.samplers[0] = images[image].sampler;
    draw.samplers[0].linear = linear;
    if (!linear)
        draw.samplers[0].mipmaps = 0;
    if (!device.ops->draw(&device, &draw)) {
        Failed("console draw");
        abort();
    }
}

void RenderSetVsync(int enabled) { device.vsync = enabled; }

int RgReadbackBusy(int reader) { return reader >= 0 && reader < 2 && readbacks[reader].busy; }
int RgReadbackBegin(int reader, int width, int height) {
    if (reader < 0 || reader >= 2 || width <= 0 || height <= 0 || readbacks[reader].busy)
        return 0;
    Readback *r = &readbacks[reader];
    r->width = width;
    r->height = height;
    if (device.ops->beginRead && device.caps.asyncReadback) {
        r->native =
            device.ops->beginRead(&device, TargetOf(readTarget), (RenderRect){0, 0, width, height});
        if (!r->native)
            return 0;
    } else {
        r->cpu = SDL_malloc((size_t)width * height * 4);
        if (!r->cpu)
            return 0;
        if (!device.ops->read(&device, TargetOf(readTarget), (RenderRect){0, 0, width, height},
                              RENDER_RGBA8, r->cpu)) {
            SDL_free(r->cpu);
            r->cpu = NULL;
            return 0;
        }
    }
    r->busy = 1;
    return 1;
}
int RgReadbackDone(int reader, int wait, void (*use)(const uint8_t *, int, int, void *),
                   void *user) {
    if (!RgReadbackBusy(reader))
        return 0;
    Readback *r = &readbacks[reader];
    const uint8_t *pixels = r->cpu;
    int status = 1;
    if (r->native)
        status = device.ops->pollRead(&device, r->native, wait, &pixels);
    if (!status)
        return 0;
    if (status > 0 && pixels)
        use(pixels, r->width, r->height, user);
    if (r->native)
        device.ops->releaseRead(&device, r->native);
    SDL_free(r->cpu);
    memset(r, 0, sizeof(*r));
    return status > 0;
}
static int DrainReadbacks(void) {
    for (int i = 0; i < 2; i++) {
        Readback *r = &readbacks[i];
        if (!r->native)
            continue;
        const uint8_t *pixels = NULL;
        int status = device.ops->pollRead(&device, r->native, 1, &pixels);
        if (status <= 0 || !pixels)
            return 0;
        uint8_t *copy = SDL_malloc((size_t)r->width * r->height * 4);
        if (!copy)
            return 0;
        memcpy(copy, pixels, (size_t)r->width * r->height * 4);
        r->cpu = copy;
        device.ops->releaseRead(&device, r->native);
        r->native = 0;
    }
    return 1;
}
