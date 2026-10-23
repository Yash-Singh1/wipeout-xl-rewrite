/* The PlayStation's drawing on OpenGL 3.3.

   All of video memory is one framebuffer at an integer multiple of the
   console's resolution, so drawing areas, fills, the displayed area and
   copies keep the console's coordinates. Textures are fetched from a copy
   of video memory in the shader the way the console's GPU fetches them
   (4- and 8-bit texels through a palette, 15-bit direct, texel 0
   transparent, the top bit marking texels that take part in
   semi-transparency, texture windows), then modulated by the vertex colour
   as the console does. Semi-transparent primitives blend with the four
   PlayStation modes. Geometry is drawn at full resolution from the GTE's
   precise vertices, with perspective-correct texturing where the depth is
   known.

   Widescreen (the settings): the screen's columns of video memory (x 0 to
   320, every row: both of the game's screen buffers) are drawn into a
   framebuffer of their own that reaches wideMargin console pixels beyond
   either side, so that what the game projects past the screen's edges is
   seen; its middle is the console's picture. Untextured primitives across
   the whole screen (clears, fades, flashes) are stretched over the sides;
   pictures keep their shape. A menu is shown wide too, but only its
   backdrop, enlarged evenly, and those fills reach the sides: the rest
   stays in the console's columns, where panels sliding in from off screen
   come into view as on the console. Whenever video memory's copy of those columns
   is read or shown 4:3 it is brought up to date first (SyncWide), and what
   the CPU writes there goes to both.

   The scale (the settings' resolution, or as many pixels as the window shows) can change while the game
   runs: the framebuffers are made again at the new scale from what they held (SetScale).

   HD textures (the player's texture pack, hdtex.c): a textured primitive that the pack has an image for
   carries where that image is in the atlas (an RGBA texture of the images in use, at the framebuffer's scale)
   and which texels of its page it covers; the shader samples the image there instead of video memory. */
#include <SDL3/SDL.h>
#include "../port_gl.h"
#include <stddef.h>
#include <stdlib.h>
#include <string.h>

#include "gpu.h"
#include "../hdtex.h"
#include "../link.h"
#include "../platform.h"
#include "../settings.h"

#ifndef PORT_GL_LINKED
#define GL_FUNCTIONS(X) \
    X(PFNGLGENBUFFERSPROC, glGenBuffers) \
    X(PFNGLBINDBUFFERPROC, glBindBuffer) \
    X(PFNGLBUFFERDATAPROC, glBufferData) \
    X(PFNGLGENVERTEXARRAYSPROC, glGenVertexArrays) \
    X(PFNGLBINDVERTEXARRAYPROC, glBindVertexArray) \
    X(PFNGLENABLEVERTEXATTRIBARRAYPROC, glEnableVertexAttribArray) \
    X(PFNGLVERTEXATTRIBPOINTERPROC, glVertexAttribPointer) \
    X(PFNGLVERTEXATTRIBIPOINTERPROC, glVertexAttribIPointer) \
    X(PFNGLCREATESHADERPROC, glCreateShader) \
    X(PFNGLSHADERSOURCEPROC, glShaderSource) \
    X(PFNGLCOMPILESHADERPROC, glCompileShader) \
    X(PFNGLGETSHADERIVPROC, glGetShaderiv) \
    X(PFNGLGETSHADERINFOLOGPROC, glGetShaderInfoLog) \
    X(PFNGLCREATEPROGRAMPROC, glCreateProgram) \
    X(PFNGLATTACHSHADERPROC, glAttachShader) \
    X(PFNGLBINDATTRIBLOCATIONPROC, glBindAttribLocation) \
    X(PFNGLLINKPROGRAMPROC, glLinkProgram) \
    X(PFNGLGETPROGRAMIVPROC, glGetProgramiv) \
    X(PFNGLGETPROGRAMINFOLOGPROC, glGetProgramInfoLog) \
    X(PFNGLUSEPROGRAMPROC, glUseProgram) \
    X(PFNGLGETUNIFORMLOCATIONPROC, glGetUniformLocation) \
    X(PFNGLUNIFORM1IPROC, glUniform1i) \
    X(PFNGLUNIFORM1FPROC, glUniform1f) \
    X(PFNGLUNIFORM2FPROC, glUniform2f) \
    X(PFNGLUNIFORM4IPROC, glUniform4i) \
    X(PFNGLGENFRAMEBUFFERSPROC, glGenFramebuffers) \
    X(PFNGLBINDFRAMEBUFFERPROC, glBindFramebuffer) \
    X(PFNGLFRAMEBUFFERTEXTURE2DPROC, glFramebufferTexture2D) \
    X(PFNGLCHECKFRAMEBUFFERSTATUSPROC, glCheckFramebufferStatus) \
    X(PFNGLBLITFRAMEBUFFERPROC, glBlitFramebuffer) \
    X(PFNGLBLENDEQUATIONPROC, glBlendEquation) \
    X(PFNGLBLENDCOLORPROC, glBlendColor) \
    X(PFNGLBLENDFUNCSEPARATEPROC, glBlendFuncSeparate) \
    X(PFNGLACTIVETEXTUREPROC, glActiveTexture) \
    X(PFNGLGENERATEMIPMAPPROC, glGenerateMipmap) \
    X(PFNGLGENRENDERBUFFERSPROC, glGenRenderbuffers) \
    X(PFNGLBINDRENDERBUFFERPROC, glBindRenderbuffer) \
    X(PFNGLRENDERBUFFERSTORAGEPROC, glRenderbufferStorage) \
    X(PFNGLFRAMEBUFFERRENDERBUFFERPROC, glFramebufferRenderbuffer) \
    X(PFNGLDELETEFRAMEBUFFERSPROC, glDeleteFramebuffers) \
    X(PFNGLDELETERENDERBUFFERSPROC, glDeleteRenderbuffers)

#ifndef __EMSCRIPTEN__
/* The desktop's: a picture read back without waiting for it (GpuReadPictureStart; WebGL 2 maps no buffers). */
#define GPU_READ_ASYNC 1
#define ASYNC_GL_FUNCTIONS(X) \
    X(PFNGLFENCESYNCPROC, glFenceSync) \
    X(PFNGLCLIENTWAITSYNCPROC, glClientWaitSync) \
    X(PFNGLDELETESYNCPROC, glDeleteSync) \
    X(PFNGLMAPBUFFERRANGEPROC, glMapBufferRange) \
    X(PFNGLUNMAPBUFFERPROC, glUnmapBuffer)
#else
#define ASYNC_GL_FUNCTIONS(X)
#endif

#define DECLARE_GL(type, name) static type p_##name;
GL_FUNCTIONS(DECLARE_GL)
ASYNC_GL_FUNCTIONS(DECLARE_GL)
#define glFenceSync p_glFenceSync
#define glClientWaitSync p_glClientWaitSync
#define glDeleteSync p_glDeleteSync
#define glMapBufferRange p_glMapBufferRange
#define glUnmapBuffer p_glUnmapBuffer
#define glGenBuffers p_glGenBuffers
#define glBindBuffer p_glBindBuffer
#define glBufferData p_glBufferData
#define glGenVertexArrays p_glGenVertexArrays
#define glBindVertexArray p_glBindVertexArray
#define glEnableVertexAttribArray p_glEnableVertexAttribArray
#define glVertexAttribPointer p_glVertexAttribPointer
#define glVertexAttribIPointer p_glVertexAttribIPointer
#define glCreateShader p_glCreateShader
#define glShaderSource p_glShaderSource
#define glCompileShader p_glCompileShader
#define glGetShaderiv p_glGetShaderiv
#define glGetShaderInfoLog p_glGetShaderInfoLog
#define glCreateProgram p_glCreateProgram
#define glAttachShader p_glAttachShader
#define glBindAttribLocation p_glBindAttribLocation
#define glLinkProgram p_glLinkProgram
#define glGetProgramiv p_glGetProgramiv
#define glGetProgramInfoLog p_glGetProgramInfoLog
#define glUseProgram p_glUseProgram
#define glGetUniformLocation p_glGetUniformLocation
#define glUniform1i p_glUniform1i
#define glUniform1f p_glUniform1f
#define glUniform2f p_glUniform2f
#define glUniform4i p_glUniform4i
#define glGenFramebuffers p_glGenFramebuffers
#define glBindFramebuffer p_glBindFramebuffer
#define glFramebufferTexture2D p_glFramebufferTexture2D
#define glCheckFramebufferStatus p_glCheckFramebufferStatus
#define glBlitFramebuffer p_glBlitFramebuffer
#define glBlendEquation p_glBlendEquation
#define glBlendColor p_glBlendColor
#define glBlendFuncSeparate p_glBlendFuncSeparate
#define glActiveTexture p_glActiveTexture
#define glGenerateMipmap p_glGenerateMipmap
#define glGenRenderbuffers p_glGenRenderbuffers
#define glBindRenderbuffer p_glBindRenderbuffer
#define glRenderbufferStorage p_glRenderbufferStorage
#define glFramebufferRenderbuffer p_glFramebufferRenderbuffer
#define glDeleteFramebuffers p_glDeleteFramebuffers
#define glDeleteRenderbuffers p_glDeleteRenderbuffers

#ifndef GL_R16UI
#define GL_R16UI 0x8234
#endif
#ifndef GL_RED_INTEGER
#define GL_RED_INTEGER 0x8D94
#endif
#ifndef GL_FUNC_REVERSE_SUBTRACT
#define GL_FUNC_REVERSE_SUBTRACT 0x800B
#endif

