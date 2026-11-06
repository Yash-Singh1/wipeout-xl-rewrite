/* Resource/state builder used by the shared emulated GPU. The backend receives
   immutable RenderDraw/RenderClear/RenderCopy descriptors, not these setters. */
#ifndef PORT_RENDER_STATE_H
#define PORT_RENDER_STATE_H
#include "device.h"
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
typedef uint32_t RgHandle;
typedef unsigned RgBits;
enum {
    RG_TEXTURE_2D = 1,
    RG_TEXTURE_2D_ARRAY,
    RG_FRAMEBUFFER,
    RG_READ_FRAMEBUFFER,
    RG_DRAW_FRAMEBUFFER,
    RG_ARRAY_BUFFER,
    RG_ELEMENT_ARRAY_BUFFER,
    RG_PIXEL_PACK_BUFFER,
    RG_RENDERBUFFER,
    RG_COLOUR_ATTACHMENT = 20,
    RG_COLOR_ATTACHMENT0 = RG_COLOUR_ATTACHMENT,
    RG_DEPTH_ATTACHMENT,
    RG_RGBA8 = 30,
    RG_RGB8,
    RG_R16UI,
    RG_DEPTH_COMPONENT24,
    RG_RGBA,
    RG_RGB,
    RG_RED_INTEGER,
    RG_DEPTH_COMPONENT,
    RG_UNSIGNED_BYTE = 40,
    RG_UNSIGNED_SHORT,
    RG_SHORT,
    RG_FLOAT,
    RG_TEXTURE_MIN_FILTER = 50,
    RG_TEXTURE_MAG_FILTER,
    RG_TEXTURE_WRAP_S,
    RG_TEXTURE_WRAP_T,
    RG_LINEAR = 60,
    RG_NEAREST,
    RG_LINEAR_MIPMAP_LINEAR,
    RG_CLAMP_TO_EDGE,
    RG_SCISSOR_TEST = 70,
    RG_DEPTH_TEST,
    RG_BLEND,
    RG_LESS = 80,
    RG_LEQUAL,
    RG_FUNC_ADD,
    RG_FUNC_REVERSE_SUBTRACT,
    RG_ONE,
    RG_ZERO,
    RG_ONE_MINUS_SRC_ALPHA,
    RG_VIEWPORT = 90,
    RG_MAX_TEXTURE_SIZE,
    RG_UNPACK_ALIGNMENT,
    RG_PACK_ALIGNMENT,
    RG_UNPACK_ROW_LENGTH,
    RG_STREAM_DRAW = 100,
    RG_STREAM_READ,
    RG_TRIANGLES,
    RG_FRAMEBUFFER_COMPLETE = 110,
    RG_TEXTURE0 = 120,
    RG_TEXTURE1,
    RG_TEXTURE2,
    RG_TEXTURE3,
    RG_TEXTURE4,
    RG_FALSE = 0,
    RG_TRUE = 1,
    RG_COLOR_BUFFER_BIT = 0x1000,
    RG_DEPTH_BUFFER_BIT = 0x2000
};
int RgInit(void);
RgHandle RgProgram(RenderShader kind);
int RgGetUniformLocation(RgHandle program, const char *name);
void RgUseProgram(RgHandle program);
void RgUniform1i(int field, int value);
void RgUniform1f(int field, float value);
void RgUniform2f(int field, float x, float y);
void RgUniform4i(int field, int x, int y, int z, int w);
void RgGenTextures(int count, RgHandle *out);
void RgDeleteTextures(int count, const RgHandle *images);
void RgActiveTexture(int unit);
void RgBindTexture(int kind, RgHandle image);
void RgTexParameteri(int kind, int parameter, int value);
void RgTexImage2D(int kind, int level, int format, int width, int height, int border, int input,
                  int type, const void *data);
void RgTexImage3D(int kind, int level, int format, int width, int height, int layers, int border,
                  int input, int type, const void *data);
