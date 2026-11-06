/* Dear ImGui draws through the same portable device contract as the game.
   Texture IDs are logical resources and remain valid across backend changes. */
#include "../third_party/imgui/imgui.h"
#include "imgui.h"
#include "state.h"
static RgHandle vertexBuffer, indexBuffer;
static bool linear = true;
static void Linear(const ImDrawList *, const ImDrawCmd *) { linear = true; }
static void Nearest(const ImDrawList *, const ImDrawCmd *) { linear = false; }
void RenderImGuiInit() {
    static_assert(sizeof(ImDrawVert) == 20, "The device UI vertex layout must match Dear ImGui");
    ImGui::GetPlatformIO().Renderer_TextureMaxWidth = RenderActiveDevice()->caps.maxTextureSize;
    ImGui::GetPlatformIO().Renderer_TextureMaxHeight = RenderActiveDevice()->caps.maxTextureSize;
    ImGuiIO &io = ImGui::GetIO();
    io.BackendRendererName = "wipeout_device";
    io.BackendFlags |=
        ImGuiBackendFlags_RendererHasTextures | ImGuiBackendFlags_RendererHasVtxOffset;
    ImGui::GetPlatformIO().DrawCallback_SetSamplerLinear = Linear;
    ImGui::GetPlatformIO().DrawCallback_SetSamplerNearest = Nearest;
    RgGenBuffers(1, &vertexBuffer);
    RgGenBuffers(1, &indexBuffer);
}
void RenderImGuiShutdown() {
    for (ImTextureData *t : ImGui::GetPlatformIO().Textures) {
        if (t->TexID != ImTextureID_Invalid) {
            RgHandle id = (RgHandle)t->TexID;
            RgDeleteTextures(1, &id);
            t->SetTexID(ImTextureID_Invalid);
            t->SetStatus(ImTextureStatus_Destroyed);
        }
    }
    ImGui::GetIO().BackendRendererName = nullptr;
    ImGui::GetIO().BackendFlags &=
        ~(ImGuiBackendFlags_RendererHasTextures | ImGuiBackendFlags_RendererHasVtxOffset);
}
void RenderImGuiNewFrame() {
    ImGui::GetPlatformIO().Renderer_TextureMaxWidth = RenderActiveDevice()->caps.maxTextureSize;
    ImGui::GetPlatformIO().Renderer_TextureMaxHeight = RenderActiveDevice()->caps.maxTextureSize;
}
static void Texture(ImTextureData *t) {
    if (t->Status == ImTextureStatus_OK || t->Status == ImTextureStatus_Destroyed)
        return;
    if (t->Status == ImTextureStatus_WantDestroy) {
        RgHandle id = (RgHandle)t->TexID;
        RgDeleteTextures(1, &id);
        t->SetTexID(ImTextureID_Invalid);
        t->SetStatus(ImTextureStatus_Destroyed);
        return;
    }
    IM_ASSERT(t->Format == ImTextureFormat_RGBA32);
    RgActiveTexture(RG_TEXTURE0);
    RgPixelStorei(RG_UNPACK_ALIGNMENT, 1);
    if (t->Status == ImTextureStatus_WantCreate) {
        RgHandle id;
        RgGenTextures(1, &id);
        RgBindTexture(RG_TEXTURE_2D, id);
        RgTexImage2D(RG_TEXTURE_2D, 0, RG_RGBA8, t->Width, t->Height, 0, RG_RGBA, RG_UNSIGNED_BYTE,
                     t->Pixels);
        RgTexParameteri(RG_TEXTURE_2D, RG_TEXTURE_MIN_FILTER, RG_LINEAR);
        RgTexParameteri(RG_TEXTURE_2D, RG_TEXTURE_MAG_FILTER, RG_LINEAR);
        RgTexParameteri(RG_TEXTURE_2D, RG_TEXTURE_WRAP_S, RG_CLAMP_TO_EDGE);
        t->SetTexID((ImTextureID)id);
    } else {
        RgBindTexture(RG_TEXTURE_2D, (RgHandle)t->TexID);
        RgPixelStorei(RG_UNPACK_ROW_LENGTH, t->Width);
        for (const ImTextureRect &r : t->Updates)
            RgTexSubImage2D(RG_TEXTURE_2D, 0, r.x, r.y, r.w, r.h, RG_RGBA, RG_UNSIGNED_BYTE,
                            t->GetPixelsAt(r.x, r.y));
        RgPixelStorei(RG_UNPACK_ROW_LENGTH, 0);
    }
    RgPixelStorei(RG_UNPACK_ALIGNMENT, 2);
    t->SetStatus(ImTextureStatus_OK);
}
void RenderImGuiDraw(ImDrawData *data) {
    if (data->Textures)
        for (ImTextureData *t : *data->Textures)
            Texture(t);
    int width = (int)(data->DisplaySize.x * data->FramebufferScale.x),
        height = (int)(data->DisplaySize.y * data->FramebufferScale.y);
    if (width <= 0 || height <= 0)
        return;
    for (const ImDrawList *list : data->CmdLists) {
        RgBindBuffer(RG_ARRAY_BUFFER, vertexBuffer);
        RgBufferData(RG_ARRAY_BUFFER, list->VtxBuffer.Size * sizeof(ImDrawVert),
                     list->VtxBuffer.Data, RG_STREAM_DRAW);
        RgBindBuffer(RG_ELEMENT_ARRAY_BUFFER, indexBuffer);
        RgBufferData(RG_ELEMENT_ARRAY_BUFFER, list->IdxBuffer.Size * sizeof(ImDrawIdx),
                     list->IdxBuffer.Data, RG_STREAM_DRAW);
        linear = true;
        for (const ImDrawCmd &cmd : list->CmdBuffer) {
            if (cmd.UserCallback) {
                if (cmd.UserCallback != ImDrawCallback_ResetRenderState)
                    cmd.UserCallback(list, &cmd);
                continue;
            }
            float x = (cmd.ClipRect.x - data->DisplayPos.x) * data->FramebufferScale.x,
                  y = (cmd.ClipRect.y - data->DisplayPos.y) * data->FramebufferScale.y;
            float r = (cmd.ClipRect.z - data->DisplayPos.x) * data->FramebufferScale.x,
                  b = (cmd.ClipRect.w - data->DisplayPos.y) * data->FramebufferScale.y;
            if (r <= x || b <= y)
                continue;
            RenderRect clip = {(int)x, height - (int)b, (int)(r - x), (int)(b - y)};
            float sx = 2 / data->DisplaySize.x, sy = -2 / data->DisplaySize.y;
            RgDrawUI(vertexBuffer, indexBuffer, (RgHandle)cmd.GetTexID(), clip, width, height,
                     cmd.IdxOffset, cmd.ElemCount, sizeof(ImDrawIdx),
                     cmd.VtxOffset * sizeof(ImDrawVert), sx, sy, -1 - data->DisplayPos.x * sx,
                     1 - data->DisplayPos.y * sy, linear);
        }
    }
}
