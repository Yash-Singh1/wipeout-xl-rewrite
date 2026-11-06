/* Native Metal implementation. Device resources and command encoding stay here. */
#include "device.h"
#ifdef PORT_PRECOMPILED_METAL
#include "shaders_metallib.h"
#else
#include "shaders_metal.h"
#endif
#import <AppKit/AppKit.h>
#import <Metal/Metal.h>
#import <QuartzCore/CAMetalLayer.h>
#include <SDL3/SDL.h>
#include <string.h>

typedef struct MetalUniforms {
    RenderUniforms common;
    int window;
    float surfaceHeight;
} MetalUniforms;
/* Keep the SDL window and its GL context intact. SDL_Metal_CreateView converts
   the window's API flags and unloads GL, which prevents a later live switch. */
@interface WipeoutMetalView : NSView
@end
@implementation WipeoutMetalView
- (NSView *)hitTest:(NSPoint)point {
    return nil;
}
@end
@interface WipeoutMetalReusableBuffer : NSObject
@property id<MTLBuffer> buffer;
@property id<MTLCommandBuffer> lastUse;
@end
@implementation WipeoutMetalReusableBuffer
@end
@interface WipeoutMetalDevice : NSObject
@property id<MTLDevice> gpu;
@property id<MTLCommandQueue> queue;
@property id<MTLLibrary> library;
@property id<MTLCommandBuffer> command;
@property id<MTLCommandBuffer> lastCommand;
@property NSMutableArray<WipeoutMetalReusableBuffer *> *buffers;
@property NSUInteger pooledBytes;
@property id<MTLBuffer> readbackBuffer;
@property RenderDraw previousDraw;
@property BOOL drawValid;
@property id<MTLRenderCommandEncoder> encoder;
@property id<MTLTexture> encoderColour;
@property id<MTLTexture> encoderDepth;
@property id<CAMetalDrawable> drawable;
@property id<MTLTexture> headlessSurface;
@property CAMetalLayer *layer;
@property NSMutableDictionary *pipelines;
@property NSMutableDictionary *depthStates;
@property NSMutableDictionary *samplers;
@property dispatch_semaphore_t inflight;
@property NSView *view;
@property SDL_Window *window;
@end
@implementation WipeoutMetalDevice
@end
static WipeoutMetalDevice *Data(RenderDevice *d) { return (__bridge WipeoutMetalDevice *)d->data; }
static id<MTLTexture> Texture(uintptr_t h) { return (__bridge id<MTLTexture>)(void *)h; }
static id<MTLBuffer> VertexBuffer(uintptr_t h) { return (__bridge id<MTLBuffer>)(void *)h; }
static uintptr_t Keep(id object) { return (uintptr_t)(__bridge_retained void *)object; }
static void Release(uintptr_t object) {
    if (object)
        CFBridgingRelease((void *)object);
}
static int Error(NSString *what) {
    SDL_SetError("%s", what.UTF8String);
    return 0;
}
static void End(WipeoutMetalDevice *m) {
    m.drawValid = NO;
    if (m.encoder) {
        [m.encoder endEncoding];
        m.encoder = nil;
        m.encoderColour = nil;
        m.encoderDepth = nil;
    }
}
static id<MTLCommandBuffer> Command(WipeoutMetalDevice *m) {
    if (!m.command) {
        dispatch_semaphore_wait(m.inflight, DISPATCH_TIME_FOREVER);
        m.command = [m.queue commandBuffer];
        m.lastCommand = m.command;
        dispatch_semaphore_t gate = m.inflight;
        [m.command addCompletedHandler:^(id<MTLCommandBuffer> command) {
          dispatch_semaphore_signal(gate);
        }];
    }
    return m.command;
}
static void Finish(RenderDevice *d) {
    WipeoutMetalDevice *m = Data(d);
    End(m);
    if (m.command) {
        id<MTLCommandBuffer> c = m.command;
        [c commit];
        [c waitUntilCompleted];
        if (c.error)
            SDL_Log("Metal: %s", c.error.localizedDescription.UTF8String);
        m.command = nil;
    }
}
static MTLPixelFormat Format(RenderFormat f) {
    return f == RENDER_DEPTH      ? MTLPixelFormatDepth32Float
           : f == RENDER_R16_UINT ? MTLPixelFormatR16Uint
                                  : MTLPixelFormatRGBA8Unorm;
}
static uintptr_t Image(RenderDevice *d, RenderImageDesc desc) {
    @autoreleasepool {
        WipeoutMetalDevice *m = Data(d);
        MTLTextureDescriptor *td =
            [MTLTextureDescriptor texture2DDescriptorWithPixelFormat:Format(desc.format)
                                                               width:desc.width
                                                              height:desc.height
                                                           mipmapped:desc.mipmaps != 0];
        td.storageMode = MTLStorageModePrivate;
        td.usage = MTLTextureUsageShaderRead | MTLTextureUsageRenderTarget;
        if (desc.layers > 1) {
            td.textureType = MTLTextureType2DArray;
            td.arrayLength = desc.layers;
        }
        id<MTLTexture> texture = [m.gpu newTextureWithDescriptor:td];
        if (!texture) {
            Error(@"Metal texture allocation failed");
            return 0;
        }
        return Keep(texture);
    }
}
static void ReleaseImage(RenderDevice *d, uintptr_t image) { Release(image); }
/* A retired buffer may still be referenced by an encoder or an in-flight
   command. Reuse only after the last possible command has completed. The pool
   is bounded so a temporary large recording/upload cannot retain memory forever. */