static int LoadFunctions(void)
{
#define LOAD_GL(type, name)                                    \
    p_##name = (type)SDL_GL_GetProcAddress(#name);             \
    if (!p_##name) {                                           \
        SDL_Log("OpenGL function %s is missing", #name);       \
        return 0;                                              \
    }
    GL_FUNCTIONS(LOAD_GL)
    ASYNC_GL_FUNCTIONS(LOAD_GL)
    return 1;
}
#else
#define GPU_READ_ASYNC 1
static int LoadFunctions(void)
{
    return 1; /* (the platform's OpenGL ES library is linked) */
}
#endif

/* ---- state ---- */

static int scale = 1;
static GLuint framebuffer, colour;   /* all of video memory, scaled */
static GLuint depthBuffer;           /* ... its depth (the depth buffer: Flush) */
static GLuint vramTexture;           /* the CPU copy, R16UI */
static GLuint glyphTexture;          /* the glyph page (GpuSetGlyphMemory): RGBA, premultiplied, filtered */
static GLuint hdAtlas;               /* HD textures: the images in use (HdPlaced) */
static GLuint program, vertexArray, vertexBuffer;
static GLint uniformVram, uniformGlyphs, uniformHdAtlas, uniformPass, uniformTarget;
static GLint uniformDepthPull, uniformDepthPass;
static int vramDirty = 1;
static GLuint windowFramebuffer;     /* the window's own: 0, but on iOS SDL's view has one of its own */
static int dirtyLeft, dirtyTop, dirtyRight, dirtyBottom;
static int areaX, areaY, areaW = VRAM_WIDTH, areaH = VRAM_HEIGHT;
static int initialised;

#define SCREEN_COLUMNS 320
#define MAX_WIDE_MARGIN 400
static int wideMargin;                         /* console pixels beyond each side; 0: no wide framebuffer */
static GLuint wideFramebuffer, wideColour, wideDepth;
static int wideDirty;                          /* drawn into since its middle was copied to video memory's */
static int areaWide;                           /* the draw area is in the screen's columns: drawn wide */
static int sceneWide;                          /* the game built a race's scene since the last new picture */
static int drawnSincePresent;                  /* the game drew anything since the last picture shown */
static int interpolatingDraw;                  /* libgpu draws a picture between two of the game's frames */
static int picturesWithoutScene = 1000;        /* new pictures in a row that were not a race's */
static int menuWide;                           /* the game built a menu's backdrop since the last new picture */
static int picturesWithoutMenu = 1000;         /* ... and new pictures in a row that were not a menu's */
static int backdropNext;                       /* the primitive being drawn is a menu's backdrop (GpuBackdrop) */
static GLint uniformOrigin;

/* A batch of triangles drawn with one blending state. What else a primitive's draw state says
   (texture page, palette, texture window, colour depth) goes with each vertex, so that primitives of
   different textures, which the ordering tables interleave, are drawn together. */
#define BATCH_VERTICES 8192
typedef struct BatchVertex {
    float x, y, w;
    float r, g, b, a;
    float u, v;
    int16_t texture[4]; /* page x, page y, palette x, palette y */
    uint8_t window[4];  /* texture window: mask x, mask y, offset x, offset y */
    uint8_t mode[4];    /* textured, raw, colour depth */
    uint16_t hd[4];     /* HD texture: its part of the atlas, x, y, w, h (w 0: none) */
    int16_t hdTexels[4]; /* ... the texels of the page it covers, x, y, w, h */
} BatchVertex;
static BatchVertex batch[BATCH_VERTICES];
static int batchCount;
static GpuDrawState batchState;
static int batchDepth; /* a vertex of the batch can write the depth buffer (else Flush leaves out that pass) */

/* The shaders are written once for OpenGL 3.3 and OpenGL ES 3 (WebGL 2): Compile() puts the version
   line in front (PORT_GLSL_VERSION); ES needs the precision of floats, integers and integer textures. */
static const char *vertexSource =
    "in vec3 position;\n"
    "in vec4 colour;\n"
    "in vec2 texel;\n"
    "in ivec4 texture;\n"
    "in uvec4 window;\n"              /* unsigned bytes: WebGL wants an attribute's signedness kept */
    "in uvec4 modeIn;\n"
    "in uvec4 hdIn;\n"
    "in ivec4 hdTexelsIn;\n"
    "flat out uvec4 hd;\n"
    "flat out ivec4 hdTexels;\n"
    "flat out ivec4 texturePage;\n"
    "flat out ivec4 clut;\n"
    "flat out ivec4 vWindow;\n"
    "flat out ivec4 mode;\n"
    "flat out int semiMode;\n"        /* the primitive's blending: -1 opaque, else the console's mode 0-3 */
    "uniform int pass;\n"
    "uniform float depthPull;\n"       /* the depth buffer (Flush): depth tested this much nearer */
    "uniform int depthPass;\n"         /* ... 1: depth written, at the true depth */
    "uniform vec2 target;\n"           /* framebuffer size in video-memory pixels */
    "uniform vec2 origin;\n"           /* the video-memory position of its first pixel */
    "out vec4 vColour;\n"
    "out vec3 vTexel;\n"
    "void main() {\n"
    /* position.z: the depth (perspective), negative when left out of the depth buffer, 0 in 2D. The depth buffer
       holds 1 - 1/depth; what is left out and 2D are in front of it (always drawn) and never written. */
    "    float w = position.z != 0.0 ? abs(position.z) : 1.0;\n"
    "    vec2 ndc = (position.xy - origin) / target * 2.0 - 1.0;\n"
    /* modeIn.w: bit 0 a polygon seen from behind, tested without the slack (hidden by anything nearer); from bit 4
       the blending mode + 1 */
    "    float pull = depthPass == 1 || (modeIn.w & 1u) == 1u ? 1.0 : depthPull;\n"
    "    semiMode = int(modeIn.w >> 4u) - 1;\n"
    "    float z = position.z > 0.0 ? w - 2.0 * pull : (depthPass == 1 ? w : -w);\n"
    "    gl_Position = vec4(ndc * w, z, w);\n"
    "    vColour = colour;\n"
    "    texturePage = ivec4(texture.xy, 0, 0);\n"
    "    clut = ivec4(texture.zw, 0, 0);\n"
    "    vWindow = ivec4(window);\n"
    "    mode = ivec4(int(modeIn.x), int(modeIn.y), pass, int(modeIn.z));\n"
    "    vTexel = vec3(texel, 1.0);\n"
    "    hd = hdIn;\n"
    "    hdTexels = hdTexelsIn;\n"
    "}\n";

/* mode: x = textured (0/1), y = raw, z = pass, w = depth
   texturePage: page x, page y; clut: x, y; window: mask x, mask y, offset x, offset y
   Every blending but the console's subtraction is the one premultiplied blend (Flush): what a fragment writes says
   how much of what is behind stays (its alpha). Pass 0: all of it; for a batch of subtracting primitives pass 1
   draws what is opaque and pass 2 the rest, subtracted; pass 3 writes the depth of what is opaque. */
static const char *fragmentSource =
    "#ifdef GL_ES\n"
    "precision highp float;\n"
    "precision highp int;\n"
    "precision highp usampler2D;\n"
    "#endif\n"
    "in vec4 vColour;\n"
    "in vec3 vTexel;\n"
    "flat in int semiMode;\n"
    "uniform usampler2D vram;\n"
    "uniform sampler2D glyphs;\n"
    "uniform sampler2D hdAtlas;\n"
    "flat in uvec4 hd;\n"
    "flat in ivec4 hdTexels;\n"
    "flat in ivec4 texturePage;\n"
    "flat in ivec4 clut;\n"
    "flat in ivec4 vWindow;\n"
    "flat in ivec4 mode;\n"
    "out vec4 fragment;\n"
    /* texels and palettes from either half of video memory (rows to 1023) */
    "uint Fetch(ivec2 p) { return texelFetch(vram, ivec2(p.x & 1023, p.y & 1023), 0).r; }\n"
    "uint FetchClut(ivec2 p) { return texelFetch(vram, ivec2(p.x & 1023, p.y & 1023), 0).r; }\n"
    /* colour col with coverage cov (premultiplied), blended or opaque, faded with distance (vColour.a) */
    "void Out(vec3 col, float cov, bool blended) {\n"
    "    float fade = vColour.a;\n"
    "    if (mode.z == 3) {\n"
    "        if (blended || fade < 0.999 || cov < 0.999) discard;\n"
    "        fragment = vec4(0.0);\n"
    "        return;\n"
    "    }\n"
    "    if (mode.z == 1 && blended) discard;\n"
    "    if (mode.z == 2 && !blended) discard;\n"
    "    col *= fade;\n"
    "    cov *= fade;\n"
    "    if (!blended) fragment = vec4(col, cov);\n"
    "    else if (semiMode == 0) fragment = vec4(col * 0.5, cov * 0.5);\n"  /* B/2 + F/2 */
    "    else if (semiMode == 3) fragment = vec4(col * 0.25, 0.0);\n"      /* B + F/4 */
    "    else fragment = vec4(col, 0.0);\n"                               /* B + F; B - F (its own blend) */
    "}\n"
    "void main() {\n"
    "    vec3 c = vColour.rgb;\n"
    "    float fade = vColour.a;\n"
    "    if (mode.x == 0) {\n"
    "        Out(c, 1.0, semiMode >= 0);\n"
    "        return;\n"
    "    }\n"
    "    vec2 t = vTexel.xy;\n"
    "    if (mode.w == 3) {\n"                 /* the platform's glyphs: smooth, with coverage */
    "        vec4 g = texture(glyphs, t / 256.0);\n"
    "        if (g.a < 0.004) discard;\n"
    "        vec3 rgb = mode.y == 0 ? min(g.rgb * c * (255.0 / 128.0), vec3(g.a)) : g.rgb;\n"
    "        Out(rgb, g.a, semiMode >= 0);\n"
    "        return;\n"
    "    }\n"
    "    ivec2 uv = ivec2(floor(t)) & 255;\n"
    "    uv.x = (uv.x & ~(vWindow.x * 8)) | ((vWindow.z & vWindow.x) * 8);\n"
    "    uv.y = (uv.y & ~(vWindow.y * 8)) | ((vWindow.w & vWindow.y) * 8);\n"
    /* the HD texture over the texels it covers: alpha 0 transparent, 0.5 semi-transparent, 1 opaque */
    "    if (hd.z > 0u) {\n"
    "        vec2 into = vec2(uv - hdTexels.xy) + (t - floor(t));\n"
    "        if (all(greaterThanEqual(into, vec2(0.0))) && all(lessThan(into, vec2(hdTexels.zw)))) {\n"
    "            ivec2 at = min(ivec2(into * vec2(hd.zw) / vec2(hdTexels.zw)), ivec2(hd.zw) - 1);\n"
    "            vec4 h = texelFetch(hdAtlas, ivec2(hd.xy) + at, 0);\n"
    "            if (h.a < 0.25) discard;\n"
    "            bool hdSemi = h.a < 0.75;\n"
    "            vec3 hc = mode.y == 0 ? min(h.rgb * c * (255.0 / 128.0), vec3(1.0)) : h.rgb;\n"
    "            Out(hc, 1.0, semiMode >= 0 && hdSemi);\n"
    "            return;\n"
    "        }\n"
    "    }\n"
    "    uint texel;\n"
    "    if (mode.w == 0) {\n"
    "        uint word = Fetch(texturePage.xy + ivec2(uv.x >> 2, uv.y));\n"
    "        uint index = (word >> uint((uv.x & 3) * 4)) & 15u;\n"
    "        texel = FetchClut(clut.xy + ivec2(int(index), 0));\n"
    "    } else if (mode.w == 1) {\n"
    "        uint word = Fetch(texturePage.xy + ivec2(uv.x >> 1, uv.y));\n"
    "        uint index = (word >> uint((uv.x & 1) * 8)) & 255u;\n"
    "        texel = FetchClut(clut.xy + ivec2(int(index), 0));\n"
    "    } else {\n"
    "        texel = Fetch(texturePage.xy + uv);\n"
    "    }\n"
    "    if (texel == 0u) discard;\n"
    "    bool semi = (texel & 0x8000u) != 0u;\n"
    "    vec3 t8 = vec3(float(texel & 31u), float((texel >> 5) & 31u), float((texel >> 10) & 31u)) * 8.0 / 255.0;\n"
    "    if (mode.y == 0) {\n"
    "        t8 = min(t8 * c * (255.0 / 128.0), vec3(1.0));\n"
    "    }\n"
    "    Out(t8, 1.0, semiMode >= 0 && semi);\n"
    "}\n";

static GLuint Compile(GLenum type, const char *source)
{
    GLuint shader = glCreateShader(type);
    const char *sources[2] = {PORT_GLSL_VERSION, source};
    GLint ok;

    glShaderSource(shader, 2, sources, NULL);
    glCompileShader(shader);
    glGetShaderiv(shader, GL_COMPILE_STATUS, &ok);
    if (!ok) {
        char log[4096];
        glGetShaderInfoLog(shader, sizeof(log), NULL, log);
        SDL_Log("shader: %s", log);
        abort();
    }
    return shader;
}

/* The largest scale: the wide framebuffer at its widest within the GPU's textures, and at most 12 (the
   framebuffers then take some 560 MB). */
int GpuMaxScale(void)
{
    static int largest;

    if (!largest) {
        GLint size = 4096;

        glGetIntegerv(GL_MAX_TEXTURE_SIZE, &size);
        largest = SDL_clamp(size / (SCREEN_COLUMNS + 2 * MAX_WIDE_MARGIN), 1, 12);
    }
    return largest;
}

/* The scale the window asks for: enough for its height, at least 1. */
int GpuWindowScale(void)
{
    int width, height;

    PortDrawableSize(&width, &height);
    return SDL_clamp((height + 239) / 240, 1, SDL_min(8, GpuMaxScale()));
}

/* The scale wanted: the settings' resolution, or the window's. */
static int ChooseScale(void)
{
#ifdef PORT_MOBILE
    /* a phone or tablet: automatic stops at 3 (all of video memory at 3072x1536; its screen's 6 would be 75 MB
       drawn into); the settings' resolution goes higher */
    return portSettings.renderScale > 0 ? SDL_min(portSettings.renderScale, GpuMaxScale()) : SDL_min(GpuWindowScale(), 3);
#else
    return portSettings.renderScale > 0 ? SDL_min(portSettings.renderScale, GpuMaxScale()) : GpuWindowScale();
#endif
}

int GpuScale(void)
{
    return scale;
}

/* Video memory's framebuffer at the scale. */
/* A depth buffer of that size, attached to the framebuffer bound, cleared. */
static GLuint DepthFor(int width, int height)
{
    GLuint depth;

    glGenRenderbuffers(1, &depth);
    glBindRenderbuffer(GL_RENDERBUFFER, depth);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT24, width, height);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, depth);
    glDisable(GL_SCISSOR_TEST);
    glDepthMask(GL_TRUE);
    glClear(GL_DEPTH_BUFFER_BIT);
    return depth;
}

