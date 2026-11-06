/* Device contract. No graphics API headers or native handles cross this boundary.
   Coordinates are in texture rows, with row zero at the bottom for all devices.
   Backends translate clip depth, surface orientation, and transfer alignment. */
#ifndef PORT_RENDER_DEVICE_H
#define PORT_RENDER_DEVICE_H
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct RenderVertex {
    float x, y, w, r, g, b, a, u, v;
    int16_t texture[4];
    uint8_t window[4], mode[4];
    uint16_t hd[4];
    int16_t hdTexels[4];
} RenderVertex;

typedef enum RenderFormat { RENDER_RGBA8, RENDER_R16_UINT, RENDER_DEPTH } RenderFormat;
typedef enum RenderBufferKind { RENDER_VERTEX_BUFFER, RENDER_INDEX_BUFFER } RenderBufferKind;

typedef enum RenderShader {
    RENDER_GAME,
    RENDER_AVERAGE,
    RENDER_DETAIL,
    RENDER_COMPOSITE,
    RENDER_UI,
    RENDER_SHADER_COUNT
} RenderShader;
typedef enum RenderBackend {
    RENDER_OPENGL,
    RENDER_WEBGL,
    RENDER_METAL,
    RENDER_BACKEND_COUNT
} RenderBackend;
typedef struct RenderImageDesc {
    int width, height, layers, mipmaps;
    RenderFormat format;
} RenderImageDesc;
typedef struct RenderRect {
    int x, y, w, h;
} RenderRect;
typedef struct RenderSampler {
    int linear, mipmaps, clamp;
} RenderSampler;
typedef struct RenderTarget {
    uintptr_t colour, depth;
    int window;
} RenderTarget;
/* Scalar layout is shared by shader uniform data, without compiler vector alignment rules. */
typedef struct RenderUniforms {
    float targetX, targetY, originX, originY, depthPull;
    int pass, depthPass;
    int left, top, right, bottom, factor, targetScale, adaptive;
    float stepSize;
    float uiScaleX, uiScaleY, uiOffsetX, uiOffsetY;
    float rasterScale;
} RenderUniforms;
typedef struct RenderPipeline {
    RenderShader shader;
    int blend; /* 0 disabled, 1 premultiplied, 2 reverse subtract */
    int depthTest, depthWrite, depthLess, colourMask;
} RenderPipeline;
typedef struct RenderDraw {
    RenderTarget target;
    RenderRect viewport, scissor;
    int scissorEnabled;
    RenderPipeline pipeline;
    RenderUniforms uniforms;
    uintptr_t vertices, indices;
    size_t vertexOffset, indexOffset;
    int first, count, indexBytes;
    uintptr_t images[5];
    RenderSampler samplers[5];
} RenderDraw;
typedef struct RenderClear {
    RenderTarget target;
    RenderRect scissor;
    int scissorEnabled, colour, depth, colourMask;
    float rgba[4];
} RenderClear;
typedef struct RenderCopy {
    RenderTarget source, destination;
    RenderRect from, to; /* negative extents flip a copy */
    int linear, depth;
} RenderCopy;
typedef struct RenderCaps {
    int maxTextureSize, maxArrayLayers, asyncReadback;
    char deviceName[160];
} RenderCaps;
typedef struct RenderDevice RenderDevice;
typedef struct RenderDeviceOps {
    int (*create)(RenderDevice *, void *window);
    void (*destroy)(RenderDevice *);
    uintptr_t (*image)(RenderDevice *, RenderImageDesc);
    void (*releaseImage)(RenderDevice *, uintptr_t);
    int (*upload)(RenderDevice *, uintptr_t, RenderRect, int layer, const void *, size_t rowBytes);
    int (*read)(RenderDevice *, RenderTarget, RenderRect, RenderFormat, void *);
    void (*mipmaps)(RenderDevice *, uintptr_t);
    uintptr_t (*buffer)(RenderDevice *, const void *, size_t, RenderBufferKind);
    void (*releaseBuffer)(RenderDevice *, uintptr_t);
    int (*clear)(RenderDevice *, const RenderClear *);
    int (*draw)(RenderDevice *, const RenderDraw *);
    int (*copy)(RenderDevice *, const RenderCopy *);
    void (*finish)(RenderDevice *);
    int (*present)(RenderDevice *);
    void (*activate)(RenderDevice *, int);
    /* Optional nonblocking readbacks. Returned pixels live until releaseRead.
       Missing callbacks use the shared synchronous fallback. */
    uintptr_t (*beginRead)(RenderDevice *, RenderTarget, RenderRect);
    int (*pollRead)(RenderDevice *, uintptr_t, int wait, const uint8_t **pixels);
    void (*releaseRead)(RenderDevice *, uintptr_t);
    /* Optional streaming update. Drivers may orphan native storage while
       keeping the buffer identity. Otherwise the shared layer replaces it. */
    int (*writeBuffer)(RenderDevice *, uintptr_t, const void *, size_t, RenderBufferKind);
    /* Optional cache release at explicit idle/freeze points. Live resources remain intact. */
    void (*trim)(RenderDevice *);
} RenderDeviceOps;
struct RenderDevice {
    const RenderDeviceOps *ops;
    void *data;
    RenderCaps caps;
    int vsync;
};

const RenderDeviceOps *RenderOpenGLDevice(void);
#ifdef PORT_HAVE_METAL
const RenderDeviceOps *RenderMetalDevice(void);
int RenderMetalAvailable(void);
#endif
#ifdef __cplusplus
}
#endif
#endif
