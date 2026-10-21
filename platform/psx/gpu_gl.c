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
   known. */
#include <SDL3/SDL.h>
#include <SDL3/SDL_opengl.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>

#include "gpu.h"
#include "../platform.h"

#define GL_FUNCTIONS(X) \
    X(PFNGLGENBUFFERSPROC, glGenBuffers) \
    X(PFNGLBINDBUFFERPROC, glBindBuffer) \
    X(PFNGLBUFFERDATAPROC, glBufferData) \
    X(PFNGLGENVERTEXARRAYSPROC, glGenVertexArrays) \
    X(PFNGLBINDVERTEXARRAYPROC, glBindVertexArray) \
    X(PFNGLENABLEVERTEXATTRIBARRAYPROC, glEnableVertexAttribArray) \
    X(PFNGLVERTEXATTRIBPOINTERPROC, glVertexAttribPointer) \
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
    X(PFNGLUNIFORM2FPROC, glUniform2f) \
    X(PFNGLUNIFORM4IPROC, glUniform4i) \
    X(PFNGLGENFRAMEBUFFERSPROC, glGenFramebuffers) \
    X(PFNGLBINDFRAMEBUFFERPROC, glBindFramebuffer) \
    X(PFNGLFRAMEBUFFERTEXTURE2DPROC, glFramebufferTexture2D) \
    X(PFNGLCHECKFRAMEBUFFERSTATUSPROC, glCheckFramebufferStatus) \
    X(PFNGLBLITFRAMEBUFFERPROC, glBlitFramebuffer) \
    X(PFNGLBLENDEQUATIONPROC, glBlendEquation) \
    X(PFNGLBLENDCOLORPROC, glBlendColor) \
    X(PFNGLACTIVETEXTUREPROC, glActiveTexture) \
    X(PFNGLGENERATEMIPMAPPROC, glGenerateMipmap)

#define DECLARE_GL(type, name) static type p_##name;
GL_FUNCTIONS(DECLARE_GL)
#define glGenBuffers p_glGenBuffers
#define glBindBuffer p_glBindBuffer
#define glBufferData p_glBufferData
#define glGenVertexArrays p_glGenVertexArrays
#define glBindVertexArray p_glBindVertexArray
#define glEnableVertexAttribArray p_glEnableVertexAttribArray
#define glVertexAttribPointer p_glVertexAttribPointer
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
#define glUniform2f p_glUniform2f
#define glUniform4i p_glUniform4i
#define glGenFramebuffers p_glGenFramebuffers
#define glBindFramebuffer p_glBindFramebuffer
#define glFramebufferTexture2D p_glFramebufferTexture2D
#define glCheckFramebufferStatus p_glCheckFramebufferStatus
#define glBlitFramebuffer p_glBlitFramebuffer
#define glBlendEquation p_glBlendEquation
#define glBlendColor p_glBlendColor
#define glActiveTexture p_glActiveTexture
#define glGenerateMipmap p_glGenerateMipmap

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
    return 1;
}

/* ---- state ---- */

static int scale = 1;
static GLuint framebuffer, colour;   /* all of video memory, scaled */
static GLuint vramTexture;           /* the CPU copy, R16UI */
static GLuint glyphTexture;          /* the glyph page (GpuSetGlyphMemory): RGBA, premultiplied, filtered */
static GLuint program, vertexArray, vertexBuffer;
static GLint uniformVram, uniformGlyphs, uniformTexture, uniformClut, uniformWindow, uniformMode, uniformTarget;
static int vramDirty = 1;
static int dirtyLeft, dirtyTop, dirtyRight, dirtyBottom;
static int areaX, areaY, areaW = VRAM_WIDTH, areaH = VRAM_HEIGHT;
static int initialised;

/* A batch of triangles with one draw state. */
#define BATCH_VERTICES 8192
typedef struct BatchVertex {
    float x, y, w;
    float r, g, b;
    float u, v;
} BatchVertex;
static BatchVertex batch[BATCH_VERTICES];
static int batchCount;
static GpuDrawState batchState;