static int CreateFramebuffer(void)
{
    glGenTextures(1, &colour);
    glBindTexture(GL_TEXTURE_2D, colour);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, VRAM_WIDTH * scale, VRAM_HEIGHT * scale, 0, GL_RGBA, GL_UNSIGNED_BYTE,
                 NULL);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glGenFramebuffers(1, &framebuffer);
    glBindFramebuffer(GL_FRAMEBUFFER, framebuffer);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, colour, 0);
    depthBuffer = DepthFor(VRAM_WIDTH * scale, VRAM_HEIGHT * scale);
    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
        SDL_Log("framebuffer incomplete at scale %d", scale);
        return 0;
    }
    return 1;
}

int GpuInit(void)
{
    GLint ok;

    if (initialised) {
        return 1;
    }
    if (!LoadFunctions()) {
        return 0;
    }
    program = glCreateProgram();
    glAttachShader(program, Compile(GL_VERTEX_SHADER, vertexSource));
    glAttachShader(program, Compile(GL_FRAGMENT_SHADER, fragmentSource));
    glBindAttribLocation(program, 0, "position");
    glBindAttribLocation(program, 1, "colour");
    glBindAttribLocation(program, 2, "texel");
    glBindAttribLocation(program, 3, "texture");
    glBindAttribLocation(program, 4, "window");
    glBindAttribLocation(program, 5, "modeIn");
    glBindAttribLocation(program, 6, "hdIn");
    glBindAttribLocation(program, 7, "hdTexelsIn");
    glLinkProgram(program);
    glGetProgramiv(program, GL_LINK_STATUS, &ok);
    if (!ok) {
        char log[4096];
        glGetProgramInfoLog(program, sizeof(log), NULL, log);
        SDL_Log("program: %s", log);
        return 0;
    }
    uniformVram = glGetUniformLocation(program, "vram");
    uniformGlyphs = glGetUniformLocation(program, "glyphs");
    uniformHdAtlas = glGetUniformLocation(program, "hdAtlas");
    uniformPass = glGetUniformLocation(program, "pass");
    uniformDepthPull = glGetUniformLocation(program, "depthPull");
    uniformDepthPass = glGetUniformLocation(program, "depthPass");
    uniformTarget = glGetUniformLocation(program, "target");
    uniformOrigin = glGetUniformLocation(program, "origin");
    /* each sampler its own unit from the start (an integer and a float texture may not share one) */
    glUseProgram(program);
    glUniform1i(uniformVram, 0);
    glUniform1i(uniformGlyphs, 1);
    glUniform1i(uniformHdAtlas, 2);

    glGenVertexArrays(1, &vertexArray);
    glBindVertexArray(vertexArray);
    glGenBuffers(1, &vertexBuffer);
    glBindBuffer(GL_ARRAY_BUFFER, vertexBuffer);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(BatchVertex), (void *)offsetof(BatchVertex, x));
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 4, GL_FLOAT, GL_FALSE, sizeof(BatchVertex), (void *)offsetof(BatchVertex, r));
    glEnableVertexAttribArray(2);
    glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, sizeof(BatchVertex), (void *)offsetof(BatchVertex, u));
    glEnableVertexAttribArray(3);
    glVertexAttribIPointer(3, 4, GL_SHORT, sizeof(BatchVertex), (void *)offsetof(BatchVertex, texture));
    glEnableVertexAttribArray(4);
    glVertexAttribIPointer(4, 4, GL_UNSIGNED_BYTE, sizeof(BatchVertex), (void *)offsetof(BatchVertex, window));
    glEnableVertexAttribArray(5);
    glVertexAttribIPointer(5, 4, GL_UNSIGNED_BYTE, sizeof(BatchVertex), (void *)offsetof(BatchVertex, mode));
    glEnableVertexAttribArray(6);
    glVertexAttribIPointer(6, 4, GL_UNSIGNED_SHORT, sizeof(BatchVertex), (void *)offsetof(BatchVertex, hd));
    glEnableVertexAttribArray(7);
    glVertexAttribIPointer(7, 4, GL_SHORT, sizeof(BatchVertex), (void *)offsetof(BatchVertex, hdTexels));

    glPixelStorei(GL_UNPACK_ALIGNMENT, 2);
    glPixelStorei(GL_PACK_ALIGNMENT, 1);
    glGenTextures(1, &vramTexture);
    glBindTexture(GL_TEXTURE_2D, vramTexture);
    /* both halves of video memory (gpu.h): the console's, then the second */
    glTexImage2D(GL_TEXTURE_2D, 0, GL_R16UI, VRAM_WIDTH, VRAM_HEIGHT * 2, 0, GL_RED_INTEGER, GL_UNSIGNED_SHORT, NULL);
    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, VRAM_WIDTH, VRAM_HEIGHT, GL_RED_INTEGER, GL_UNSIGNED_SHORT, psxVram);
    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, VRAM_HEIGHT, VRAM_WIDTH, VRAM_HEIGHT, GL_RED_INTEGER, GL_UNSIGNED_SHORT,
                    psxVramHigh);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

    glGenTextures(1, &glyphTexture);
    glBindTexture(GL_TEXTURE_2D, glyphTexture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, GLYPH_ATLAS_SIZE, GLYPH_ATLAS_SIZE, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glGenerateMipmap(GL_TEXTURE_2D); /* (complete before the glyphs come: a texture without its mipmaps is not) */
    /* the HD textures' atlas: one texel until there are images (a texture the shader can sample, as drivers want) */
    glGenTextures(1, &hdAtlas);
    glBindTexture(GL_TEXTURE_2D, hdAtlas);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, 1, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, "\0\0\0\0");
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

    scale = ChooseScale();
    HdTexSetScale(scale);
    if (!CreateFramebuffer()) {
        return 0;
    }
    glViewport(0, 0, VRAM_WIDTH * scale, VRAM_HEIGHT * scale);
    glClearColor(0, 0, 0, 1);
    glClear(GL_COLOR_BUFFER_BIT);
    initialised = 1;
    return 1;
}

void GpuVramChanged(int x, int y, int w, int h)
{
    int right = x + w, bottom = y + h;

    if (w <= 0 || h <= 0) {
        return;
    }
    HdTexChanged(x, y, w, h);
    if (x < 0 || y < 0 || right > VRAM_WIDTH || bottom > VRAM_HEIGHT) {
        x = 0;
        y = 0;
        right = VRAM_WIDTH;
        bottom = VRAM_HEIGHT;
    }
    if (!vramDirty) {
        dirtyLeft = x;
        dirtyTop = y;
        dirtyRight = right;
        dirtyBottom = bottom;
        vramDirty = 1;
    } else {
        dirtyLeft = SDL_min(dirtyLeft, x);
        dirtyTop = SDL_min(dirtyTop, y);
        dirtyRight = SDL_max(dirtyRight, right);
        dirtyBottom = SDL_max(dirtyBottom, bottom);
    }
}

