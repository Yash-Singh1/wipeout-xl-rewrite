#ifndef PORT_RENDER_IMGUI_H
#define PORT_RENDER_IMGUI_H
struct ImDrawData;
void RenderImGuiInit();
void RenderImGuiShutdown();
void RenderImGuiNewFrame();
void RenderImGuiDraw(ImDrawData *);
#endif