static const char *vertexSource =
    "#version 330 core\n"
    "in vec3 position;\n"
    "in vec3 colour;\n"
    "in vec2 texel;\n"
    "uniform vec2 target;\n"           /* framebuffer size in video-memory pixels */
    "out vec3 vColour;\n"
    "out vec3 vTexel;\n"               /* texel * w, w (perspective) */
    "noperspective out vec2 vTexelAffine;\n"
    "void main() {\n"
    "    float w = position.z > 0.0 ? position.z : 1.0;\n"
    "    vec2 ndc = vec2(position.x / target.x * 2.0 - 1.0, position.y / target.y * 2.0 - 1.0);\n"
    "    gl_Position = vec4(ndc * w, 0.0, w);\n"
    "    vColour = colour;\n"
    "    vTexel = vec3(texel, 1.0);\n"
    "    vTexelAffine = texel;\n"
    "}\n";

/* mode: x = textured (0/1), y = raw, z = pass (0 all, 1 opaque texels, 2 semi texels), w = depth
   texturePage: page x, page y; clut: x, y; window: mask x, mask y, offset x, offset y */
static const char *fragmentSource =
    "#version 330 core\n"
    "in vec3 vColour;\n"
    "in vec3 vTexel;\n"
    "noperspective in vec2 vTexelAffine;\n"
    "uniform usampler2D vram;\n"
    "uniform sampler2D glyphs;\n"
    "uniform ivec4 texturePage;\n"
    "uniform ivec4 clut;\n"
    "uniform ivec4 window;\n"
    "uniform ivec4 mode;\n"
    "out vec4 fragment;\n"
    "uint Fetch(ivec2 p) { return texelFetch(vram, ivec2(p.x & 1023, p.y & 511), 0).r; }\n"
    "void main() {\n"
    "    vec3 c = vColour;\n"
    "    if (mode.x == 0) {\n"
    "        if (mode.z == 1) discard;\n"
    "        fragment = vec4(c, 1.0);\n"
    "        return;\n"
    "    }\n"
    "    vec2 t = vTexel.xy;\n"
    "    if (mode.w == 3) {\n"                 /* the platform's glyphs: smooth, with coverage */
    "        vec4 g = texture(glyphs, t / 256.0);\n"
    "        if (g.a < 0.004) discard;\n"
    "        vec3 rgb = mode.y == 0 ? min(g.rgb * c * (255.0 / 128.0), vec3(g.a)) : g.rgb;\n"
    "        fragment = vec4(rgb, g.a);\n"
    "        return;\n"
    "    }\n"
    "    ivec2 uv = ivec2(floor(t)) & 255;\n"
    "    uv.x = (uv.x & ~(window.x * 8)) | ((window.z & window.x) * 8);\n"
    "    uv.y = (uv.y & ~(window.y * 8)) | ((window.w & window.y) * 8);\n"
    "    uint texel;\n"
    "    if (mode.w == 0) {\n"
    "        uint word = Fetch(texturePage.xy + ivec2(uv.x >> 2, uv.y));\n"
    "        uint index = (word >> uint((uv.x & 3) * 4)) & 15u;\n"
    "        texel = Fetch(clut.xy + ivec2(int(index), 0));\n"
    "    } else if (mode.w == 1) {\n"
    "        uint word = Fetch(texturePage.xy + ivec2(uv.x >> 1, uv.y));\n"
    "        uint index = (word >> uint((uv.x & 1) * 8)) & 255u;\n"
    "        texel = Fetch(clut.xy + ivec2(int(index), 0));\n"
    "    } else {\n"
    "        texel = Fetch(texturePage.xy + uv);\n"
    "    }\n"
    "    if (texel == 0u) discard;\n"
    "    bool semi = (texel & 0x8000u) != 0u;\n"
    "    if (mode.z == 1 && semi) discard;\n"
    "    if (mode.z == 2 && !semi) discard;\n"
    "    vec3 t8 = vec3(float(texel & 31u), float((texel >> 5) & 31u), float((texel >> 10) & 31u)) * 8.0 / 255.0;\n"
    "    if (mode.y == 0) {\n"
    "        t8 = min(t8 * c * (255.0 / 128.0), vec3(1.0));\n"
    "    }\n"
    "    fragment = vec4(t8, 1.0);\n"
    "}\n";