void RgTexSubImage2D(int kind, int level, int x, int y, int width, int height, int format, int type,
                     const void *data);
void RgTexSubImage3D(int kind, int level, int x, int y, int layer, int width, int height,
                     int layers, int format, int type, const void *data);
void RgCopyTexSubImage2D(int kind, int level, int x, int y, int fromX, int fromY, int width,
                         int height);
void RgGenerateMipmap(int kind);
void RgGenFramebuffers(int count, RgHandle *out);
void RgDeleteFramebuffers(int count, const RgHandle *targets);
void RgBindFramebuffer(int kind, RgHandle target);
void RgFramebufferTexture2D(int kind, int attachment, int textureKind, RgHandle image, int level);
int RgCheckFramebufferStatus(int kind);
void RgGenRenderbuffers(int count, RgHandle *out);
void RgDeleteRenderbuffers(int count, const RgHandle *images);
void RgBindRenderbuffer(int kind, RgHandle image);
void RgRenderbufferStorage(int kind, int format, int w, int h);
void RgFramebufferRenderbuffer(int kind, int attachment, int renderbufferKind, RgHandle image);
void RgGenBuffers(int count, RgHandle *out);
void RgBindBuffer(int kind, RgHandle buffer);
void RgBufferData(int kind, size_t size, const void *data, int usage);
void RgGenVertexArrays(int count, RgHandle *out);
void RgBindVertexArray(RgHandle array);
void RgEnableVertexAttribArray(int i);
void RgVertexAttribPointer(int i, int n, int type, int normal, int stride, const void *offset);
void RgVertexAttribIPointer(int i, int n, int type, int stride, const void *offset);
void RgViewport(int x, int y, int w, int h);
void RgScissor(int x, int y, int w, int h);
void RgEnable(int state);
void RgDisable(int state);
int RgIsEnabled(int state);
void RgColorMask(int r, int g, int b, int a);
void RgDepthMask(int enabled);
void RgDepthFunc(int comparison);
void RgBlendEquation(int equation);
void RgBlendFuncSeparate(int r, int g, int b, int a);
void RgBlendColor(float r, float g, float b, float a);
void RgClearColor(float r, float g, float b, float a);
void RgClear(RgBits flags);
void RgGetIntegerv(int field, int *value);
void RgPixelStorei(int field, int value);
void RgDrawArrays(int kind, int first, int count);
void RgBlitFramebuffer(int x, int y, int right, int bottom, int toX, int toY, int toRight,
                       int toBottom, RgBits flags, int filter);
void RgReadPixels(int x, int y, int w, int h, int format, int type, void *pixels);
void RgFinish(void);
void RgFlush(void);

int RenderStart(void *window);
int RenderStartBackend(void *window, RenderBackend backend);
RenderBackend RenderDefaultBackend(void);
int RenderFindBackend(const char *name, RenderBackend *backend);
const RenderDevice *RenderActiveDevice(void);
RenderBackend RenderActiveBackend(void);
const char *RenderBackendName(RenderBackend);
int RenderBackendAvailable(RenderBackend);
int RenderRequestBackend(RenderBackend);
int RenderRequestBackendName(const char *name);
int RenderApplyBackend(void);
const char *RenderBackendError(void);
void RenderStop(void);
void RenderPresent(void);
void RenderTrimMemory(void);
void RenderSetVsync(int enabled);
int RgReadbackBusy(int reader);
int RgReadbackBegin(int reader, int width, int height);
int RgReadbackDone(int reader, int wait, void (*use)(const uint8_t *, int, int, void *),
                   void *user);
void RgDrawUI(RgHandle vertices, RgHandle indices, RgHandle image, RenderRect clip, int width,
              int height, int first, int count, int indexBytes, size_t vertexOffset, float sx,
              float sy, float ox, float oy, int linear);
#ifdef __cplusplus
}
#endif
#endif