void GpuSetGlyphMemory(const uint32_t *rgba)
{
    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, glyphTexture);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 4);
    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, GLYPH_ATLAS_SIZE, GLYPH_ATLAS_SIZE, GL_RGBA, GL_UNSIGNED_BYTE, rgba);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 2);
    glGenerateMipmap(GL_TEXTURE_2D);
    glActiveTexture(GL_TEXTURE0);
}

static int vramHighDirty;

void GpuVramHighChanged(void)
{
    vramHighDirty = 1;
}

static void UploadVram(void)
{
    if (vramHighDirty) {
        glBindTexture(GL_TEXTURE_2D, vramTexture);
        glTexSubImage2D(GL_TEXTURE_2D, 0, 0, VRAM_HEIGHT, VRAM_WIDTH, VRAM_HEIGHT, GL_RED_INTEGER, GL_UNSIGNED_SHORT,
                        psxVramHigh);
        vramHighDirty = 0;
    }
    if (!vramDirty) {
        return;
    }
    glBindTexture(GL_TEXTURE_2D, vramTexture);
    if (dirtyRight <= dirtyLeft) {
        glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, VRAM_WIDTH, VRAM_HEIGHT, GL_RED_INTEGER, GL_UNSIGNED_SHORT, psxVram);
    } else {
        int row;
        for (row = dirtyTop; row < dirtyBottom; row++) {
            glTexSubImage2D(GL_TEXTURE_2D, 0, dirtyLeft, row, dirtyRight - dirtyLeft, 1, GL_RED_INTEGER,
                            GL_UNSIGNED_SHORT, &psxVram[row][dirtyLeft]);
        }
    }
    vramDirty = 0;
    dirtyLeft = dirtyTop = dirtyRight = dirtyBottom = 0;
}

/* ---- widescreen ---- */

static int WideWidth(void)
{
    return SCREEN_COLUMNS + 2 * wideMargin;
}

/* Whether a rectangle of video memory reaches into the screen's columns (with a wide framebuffer). */
static int InScreenColumns(int x, int w)
{
    return wideMargin && x < SCREEN_COLUMNS && x + w > 0;
}

/* The middle of the wide framebuffer into video memory's, before that is read. */
static void SyncWide(void)
{
    if (!wideDirty) {
        return;
    }
    glBindFramebuffer(GL_READ_FRAMEBUFFER, wideFramebuffer);
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, framebuffer);
    glDisable(GL_SCISSOR_TEST);
    glBlitFramebuffer(wideMargin * scale, 0, (wideMargin + SCREEN_COLUMNS) * scale, VRAM_HEIGHT * scale, 0, 0,
                      SCREEN_COLUMNS * scale, VRAM_HEIGHT * scale, GL_COLOR_BUFFER_BIT, GL_NEAREST);
    glBindFramebuffer(GL_FRAMEBUFFER, framebuffer);
    wideDirty = 0;
}

/* Clears rows y to y + h of the wide framebuffer beyond the screen's columns. */
static void ClearWideSides(int y, int h, float r, float g, float b)
{
    glBindFramebuffer(GL_FRAMEBUFFER, wideFramebuffer);
    glEnable(GL_SCISSOR_TEST);
    glClearColor(r, g, b, 1.0f);
    glScissor(0, y * scale, wideMargin * scale, h * scale);
    glClear(GL_COLOR_BUFFER_BIT);
    glScissor((wideMargin + SCREEN_COLUMNS) * scale, y * scale, wideMargin * scale, h * scale);
    glClear(GL_COLOR_BUFFER_BIT);
    glDisable(GL_SCISSOR_TEST);
    glBindFramebuffer(GL_FRAMEBUFFER, framebuffer);
}

/* What the CPU wrote into video memory's framebuffer, in the screen's columns, into the wide one too. A
   picture across the whole screen is shown as the console shows it, with black beside it. */
static void CopyToWide(int x, int y, int w, int h)
{
    int left = SDL_max(x, 0), right = SDL_min(x + w, SCREEN_COLUMNS);

    if (!InScreenColumns(x, w) || h <= 0) {
        return;
    }
    glBindFramebuffer(GL_READ_FRAMEBUFFER, framebuffer);
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, wideFramebuffer);
    glDisable(GL_SCISSOR_TEST);
    glBlitFramebuffer(left * scale, y * scale, right * scale, (y + h) * scale, (left + wideMargin) * scale, y * scale,
                      (right + wideMargin) * scale, (y + h) * scale, GL_COLOR_BUFFER_BIT, GL_NEAREST);
    glBindFramebuffer(GL_FRAMEBUFFER, framebuffer);
    if (left == 0 && right == SCREEN_COLUMNS) {
        ClearWideSides(y, h, 0, 0, 0);
    }
}

/* The margin the window asks for: the console's 240 lines across its height, the width it leaves (rounded up:
   the picture covers the window, the part of a column left over is cut off its sides when shown). */
static int WantedMargin(void)
{
    int width, height, margin;

    if (PortLinkRole() == LINK_SLAVE) {
        /* the second player's game draws its half of the split window as wide as the first player's game */
        return PortLinkSplit() && PortLinkBand(&margin, &width, &height) ? SDL_clamp(margin, 0, MAX_WIDE_MARGIN) : 0;
    }
    if (!portSettings.widescreen || (picturesWithoutScene > 1 && picturesWithoutMenu > 1)) {
        return 0;
    }
    PortDrawableSize(&width, &height);
    if (height <= 0) {
        return 0;
    }
    if (PortLinkSplit()) {
        /* each half of a split window: the band's rows across half the height */
        margin = (int)SDL_ceil((GPU_SPLIT_ROWS * 2.0 * width / height - SCREEN_COLUMNS) / 2);
    } else {
        margin = (int)SDL_ceil((240.0 * width / height - SCREEN_COLUMNS) / 2);
    }
    return SDL_clamp(margin, 0, MAX_WIDE_MARGIN);
}

/* Makes the wide framebuffer for a margin (none for 0), from what video memory's shows. */
static void SetWideMargin(int margin)
{
    if (margin == wideMargin) {
        return;
    }
    SyncWide();
    if (wideFramebuffer) {
        glDeleteFramebuffers(1, &wideFramebuffer);
        glDeleteTextures(1, &wideColour);
        glDeleteRenderbuffers(1, &wideDepth);
        wideFramebuffer = wideColour = wideDepth = 0;
    }
    wideMargin = margin;
    if (margin) {
        glGenTextures(1, &wideColour);
        glBindTexture(GL_TEXTURE_2D, wideColour);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, WideWidth() * scale, VRAM_HEIGHT * scale, 0, GL_RGBA, GL_UNSIGNED_BYTE,
                     NULL);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glGenFramebuffers(1, &wideFramebuffer);
        glBindFramebuffer(GL_FRAMEBUFFER, wideFramebuffer);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, wideColour, 0);
        wideDepth = DepthFor(WideWidth() * scale, VRAM_HEIGHT * scale);
        glDisable(GL_SCISSOR_TEST);
        glClearColor(0, 0, 0, 1);
        glClear(GL_COLOR_BUFFER_BIT);
        CopyToWide(0, 0, SCREEN_COLUMNS, VRAM_HEIGHT);
    }
    glBindFramebuffer(GL_FRAMEBUFFER, framebuffer);
    areaWide = InScreenColumns(areaX, areaW) && areaX >= 0 && areaX + areaW <= SCREEN_COLUMNS;
}

int PortScreenMargin(void)
{
    return wideMargin;
}

void PortWideScene(void)
{
    sceneWide = 1;
}

void GpuWideMenu(void)
{
    menuWide = 1;
}

static void Flush(void);

void GpuWideLoadScreen(void)
{
    menuWide = 1;
    if (!wideMargin) {
        /* at once, before its first picture is drawn: the screen before it (the title) was 4:3 */
        Flush();
        picturesWithoutMenu = 0;
        SetWideMargin(WantedMargin());
    }
}

int GpuRaceWide(void)
{
    /* (the pictures between a race's frames are drawn after the new picture that ended sceneWide) */
    return sceneWide || picturesWithoutScene == 0;
}

void GpuBackdrop(int on)
{
    backdropNext = on;
}

/* The framebuffer the next primitives go to, its viewport and the draw area's scissor. */
static void BindTarget(void)
{
    if (areaWide) {
        /* a draw area across the screen reaches over the sides as well: a race's scene, and in a menu what fills
           them and the panels that slide out to the window's edges and back */
        int full = areaX == 0 && areaW == SCREEN_COLUMNS;

        glBindFramebuffer(GL_FRAMEBUFFER, wideFramebuffer);
        glViewport(0, 0, WideWidth() * scale, VRAM_HEIGHT * scale);
        glEnable(GL_SCISSOR_TEST);
        glScissor(full ? 0 : (areaX + wideMargin) * scale, areaY * scale, (full ? WideWidth() : areaW) * scale,
                  areaH * scale);
        glUniform2f(uniformTarget, (float)WideWidth(), (float)VRAM_HEIGHT);
        glUniform2f(uniformOrigin, (float)-wideMargin, 0.0f);
        wideDirty = 1;
        drawnSincePresent |= !interpolatingDraw;
        return;
    }
    glBindFramebuffer(GL_FRAMEBUFFER, framebuffer);
    glViewport(0, 0, VRAM_WIDTH * scale, VRAM_HEIGHT * scale);
    glEnable(GL_SCISSOR_TEST);
    glScissor(areaX * scale, areaY * scale, areaW * scale, areaH * scale);
    glUniform2f(uniformTarget, (float)VRAM_WIDTH, (float)VRAM_HEIGHT);
    glUniform2f(uniformOrigin, 0.0f, 0.0f);
    drawnSincePresent |= !interpolatingDraw;
}

/* Whether two primitives can be drawn in one batch: the same blending and passes (what else their states say
   goes with the vertices). */
/* Whether a primitive is drawn over what is behind (semi-transparent, faded, or glyphs with their coverage). */
static int Blended(const GpuDrawState *s)
{
    return s->semi >= 0 || s->fade || (s->textured && s->depth == 3);
}

static int SameState(const GpuDrawState *a, const GpuDrawState *b)
{
    /* (the console's subtraction is a blend of its own; the opaque are drawn without blending, which is cheaper) */
    return (a->semi == 2) == (b->semi == 2) && Blended(a) == Blended(b);
}

/* The premultiplied blend every fragment says how much of what is behind stays in (the shader's Out), or the
   console's subtraction; the framebuffer's alpha stays as it is. */
