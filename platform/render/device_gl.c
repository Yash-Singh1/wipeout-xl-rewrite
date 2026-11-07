/* OpenGL 3.3 / GLES 3 / WebGL 2 implementation of the device contract. */
#include "../platform.h"
#include "../port_gl.h"
#include "device.h"
#include "gl_functions.h"
#include "shaders_gl.h"
#include <SDL3/SDL.h>
#include <stddef.h>
#include <string.h>
#ifdef __EMSCRIPTEN__
#include <emscripten.h>
EM_JS(void, WebFrameSubmitted, (void), { Module.gamePresentation.submitted(GLctx); });
#endif

typedef struct GlImage {
    GLuint name;
    RenderImageDesc desc;
} GlImage;
typedef struct GlProgram {
    GLuint name;
    GLint uniform[13];
    RenderUniforms last;
    int valid;
} GlProgram;
typedef struct GlDevice {
    SDL_Window *window;
    SDL_GLContext context;
    int swapInterval;
    GLuint framebuffer, readFramebuffer, vertexArray, windowFramebuffer;
    GlProgram programs[RENDER_SHADER_COUNT];
    RenderDraw last;
    int valid;
    struct GlReadback *freeReads[2];
} GlDevice;
#ifdef GPU_READ_ASYNC
static void DestroyReadPool(GlDevice *g);
#endif
static GlImage *Image(uintptr_t p) { return (GlImage *)p; }
static GLuint Texture(uintptr_t p) { return p ? Image(p)->name : 0; }
static GLenum ImageFormat(RenderFormat f) {
    return f == RENDER_R16_UINT ? GL_RED_INTEGER : f == RENDER_DEPTH ? GL_DEPTH_COMPONENT : GL_RGBA;
}
static GLenum ImageType(RenderFormat f) {
    return f == RENDER_R16_UINT ? GL_UNSIGNED_SHORT
           : f == RENDER_DEPTH  ? GL_FLOAT
                                : GL_UNSIGNED_BYTE;
}
static GLuint Shader(GLenum type, const char *source) {
    const char *parts[] = {PORT_GLSL_VERSION, source};
    GLuint shader = glCreateShader(type);
    GLint ok;
    glShaderSource(shader, 2, parts, NULL);
    glCompileShader(shader);
    glGetShaderiv(shader, GL_COMPILE_STATUS, &ok);
    if (!ok) {
        char why[2048];
        glGetShaderInfoLog(shader, sizeof(why), NULL, why);
        SDL_SetError("OpenGL shader compilation failed: %s", why);
        SDL_Log("OpenGL shader: %s", why);
        glDeleteShader(shader);
        return 0;
    }
    return shader;
}
static int Program(GlProgram *p, RenderShader kind) {
    const char *fragments[] = {gameFragment, averageFragment, detailFragment, compositeFragment,
                               uiFragment};
    GLuint vs = Shader(GL_VERTEX_SHADER, kind == RENDER_GAME ? gameVertex
                                         : kind == RENDER_UI ? uiVertex
                                                             : pictureVertex);
    GLuint fs = Shader(GL_FRAGMENT_SHADER, fragments[kind]);
    GLint ok;
    if (!vs || !fs) {
        if (vs)
            glDeleteShader(vs);
        if (fs)
            glDeleteShader(fs);
        return 0;
    }
    p->name = glCreateProgram();
    glAttachShader(p->name, vs);
    glAttachShader(p->name, fs);
    const char *attrs[] = {"position", "colour", "texel", "texture",
                           "window",   "modeIn", "hdIn",  "hdTexelsIn"};
    for (int i = 0; i < 8; i++)
        glBindAttribLocation(p->name, i, attrs[i]);
    glLinkProgram(p->name);
    glGetProgramiv(p->name, GL_LINK_STATUS, &ok);
    glDeleteShader(vs);
    glDeleteShader(fs);
    if (!ok) {
        char why[2048];
        glGetProgramInfoLog(p->name, sizeof(why), NULL, why);
        SDL_SetError("OpenGL program link failed: %s", why);
        SDL_Log("OpenGL program: %s", why);
        return 0;
    }
    glUseProgram(p->name);
    const char *samplers[] = {"vram", "glyphs", "hdAtlas", "portPages", "decoded"};
    for (int i = 0; i < 5; i++)
        glUniform1i(glGetUniformLocation(p->name, samplers[i]), i);
    glUniform1i(glGetUniformLocation(p->name, "source"), 0);
    glUniform1i(glGetUniformLocation(p->name, "coarse"), 1);
    const char *names[] = {"target",  "origin",   "depthPull",   "pass",     "depthPass",
                           "rect",    "factor",   "targetScale", "adaptive", "stepSize",
                           "uiScale", "uiOffset", "rasterScale"};
    for (int i = 0; i < 13; i++)
        p->uniform[i] = glGetUniformLocation(p->name, names[i]);
    return 1;
}
static int Create(RenderDevice *d, void *window) {
    SDL_Window *surface = window ? window : SDL_GL_GetCurrentWindow();
    SDL_GLContext context = SDL_GL_GetCurrentContext();
    if (!context && surface)
        context = SDL_GetPointerProperty(SDL_GetWindowProperties(surface), "wipeout.opengl.context",
                                         NULL);
    if (context && surface && !SDL_GL_MakeCurrent(surface, context))
        return 0;
    if (!LoadFunctions())
        return 0;
    GlDevice *g = SDL_calloc(1, sizeof(*g));
    if (!g)
        return 0;
    d->data = g;
    g->window = surface;
    g->context = context;
    g->swapInterval = -2;
#ifdef SDL_PLATFORM_IOS
    if (window)
        g->windowFramebuffer = (GLuint)SDL_GetNumberProperty(
            SDL_GetWindowProperties(window), SDL_PROP_WINDOW_UIKIT_OPENGL_FRAMEBUFFER_NUMBER, 0);
#endif
    glGenFramebuffers(1, &g->framebuffer);
    glGenFramebuffers(1, &g->readFramebuffer);
    glGenVertexArrays(1, &g->vertexArray);
    glGetIntegerv(GL_MAX_TEXTURE_SIZE, &d->caps.maxTextureSize);
    glGetIntegerv(GL_MAX_ARRAY_TEXTURE_LAYERS, &d->caps.maxArrayLayers);
    SDL_strlcpy(d->caps.deviceName, (const char *)glGetString(GL_RENDERER),
                sizeof(d->caps.deviceName));
#ifdef GPU_READ_ASYNC
    d->caps.asyncReadback = 1;
#endif
    for (int i = 0; i < RENDER_SHADER_COUNT; i++)
        if (!Program(&g->programs[i], (RenderShader)i))
            return 0;
    return 1;
}
static void Destroy(RenderDevice *d) {
    GlDevice *g = d->data;
    if (!g)
        return;
    glFinish();
#ifdef GPU_READ_ASYNC
    DestroyReadPool(g);
#endif
    for (int i = 0; i < RENDER_SHADER_COUNT; i++)
        if (g->programs[i].name)
            glDeleteProgram(g->programs[i].name);
    glDeleteFramebuffers(1, &g->framebuffer);
    glDeleteFramebuffers(1, &g->readFramebuffer);
    glDeleteVertexArrays(1, &g->vertexArray);
    SDL_free(g);
    d->data = NULL;
}
static uintptr_t NewImage(RenderDevice *d, RenderImageDesc desc) {
    ((GlDevice *)d->data)->valid = 0;
    GlImage *im = SDL_calloc(1, sizeof(*im));
    if (!im)
        return 0;
    im->desc = desc;
    GLenum target = desc.layers > 1 ? GL_TEXTURE_2D_ARRAY : GL_TEXTURE_2D;
    GLint format = desc.format == RENDER_R16_UINT ? GL_R16UI
                   : desc.format == RENDER_DEPTH  ? GL_DEPTH_COMPONENT24
                                                  : GL_RGBA8;
    glGenTextures(1, &im->name);
    glBindTexture(target, im->name);
    if (desc.layers > 1)
        glTexImage3D(target, 0, format, desc.width, desc.height, desc.layers, 0,
                     ImageFormat(desc.format),
                     desc.format == RENDER_DEPTH ? GL_UNSIGNED_INT : ImageType(desc.format), NULL);
    else
        glTexImage2D(target, 0, format, desc.width, desc.height, 0, ImageFormat(desc.format),
                     desc.format == RENDER_DEPTH ? GL_UNSIGNED_INT : ImageType(desc.format), NULL);
    glTexParameteri(target, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(target, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    if (glGetError() != GL_NO_ERROR) {
        glDeleteTextures(1, &im->name);
        SDL_free(im);
        return 0;
    }
    return (uintptr_t)im;
}
static void ReleaseImage(RenderDevice *d, uintptr_t p) {
    if (p) {
        GlImage *im = Image(p);
        glDeleteTextures(1, &im->name);
        SDL_free(im);
    }
}
static int Upload(RenderDevice *d, uintptr_t p, RenderRect r, int layer, const void *pixels,
                  size_t rowBytes) {
    ((GlDevice *)d->data)->valid = 0;
    GlImage *im = Image(p);
    GLenum target = im->desc.layers > 1 ? GL_TEXTURE_2D_ARRAY : GL_TEXTURE_2D;
    int bpp = im->desc.format == RENDER_R16_UINT ? 2 : 4;
    glBindTexture(target, im->name);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glPixelStorei(GL_UNPACK_ROW_LENGTH, (GLint)(rowBytes / bpp));
    if (im->desc.layers > 1)
        glTexSubImage3D(target, 0, r.x, r.y, layer, r.w, r.h, 1, ImageFormat(im->desc.format),
                        ImageType(im->desc.format), pixels);
    else
        glTexSubImage2D(target, 0, r.x, r.y, r.w, r.h, ImageFormat(im->desc.format),
                        ImageType(im->desc.format), pixels);
    glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
    /* Validated rectangles/formats come from the shared renderer. Polling the
       driver after every upload forces WebGL command-stream synchronization.
       Allocation and shader errors are still checked on their cold paths. */
#ifdef __EMSCRIPTEN__
    return 1;
#else
    return glGetError() == GL_NO_ERROR;
#endif
}
static void Target(GlDevice *g, RenderTarget t, GLenum which) {
    if (t.window) {
        glBindFramebuffer(which, g->windowFramebuffer);
        return;
    }
    glBindFramebuffer(which, which == GL_READ_FRAMEBUFFER ? g->readFramebuffer : g->framebuffer);
    glFramebufferTexture2D(which, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, Texture(t.colour), 0);
    glFramebufferTexture2D(which, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, Texture(t.depth), 0);
    GLenum attachment = t.colour ? GL_COLOR_ATTACHMENT0 : GL_NONE;
    if (which != GL_READ_FRAMEBUFFER)
        glDrawBuffers(1, &attachment);
    if (which != GL_DRAW_FRAMEBUFFER)
        glReadBuffer(attachment);
}
static int Read(RenderDevice *d, RenderTarget target, RenderRect r, RenderFormat format,
                void *pixels) {
    ((GlDevice *)d->data)->valid = 0;
    Target(d->data, target, GL_READ_FRAMEBUFFER);
    glPixelStorei(GL_PACK_ALIGNMENT, 1);
    glReadPixels(r.x, r.y, r.w, r.h, ImageFormat(format), ImageType(format), pixels);
    return glGetError() == GL_NO_ERROR;
}
static void Mipmaps(RenderDevice *d, uintptr_t p) {
    ((GlDevice *)d->data)->valid = 0;
    GlImage *im = Image(p);
    glBindTexture(GL_TEXTURE_2D, im->name);
    glGenerateMipmap(GL_TEXTURE_2D);
}
static int WriteBuffer(RenderDevice *d, uintptr_t buffer, const void *data, size_t size,
                       RenderBufferKind kind) {
    ((GlDevice *)d->data)->valid = 0;
    GLenum target = kind == RENDER_INDEX_BUFFER ? GL_ELEMENT_ARRAY_BUFFER : GL_ARRAY_BUFFER;
    if (kind == RENDER_INDEX_BUFFER)
        glBindVertexArray(((GlDevice *)d->data)->vertexArray);
    glBindBuffer(target, (GLuint)buffer);
    glBufferData(target, size, data, GL_STREAM_DRAW);
    return 1;
}
static uintptr_t Buffer(RenderDevice *d, const void *data, size_t size, RenderBufferKind kind) {
    GLuint b;
    glGenBuffers(1, &b);
    WriteBuffer(d, b, data, size, kind);
    return b;
}
static void ReleaseBuffer(RenderDevice *d, uintptr_t p) {
    ((GlDevice *)d->data)->valid = 0;
    GLuint b = (GLuint)p;
    glDeleteBuffers(1, &b);
}
static void Scissor(int enabled, RenderRect r) {
    if (enabled) {
        glEnable(GL_SCISSOR_TEST);
        glScissor(r.x, r.y, r.w, r.h);
    } else
        glDisable(GL_SCISSOR_TEST);
}
static int Clear(RenderDevice *d, const RenderClear *c) {
    ((GlDevice *)d->data)->valid = 0;
    Target(d->data, c->target, GL_DRAW_FRAMEBUFFER);
    Scissor(c->scissorEnabled, c->scissor);
    glColorMask(c->colourMask & 1, (c->colourMask >> 1) & 1, (c->colourMask >> 2) & 1,
                (c->colourMask >> 3) & 1);
    glDepthMask(GL_TRUE);
    glClearColor(c->rgba[0], c->rgba[1], c->rgba[2], c->rgba[3]);
    glClear((c->colour ? GL_COLOR_BUFFER_BIT : 0) | (c->depth ? GL_DEPTH_BUFFER_BIT : 0));
    return 1;
}
static void Uniforms(GlProgram *p, const RenderUniforms *u) {
    const RenderUniforms *l = &p->last;
    int all = !p->valid;
    if (all || u->targetX != l->targetX || u->targetY != l->targetY)
        glUniform2f(p->uniform[0], u->targetX, u->targetY);
    if (all || u->originX != l->originX || u->originY != l->originY)
        glUniform2f(p->uniform[1], u->originX, u->originY);
    if (all || u->depthPull != l->depthPull)
        glUniform1f(p->uniform[2], u->depthPull);
    if (all || u->pass != l->pass)
        glUniform1i(p->uniform[3], u->pass);
    if (all || u->depthPass != l->depthPass)
        glUniform1i(p->uniform[4], u->depthPass);
    if (all || memcmp(&u->left, &l->left, 4 * sizeof(int)))
        glUniform4i(p->uniform[5], u->left, u->top, u->right, u->bottom);
    if (all || u->factor != l->factor)
        glUniform1i(p->uniform[6], u->factor);
    if (all || u->targetScale != l->targetScale)
        glUniform1i(p->uniform[7], u->targetScale);
    if (all || u->adaptive != l->adaptive)
        glUniform1i(p->uniform[8], u->adaptive);
    if (all || u->stepSize != l->stepSize)
        glUniform1f(p->uniform[9], u->stepSize);
    if (all || u->uiScaleX != l->uiScaleX || u->uiScaleY != l->uiScaleY)
        glUniform2f(p->uniform[10], u->uiScaleX, u->uiScaleY);
    if (all || u->uiOffsetX != l->uiOffsetX || u->uiOffsetY != l->uiOffsetY)
        glUniform2f(p->uniform[11], u->uiOffsetX, u->uiOffsetY);
    if (all || u->rasterScale != l->rasterScale)
        glUniform1f(p->uniform[12], u->rasterScale);
    p->last = *u;
    p->valid = 1;
}
static int Draw(RenderDevice *d, const RenderDraw *draw) {
    GlDevice *g = d->data;
    GlProgram *p = &g->programs[draw->pipeline.shader];
    if (!p->name && !Program(p, draw->pipeline.shader))
        return 0;
    const RenderDraw *last = &g->last;
    int all = !g->valid;
    if (all || last->target.colour != draw->target.colour ||
        last->target.depth != draw->target.depth || last->target.window != draw->target.window)
        Target(g, draw->target, GL_DRAW_FRAMEBUFFER);
    if (all || memcmp(&last->viewport, &draw->viewport, sizeof(draw->viewport)))
        glViewport(draw->viewport.x, draw->viewport.y, draw->viewport.w, draw->viewport.h);
    if (all || last->scissorEnabled != draw->scissorEnabled ||
        memcmp(&last->scissor, &draw->scissor, sizeof(draw->scissor)))
        Scissor(draw->scissorEnabled, draw->scissor);
    if (all || last->pipeline.shader != draw->pipeline.shader)
        glUseProgram(p->name);
    Uniforms(p, &draw->uniforms);
    const RenderPipeline *s = &draw->pipeline;
    if (all || last->pipeline.depthTest != s->depthTest) {
        if (s->depthTest)
            glEnable(GL_DEPTH_TEST);
        else
            glDisable(GL_DEPTH_TEST);
    }
    if (all || last->pipeline.depthWrite != s->depthWrite)
        glDepthMask(s->depthWrite);
    if (all || last->pipeline.depthLess != s->depthLess)
        glDepthFunc(s->depthLess ? GL_LESS : GL_LEQUAL);
    if (all || last->pipeline.colourMask != s->colourMask)
        glColorMask(s->colourMask & 1, (s->colourMask >> 1) & 1, (s->colourMask >> 2) & 1,
                    (s->colourMask >> 3) & 1);
    if (all || last->pipeline.blend != s->blend) {
        if (!s->blend)
            glDisable(GL_BLEND);
        else {
            glEnable(GL_BLEND);
            glBlendEquation(s->blend == 2 ? GL_FUNC_REVERSE_SUBTRACT : GL_FUNC_ADD);
            glBlendFuncSeparate(GL_ONE, s->blend == 2 ? GL_ONE : GL_ONE_MINUS_SRC_ALPHA, GL_ZERO,
                                GL_ONE);
        }
    }
    for (int i = 0; i < 5; i++) {
        GlImage *im = Image(draw->images[i]);
        if (!im)
            continue;
        if (!all && last->images[i] == draw->images[i] &&
            !memcmp(&last->samplers[i], &draw->samplers[i], sizeof(RenderSampler)))
            continue;
        GLenum target = im->desc.layers > 1 ? GL_TEXTURE_2D_ARRAY : GL_TEXTURE_2D;
        glActiveTexture(GL_TEXTURE0 + i);
        glBindTexture(target, im->name);
        RenderSampler sam = draw->samplers[i];
        glTexParameteri(target, GL_TEXTURE_MIN_FILTER,
                        sam.mipmaps  ? GL_LINEAR_MIPMAP_LINEAR
                        : sam.linear ? GL_LINEAR
                                     : GL_NEAREST);
        glTexParameteri(target, GL_TEXTURE_MAG_FILTER, sam.linear ? GL_LINEAR : GL_NEAREST);
        glTexParameteri(target, GL_TEXTURE_WRAP_S, sam.clamp ? GL_CLAMP_TO_EDGE : GL_REPEAT);
        glTexParameteri(target, GL_TEXTURE_WRAP_T, sam.clamp ? GL_CLAMP_TO_EDGE : GL_REPEAT);
    }
    if (all)
        glBindVertexArray(g->vertexArray);
    int layout = all || last->vertices != draw->vertices ||
                 last->vertexOffset != draw->vertexOffset || last->pipeline.shader != s->shader;
    if (s->shader == RENDER_GAME && layout) {
        glBindBuffer(GL_ARRAY_BUFFER, (GLuint)draw->vertices);
        const size_t offsets[] = {offsetof(RenderVertex, x),      offsetof(RenderVertex, r),
                                  offsetof(RenderVertex, u),      offsetof(RenderVertex, texture),
                                  offsetof(RenderVertex, window), offsetof(RenderVertex, mode),
                                  offsetof(RenderVertex, hd),     offsetof(RenderVertex, hdTexels)};
        const int counts[] = {3, 4, 2, 4, 4, 4, 4, 4};
        const GLenum types[] = {GL_FLOAT,         GL_FLOAT,         GL_FLOAT,          GL_SHORT,
                                GL_UNSIGNED_BYTE, GL_UNSIGNED_BYTE, GL_UNSIGNED_SHORT, GL_SHORT};
        for (int i = 0; i < 8; i++) {
            glEnableVertexAttribArray(i);
            const void *at = (const void *)(draw->vertexOffset + offsets[i]);
            if (i < 3)
                glVertexAttribPointer(i, counts[i], types[i], GL_FALSE, sizeof(RenderVertex), at);
            else
                glVertexAttribIPointer(i, counts[i], types[i], sizeof(RenderVertex), at);
        }
    }
    if (s->shader == RENDER_UI) {
        if (layout) {
            glBindBuffer(GL_ARRAY_BUFFER, (GLuint)draw->vertices);
            glEnableVertexAttribArray(0);
            glEnableVertexAttribArray(1);
            glEnableVertexAttribArray(2);
            glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 20, (void *)draw->vertexOffset);
            glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, 20, (void *)(draw->vertexOffset + 8));
            glVertexAttribPointer(1, 4, GL_UNSIGNED_BYTE, GL_TRUE, 20,
                                  (void *)(draw->vertexOffset + 16));
        }
        if (all || last->indices != draw->indices)
            glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, (GLuint)draw->indices);
        glDrawElements(GL_TRIANGLES, draw->count,
                       draw->indexBytes == 2 ? GL_UNSIGNED_SHORT : GL_UNSIGNED_INT,
                       (void *)((size_t)draw->first * draw->indexBytes));
    } else
        glDrawArrays(GL_TRIANGLES, draw->first, draw->count);
    g->last = *draw;
    g->valid = 1;
    return 1;
}
static int Copy(RenderDevice *d, const RenderCopy *c) {
    ((GlDevice *)d->data)->valid = 0;
    GlDevice *g = d->data;
    Target(g, c->source, GL_READ_FRAMEBUFFER);
    Target(g, c->destination, GL_DRAW_FRAMEBUFFER);
    glDisable(GL_SCISSOR_TEST);
    glBlitFramebuffer(c->from.x, c->from.y, c->from.x + c->from.w, c->from.y + c->from.h, c->to.x,
                      c->to.y, c->to.x + c->to.w, c->to.y + c->to.h,
                      c->depth ? GL_DEPTH_BUFFER_BIT : GL_COLOR_BUFFER_BIT,
                      c->linear ? GL_LINEAR : GL_NEAREST);
    return 1;
}
static void Finish(RenderDevice *d) { glFinish(); }
static int Present(RenderDevice *d) {
    GlDevice *g = d->data;
    g->valid = 0;
#ifdef __EMSCRIPTEN__
    /* SDL's WebGL swap only sleeps through Asyncify. ShowPicture owns that
       handoff and its refresh tracking; a second yield exposes an extra task
       boundary after the browser has consumed the default framebuffer. */
    WebFrameSubmitted();
    return 1;
#else
    if (g->swapInterval != d->vsync) {
        g->swapInterval = d->vsync;
        if (!d->vsync || !SDL_GL_SetSwapInterval(-1))
            SDL_GL_SetSwapInterval(d->vsync);
    }
    return !g->window || SDL_GL_SwapWindow(g->window);
#endif
}
static void Activate(RenderDevice *d, int active) {
    GlDevice *g = d->data;
    if (active && g->window && g->context)
        SDL_GL_MakeCurrent(g->window, g->context);
    g->valid = 0;
}
#ifdef GPU_READ_ASYNC
typedef struct GlReadback {
    GLuint buffer;
    GLsync fence;
    void *mapped;
    size_t bytes, capacity;
} GlReadback;
static uintptr_t BeginRead(RenderDevice *d, RenderTarget target, RenderRect r) {
    GlDevice *g = d->data;
    g->valid = 0;
    Target(g, target, GL_READ_FRAMEBUFFER);
    GlReadback *read = NULL;
    for (int i = 0; i < 2; i++)
        if (g->freeReads[i]) {
            read = g->freeReads[i];
            g->freeReads[i] = NULL;
            break;
        }
    if (!read)
        read = SDL_calloc(1, sizeof(*read));
    if (!read)
        return 0;
    read->bytes = (size_t)r.w * r.h * 4;
    if (!read->buffer)
        glGenBuffers(1, &read->buffer);
    glBindBuffer(GL_PIXEL_PACK_BUFFER, read->buffer);
    if (read->bytes > read->capacity) {
        glBufferData(GL_PIXEL_PACK_BUFFER, read->bytes, NULL, GL_STREAM_READ);
        read->capacity = read->bytes;
    }
    glPixelStorei(GL_PACK_ALIGNMENT, 1);
    glReadPixels(r.x, r.y, r.w, r.h, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
    glBindBuffer(GL_PIXEL_PACK_BUFFER, 0);
    read->fence = glFenceSync(GL_SYNC_GPU_COMMANDS_COMPLETE, 0);
    glFlush();
    return (uintptr_t)read;
}
static int PollRead(RenderDevice *d, uintptr_t ticket, int wait, const uint8_t **pixels) {
    GlReadback *r = (GlReadback *)ticket;
    if (r->mapped) {
        *pixels = r->mapped;
        return 1;
    }
    GLenum status = glClientWaitSync(r->fence, wait ? GL_SYNC_FLUSH_COMMANDS_BIT : 0,
                                     wait ? UINT64_C(1000000000) : 0);
    if (status == GL_TIMEOUT_EXPIRED)
        return 0;
    if (status == GL_WAIT_FAILED)
        return -1;
    glBindBuffer(GL_PIXEL_PACK_BUFFER, r->buffer);
    r->mapped = glMapBufferRange(GL_PIXEL_PACK_BUFFER, 0, r->bytes, GL_MAP_READ_BIT);
    glBindBuffer(GL_PIXEL_PACK_BUFFER, 0);
    *pixels = r->mapped;
    return r->mapped ? 1 : -1;
}
static void ReleaseRead(RenderDevice *d, uintptr_t ticket) {
    GlReadback *r = (GlReadback *)ticket;
    if (r->mapped) {
        glBindBuffer(GL_PIXEL_PACK_BUFFER, r->buffer);
        glUnmapBuffer(GL_PIXEL_PACK_BUFFER);
        glBindBuffer(GL_PIXEL_PACK_BUFFER, 0);
    }
    glDeleteSync(r->fence);
    r->fence = NULL;
    r->mapped = NULL;
    GlDevice *g = d->data;
    g->valid = 0;
    for (int i = 0; i < 2; i++)
        if (!g->freeReads[i]) {
            g->freeReads[i] = r;
            return;
        }
    glDeleteBuffers(1, &r->buffer);
    SDL_free(r);
}
static void DestroyReadPool(GlDevice *g) {
    for (int i = 0; i < 2; i++)
        if (g->freeReads[i]) {
            glDeleteBuffers(1, &g->freeReads[i]->buffer);
            SDL_free(g->freeReads[i]);
        }
}
#endif
const RenderDeviceOps *RenderOpenGLDevice(void) {
    static const RenderDeviceOps ops = {Create,     Destroy,  NewImage,    ReleaseImage,  Upload,
                                        Read,       Mipmaps,  Buffer,      ReleaseBuffer, Clear,
                                        Draw,       Copy,     Finish,      Present,       Activate,
#ifdef GPU_READ_ASYNC
                                        BeginRead,  PollRead, ReleaseRead,
#else
                                        NULL,       NULL,    NULL,
#endif
                                        WriteBuffer};
    return &ops;
}