static GLuint Compile(GLenum type, const char *source)
{
    GLuint shader = glCreateShader(type);
    GLint ok;

    glShaderSource(shader, 1, &source, NULL);
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

/* The framebuffer's scale: enough for the window, at least 1. */
static int ChooseScale(void)
{
    int width, height, s;

    PortDrawableSize(&width, &height);
    s = (height + 239) / 240;
    if (s < 1) {
        s = 1;
    }
    if (s > 8) {
        s = 8;
    }
    return s;
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
    uniformTexture = glGetUniformLocation(program, "texturePage");
    uniformClut = glGetUniformLocation(program, "clut");
    uniformWindow = glGetUniformLocation(program, "window");
    uniformMode = glGetUniformLocation(program, "mode");
    uniformTarget = glGetUniformLocation(program, "target");

    glGenVertexArrays(1, &vertexArray);
    glBindVertexArray(vertexArray);
    glGenBuffers(1, &vertexBuffer);
    glBindBuffer(GL_ARRAY_BUFFER, vertexBuffer);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(BatchVertex), (void *)offsetof(BatchVertex, x));
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(BatchVertex), (void *)offsetof(BatchVertex, r));
    glEnableVertexAttribArray(2);
    glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, sizeof(BatchVertex), (void *)offsetof(BatchVertex, u));

    glPixelStorei(GL_UNPACK_ALIGNMENT, 2);
    glPixelStorei(GL_PACK_ALIGNMENT, 1);
    glGenTextures(1, &vramTexture);
    glBindTexture(GL_TEXTURE_2D, vramTexture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_R16UI, VRAM_WIDTH, VRAM_HEIGHT, 0, GL_RED_INTEGER, GL_UNSIGNED_SHORT, psxVram);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

    glGenTextures(1, &glyphTexture);
    glBindTexture(GL_TEXTURE_2D, glyphTexture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, GLYPH_ATLAS_SIZE, GLYPH_ATLAS_SIZE, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    scale = ChooseScale();
    glGenTextures(1, &colour);
    glBindTexture(GL_TEXTURE_2D, colour);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, VRAM_WIDTH * scale, VRAM_HEIGHT * scale, 0, GL_RGBA, GL_UNSIGNED_BYTE,
                 NULL);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glGenFramebuffers(1, &framebuffer);
    glBindFramebuffer(GL_FRAMEBUFFER, framebuffer);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, colour, 0);
    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
        SDL_Log("framebuffer incomplete");
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