static void SetBlend(int subtract)
{
    if (!Blended(&batchState)) {
        glDisable(GL_BLEND); /* (the shader writes the colour itself: coverage 1, no fade) */
        return;
    }
    glEnable(GL_BLEND);
    glBlendEquation(subtract ? GL_FUNC_REVERSE_SUBTRACT : GL_FUNC_ADD);
    glBlendFuncSeparate(GL_ONE, subtract ? GL_ONE : GL_ONE_MINUS_SRC_ALPHA, GL_ZERO, GL_ONE);
}

/* The depth buffer's slack: what is drawn after may be this much farther (a fraction of its depth) than what is
   drawn before at that pixel and still be drawn over it, as in the game's order. */
#define DEPTH_SLACK 0.02f

static void DrawBatch(int pass)
{
    glUniform1i(uniformPass, pass);
    SetBlend(pass == 2);
    glDrawArrays(GL_TRIANGLES, 0, batchCount);
}

static void Flush(void)
{
    const GpuDrawState *s = &batchState;

    if (!batchCount) {
        return;
    }
    UploadVram();
    glUseProgram(program);
    BindTarget();
    glBindVertexArray(vertexArray);
    glBindBuffer(GL_ARRAY_BUFFER, vertexBuffer);
    glBufferData(GL_ARRAY_BUFFER, (GLsizeiptr)(batchCount * sizeof(BatchVertex)), batch, GL_STREAM_DRAW);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, vramTexture);
    glUniform1i(uniformVram, 0);
    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, glyphTexture);
    glUniform1i(uniformGlyphs, 1);
    glActiveTexture(GL_TEXTURE2);
    glBindTexture(GL_TEXTURE_2D, hdAtlas);
    glUniform1i(uniformHdAtlas, 2);
    glActiveTexture(GL_TEXTURE0);
    /* The depth buffer: what has a depth is drawn where nothing nearer was drawn, give or take DEPTH_SLACK (the
       game's own order decides among things at about the same depth: a shadow or a pad on the track), and the
       opaque write their depth once drawn. Without it (the console's look) the game's order alone decides. */
    if (!portSettings.consoleDepth) {
        glEnable(GL_DEPTH_TEST);
        glDepthFunc(GL_LEQUAL);
        glDepthMask(GL_FALSE);
        glUniform1f(uniformDepthPull, 1.0f / (1.0f - DEPTH_SLACK));
        glUniform1i(uniformDepthPass, 0);
    } else {
        glDisable(GL_DEPTH_TEST);
    }
    if (s->semi == 2) {
        /* the console's subtraction: what is opaque (texels without the semi-transparency bit) drawn first, then
           the rest subtracted */
        DrawBatch(1);
        DrawBatch(2);
    } else {
        DrawBatch(0);
    }
    if (!portSettings.consoleDepth && batchDepth && !(batchState.semi >= 0 && !batchState.textured)) {
        /* the depth of what was drawn opaque (the shader leaves out the rest) */
        glColorMask(GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE);
        glDepthMask(GL_TRUE);
        glDepthFunc(GL_LESS);
        glUniform1i(uniformDepthPass, 1);
        glUniform1i(uniformPass, 3);
        glDrawArrays(GL_TRIANGLES, 0, batchCount);
        glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
        glDepthMask(GL_FALSE);
    }
    glDisable(GL_DEPTH_TEST);
    batchCount = 0;
}

void GpuFlush(void)
{
    Flush();
}

void GpuInterpolating(int on)
{
    Flush();
    interpolatingDraw = on;
}

void GpuSetDrawArea(int x, int y, int w, int h, int offsetX, int offsetY)
{
    (void)offsetX;
    (void)offsetY;
    Flush();
    areaX = x;
    areaY = y;
    areaW = w > 0 ? w : 0;
    areaH = h > 0 ? h : 0;
    areaWide = InScreenColumns(x, w) && x >= 0 && x + w <= SCREEN_COLUMNS;
}

/* Semi-transparent textured primitives must not share a batch: each one's
   opaque and blended passes have to come before the next primitive. */
static GpuDrawState primitiveState; /* the primitive being added */
static uint16_t primitiveHd[4];     /* ... its HD texture (BatchVertex) */
static int16_t primitiveHdTexels[4];

/* ---- HD textures: the images in use, side by side in one texture ---- */

static int hdAtlasSize;
static unsigned hdEpoch;            /* the images' (HdTexEpoch) the atlas holds */
static unsigned hdFill = 1;         /* the atlas's filling: an image is in it when hdPlaced[] says this one */
static unsigned *hdPlaced;
static int *hdPlace;                /* x, y, w, h of each image in it */
static int hdImages;
static int hdShelfX, hdShelfY, hdShelfH; /* the shelf being filled, left to right, the shelves top to bottom */
static int pictures, hdEmptiedAt = -1;

/* The atlas empty again (what was drawn from it is drawn first). */
static void HdEmpty(void)
{
    Flush();
    hdFill++;
    hdShelfX = hdShelfY = hdShelfH = 0;
}

/* Where image is in the atlas (put there now if it was not); 0 while it is not ready, or does not fit. */
static int HdPlaced(int image, int *at)
{
    const uint8_t *pixels;
    int w, h, size;

    if (HdTexEpoch() != hdEpoch || HdTexImageCount() != hdImages) {
        /* other images (another scale, another pack): an atlas for them */
        size = SDL_min(scale <= 6 ? 4096 : 8192, GpuMaxScale() * (SCREEN_COLUMNS + 2 * MAX_WIDE_MARGIN));
        HdEmpty();
        hdEpoch = HdTexEpoch();
        hdImages = HdTexImageCount();
        hdPlaced = SDL_realloc(hdPlaced, sizeof(unsigned) * (size_t)SDL_max(hdImages, 1));
        hdPlace = SDL_realloc(hdPlace, sizeof(int) * 4 * (size_t)SDL_max(hdImages, 1));
        memset(hdPlaced, 0, sizeof(unsigned) * (size_t)SDL_max(hdImages, 1));
        if (size != hdAtlasSize) {
            if (!hdAtlas) {
                glGenTextures(1, &hdAtlas);
            }
            glBindTexture(GL_TEXTURE_2D, hdAtlas);
            glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, size, size, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
            hdAtlasSize = size;
        }
    }
    if (image >= hdImages) {
        return 0;
    }
    if (hdPlaced[image] != hdFill) {
        if (!(pixels = HdTexImage(image, &w, &h)) || w > hdAtlasSize || h > hdAtlasSize) {
            return 0;
        }
        if (hdShelfX + w > hdAtlasSize) {
            hdShelfY += hdShelfH;
            hdShelfX = hdShelfH = 0;
        }
        if (hdShelfY + h > hdAtlasSize) {
            /* full: emptied for the images in use now (once a picture: past that some go without) */
            if (hdEmptiedAt == pictures) {
                return 0;
            }
            HdEmpty();
            hdEmptiedAt = pictures;
        }
        glBindTexture(GL_TEXTURE_2D, hdAtlas);
        glPixelStorei(GL_UNPACK_ALIGNMENT, 4);
        glTexSubImage2D(GL_TEXTURE_2D, 0, hdShelfX, hdShelfY, w, h, GL_RGBA, GL_UNSIGNED_BYTE, pixels);
        glPixelStorei(GL_UNPACK_ALIGNMENT, 2);
        hdPlace[image * 4] = hdShelfX;
        hdPlace[image * 4 + 1] = hdShelfY;
        hdPlace[image * 4 + 2] = w;
        hdPlace[image * 4 + 3] = h;
        hdPlaced[image] = hdFill;
        hdShelfX += w;
        hdShelfH = SDL_max(hdShelfH, h);
    }
    memcpy(at, &hdPlace[image * 4], sizeof(int) * 4);
    return 1;
}

/* The primitive's HD texture, if the pack has one for its texels. */
static void FindHd(const GpuDrawState *state, const GpuVertex *vertices, int count)
{
    float u0 = vertices[0].u, v0 = vertices[0].v, u1 = u0, v1 = v0;
    HdTexHit hit;
    int at[4], i;

    memset(primitiveHd, 0, sizeof(primitiveHd));
    if (!state->textured || state->depth > 2 || !HdTexActive()) {
        return;
    }
    for (i = 1; i < count; i++) {
        u0 = SDL_min(u0, vertices[i].u);
        v0 = SDL_min(v0, vertices[i].v);
        u1 = SDL_max(u1, vertices[i].u);
        v1 = SDL_max(v1, vertices[i].v);
    }
    if (!HdTexFind(state->pageX, state->pageY, state->depth, state->clutX, state->clutY, u0, v0, u1, v1, &hit) ||
        !HdPlaced(hit.image, at)) {
        return;
    }
    /* the image's part over those texels: the replaced texture's texels are at[2] / nameW pixels wide */
    primitiveHd[0] = (uint16_t)(at[0] + hit.srcX * at[2] / hit.nameW);
    primitiveHd[1] = (uint16_t)(at[1] + hit.srcY * at[3] / hit.nameH);
    primitiveHd[2] = (uint16_t)SDL_max(hit.w * at[2] / hit.nameW, 1);
    primitiveHd[3] = (uint16_t)SDL_max(hit.h * at[3] / hit.nameH, 1);
    primitiveHdTexels[0] = (int16_t)hit.x;
    primitiveHdTexels[1] = (int16_t)hit.y;
    primitiveHdTexels[2] = (int16_t)hit.w;
    primitiveHdTexels[3] = (int16_t)hit.h;
}

/* The depth buffer: a batch's depth is written once it is drawn (Flush), so a polygon seen from behind does not go
   in a batch after polygons seen from the front: it meets their depth, and is hidden where they are nearer (the
   other way round the game's order already has the front, nearer, drawn over it). */
static int batchFront;

static void Begin(const GpuDrawState *state, int vertices, int back)
{
    if (batchCount && (!SameState(state, &batchState) || batchCount + vertices > BATCH_VERTICES ||
                       (state->textured && state->semi == 2) || (back && batchFront && !portSettings.consoleDepth))) {
        Flush();
    }
    if (!batchCount) {
        batchState = *state;
        batchFront = 0;
        batchDepth = 0;
    }
    batchFront |= !back;
    primitiveState = *state;
}

