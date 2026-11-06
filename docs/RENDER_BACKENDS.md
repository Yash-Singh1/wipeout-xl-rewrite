# Graphics backends

Open the debug console with `~`, select **Graphics**, then choose **Graphics backend**. The change takes effect after the current picture finishes. The race, controller input, graphics settings, and recording remain active. The selected backend is saved only after a successful switch.

Desktop builds provide OpenGL. macOS builds also provide Metal when a Metal device is available. Browser builds provide WebGL 2 through the OpenGL ES implementation. Unavailable implementations are omitted from the selector.

## Boundaries

```
Game / GTE / libgpu
        |
psx/gpu_renderer.c       VRAM semantics, geometry, batching, widescreen, post-processing
        |
render/state.c          logical resources, draw descriptors, migration, readback ownership
        |
render/device.h         API-independent device contract
        |
   OpenGL / WebGL       Metal       future device implementations
```

`gpu.h` remains the interface used by the emulated GPU libraries. `gpu_renderer.c` owns the console's texture and palette rules, depth slack, ordered draws, texture-page caching, and screen layout. This code does not include graphics API headers.

`state.h` is a private state builder used by that renderer. Its setters collect explicit `RenderDraw`, `RenderClear`, and `RenderCopy` descriptors. They are not the interface a new backend implements. The device receives complete draw state, with no requirement to emulate an OpenGL context or its state machine.

`device.h` contains the device contract. Image formats, vertex layouts, samplers, blend/depth rules, shaders, rectangles, and transfers use portable C types. Native textures, buffers, pipelines, descriptor sets, command encoders, and synchronization objects stay inside device implementations. GPU-facing object values are opaque to the shared layer.

`render/imgui.cpp` draws the console through the same device interface. Font atlases and recorder images use stable logical texture IDs, so the console does not need a separate API-specific renderer when changing devices.

The SDL host keeps an OpenGL-capable window/context available on desktop for live switching. Metal renders with its own Cocoa view and native Metal commands. Its view does not convert the SDL window's API flags or replace the existing OpenGL context. A different platform host can supply its own native window through the opaque device-creation argument.

## Adding a backend

1. Add an implementation of `RenderDeviceOps`, keeping native types private to that file. An optional asynchronous readback implementation can be omitted; the shared layer then uses synchronous reads.
2. Provide native shader implementations for the shader kinds in `RenderShader`. Match the layouts of `RenderVertex`, UI vertices, and `RenderUniforms`. The game shader must preserve palette decoding, transparency, texture windows, depth slack, fade, HD/glyph coverage, and blend-pass decisions.
3. Add its ID to `RenderBackend`, register its name, factory, and availability probe in the `backends` table in `state.c`, and include the platform-conditional source/libraries in CMake. Settings and the debug selector use the registry and enum count.
4. Report device limits accurately. If an older API implements logical array images using separate textures or an atlas, that translation belongs inside the backend. If it cannot support the current resources or settings, reject creation/import and leave the old device running.
5. Run the resource-migration tests and real GPU tests, including image transfers, depth, readbacks, and repeated switches. Building a backend does not establish parity on its hardware.

Adding a device does not require changing game code, GTE interpolation, ordering tables, the console UI, or shared texture-page decoding. A Vulkan, DirectX, or console backend can map a draw descriptor to its own pipeline/resource model without implementing the shared renderer's private setters.

## Coordinates and resources

Texture row zero is the same logical row on every backend. A window target uses the host display's orientation. Each driver translates viewport/scissor coordinates, clip depth, window orientation, and transfer alignment. `RenderCopy` allows signed destination extents for flipped presentation.

Color images are RGBA8, indexed texture data uses exact R16 unsigned values, and depth transfers use normalized floating-point values. OpenGL uses 24-bit depth storage; Metal uses 32-bit floating-point depth. Their stored depth bits are therefore not interchangeable hashes. Migration transfers depth values rather than native storage bytes.

Native APIs can also differ in interpolation, edge coverage, and color rounding. Metal and OpenGL are not promised to produce identical pixel hashes. The regression suite checks palette and blend semantics, occlusion, mesh coverage, and presentation on each driver; the OpenGL refactor additionally preserves the previous renderer's exact color/depth output.

Render images and buffers have stable logical identities. Immutable uploads retain CPU shadows; rendered images are read from the previous device when switching. Mipmaps are regenerated after upload. Native handles and backend-specific caches are never copied between implementations.

Drivers can implement `writeBuffer` to retain a native streaming buffer and orphan its storage, as OpenGL does. Drivers that need immutable storage can omit it and receive replacement buffers. The CPU shadow always holds the latest bytes for migration.

## Switching and failure handling

A switch finishes the old work, drains outstanding asynchronous reads into CPU-owned results, creates the new device, and rebuilds its resources. The new device becomes active only after all resources transfer successfully. If creation or import fails, temporary resources are released and the old renderer remains active; the console reports the error.

Full-resolution color and depth buffers are transferred, including widescreen content and intermediate images. At large rendering scales this can briefly pause presentation and temporarily increase memory use. Game state is not reinitialized.

Native Metal shader source is embedded at build time by `tools/embed_shader.py` and compiled by Metal at runtime. The build needs Python and Apple's Metal frameworks, not a third-party shader translator or a separate runtime shader file.

## Validation

```
WIPEOUT_TEST_GL=1 sh tools/bugtest/run.sh
WIPEOUT_TEST_GL=1 WIPEOUT_TEST_METAL=1 sh tools/bugtest/run.sh   # Metal-capable Mac
```

The fake-device migration test exercises rendered and immutable images, array layers, buffers, uniforms, pending reads, repeated switches, unavailable devices, allocation failure, and resource cleanup. GPU tests exercise the same shared renderer on the selected real API.

For a hidden integration run, `WIPEOUT_RENDERER=metal` selects a startup backend. `WIPEOUT_RENDERER_SWITCH=100:metal,300:opengl,500:metal` requests switches after the numbered displayed pictures. Visible games use the console selector. `WIPEOUT_DEBUG_UI=graphics WIPEOUT_DEBUG_CONSOLE=1` opens that tab for UI validation.