static void UploadVram(void)
{
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

static void SetScissor(void)
{
    glEnable(GL_SCISSOR_TEST);
    glScissor(areaX * scale, areaY * scale, areaW * scale, areaH * scale);
}

static int SameState(const GpuDrawState *a, const GpuDrawState *b)
{
    return memcmp(a, b, sizeof(*a)) == 0;
}

static void SetBlend(int semi)
{
    switch (semi) {
    case 0: /* B/2 + F/2 */
        glEnable(GL_BLEND);
        glBlendEquation(GL_FUNC_ADD);
        glBlendColor(0, 0, 0, 0.5f);
        glBlendFunc(GL_CONSTANT_ALPHA, GL_CONSTANT_ALPHA);
        break;
    case 1: /* B + F */
        glEnable(GL_BLEND);
        glBlendEquation(GL_FUNC_ADD);
        glBlendFunc(GL_ONE, GL_ONE);
        break;
    case 2: /* B - F */
        glEnable(GL_BLEND);
        glBlendEquation(GL_FUNC_REVERSE_SUBTRACT);
        glBlendFunc(GL_ONE, GL_ONE);
        break;
    case 3: /* B + F/4 */
        glEnable(GL_BLEND);
        glBlendEquation(GL_FUNC_ADD);
        glBlendColor(0, 0, 0, 0.25f);
        glBlendFunc(GL_CONSTANT_ALPHA, GL_ONE);
        break;
    case 4: /* the platform's glyphs: their coverage, premultiplied */
        glEnable(GL_BLEND);
        glBlendEquation(GL_FUNC_ADD);
        glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
        break;
    default:
        glDisable(GL_BLEND);
        break;
    }
}

static void DrawBatch(int pass)
{
    const GpuDrawState *s = &batchState;

    glUniform4i(uniformMode, s->textured, s->raw, pass, s->depth);
    if (pass == 1) {
        SetBlend(-1);
    } else if (s->textured && s->depth == 3 && s->semi < 0) {
        SetBlend(4);
    } else {
        SetBlend(s->semi);
    }
    glDrawArrays(GL_TRIANGLES, 0, batchCount);
}

static void Flush(void)
{
    const GpuDrawState *s = &batchState;

    if (!batchCount) {
        return;
    }
    UploadVram();
    glBindFramebuffer(GL_FRAMEBUFFER, framebuffer);
    glViewport(0, 0, VRAM_WIDTH * scale, VRAM_HEIGHT * scale);
    SetScissor();
    glUseProgram(program);
    glBindVertexArray(vertexArray);
    glBindBuffer(GL_ARRAY_BUFFER, vertexBuffer);
    glBufferData(GL_ARRAY_BUFFER, (GLsizeiptr)(batchCount * sizeof(BatchVertex)), batch, GL_STREAM_DRAW);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, vramTexture);
    glUniform1i(uniformVram, 0);
    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, glyphTexture);
    glUniform1i(uniformGlyphs, 1);
    glActiveTexture(GL_TEXTURE0);
    glUniform2f(uniformTarget, (float)VRAM_WIDTH, (float)VRAM_HEIGHT);
    glUniform4i(uniformTexture, s->pageX, s->pageY, 0, 0);
    glUniform4i(uniformClut, s->clutX, s->clutY, 0, 0);
    glUniform4i(uniformWindow, s->windowMaskX, s->windowMaskY, s->windowOffsetX, s->windowOffsetY);
    if (s->textured && s->semi >= 0) {
        /* Texels without the semi-transparency bit are drawn opaque, the
           others blended: two passes, each discarding the other's texels. */
        DrawBatch(1);
        DrawBatch(2);
    } else {
        DrawBatch(0);
    }
    batchCount = 0;
}

void GpuFlush(void)
{
    Flush();
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
}

/* Semi-transparent textured primitives must not share a batch: each one's
   opaque and blended passes have to come before the next primitive. */
static void Begin(const GpuDrawState *state, int vertices)
{
    if (batchCount && (!SameState(state, &batchState) || batchCount + vertices > BATCH_VERTICES ||
                       (state->textured && state->semi >= 0))) {
        Flush();
    }
    batchState = *state;
}