static void Add(const GpuVertex *v)
{
    BatchVertex *out = &batch[batchCount++];

    out->x = v->x;
    out->y = v->y;
    out->w = v->w > 0.0f && v->noDepth ? -v->w : v->w; /* (the vertex shader's position.z) */
    batchDepth |= out->w > 0.0f && v->a == 255;
    out->r = v->r / 255.0f;
    out->g = v->g / 255.0f;
    out->b = v->b / 255.0f;
    out->a = v->a / 255.0f;
    out->u = v->u;
    out->v = v->v;
    out->texture[0] = (int16_t)primitiveState.pageX;
    out->texture[1] = (int16_t)primitiveState.pageY;
    out->texture[2] = (int16_t)primitiveState.clutX;
    out->texture[3] = (int16_t)primitiveState.clutY;
    out->window[0] = (uint8_t)primitiveState.windowMaskX;
    out->window[1] = (uint8_t)primitiveState.windowMaskY;
    out->window[2] = (uint8_t)primitiveState.windowOffsetX;
    out->window[3] = (uint8_t)primitiveState.windowOffsetY;
    out->mode[0] = (uint8_t)primitiveState.textured;
    out->mode[1] = (uint8_t)primitiveState.raw;
    out->mode[2] = (uint8_t)primitiveState.depth;
    /* (the vertex shader's: a polygon seen from behind; the blending) */
    out->mode[3] = (uint8_t)((v->backFacing != 0) | (primitiveState.semi + 1) << 4);
    memcpy(out->hd, primitiveHd, sizeof(out->hd));
    memcpy(out->hdTexels, primitiveHdTexels, sizeof(out->hdTexels));
}

/* Video memory is kept twice: psxVram (what the CPU uploads and reads, and
   what textures sample) and the framebuffer (what the GPU draws, at the
   display's scale). The blocks the GPU has drawn into since they were last
   written by the CPU hold their true pixels in the framebuffer only. */
#define BLOCK 8
static uint8_t drawnBlocks[VRAM_HEIGHT / BLOCK][VRAM_WIDTH / BLOCK];

static void MarkDrawn(float minX, float minY, float maxX, float maxY)
{
    int left = SDL_max((int)minX, areaX), top = SDL_max((int)minY, areaY);
    int right = SDL_min((int)maxX + 1, areaX + areaW), bottom = SDL_min((int)maxY + 1, areaY + areaH);
    int bx, by;

    if (right <= left || bottom <= top) {
        return;
    }
    for (by = top / BLOCK; by <= (bottom - 1) / BLOCK && by < VRAM_HEIGHT / BLOCK; by++) {
        for (bx = left / BLOCK; bx <= (right - 1) / BLOCK && bx < VRAM_WIDTH / BLOCK; bx++) {
            drawnBlocks[by][bx] = 1;
        }
    }
}

static void ClearDrawn(int x, int y, int w, int h)
{
    int bx, by;

    /* only blocks the rectangle covers completely */
    for (by = (y + BLOCK - 1) / BLOCK; (by + 1) * BLOCK <= y + h && by < VRAM_HEIGHT / BLOCK; by++) {
        for (bx = (x + BLOCK - 1) / BLOCK; (bx + 1) * BLOCK <= x + w && bx < VRAM_WIDTH / BLOCK; bx++) {
            drawnBlocks[by][bx] = 0;
        }
    }
}

void GpuTriangles(const GpuDrawState *given, const GpuVertex *vertices, int count)
{
    GpuDrawState faded = *given;
    const GpuDrawState *state = &faded;
    int i;
    int perspective = 1, stretch, backdrop;
    float minX = vertices[0].x, minY = vertices[0].y, maxX = minX, maxY = minY;

    for (i = 1; i < count; i++) {
        minX = SDL_min(minX, vertices[i].x);
        minY = SDL_min(minY, vertices[i].y);
        maxX = SDL_max(maxX, vertices[i].x);
        maxY = SDL_max(maxY, vertices[i].y);
    }
    MarkDrawn(minX, minY, maxX, maxY);
    for (i = 0; i < count; i++) {
        faded.fade |= vertices[i].a < 255;
    }

    for (i = 0; i < count; i++) {
        if (vertices[i].w <= 0.0f) {
            perspective = 0;
        }
    }
    /* widescreen: an untextured primitive in 2D across the whole screen (a clear, a fade, a flash) covers
       the sides as well */
    stretch = areaWide && areaX == 0 && areaW == SCREEN_COLUMNS && !given->textured && !perspective && minX <= 0.0f &&
              maxX >= (float)SCREEN_COLUMNS;
    /* ... and a menu's backdrop is enlarged about the middle of the draw area to fill them, keeping its shape */
    backdrop = backdropNext && areaWide && areaX == 0 && areaW == SCREEN_COLUMNS && !perspective && !GpuRaceWide();
    FindHd(state, vertices, count);
    Begin(state, count, vertices[0].backFacing);
    for (i = 0; i < count; i++) {
        GpuVertex v = vertices[i];
        if (!perspective) {
            v.w = 0.0f;
        }
        if (stretch) {
            v.x = v.x * WideWidth() / SCREEN_COLUMNS - wideMargin;
        } else if (backdrop) {
            float zoom = (float)WideWidth() / SCREEN_COLUMNS, middle = areaY + areaH * 0.5f;

            v.x = SCREEN_COLUMNS * 0.5f + (v.x - SCREEN_COLUMNS * 0.5f) * zoom;
            v.y = middle + (v.y - middle) * zoom;
        }
        Add(&v);
    }
}

/* A one-pixel line as a thin quad. */
void GpuLine(const GpuDrawState *state, const GpuVertex *a, const GpuVertex *b)
{
    GpuVertex q[4];
    float dx = b->x - a->x, dy = b->y - a->y;
    float length = SDL_sqrtf(dx * dx + dy * dy);
    float nx, ny;

    if (length < 0.001f) {
        dx = 1.0f;
        dy = 0.0f;
        length = 1.0f;
    }
    nx = -dy / length * 0.5f;
    ny = dx / length * 0.5f;
    q[0] = *a;
    q[1] = *a;
    q[2] = *b;
    q[3] = *b;
    q[0].x += nx + 0.5f;
    q[0].y += ny + 0.5f;
    q[1].x -= nx - 0.5f;
    q[1].y -= ny - 0.5f;
    q[2].x += nx + 0.5f;
    q[2].y += ny + 0.5f;
    q[3].x -= nx - 0.5f;
    q[3].y -= ny - 0.5f;
    q[0].w = q[1].w = q[2].w = q[3].w = 0.0f;
    GpuTriangles(state, q, 3);
    GpuTriangles(state, q + 1, 3);
}

void GpuFill(int x, int y, int w, int h, uint8_t r, uint8_t g, uint8_t b)
{
    int i, j;
    uint16_t pixel = (uint16_t)((r >> 3) | ((g >> 3) << 5) | ((b >> 3) << 10));

    Flush();
    /* video memory as well, for anything the game reads back or textures from */
    for (j = 0; j < h; j++) {
        for (i = 0; i < w; i++) {
            psxVram[(y + j) & (VRAM_HEIGHT - 1)][(x + i) & (VRAM_WIDTH - 1)] = pixel;
        }
    }
    GpuVramChanged(x, y, w, h);
    HdTexOverwritten(x, y, w, h);
    ClearDrawn(x, y, w, h);
    GpuClearArea(x, y, w, h, r, g, b);
}

/* Clears (x, y, w, h) of the framebuffers: `buffers` (colour to r, g, b, depth to the farthest). */
static void Clear(int x, int y, int w, int h, GLbitfield buffers, uint8_t r, uint8_t g, uint8_t b)
{
    Flush();
    glBindFramebuffer(GL_FRAMEBUFFER, framebuffer);
    glEnable(GL_SCISSOR_TEST);
    glScissor(x * scale, y * scale, w * scale, h * scale);
    glClearColor(r / 255.0f, g / 255.0f, b / 255.0f, 1.0f);
    glDepthMask(GL_TRUE);
    glClear(buffers);
    if (InScreenColumns(x, w) && wideFramebuffer) {
        /* across the whole screen: the sides too */
        int left = x <= 0 && x + w >= SCREEN_COLUMNS ? 0 : SDL_max(x, 0) + wideMargin;
        int right = x <= 0 && x + w >= SCREEN_COLUMNS ? WideWidth() : SDL_min(x + w, SCREEN_COLUMNS) + wideMargin;

        glBindFramebuffer(GL_FRAMEBUFFER, wideFramebuffer);
        glScissor(left * scale, y * scale, (right - left) * scale, h * scale);
        glClear(buffers);
        glBindFramebuffer(GL_FRAMEBUFFER, framebuffer);
    }
    glDisable(GL_SCISSOR_TEST);
}

void GpuClearArea(int x, int y, int w, int h, uint8_t r, uint8_t g, uint8_t b)
{
    Clear(x, y, w, h, GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT, r, g, b);
}

void GpuClearDepth(int x, int y, int w, int h)
{
    Clear(x, y, w, h, GL_DEPTH_BUFFER_BIT, 0, 0, 0);
}

/* Reads drawn pixels back into video memory (15-bit, each the framebuffer's
   pixel at the centre of the console pixel). */
void GpuReadBack(int x, int y, int w, int h)
{
    uint8_t *pixels;
    int i, j;

    if (w <= 0 || h <= 0) {
        return;
    }
    Flush();
    if (x < 0 || y < 0 || x + w > VRAM_WIDTH || y + h > VRAM_HEIGHT) {
        return;
    }
    SyncWide();
    pixels = malloc((size_t)w * h * scale * scale * 4);
    glBindFramebuffer(GL_FRAMEBUFFER, framebuffer);
    glReadPixels(x * scale, y * scale, w * scale, h * scale, GL_RGBA, GL_UNSIGNED_BYTE, pixels);
    for (j = 0; j < h; j++) {
        for (i = 0; i < w; i++) {
            const uint8_t *p = pixels + (((size_t)j * scale + scale / 2) * w * scale + (size_t)i * scale + scale / 2) * 4;
            uint16_t pixel = (uint16_t)((p[0] >> 3) | ((p[1] >> 3) << 5) | ((p[2] >> 3) << 10));
            if (!drawnBlocks[(y + j) / BLOCK][(x + i) / BLOCK]) {
                continue; /* uploaded, never drawn over: psxVram holds it */
            }
            psxVram[y + j][x + i] = (psxVram[y + j][x + i] & 0x8000) | pixel;
        }
    }
    free(pixels);
    GpuVramChanged(x, y, w, h);
}

/* Pixels the CPU wrote (LoadImage, MoveImage) go to the framebuffer too, so
   they are shown and drawn over as on the console. */