static void ReleaseCompletedCommands(WipeoutMetalDevice *m) {
    for (WipeoutMetalReusableBuffer *entry in m.buffers)
        if (entry.lastUse.status == MTLCommandBufferStatusCompleted)
            entry.lastUse = nil;
    if (!m.command && m.lastCommand.status == MTLCommandBufferStatusCompleted)
        m.lastCommand = nil;
}
static id<MTLBuffer> ReuseBuffer(WipeoutMetalDevice *m, size_t size) {
    ReleaseCompletedCommands(m);
    for (NSUInteger i = 0; i < m.buffers.count; i++) {
        WipeoutMetalReusableBuffer *entry = m.buffers[i];
        if (entry.buffer.length >= size && entry.buffer.length <= SDL_max(size * 2, 4096) &&
            (!entry.lastUse || entry.lastUse.status == MTLCommandBufferStatusCompleted)) {
            id<MTLBuffer> buffer = entry.buffer;
            m.pooledBytes -= buffer.length;
            [m.buffers removeObjectAtIndex:i];
            return buffer;
        }
    }
    return [m.gpu newBufferWithLength:SDL_max(size, 1) options:MTLResourceStorageModeShared];
}
static void RetireBuffer(WipeoutMetalDevice *m, id<MTLBuffer> buffer) {
    if (m.buffers.count >= 64 || m.pooledBytes + buffer.length > 64 * 1024 * 1024)
        return;
    WipeoutMetalReusableBuffer *entry = [WipeoutMetalReusableBuffer new];
    entry.buffer = buffer;
    entry.lastUse = m.lastCommand;
    [m.buffers addObject:entry];
    m.pooledBytes += buffer.length;
}
static int Upload(RenderDevice *d, uintptr_t image, RenderRect r, int layer, const void *data,
                  size_t rowBytes) {
    @autoreleasepool {
        WipeoutMetalDevice *m = Data(d);
        id<MTLTexture> t = Texture(image);
        int bpp = t.pixelFormat == MTLPixelFormatR16Uint ? 2 : 4;
        size_t pitch = ((size_t)r.w * bpp + 255) & ~(size_t)255;
        id<MTLBuffer> staging = ReuseBuffer(m, pitch * r.h);
        if (!staging)
            return Error(@"Metal upload allocation failed");
        for (int y = 0; y < r.h; y++)
            memcpy((uint8_t *)staging.contents + y * pitch, (const uint8_t *)data + y * rowBytes,
                   (size_t)r.w * bpp);
        End(m);
        id<MTLBlitCommandEncoder> b = [Command(m) blitCommandEncoder];
        [b copyFromBuffer:staging
                   sourceOffset:0
              sourceBytesPerRow:pitch
            sourceBytesPerImage:pitch * r.h
                     sourceSize:MTLSizeMake(r.w, r.h, 1)
                      toTexture:t
               destinationSlice:layer
               destinationLevel:0
              destinationOrigin:MTLOriginMake(r.x, r.y, 0)];
        [b endEncoding];
        RetireBuffer(m, staging);
        return 1;
    }
}
static void Mipmaps(RenderDevice *d, uintptr_t image) {
    @autoreleasepool {
        WipeoutMetalDevice *m = Data(d);
        End(m);
        id<MTLBlitCommandEncoder> b = [Command(m) blitCommandEncoder];
        [b generateMipmapsForTexture:Texture(image)];
        [b endEncoding];
    }
}
static uintptr_t Buffer(RenderDevice *d, const void *data, size_t size, RenderBufferKind kind) {
    @autoreleasepool {
        id<MTLBuffer> b = ReuseBuffer(Data(d), size);
        if (b && data)
            memcpy(b.contents, data, size);
        return b ? Keep(b) : 0;
    }
}
static void ReleaseBuffer(RenderDevice *d, uintptr_t buffer) {
    @autoreleasepool {
        RetireBuffer(Data(d), VertexBuffer(buffer));
        Release(buffer);
    }
}
static id<MTLTexture> Surface(WipeoutMetalDevice *m) {
    int w, h;
    SDL_GetWindowSizeInPixels(m.window, &w, &h);
    w = SDL_max(w, 1);
    h = SDL_max(h, 1);
    m.layer.drawableSize = CGSizeMake(w, h);
    if (!(SDL_GetWindowFlags(m.window) & SDL_WINDOW_HIDDEN) && !m.drawable)
        m.drawable = [m.layer nextDrawable];
    if (m.drawable)
        return m.drawable.texture;
    if (!m.headlessSurface || m.headlessSurface.width != w || m.headlessSurface.height != h) {
        MTLTextureDescriptor *td =
            [MTLTextureDescriptor texture2DDescriptorWithPixelFormat:MTLPixelFormatBGRA8Unorm
                                                               width:w
                                                              height:h
                                                           mipmapped:NO];
        td.usage = MTLTextureUsageRenderTarget | MTLTextureUsageShaderRead;
        td.storageMode = MTLStorageModePrivate;
        m.headlessSurface = [m.gpu newTextureWithDescriptor:td];
    }
    return m.headlessSurface;
}
static id<MTLTexture> Colour(WipeoutMetalDevice *m, RenderTarget target) {
    return target.window ? Surface(m) : Texture(target.colour);
}
static id<MTLRenderCommandEncoder> Encoder(WipeoutMetalDevice *m, RenderTarget target) {
    id<MTLTexture> colour = Colour(m, target), depth = Texture(target.depth);
    if (m.encoder && m.encoderColour == colour && m.encoderDepth == depth)
        return m.encoder;
    End(m);
    MTLRenderPassDescriptor *pass = [MTLRenderPassDescriptor renderPassDescriptor];
    if (colour) {
        pass.colorAttachments[0].texture = colour;
        pass.colorAttachments[0].loadAction = MTLLoadActionLoad;
        pass.colorAttachments[0].storeAction = MTLStoreActionStore;
    }
    if (depth) {
        pass.depthAttachment.texture = depth;
        pass.depthAttachment.loadAction = MTLLoadActionLoad;
        pass.depthAttachment.storeAction = MTLStoreActionStore;
    }
    m.encoder = [Command(m) renderCommandEncoderWithDescriptor:pass];
    m.encoderColour = colour;
    m.encoderDepth = depth;
    return m.encoder;
}
static id<MTLDepthStencilState> Depth(WipeoutMetalDevice *m, int test, int write, int less) {
    NSNumber *key = @(test | (write << 1) | (less << 2));
    id<MTLDepthStencilState> state = m.depthStates[key];
    if (!state) {
        MTLDepthStencilDescriptor *ds = [MTLDepthStencilDescriptor new];
        ds.depthCompareFunction = !test  ? MTLCompareFunctionAlways
                                  : less ? MTLCompareFunctionLess
                                         : MTLCompareFunctionLessEqual;
        ds.depthWriteEnabled = write;
        state = [m.gpu newDepthStencilStateWithDescriptor:ds];
        m.depthStates[key] = state;
    }
    return state;
}
static id<MTLSamplerState> Sampler(WipeoutMetalDevice *m, RenderSampler s) {
    NSNumber *key = @(s.linear | (s.mipmaps << 1) | (s.clamp << 2));
    id<MTLSamplerState> sam = m.samplers[key];
    if (!sam) {
        MTLSamplerDescriptor *sd = [MTLSamplerDescriptor new];
        sd.minFilter = s.linear ? MTLSamplerMinMagFilterLinear : MTLSamplerMinMagFilterNearest;
        sd.magFilter = sd.minFilter;
        sd.mipFilter = s.mipmaps ? MTLSamplerMipFilterLinear : MTLSamplerMipFilterNotMipmapped;
        sd.sAddressMode = s.clamp ? MTLSamplerAddressModeClampToEdge : MTLSamplerAddressModeRepeat;
        sd.tAddressMode = sd.sAddressMode;
        sam = [m.gpu newSamplerStateWithDescriptor:sd];
        m.samplers[key] = sam;
    }
    return sam;
}
static id<MTLRenderPipelineState> Pipeline(WipeoutMetalDevice *m, int shader, RenderTarget target,
                                           int blend, int mask) {
    id<MTLTexture> colour = Colour(m, target);
    BOOL hasDepth = target.depth != 0;
    NSNumber *key = @(shader | (blend << 4) | (mask << 8) | (hasDepth << 12) |
                      ((colour.pixelFormat == MTLPixelFormatBGRA8Unorm) << 13));
    id<MTLRenderPipelineState> pipeline = m.pipelines[key];
    if (pipeline)
        return pipeline;
    static NSString *const fs[] = {@"gameFragment",      @"averageFragment",   @"detailFragment",
                                   @"compositeFragment", @"uiFragment",        @"copyFragment",
                                   @"clearFragment",     @"clearDepthFragment"};
    MTLRenderPipelineDescriptor *pd = [MTLRenderPipelineDescriptor new];
    pd.vertexFunction = [m.library newFunctionWithName:shader == RENDER_GAME ? @"gameVertex"
                                                       : shader == RENDER_UI ? @"uiVertex"
                                                                             : @"pictureVertex"];
    pd.fragmentFunction = [m.library newFunctionWithName:fs[shader]];
    MTLRenderPipelineColorAttachmentDescriptor *ca = pd.colorAttachments[0];
    ca.pixelFormat = colour.pixelFormat;
    ca.writeMask =
        ((mask & 1) ? MTLColorWriteMaskRed : 0) | ((mask & 2) ? MTLColorWriteMaskGreen : 0) |
        ((mask & 4) ? MTLColorWriteMaskBlue : 0) | ((mask & 8) ? MTLColorWriteMaskAlpha : 0);
    if (hasDepth)
        pd.depthAttachmentPixelFormat = MTLPixelFormatDepth32Float;
    if (blend) {
        ca.blendingEnabled = YES;
        ca.rgbBlendOperation = blend == 2 ? MTLBlendOperationReverseSubtract : MTLBlendOperationAdd;
        ca.alphaBlendOperation = MTLBlendOperationAdd;
        ca.sourceRGBBlendFactor = MTLBlendFactorOne;
        ca.destinationRGBBlendFactor =
            blend == 2 ? MTLBlendFactorOne : MTLBlendFactorOneMinusSourceAlpha;
        ca.sourceAlphaBlendFactor = MTLBlendFactorZero;
        ca.destinationAlphaBlendFactor = MTLBlendFactorOne;
    }
    if (shader == RENDER_GAME || shader == RENDER_UI) {
        MTLVertexDescriptor *vd = [MTLVertexDescriptor vertexDescriptor];
        if (shader == RENDER_GAME) {
            const MTLVertexFormat formats[] = {MTLVertexFormatFloat3,  MTLVertexFormatFloat4,
                                               MTLVertexFormatFloat2,  MTLVertexFormatShort4,
                                               MTLVertexFormatUChar4,  MTLVertexFormatUChar4,
                                               MTLVertexFormatUShort4, MTLVertexFormatShort4};
            const size_t offsets[] = {
                offsetof(RenderVertex, x),      offsetof(RenderVertex, r),
                offsetof(RenderVertex, u),      offsetof(RenderVertex, texture),
                offsetof(RenderVertex, window), offsetof(RenderVertex, mode),
                offsetof(RenderVertex, hd),     offsetof(RenderVertex, hdTexels)};
            for (int i = 0; i < 8; i++) {
                vd.attributes[i].format = formats[i];
                vd.attributes[i].offset = offsets[i];
                vd.attributes[i].bufferIndex = 0;
            }
            vd.layouts[0].stride = sizeof(RenderVertex);
        } else {
            vd.attributes[0].format = MTLVertexFormatFloat2;
            vd.attributes[0].offset = 0;
            vd.attributes[1].format = MTLVertexFormatUChar4Normalized;
            vd.attributes[1].offset = 16;
            vd.attributes[2].format = MTLVertexFormatFloat2;
            vd.attributes[2].offset = 8;
            for (int i = 0; i < 3; i++)
                vd.attributes[i].bufferIndex = 0;
            vd.layouts[0].stride = 20;
        }
        pd.vertexDescriptor = vd;
    }
    NSError *error = nil;
    pipeline = [m.gpu newRenderPipelineStateWithDescriptor:pd error:&error];
    if (!pipeline) {
        Error(error.localizedDescription);
        return nil;
    }
    m.pipelines[key] = pipeline;
    return pipeline;
}
static RenderRect Clip(RenderRect r, id<MTLTexture> t, int window) {
    if (window)
        r.y = (int)t.height - r.y - r.h;
    int right = SDL_min(r.x + r.w, (int)t.width), bottom = SDL_min(r.y + r.h, (int)t.height);
    r.x = SDL_max(r.x, 0);
    r.y = SDL_max(r.y, 0);
    r.w = SDL_max(0, right - r.x);
    r.h = SDL_max(0, bottom - r.y);
    return r;
}
static void SetViewport(id<MTLRenderCommandEncoder> e, RenderRect r, id<MTLTexture> t, int window) {
    if (window)
        r.y = (int)t.height - r.y - r.h;
    [e setViewport:(MTLViewport){r.x, r.y, r.w, r.h, 0.0, 1.0}];
}
static MetalUniforms Uniform(RenderUniforms u, RenderTarget target, id<MTLTexture> texture) {
    return (MetalUniforms){u, target.window, (float)texture.height};
}
static int Draw(RenderDevice *d, const RenderDraw *draw) {
    @autoreleasepool {
        WipeoutMetalDevice *m = Data(d);
        id<MTLTexture> colour = Colour(m, draw->target);
        id<MTLRenderPipelineState> p = Pipeline(m, draw->pipeline.shader, draw->target,
                                                draw->pipeline.blend, draw->pipeline.colourMask);
        if (!p)
            return 0;
        RenderRect scissor =
            Clip(draw->scissorEnabled ? draw->scissor
                                      : (RenderRect){0, 0, (int)colour.width, (int)colour.height},
                 colour, draw->target.window);
        if (!scissor.w || !scissor.h)
            return 1;
        id<MTLRenderCommandEncoder> e = Encoder(m, draw->target);
        RenderDraw last = m.previousDraw;
        BOOL all = !m.drawValid;
        if (all || memcmp(&last.pipeline, &draw->pipeline, sizeof(RenderPipeline))) {
            [e setRenderPipelineState:p];
            [e setDepthStencilState:Depth(m, draw->pipeline.depthTest, draw->pipeline.depthWrite,
                                          draw->pipeline.depthLess)];
        }
        if (all || memcmp(&last.viewport, &draw->viewport, sizeof(RenderRect)))
            SetViewport(e, draw->viewport, colour, draw->target.window);
        if (all || last.scissorEnabled != draw->scissorEnabled ||
            memcmp(&last.scissor, &draw->scissor, sizeof(RenderRect)))
            [e setScissorRect:(MTLScissorRect){scissor.x, scissor.y, scissor.w, scissor.h}];
        if (all)
            [e setCullMode:MTLCullModeNone];
        if (all || memcmp(&last.uniforms, &draw->uniforms, sizeof(RenderUniforms))) {
            MetalUniforms u = Uniform(draw->uniforms, draw->target, colour);
            [e setVertexBytes:&u length:sizeof(u) atIndex:1];
            [e setFragmentBytes:&u length:sizeof(u) atIndex:1];
        }
        if (draw->vertices &&
            (all || last.vertices != draw->vertices || last.vertexOffset != draw->vertexOffset))
            [e setVertexBuffer:VertexBuffer(draw->vertices) offset:draw->vertexOffset atIndex:0];
        for (int i = 0; i < 5; i++) {
            if (draw->images[i]) {
                if (all || last.images[i] != draw->images[i])
                    [e setFragmentTexture:Texture(draw->images[i]) atIndex:i];
                if (all || !last.images[i] ||
                    memcmp(&last.samplers[i], &draw->samplers[i], sizeof(RenderSampler)))
                    [e setFragmentSamplerState:Sampler(m, draw->samplers[i]) atIndex:i];
            }
        }
        m.previousDraw = *draw;
        m.drawValid = YES;
        if (draw->indices)
            [e drawIndexedPrimitives:MTLPrimitiveTypeTriangle
                          indexCount:draw->count
                           indexType:draw->indexBytes == 2 ? MTLIndexTypeUInt16 : MTLIndexTypeUInt32
                         indexBuffer:VertexBuffer(draw->indices)
                   indexBufferOffset:draw->indexOffset + (size_t)draw->first * draw->indexBytes];
        else
            [e drawPrimitives:MTLPrimitiveTypeTriangle
                  vertexStart:draw->first
                  vertexCount:draw->count];
        return 1;
    }
}
static int Clear(RenderDevice *d, const RenderClear *c) {
    @autoreleasepool {
        WipeoutMetalDevice *m = Data(d);
        m.drawValid = NO;
        id<MTLTexture> colour = Colour(m, c->target), depth = Texture(c->target.depth);
        RenderRect r =
            Clip(c->scissorEnabled ? c->scissor
                                   : (RenderRect){0, 0, (int)colour.width, (int)colour.height},
                 colour, c->target.window);
        if (!r.w || !r.h)
            return 1;
        if (r.x == 0 && r.y == 0 && r.w == colour.width && r.h == colour.height &&
            (!c->colour || c->colourMask == 15)) {
            End(m);
            MTLRenderPassDescriptor *pass = [MTLRenderPassDescriptor renderPassDescriptor];
            pass.colorAttachments[0].texture = colour;
            pass.colorAttachments[0].loadAction =
                c->colour ? MTLLoadActionClear : MTLLoadActionLoad;
            pass.colorAttachments[0].storeAction = MTLStoreActionStore;
            pass.colorAttachments[0].clearColor =
                MTLClearColorMake(c->rgba[0], c->rgba[1], c->rgba[2], c->rgba[3]);
            if (depth) {
                pass.depthAttachment.texture = depth;
                pass.depthAttachment.loadAction = c->depth ? MTLLoadActionClear : MTLLoadActionLoad;
                pass.depthAttachment.storeAction = MTLStoreActionStore;
                pass.depthAttachment.clearDepth = 1.0;
            }
            id<MTLRenderCommandEncoder> e = [Command(m) renderCommandEncoderWithDescriptor:pass];
            m.encoder = e;
            m.encoderColour = colour;
            m.encoderDepth = depth;
            return 1;
        }
        id<MTLRenderPipelineState> p =
            Pipeline(m, c->depth && depth ? 7 : 6, c->target, 0, c->colour ? c->colourMask : 0);
        if (!p)
            return 0;
        id<MTLRenderCommandEncoder> e = Encoder(m, c->target);
        [e setRenderPipelineState:p];
        [e setDepthStencilState:Depth(m, 0, c->depth && depth, 0)];
        [e setViewport:(MTLViewport){0, 0, colour.width, colour.height, 0.0, 1.0}];
        [e setScissorRect:(MTLScissorRect){r.x, r.y, r.w, r.h}];
        MetalUniforms u = Uniform((RenderUniforms){0}, c->target, colour);
        [e setVertexBytes:&u length:sizeof(u) atIndex:1];
        [e setFragmentBytes:c->rgba length:sizeof(c->rgba) atIndex:2];
        [e drawPrimitives:MTLPrimitiveTypeTriangle vertexStart:0 vertexCount:3];
        return 1;
    }
}
static int Copy(RenderDevice *d, const RenderCopy *c) {
    @autoreleasepool {
        WipeoutMetalDevice *m = Data(d);
        m.drawValid = NO;
        id<MTLTexture> source = c->depth ? Texture(c->source.depth) : Colour(m, c->source),
                       dest = c->depth ? Texture(c->destination.depth) : Colour(m, c->destination);
        if (!source || !dest)
            return Error(@"Metal copy has no attachment");
        if (!c->source.window && !c->destination.window && c->from.w == c->to.w &&
            c->from.h == c->to.h && c->from.w > 0 && c->from.h > 0 &&
            source.pixelFormat == dest.pixelFormat) {
            End(m);
            id<MTLBlitCommandEncoder> b = [Command(m) blitCommandEncoder];
            [b copyFromTexture:source
                      sourceSlice:0
                      sourceLevel:0
                     sourceOrigin:MTLOriginMake(c->from.x, c->from.y, 0)
                       sourceSize:MTLSizeMake(c->from.w, c->from.h, 1)
                        toTexture:dest
                 destinationSlice:0
                 destinationLevel:0
                destinationOrigin:MTLOriginMake(c->to.x, c->to.y, 0)];
            [b endEncoding];
            return 1;
        }
        if (c->depth)
            return Error(@"Scaled depth transfers are unsupported");
        id<MTLRenderPipelineState> p = Pipeline(m, 5, c->destination, 0, 15);
        if (!p)
            return 0;
        id<MTLRenderCommandEncoder> e = Encoder(m, c->destination);
        [e setRenderPipelineState:p];
        [e setDepthStencilState:Depth(m, 0, 0, 0)];
        RenderRect region = {SDL_min(c->to.x, c->to.x + c->to.w),
                             SDL_min(c->to.y, c->to.y + c->to.h), abs(c->to.w), abs(c->to.h)};
        region = Clip(region, dest, c->destination.window);
        if (!region.w || !region.h)
            return 1;
        [e setViewport:(MTLViewport){0, 0, dest.width, dest.height, 0.0, 1.0}];
        [e setScissorRect:(MTLScissorRect){region.x, region.y, region.w, region.h}];
        MetalUniforms u = Uniform((RenderUniforms){0}, c->destination, dest);
        float fromY = c->from.y, fromH = c->from.h;
        if (c->source.window) {
            fromY = (float)source.height - fromY;
            fromH = -fromH;
        }
        float copy[] = {c->from.x,
                        fromY,
                        c->from.w,
                        fromH,
                        c->to.x,
                        c->to.y,
                        c->to.w,
                        c->to.h,
                        c->destination.window,
                        (float)dest.height};
        [e setVertexBytes:&u length:sizeof(u) atIndex:1];
        [e setFragmentBytes:copy length:sizeof(copy) atIndex:2];
        [e setFragmentTexture:source atIndex:0];
        [e setFragmentSamplerState:Sampler(m, (RenderSampler){c->linear, 0, 1}) atIndex:0];
        [e drawPrimitives:MTLPrimitiveTypeTriangle vertexStart:0 vertexCount:3];
        return 1;
    }
}
static int Read(RenderDevice *d, RenderTarget target, RenderRect r, RenderFormat f, void *out) {
    @autoreleasepool {
        WipeoutMetalDevice *m = Data(d);
        End(m);
        id<MTLTexture> t = f == RENDER_DEPTH ? Texture(target.depth) : Colour(m, target);
        if (!t)
            return Error(@"Metal read has no attachment");
        int bpp = f == RENDER_R16_UINT ? 2 : 4;
        size_t pitch = ((size_t)r.w * bpp + 255) & ~(size_t)255;
        id<MTLBuffer> staging = [m.gpu newBufferWithLength:pitch * r.h
                                                   options:MTLResourceStorageModeShared];
        if (!staging)
            return Error(@"Metal readback allocation failed");
        int y = target.window ? (int)t.height - r.y - r.h : r.y;
        id<MTLBlitCommandEncoder> b = [Command(m) blitCommandEncoder];
        [b copyFromTexture:t
                         sourceSlice:0
                         sourceLevel:0
                        sourceOrigin:MTLOriginMake(r.x, y, 0)
                          sourceSize:MTLSizeMake(r.w, r.h, 1)
                            toBuffer:staging
                   destinationOffset:0
              destinationBytesPerRow:pitch
            destinationBytesPerImage:pitch * r.h];
        [b endEncoding];
        Finish(d);
        for (int j = 0; j < r.h; j++) {
            uint8_t *dst = (uint8_t *)out + (size_t)j * r.w * bpp;
            const uint8_t *src =
                (const uint8_t *)staging.contents + (target.window ? r.h - j - 1 : j) * pitch;
            memcpy(dst, src, (size_t)r.w * bpp);
            if (t.pixelFormat == MTLPixelFormatBGRA8Unorm)
                for (int x = 0; x < r.w; x++) {
                    uint8_t v = dst[x * 4];
                    dst[x * 4] = dst[x * 4 + 2];
                    dst[x * 4 + 2] = v;
                }
        }
        return 1;
    }
}
@interface WipeoutMetalRead : NSObject
@property id<MTLBuffer> buffer;
@property id<MTLCommandBuffer> command;
@property NSMutableData *packed;
@property size_t pitch;
@property int width, height, window, bgra;
@end
@implementation WipeoutMetalRead
@end
static uintptr_t BeginRead(RenderDevice *d, RenderTarget target, RenderRect r) {
    @autoreleasepool {
        WipeoutMetalDevice *m = Data(d);
        End(m);
        id<MTLTexture> source = Colour(m, target);
        WipeoutMetalRead *read = [WipeoutMetalRead new];
        read.width = r.w;
        read.height = r.h;
        read.pitch = ((size_t)r.w * 4 + 255) & ~(size_t)255;
        read.window = target.window;
        read.bgra = source.pixelFormat == MTLPixelFormatBGRA8Unorm;
        size_t bytes = read.pitch * r.h;
        if (m.readbackBuffer.length >= bytes && m.readbackBuffer.length <= SDL_max(bytes * 2, 4096)) {
            read.buffer = m.readbackBuffer;
            m.readbackBuffer = nil;
        } else read.buffer = [m.gpu newBufferWithLength:bytes options:MTLResourceStorageModeShared];
        if (!read.buffer)
            return 0;
        int y = target.window ? (int)source.height - r.y - r.h : r.y;
        read.command = Command(m);
        id<MTLBlitCommandEncoder> b = [read.command blitCommandEncoder];
        [b copyFromTexture:source
                         sourceSlice:0
                         sourceLevel:0
                        sourceOrigin:MTLOriginMake(r.x, y, 0)
                          sourceSize:MTLSizeMake(r.w, r.h, 1)
                            toBuffer:read.buffer
                   destinationOffset:0
              destinationBytesPerRow:read.pitch
            destinationBytesPerImage:read.pitch * r.h];
        [b endEncoding];
        [read.command commit];
        m.command = nil;
        return Keep(read);
    }
}
static int PollRead(RenderDevice *d, uintptr_t ticket, int wait, const uint8_t **pixels) {
    @autoreleasepool {
        WipeoutMetalRead *r = (__bridge WipeoutMetalRead *)(void *)ticket;
        if (wait)
            [r.command waitUntilCompleted];
        if (r.command.status == MTLCommandBufferStatusError)
            return -1;
        if (r.command.status != MTLCommandBufferStatusCompleted)
            return 0;
        if (!r.window && !r.bgra && r.pitch == (size_t)r.width * 4) {
            *pixels = r.buffer.contents;
            return 1;
        }
        if (!r.packed) {
            r.packed = [NSMutableData dataWithLength:(size_t)r.width * r.height * 4];
            for (int y = 0; y < r.height; y++) {
                uint8_t *dst = (uint8_t *)r.packed.mutableBytes + (size_t)y * r.width * 4;
                memcpy(dst,
                       (const uint8_t *)r.buffer.contents +
                           (r.window ? r.height - y - 1 : y) * r.pitch,
                       (size_t)r.width * 4);
                if (r.bgra)
                    for (int x = 0; x < r.width; x++) {
                        uint8_t c = dst[x * 4];
                        dst[x * 4] = dst[x * 4 + 2];
                        dst[x * 4 + 2] = c;
                    }
            }
        }
        *pixels = r.packed.bytes;
        return 1;
    }
}
static void ReleaseRead(RenderDevice *d, uintptr_t ticket) {
    @autoreleasepool {
        WipeoutMetalRead *read = (__bridge WipeoutMetalRead *)(void *)ticket;
        WipeoutMetalDevice *m = Data(d);
        if (read.command.status == MTLCommandBufferStatusCompleted && read.buffer.length <= 64 * 1024 * 1024 &&
            read.buffer.length >= m.readbackBuffer.length) m.readbackBuffer = read.buffer;
        Release(ticket);
    }
}
static int Present(RenderDevice *d) {
    @autoreleasepool {
        WipeoutMetalDevice *m = Data(d);
        m.layer.displaySyncEnabled = d->vsync != 0;
        End(m);
        if (m.drawable)
            [Command(m) presentDrawable:m.drawable];
        if (m.command) {
            [m.command commit];
            m.command = nil;
        }
        m.drawable = nil;
        ReleaseCompletedCommands(m);
        return 1;
    }
}
static void Activate(RenderDevice *d, int active) {
    WipeoutMetalDevice *m = Data(d);
    m.view.hidden = !active;
}
static void Trim(RenderDevice *d) {
    @autoreleasepool {
        Finish(d);
        WipeoutMetalDevice *m = Data(d);
        [m.buffers removeAllObjects];
        m.pooledBytes = 0;
        m.readbackBuffer = nil;
        m.lastCommand = nil;
    }
}
static void Destroy(RenderDevice *d) {
    @autoreleasepool {
        if (!d->data)
            return;
        Finish(d);
        WipeoutMetalDevice *m = Data(d);
        [m.view removeFromSuperview];
        Release((uintptr_t)d->data);
        d->data = NULL;
    }
}
static int Create(RenderDevice *d, void *window) {
    @autoreleasepool {
        if (!window)
            return Error(@"Metal requires an SDL window");
        WipeoutMetalDevice *m = [WipeoutMetalDevice new];
        m.gpu = MTLCreateSystemDefaultDevice();
        if (!m.gpu)
            return Error(@"No Metal device is available");
        m.queue = [m.gpu newCommandQueue];
        m.inflight = dispatch_semaphore_create(3);
        m.window = window;
        m.pipelines = [NSMutableDictionary new];
        m.depthStates = [NSMutableDictionary new];
        m.samplers = [NSMutableDictionary new];
        m.buffers = [NSMutableArray new];
        NSError *error = nil;
#ifdef PORT_PRECOMPILED_METAL
        dispatch_data_t libraryData = dispatch_data_create(
            metalLibraryBytes, sizeof(metalLibraryBytes), NULL, DISPATCH_DATA_DESTRUCTOR_DEFAULT);
        m.library = [m.gpu newLibraryWithData:libraryData error:&error];
#else
        MTLCompileOptions *options = [MTLCompileOptions new];
#if __MAC_OS_X_VERSION_MAX_ALLOWED >= 150000
        if (@available(macOS 15.0, *))
            options.mathMode = MTLMathModeSafe;
        else
#endif
        {
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
            options.fastMathEnabled = NO;
#pragma clang diagnostic pop
        }
        m.library = [m.gpu newLibraryWithSource:[NSString stringWithUTF8String:metalShaderSource]
                                        options:options
                                          error:&error];
#endif
        if (!m.library)
            return Error(error.localizedDescription);
        NSWindow *native = (__bridge NSWindow *)SDL_GetPointerProperty(
            SDL_GetWindowProperties(window), SDL_PROP_WINDOW_COCOA_WINDOW_POINTER, NULL);
        if (!native)
            return Error(@"The SDL window has no Cocoa surface");
        m.view = [[WipeoutMetalView alloc] initWithFrame:native.contentView.bounds];
        m.view.autoresizingMask = NSViewWidthSizable | NSViewHeightSizable;
        m.layer = [CAMetalLayer layer];
        m.layer.contentsScale = native.backingScaleFactor;
        m.layer.device = m.gpu;
        m.layer.pixelFormat = MTLPixelFormatBGRA8Unorm;
        m.layer.framebufferOnly = NO;
        m.view.wantsLayer = YES;
        m.view.layer = m.layer;
        m.view.hidden = YES;
        [native.contentView addSubview:m.view positioned:NSWindowAbove relativeTo:nil];
        d->data = (__bridge_retained void *)m;
        d->caps.maxTextureSize = 16384;
        d->caps.maxArrayLayers = 2048;
        d->caps.asyncReadback = 1;
        SDL_strlcpy(d->caps.deviceName, m.gpu.name.UTF8String, sizeof(d->caps.deviceName));
        return 1;
    }
}
int RenderMetalAvailable(void) {
    @autoreleasepool {
        return MTLCreateSystemDefaultDevice() != nil;
    }
}
const RenderDeviceOps *RenderMetalDevice(void) {
    static const RenderDeviceOps ops = {
        Create, Destroy, Image, ReleaseImage, Upload,  Read,     Mipmaps,   Buffer,   ReleaseBuffer,
        Clear,  Draw,    Copy,  Finish,       Present, Activate, BeginRead, PollRead, ReleaseRead, NULL, Trim};
    return &ops;
}