static void Add(const GpuVertex *v)
{
    BatchVertex *out = &batch[batchCount++];

    out->x = v->x;
    out->y = v->y;
    out->w = v->w;
    out->r = v->r / 255.0f;
    out->g = v->g / 255.0f;
    out->b = v->b / 255.0f;
    out->u = v->u;
    out->v = v->v;
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

void GpuTriangles(const GpuDrawState *state, const GpuVertex *vertices, int count)
{
    int i;
    int perspective = 1;
    float minX = vertices[0].x, minY = vertices[0].y, maxX = minX, maxY = minY;

    for (i = 1; i < count; i++) {
        minX = SDL_min(minX, vertices[i].x);
        minY = SDL_min(minY, vertices[i].y);
        maxX = SDL_max(maxX, vertices[i].x);
        maxY = SDL_max(maxY, vertices[i].y);
    }
    MarkDrawn(minX, minY, maxX, maxY);

    for (i = 0; i < count; i++) {
        if (vertices[i].w <= 0.0f) {
            perspective = 0;
        }
    }
    Begin(state, count);
    for (i = 0; i < count; i++) {
        GpuVertex v = vertices[i];
        if (!perspective) {
            v.w = 0.0f;
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
    ClearDrawn(x, y, w, h);
    glBindFramebuffer(GL_FRAMEBUFFER, framebuffer);
    glEnable(GL_SCISSOR_TEST);
    glScissor(x * scale, y * scale, w * scale, h * scale);
    glClearColor(r / 255.0f, g / 255.0f, b / 255.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);
    glDisable(GL_SCISSOR_TEST);
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
}

/* Video memory row 0 is drawn at the bottom of the framebuffer (OpenGL
   counts rows upward), so the displayed area is flipped when shown. */
void GpuPresent(int x, int y, int w, int h)
{
    int width, height, outW, outH, outX, outY;

    Flush();
    PortDrawableSize(&width, &height);
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
    glDisable(GL_SCISSOR_TEST);
    glViewport(0, 0, width, height);
    glClearColor(0, 0, 0, 1);
    glClear(GL_COLOR_BUFFER_BIT);
    if (w > 0 && h > 0) {
        /* The console shows its picture at 4:3 whatever the width in pixels. */
        outH = height;
        outW = height * 4 / 3;
        if (outW > width) {
            outW = width;
            outH = width * 3 / 4;
        }
        outX = (width - outW) / 2;
        outY = (height - outH) / 2;
        glBindFramebuffer(GL_READ_FRAMEBUFFER, framebuffer);
        glBlitFramebuffer(x * scale, y * scale, (x + w) * scale, (y + h) * scale, outX, outY + outH, outX + outW, outY,
                          GL_COLOR_BUFFER_BIT, GL_LINEAR);
    }
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    PortVideoPresent();
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
    if (need > size) {
        pixels = realloc(pixels, need);
        size = need;
    }
    glBindFramebuffer(GL_READ_FRAMEBUFFER, framebuffer);
    glPixelStorei(GL_PACK_ALIGNMENT, 4);
    glReadPixels(x * scale, y * scale, w * scale, h * scale, GL_RGBA, GL_UNSIGNED_BYTE, pixels);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    *outW = w * scale;
    *outH = h * scale;
    return pixels;
}

void GpuPresentSplit(int x, int y, int w, int h, const uint8_t *lower, int lowerW, int lowerH, int lowerNew)
{
    static GLuint texture, readFramebuffer;
    static int textureW, textureH;
    int width, height, ox, oy, ow, oh;

    Flush();
    PortDrawableSize(&width, &height);
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
    glDisable(GL_SCISSOR_TEST);
    glViewport(0, 0, width, height);
    glClearColor(0, 0, 0, 1);
    glClear(GL_COLOR_BUFFER_BIT);
    /* the upper half: this game's band (OpenGL's rows go up: the upper half is from height / 2) */
    Fit(0, height - height / 2, width, height / 2, w, h, &ox, &oy, &ow, &oh);
    glBindFramebuffer(GL_READ_FRAMEBUFFER, framebuffer);
    glBlitFramebuffer(x * scale, y * scale, (x + w) * scale, (y + h) * scale, ox, oy + oh, ox + ow, oy,
                      GL_COLOR_BUFFER_BIT, GL_LINEAR);
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
        Fit(0, 0, width, height / 2, w, h, &ox, &oy, &ow, &oh);
        glBindFramebuffer(GL_READ_FRAMEBUFFER, readFramebuffer);
        glFramebufferTexture2D(GL_READ_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, texture, 0);
        glBlitFramebuffer(0, 0, lowerW, lowerH, ox, oy + oh, ox + ow, oy, GL_COLOR_BUFFER_BIT, GL_LINEAR);
    }
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    PortVideoPresent();
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
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, width, height, 0, GL_RGB, GL_UNSIGNED_BYTE, rgb);
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
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
    glDisable(GL_SCISSOR_TEST);
    glViewport(0, 0, windowWidth, windowHeight);
    glClearColor(0, 0, 0, 1);
    glClear(GL_COLOR_BUFFER_BIT);
    glBindFramebuffer(GL_READ_FRAMEBUFFER, readFramebuffer);
    glFramebufferTexture2D(GL_READ_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, texture, 0);
    /* image row 0 is the top: flip into OpenGL's upward rows */
    glBlitFramebuffer(0, 0, width, height, outX, outY + outH, outX + outW, outY, GL_COLOR_BUFFER_BIT, GL_LINEAR);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    PortVideoPresent();
}