void GpuWriteFramebuffer(int x, int y, int w, int h)
{
    static GLuint texture, readFramebuffer;
    uint8_t *rgba;
    int i, j;

    if (w <= 0 || h <= 0 || x < 0 || y < 0 || x + w > VRAM_WIDTH || y + h > VRAM_HEIGHT) {
        return;
    }
    Flush();
    if (!texture) {
        glGenTextures(1, &texture);
        glGenFramebuffers(1, &readFramebuffer);
    }
    rgba = malloc((size_t)w * h * 4);
    for (j = 0; j < h; j++) {
        for (i = 0; i < w; i++) {
            uint16_t pixel = psxVram[y + j][x + i];
            uint8_t *out = rgba + ((size_t)j * w + i) * 4;
            out[0] = (uint8_t)((pixel & 31) << 3);
            out[1] = (uint8_t)(((pixel >> 5) & 31) << 3);
            out[2] = (uint8_t)(((pixel >> 10) & 31) << 3);
            out[3] = 255;
        }
    }
    glBindTexture(GL_TEXTURE_2D, texture);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 4);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, rgba);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 2);
    free(rgba);
    glBindFramebuffer(GL_READ_FRAMEBUFFER, readFramebuffer);
    glFramebufferTexture2D(GL_READ_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, texture, 0);
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, framebuffer);
    glDisable(GL_SCISSOR_TEST);
    glBlitFramebuffer(0, 0, w, h, x * scale, y * scale, (x + w) * scale, (y + h) * scale, GL_COLOR_BUFFER_BIT, GL_NEAREST);
    glBindFramebuffer(GL_FRAMEBUFFER, framebuffer);
    ClearDrawn(x, y, w, h);
    CopyToWide(x, y, w, h);
}

/* MoveImage in the drawn picture: the rectangle copied at the display's
   scale (through a texture, as the two may overlap). */
void GpuMoveFramebuffer(int x, int y, int w, int h, int toX, int toY)
{
    static GLuint texture, readFramebuffer;

    if (w <= 0 || h <= 0 || x < 0 || y < 0 || toX < 0 || toY < 0 || x + w > VRAM_WIDTH || y + h > VRAM_HEIGHT ||
        toX + w > VRAM_WIDTH || toY + h > VRAM_HEIGHT) {
        return;
    }
    Flush();
    SyncWide();
    if (!texture) {
        glGenTextures(1, &texture);
        glGenFramebuffers(1, &readFramebuffer);
    }
    glBindFramebuffer(GL_READ_FRAMEBUFFER, framebuffer);
    glBindTexture(GL_TEXTURE_2D, texture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, w * scale, h * scale, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
    glCopyTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, x * scale, y * scale, w * scale, h * scale);
    glBindFramebuffer(GL_READ_FRAMEBUFFER, readFramebuffer);
    glFramebufferTexture2D(GL_READ_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, texture, 0);
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, framebuffer);
    glDisable(GL_SCISSOR_TEST);
    glBlitFramebuffer(0, 0, w * scale, h * scale, toX * scale, toY * scale, (toX + w) * scale, (toY + h) * scale,
                      GL_COLOR_BUFFER_BIT, GL_NEAREST);
    glBindFramebuffer(GL_FRAMEBUFFER, framebuffer);
    /* both copies now agree in the destination */
    ClearDrawn(toX, toY, w, h);
    CopyToWide(toX, toY, w, h);
}

/* Video memory row 0 is drawn at the bottom of the framebuffer (OpenGL
   counts rows upward), so the displayed area is flipped when shown. */
static void Present(int x, int y, int w, int h, int interpolated);
static void AfterPicture(void);

void GpuPresent(int x, int y, int w, int h)
{
    Present(x, y, w, h, 0);
}

void GpuPresentInterpolated(int x, int y, int w, int h)
{
    Present(x, y, w, h, 1);
}

static void Present(int x, int y, int w, int h, int interpolated)
{
    int width, height, outW, outH, outX, outY;

    Flush();
    if (interpolated) {
        PortVideoBetweenFrames();
    }
    PortDrawableSize(&width, &height);
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, windowFramebuffer);
    glDisable(GL_SCISSOR_TEST);
    glViewport(0, 0, width, height);
    glClearColor(0, 0, 0, 1);
    glClear(GL_COLOR_BUFFER_BIT);
    if (w > 0 && h > 0) {
        /* The console shows its picture at 4:3 whatever the width in pixels; the wide one is as much wider
           as its sides add. */
        int wide = wideMargin && x == 0 && w == SCREEN_COLUMNS;
        double across = wide ? WideWidth() : w;
        int left, right;

        if (wide && 240.0 * width / height < across) {
            across = 240.0 * width / height; /* the columns the window shows: the rest is cut off the sides */
        }
        outH = height;
        outW = (int)((double)height * 4 / 3 * across / w);
        if (outW > width) {
            outW = width;
            outH = (int)((double)width * 3 / 4 * w / across);
        }
        outX = (width - outW) / 2;
        outY = (height - outH) / 2;
        if (!wide) {
            SyncWide();
        }
        left = wide ? (int)SDL_lround((WideWidth() - across) / 2 * scale) : x * scale;
        right = wide ? WideWidth() * scale - left : (x + w) * scale;
        glBindFramebuffer(GL_READ_FRAMEBUFFER, wide ? wideFramebuffer : framebuffer);
        glBlitFramebuffer(left, y * scale, right, (y + h) * scale, outX, outY + outH, outX + outW, outY,
                          GL_COLOR_BUFFER_BIT, GL_LINEAR);
    }
    glBindFramebuffer(GL_FRAMEBUFFER, windowFramebuffer);
    PortVideoPresent();
    AfterPicture();
}

/* The second player's game handed its picture over instead of showing it (libgpu.c): as after a picture. */
void GpuPictureHandedOver(void)
{
    AfterPicture();
}

/* The framebuffers made again at another scale, from what they held (enlarged or reduced: the next picture
   is drawn at the new scale). */
static void SetScale(int newScale)
{
    GLuint oldColour = colour, oldFramebuffer = framebuffer, oldDepth = depthBuffer;
    int oldScale = scale, margin = wideMargin;

    Flush();
    SetWideMargin(0); /* (its middle into video memory's first) */
    scale = newScale;
    if (!CreateFramebuffer()) {
        glDeleteFramebuffers(1, &framebuffer);
        glDeleteTextures(1, &colour);
        glDeleteRenderbuffers(1, &depthBuffer);
        colour = oldColour;
        framebuffer = oldFramebuffer;
        depthBuffer = oldDepth;
        scale = oldScale;
    } else {
        glBindFramebuffer(GL_READ_FRAMEBUFFER, oldFramebuffer);
        glBindFramebuffer(GL_DRAW_FRAMEBUFFER, framebuffer);
        glDisable(GL_SCISSOR_TEST);
        glBlitFramebuffer(0, 0, VRAM_WIDTH * oldScale, VRAM_HEIGHT * oldScale, 0, 0, VRAM_WIDTH * scale,
                          VRAM_HEIGHT * scale, GL_COLOR_BUFFER_BIT, scale < oldScale ? GL_LINEAR : GL_NEAREST);
        glDeleteFramebuffers(1, &oldFramebuffer);
        glDeleteTextures(1, &oldColour);
        glDeleteRenderbuffers(1, &oldDepth);
    }
    glBindFramebuffer(GL_FRAMEBUFFER, framebuffer);
    SetWideMargin(margin);
    HdTexSetScale(scale);
}

void GpuSetWindowFramebuffer(unsigned int number)
{
    windowFramebuffer = number;
}

void GpuClearWindow(float r, float g, float b)
{
    int width, height;

    Flush();
    PortDrawableSize(&width, &height);
    glBindFramebuffer(GL_FRAMEBUFFER, windowFramebuffer);
    glDisable(GL_SCISSOR_TEST);
    glViewport(0, 0, width, height);
    glClearColor(r, g, b, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);
}

/* After a new picture: the window's shape, the setting or the end of a race may ask for another margin (a
   picture shown again, as each field shows one, or while the frame recorder holds the game, changes nothing),
   or another scale. */
static void AfterPicture(void)
{
    pictures++;
    if (ChooseScale() != scale) {
        SetScale(ChooseScale());
    }
    if (drawnSincePresent) {
        picturesWithoutScene = sceneWide ? 0 : SDL_min(picturesWithoutScene + 1, 1000);
        picturesWithoutMenu = menuWide ? 0 : SDL_min(picturesWithoutMenu + 1, 1000);
        sceneWide = menuWide = 0;
        drawnSincePresent = 0;
        SetWideMargin(WantedMargin());
    }
}

/* A movie frame: drawn straight to the window through a texture of its own. */
/* The largest rectangle of aspect aw:ah inside (x, y, w, h), centred. */
static void Fit(int x, int y, int w, int h, int aw, int ah, int *ox, int *oy, int *ow, int *oh)
{
    *ow = w;
    *oh = w * ah / aw;
    if (*oh > h) {
        *oh = h;
        *ow = h * aw / ah;
    }
    *ox = x + (w - *ow) / 2;
    *oy = y + (h - *oh) / 2;
}

uint8_t *GpuReadArea(int x, int y, int w, int h, int *outW, int *outH)
{
    static uint8_t *pixels;
    static size_t size;
    size_t need = (size_t)w * scale * h * scale * 4;

    Flush();
    SyncWide();
    if (need > size) {
        pixels = realloc(pixels, need);
        size = need;
    }
    glBindFramebuffer(GL_READ_FRAMEBUFFER, framebuffer);
    glPixelStorei(GL_PACK_ALIGNMENT, 4);
    glReadPixels(x * scale, y * scale, w * scale, h * scale, GL_RGBA, GL_UNSIGNED_BYTE, pixels);
    glBindFramebuffer(GL_FRAMEBUFFER, windowFramebuffer);
    *outW = w * scale;
    *outH = h * scale;
    return pixels;
}

/* The frame recorder (framecap.c): an area of video memory as shown, scaled to outW x outH, RGBA, top row
   first. */
int GpuShownMargin(int x, int w)
{
    return wideMargin && x == 0 && w == SCREEN_COLUMNS ? wideMargin : 0;
}

/* The picture at (x, y, w, h) with its sides (GpuReadPicture) scaled to outW x outH, in a framebuffer bound for
   reading. */
static void ScalePicture(int x, int y, int w, int h, int margin, int outW, int outH)
{
    static GLuint small, smallTexture;
    static int smallW, smallH;
    int wide = margin && margin == GpuShownMargin(x, w);

    Flush();
    SyncWide();
    if (!small || smallW != outW || smallH != outH) {
        if (!small) {
            glGenFramebuffers(1, &small);
            glGenTextures(1, &smallTexture);
        }
        glBindTexture(GL_TEXTURE_2D, smallTexture);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, outW, outH, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
        glBindFramebuffer(GL_FRAMEBUFFER, small);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, smallTexture, 0);
        smallW = outW;
        smallH = outH;
    }
    glDisable(GL_SCISSOR_TEST);
    glBindFramebuffer(GL_READ_FRAMEBUFFER, wide ? wideFramebuffer : framebuffer);
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, small);
    if (wide) {
        glBlitFramebuffer(0, y * scale, WideWidth() * scale, (y + h) * scale, 0, 0, outW, outH, GL_COLOR_BUFFER_BIT,
                          GL_LINEAR);
    } else {
        glBlitFramebuffer(x * scale, y * scale, (x + w) * scale, (y + h) * scale, 0, 0, outW, outH, GL_COLOR_BUFFER_BIT,
                          GL_LINEAR);
    }
    glBindFramebuffer(GL_READ_FRAMEBUFFER, small);
    glPixelStorei(GL_PACK_ALIGNMENT, 4);
}

uint8_t *GpuReadPicture(int x, int y, int w, int h, int margin, int outW, int outH)
{
    static uint8_t *pixels;
    static size_t size;

    if ((size_t)outW * outH * 4 > size) {
        size = (size_t)outW * outH * 4;
        pixels = realloc(pixels, size);
    }
    ScalePicture(x, y, w, h, margin, outW, outH);
    glReadPixels(0, 0, outW, outH, GL_RGBA, GL_UNSIGNED_BYTE, pixels);
    glBindFramebuffer(GL_FRAMEBUFFER, windowFramebuffer);
    return pixels; /* video memory's rows go up the framebuffer: the first row read is the picture's top */
}

/* Pictures on their way back, one at a time for each reader (GpuRead*; GPU_READ_ASYNC: into a buffer, done
   when the fence is passed). */
static struct {
    int busy, w, h;
#ifdef GPU_READ_ASYNC
    GLuint buffer;
    GLsizeiptr bufferSize; /* (made again only for a larger picture: a driver may keep every one given up) */
    GLsync fence;
#else
    uint8_t *pixels;
    size_t size;
#endif
} reading[GPU_READERS];

int GpuReadPictureBusy(int reader)
{
    return reading[reader].busy;
}

int GpuReadPictureStart(int reader, int x, int y, int w, int h, int margin, int outW, int outH)
{
    if (reading[reader].busy) {
        return 0;
    }
#ifdef GPU_READ_ASYNC
    ScalePicture(x, y, w, h, margin, outW, outH);
    if (!reading[reader].buffer) {
        glGenBuffers(1, &reading[reader].buffer);
    }
    glBindBuffer(GL_PIXEL_PACK_BUFFER, reading[reader].buffer);
    if ((GLsizeiptr)outW * outH * 4 > reading[reader].bufferSize) {
        reading[reader].bufferSize = (GLsizeiptr)outW * outH * 4;
        glBufferData(GL_PIXEL_PACK_BUFFER, reading[reader].bufferSize, NULL, GL_STREAM_READ);
    }
    glReadPixels(0, 0, outW, outH, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
    glBindBuffer(GL_PIXEL_PACK_BUFFER, 0);
    reading[reader].fence = glFenceSync(GL_SYNC_GPU_COMMANDS_COMPLETE, 0);
    glFlush();
    glBindFramebuffer(GL_FRAMEBUFFER, windowFramebuffer);
#else
    {
        /* (read at once: a copy of its own, as GpuReadPicture's buffer is shared) */
        size_t need = (size_t)outW * outH * 4;
        const uint8_t *pixels = GpuReadPicture(x, y, w, h, margin, outW, outH);

        if (need > reading[reader].size) {
            reading[reader].pixels = realloc(reading[reader].pixels, need);
            reading[reader].size = need;
        }
        memcpy(reading[reader].pixels, pixels, need);
    }
#endif
    reading[reader].w = outW;
    reading[reader].h = outH;
    reading[reader].busy = 1;
    return 1;
}

int GpuReadPictureDone(int reader, int wait, void (*use)(const uint8_t *pixels, int width, int height, void *user),
                       void *user)
{
    if (!reading[reader].busy) {
        return 0;
    }
#ifdef GPU_READ_ASYNC
    {
        GLenum status = glClientWaitSync(reading[reader].fence, wait ? GL_SYNC_FLUSH_COMMANDS_BIT : 0,
                                         wait ? 1000000000ull : 0);
        void *pixels;

        if (status == GL_TIMEOUT_EXPIRED) {
            return 0;
        }
        glDeleteSync(reading[reader].fence);
        reading[reader].fence = NULL;
        reading[reader].busy = 0;
        if (status == GL_WAIT_FAILED) {
            return 0;
        }
        glBindBuffer(GL_PIXEL_PACK_BUFFER, reading[reader].buffer);
        pixels = glMapBufferRange(GL_PIXEL_PACK_BUFFER, 0, (GLsizeiptr)reading[reader].w * reading[reader].h * 4,
                                  GL_MAP_READ_BIT);
        if (pixels) {
            use((const uint8_t *)pixels, reading[reader].w, reading[reader].h, user);
            glUnmapBuffer(GL_PIXEL_PACK_BUFFER);
        }
        glBindBuffer(GL_PIXEL_PACK_BUFFER, 0);
        return pixels != NULL;
    }
#else
    (void)wait;
    reading[reader].busy = 0;
    use(reading[reader].pixels, reading[reader].w, reading[reader].h, user);
    return 1;
#endif
}

void GpuPresentSplit(int x, int y, int w, int h, const uint8_t *lower, int lowerW, int lowerH, int lowerNew)
{
    static GLuint texture, readFramebuffer;
    static int textureW, textureH;
    int width, height, ox, oy, ow, oh;

    /* widescreen: each half as wide as the window, which is as wide as the picture with its sides (half the
       height across the same width is the band's shape) */
    int wide = GpuShownMargin(x, w) != 0, across = wide ? WideWidth() : w;

    Flush();
    if (!wide) {
        SyncWide();
    }
    PortDrawableSize(&width, &height);
    /* the other game draws the lower half as wide, and hands it over at its size in pixels */
    PortLinkSetBand(GpuShownMargin(x, w), width, height / 2);
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, windowFramebuffer);
    glDisable(GL_SCISSOR_TEST);
    glViewport(0, 0, width, height);
    glClearColor(0, 0, 0, 1);
    glClear(GL_COLOR_BUFFER_BIT);
    /* the upper half: this game's band (OpenGL's rows go up: the upper half is from height / 2) */
    Fit(0, height - height / 2, width, height / 2, across, h, &ox, &oy, &ow, &oh);
    glBindFramebuffer(GL_READ_FRAMEBUFFER, wide ? wideFramebuffer : framebuffer);
    glBlitFramebuffer(wide ? 0 : x * scale, y * scale, (wide ? across : x + w) * scale, (y + h) * scale, ox, oy + oh,
                      ox + ow, oy, GL_COLOR_BUFFER_BIT, GL_LINEAR);
    /* the lower half: the other game's band, as it read it (rows from the bottom) */
    if (lower && lowerW > 0 && lowerH > 0) {
        if (!texture) {
            glGenTextures(1, &texture);
            glGenFramebuffers(1, &readFramebuffer);
        }
        glBindTexture(GL_TEXTURE_2D, texture);
        glPixelStorei(GL_UNPACK_ALIGNMENT, 4);
        if (lowerW != textureW || lowerH != textureH) {
            glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, lowerW, lowerH, 0, GL_RGBA, GL_UNSIGNED_BYTE, lower);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
            textureW = lowerW;
            textureH = lowerH;
        } else if (lowerNew) {
            glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, lowerW, lowerH, GL_RGBA, GL_UNSIGNED_BYTE, lower);
        }
        glPixelStorei(GL_UNPACK_ALIGNMENT, 2);
        Fit(0, 0, width, height / 2, lowerW, lowerH, &ox, &oy, &ow, &oh);
        glBindFramebuffer(GL_READ_FRAMEBUFFER, readFramebuffer);
        glFramebufferTexture2D(GL_READ_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, texture, 0);
        glBlitFramebuffer(0, 0, lowerW, lowerH, ox, oy + oh, ox + ow, oy, GL_COLOR_BUFFER_BIT, GL_LINEAR);
    }
    glBindFramebuffer(GL_FRAMEBUFFER, windowFramebuffer);
    PortVideoPresent();
    AfterPicture();
}

void GpuShowImage(const uint8_t *rgb, int width, int height)
{
    static GLuint texture, readFramebuffer;
    static int textureWidth, textureHeight;
    int windowWidth, windowHeight, outW, outH, outX, outY;

    Flush();
    if (!texture) {
        glGenTextures(1, &texture);
        glGenFramebuffers(1, &readFramebuffer);
    }
    glBindTexture(GL_TEXTURE_2D, texture);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    if (width != textureWidth || height != textureHeight) {
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB8, width, height, 0, GL_RGB, GL_UNSIGNED_BYTE, rgb);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        textureWidth = width;
        textureHeight = height;
    } else {
        glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, width, height, GL_RGB, GL_UNSIGNED_BYTE, rgb);
    }
    glPixelStorei(GL_UNPACK_ALIGNMENT, 2);
    PortDrawableSize(&windowWidth, &windowHeight);
    outH = windowHeight;
    outW = windowHeight * 4 / 3;
    if (outW > windowWidth) {
        outW = windowWidth;
        outH = windowWidth * 3 / 4;
    }
    /* square pixels, as the console's 320x240 picture: a wider image is
       letterboxed */
    if (outW * height / width < outH) {
        outH = outW * height / width;
    }
    outX = (windowWidth - outW) / 2;
    outY = (windowHeight - outH) / 2;
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, windowFramebuffer);
    glDisable(GL_SCISSOR_TEST);
    glViewport(0, 0, windowWidth, windowHeight);
    glClearColor(0, 0, 0, 1);
    glClear(GL_COLOR_BUFFER_BIT);
    glBindFramebuffer(GL_READ_FRAMEBUFFER, readFramebuffer);
    glFramebufferTexture2D(GL_READ_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, texture, 0);
    /* image row 0 is the top: flip into OpenGL's upward rows */
    glBlitFramebuffer(0, 0, width, height, outX, outY + outH, outX + outW, outY, GL_COLOR_BUFFER_BIT, GL_LINEAR);
    glBindFramebuffer(GL_FRAMEBUFFER, windowFramebuffer);
    PortVideoPresent();
}

