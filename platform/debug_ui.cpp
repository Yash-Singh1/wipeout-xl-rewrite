// The debug console: the ~ key (below Esc) opens it over the whole game window (Dear ImGui). It shows the frame rates and
// changes the settings (settings.h), which are saved as they change: the frame-rate overlay, the draw
// distance and its fade, the frame rate, widescreen, the intro movie, full screen, and where the settings and the
// memory cards live. Not in the tests' hidden runs.
#include <SDL3/SDL.h>
#include <SDL3/SDL_opengl.h>

#include <stdio.h>
#include <string.h>

#include "imgui.h"
#include "backends/imgui_impl_opengl3.h"
#include "backends/imgui_impl_sdl3.h"

extern "C" {
#include "debug_ui.h"
#include "original.h"
#include "platform.h"
#include "settings.h"
#include "trace.h"
#include "psx/framecap.h"
}

static SDL_Window *window;
static ImGuiStyle baseStyle; /* the style at a scale of 1 (FitToWindow) */
static bool ready, open;
static char saveDirEdit[1024];
static char originalEdit[1024];     // the original Wipeout's disc image or folder, as typed or chosen
static char originalMessage[300];   // what opening it found
static char *originalChosen;        // a path from the file dialog (it may answer from a thread of its own)
static SDL_Mutex *originalLock;
static bool showRecorder; // Tab brings the Recorder tab up
// the frame viewer's pixel, in the console's columns and rows (left of 0 and beyond 319: widescreen's sides)
static bool picked;
static int pickX, pickY;
static bool testHover; // tests: where the pointer is taken to be over the picture
static int testHoverX, testHoverY;
static uint64_t recordingStopped; // when a RECORD_WITH_TAB recording stopped: the overlay says so a while

// the rates, over the last half second
static uint64_t rateStart, presents, swapsAtStart;
static float presentRate, gameRate;
static float frameTimes[120];
static int frameTimeNext;
static uint64_t lastPresent;

extern "C" void DebugUiInit(SDL_Window *w, void *context)
{
    window = w;
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO &io = ImGui::GetIO();
    io.IniFilename = nullptr; // the console keeps no layout file
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    ImGui::StyleColorsDark();
    ImGuiStyle &style = ImGui::GetStyle();
    style.WindowRounding = 6.0f;
    style.FrameRounding = 4.0f;
    style.Colors[ImGuiCol_WindowBg].w = 0.92f;
    baseStyle = style;
    ImGui_ImplSDL3_InitForOpenGL(w, context);
    ImGui_ImplOpenGL3_Init(PORT_GLSL_VERSION);
    SDL_strlcpy(saveDirEdit, portSettings.saveDir, sizeof(saveDirEdit));
    SDL_strlcpy(originalEdit, portSettings.originalPath, sizeof(originalEdit));
    originalLock = SDL_CreateMutex();
    ready = true;
}

extern "C" void DebugUiShutdown(void)
{
    if (!ready) {
        return;
    }
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplSDL3_Shutdown();
    ImGui::DestroyContext();
    ready = false;
}

extern "C" void DebugUiShowRecorder(void)
{
    open = true;
    showRecorder = true;
    // tests: WIPEOUT_FRAMECAP_PICK=x,y picks a pixel of the newest shown frame
    const char *pick = SDL_getenv("WIPEOUT_FRAMECAP_PICK");
    if (pick) {
        picked = SDL_sscanf(pick, "%d,%d", &pickX, &pickY) == 2;
    }
    // and WIPEOUT_FRAMECAP_HOVER=x,y shows the magnifier at a pixel
    const char *hover = SDL_getenv("WIPEOUT_FRAMECAP_HOVER");
    if (hover) {
        testHover = SDL_sscanf(hover, "%d,%d", &testHoverX, &testHoverY) == 2;
    }
}

// Tab: the frame recorder freezes the game on the last seconds of drawing, or resumes; or starts and stops a
// recording (the settings say which)
extern "C" void DebugUiTab(void)
{
    bool wasTaking = FrameCapTaking();

    FrameCapTab();
    if (FrameCapFrozen()) {
        DebugUiShowRecorder();
    } else if (wasTaking && !FrameCapTaking()) {
        showRecorder = true; // the console opens on the recording
        recordingStopped = SDL_GetTicks();
    }
}

extern "C" int DebugUiOpen(void)
{
    return ready && open;
}

extern "C" int DebugUiEvent(const SDL_Event *event)
{
    if (!ready) {
        return 0;
    }
    if (event->type == SDL_EVENT_KEY_DOWN && event->key.scancode == SDL_SCANCODE_GRAVE && !event->key.repeat) {
        open = !open;
        return 1;
    }
    if (event->type == SDL_EVENT_KEY_DOWN && event->key.scancode == SDL_SCANCODE_TAB && !event->key.repeat) {
        DebugUiTab();
        return 1;
    }
    if (!open) {
        return 0;
    }
    ImGui_ImplSDL3_ProcessEvent(event);
    // while open the keyboard and mouse are the console's; key releases still reach the game, so no
    // key stays held
    switch (event->type) {
    case SDL_EVENT_KEY_DOWN:
    case SDL_EVENT_TEXT_INPUT:
    case SDL_EVENT_MOUSE_BUTTON_DOWN:
    case SDL_EVENT_MOUSE_BUTTON_UP:
    case SDL_EVENT_MOUSE_MOTION:
    case SDL_EVENT_MOUSE_WHEEL:
        return 1;
    default:
        return 0;
    }
}

static void OpenFolder(const char *folder)
{
    char url[1200];

    SDL_snprintf(url, sizeof(url), "file://%s", folder);
    SDL_OpenURL(url);
}

static void Changed(void)
{
    PortSettingsSave();
}

static void Rates(void)
{
    uint64_t now = SDL_GetTicksNS();

    if (lastPresent) {
        frameTimes[frameTimeNext] = (float)((now - lastPresent) / 1e6);
        frameTimeNext = (frameTimeNext + 1) % IM_ARRAYSIZE(frameTimes);
    }
    lastPresent = now;
    presents++;
    if (!rateStart) {
        rateStart = now;
        swapsAtStart = PortGameFrames();
        presents = 0;
    } else if (now - rateStart >= 500000000ull) {
        double seconds = (now - rateStart) / 1e9;
        presentRate = (float)(presents / seconds);
        gameRate = (float)((PortGameFrames() - swapsAtStart) / seconds);
        rateStart = now;
        swapsAtStart = PortGameFrames();
        presents = 0;
    }
}

static void FpsOverlay(void)
{
    ImGui::SetNextWindowPos(ImVec2(8, 8), ImGuiCond_Always);
    ImGui::SetNextWindowBgAlpha(0.55f);
    ImGui::Begin("##fps", nullptr,
                 ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoInputs |
                     ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_NoNav);
    ImGui::Text("%.0f fps  (game %.0f)", presentRate, gameRate);
    ImGui::End();
}

// While a RECORD_WITH_TAB recording is made (and a while after it stops), with the console closed: in the corner.
static bool RecorderNoteWanted(void)
{
    return !open && (FrameCapTaking() || (recordingStopped && SDL_GetTicks() - recordingStopped < 4000));
}

static void RecorderNote(void)
{
    const ImGuiViewport *view = ImGui::GetMainViewport();

    ImGui::SetNextWindowPos(ImVec2(view->WorkPos.x + view->WorkSize.x - 8, view->WorkPos.y + 8), ImGuiCond_Always,
                            ImVec2(1.0f, 0.0f));
    ImGui::SetNextWindowBgAlpha(0.55f);
    ImGui::Begin("##recording", nullptr,
                 ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoInputs |
                     ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_NoNav);
    if (FrameCapTaking()) {
        ImGui::TextColored(ImVec4(1.0f, 0.25f, 0.25f, 1.0f), "REC");
        ImGui::SameLine();
        ImGui::Text("%.1f s   (Tab stops)", FrameCapFrameCount() / 30.0f);
    } else {
        ImGui::Text("Recorded %.1f s: ~ shows it (the console's Recorder tab)", FrameCapFrameCount() / 30.0f);
    }
    ImGui::End();
}

// ---- the frame recorder's viewer (psx/framecap.h) ----

static int frameShown = -1, frameUploaded = -1;
static GLuint frameTexture;
static int pickHit = -1;

// The recorded picture, its pixels sharp (the backend samples linearly unless told otherwise).
static void PixelImage(ImVec2 size, ImVec2 uv0, ImVec2 uv1)
{
    ImGuiPlatformIO &io = ImGui::GetPlatformIO();
    ImDrawList *draw = ImGui::GetWindowDrawList();

    if (io.DrawCallback_SetSamplerNearest) {
        draw->AddCallback(io.DrawCallback_SetSamplerNearest, nullptr);
    }
    ImGui::Image((ImTextureID)(intptr_t)frameTexture, size, uv0, uv1);
    if (io.DrawCallback_SetSamplerLinear) {
        draw->AddCallback(io.DrawCallback_SetSamplerLinear, nullptr);
    }
}
static FrameCapHit hits[256];
static int hitCount;

static const char *PrimName(const FrameCapPrim *p, char *out, size_t size)
{
    static const char *const kinds[] = {"polygon", "line", "rectangle", "fill"};

    if (p->kind == FRAMECAP_POLYGON) {
        SDL_snprintf(out, size, "%s%s%d%s%s", p->code & 0x10 ? "G" : "F", p->code & 0x04 ? "T" : "", p->count,
                     p->code & 0x02 ? " semi" : "", p->code & 0x01 ? " raw" : "");
    } else {
        SDL_snprintf(out, size, "%s%s", kinds[p->kind & 3], p->kind != FRAMECAP_FILL && (p->code & 0x02) ? " semi" : "");
    }
    return out;
}

static void Explain(const FrameCapPrim *p, const FrameCapHit *h, char *out, size_t size)
{
    static const char *const blends[] = {"B/2+F/2", "B+F", "B-F (subtracts)", "B+F/4"};
    size_t n = 0;

    out[0] = 0;
    if (h->skipped) {
        n += SDL_snprintf(out + n, size - n, "NOT DRAWN (%s); ",
                          p->flags & FRAMECAP_DROPPED ? "dropped" : "over the GPU's 1023x511 limit");
    }
    if (h->transparent) {
        n += SDL_snprintf(out + n, size - n, "transparent texel: what is below shows; ");
    }
    if (p->draw.semi >= 0 && p->kind != FRAMECAP_FILL) {
        n += SDL_snprintf(out + n, size - n, "blend %s; ", blends[p->draw.semi & 3]);
    }
    if (h->alpha < 254.5f && p->kind != FRAMECAP_FILL) {
        n += SDL_snprintf(out + n, size - n, "faded to %.0f%%; ", h->alpha / 2.55f);
    }
    if (p->flags & FRAMECAP_FAR) {
        n += SDL_snprintf(out + n, size - n, "port's far view; ");
    }
    if (p->kind == FRAMECAP_POLYGON && !(p->flags & FRAMECAP_PRECISE)) {
        n += SDL_snprintf(out + n, size - n, "no precise corners (affine); ");
    }
    if (!h->skipped && !h->transparent && h->r + h->g + h->b < 24) {
        n += SDL_snprintf(out + n, size - n, "writes BLACK; ");
    }
}

static void Pick(int frame)
{
    const FrameCapFrame *f = FrameCapFrameAt(frame);

    hitCount = 0;
    pickHit = -1;
    if (f && picked) {
        hitCount = FrameCapHits(frame, f->areaX + pickX, f->areaY + pickY, hits, IM_ARRAYSIZE(hits));
        pickHit = hitCount - 1; // the topmost
    }
}

static void RecorderTab(void)
{
    static const char *const modes[] = {"Off", "Keep the last 3 seconds (Tab freezes the game)",
                                        "Record with Tab (Tab starts and stops; the game goes on)"};
    static int shownGeneration = -1;
    int mode = FrameCapMode();
    int total = FrameCapFrameCount();
    float ui = ImGui::GetIO().FontGlobalScale;
    const ImVec4 note(1.0f, 0.8f, 0.3f, 1.0f);

    if (ImGui::Combo("Frame recorder", &mode, modes, IM_ARRAYSIZE(modes))) {
        portSettings.recordFrames = mode;
        FrameCapClear();
        Changed();
    }
    if (mode == RECORD_LAST_SECONDS && !FrameCapFrozen()) {
        ImGui::TextWrapped("Tab freezes the game and saves the last 3 seconds; here you can then step back through "
                           "them and click a pixel to see every primitive drawn there.");
    } else if (mode == RECORD_LAST_SECONDS) {
        ImGui::TextColored(note, "Frozen. Tab resumes the game.");
    } else if (mode == RECORD_WITH_TAB && FrameCapTaking()) {
        ImGui::TextColored(note, "Recording: %d frames so far. Tab stops.", total);
    } else if (mode == RECORD_WITH_TAB) {
        ImGui::TextWrapped("Tab starts recording and Tab again stops it (it keeps up to the last 10 seconds). The game "
                           "goes on meanwhile: pause it yourself to look at the recording here, frame by frame, and "
                           "click a pixel to see every primitive drawn there.");
        if (FrameCapViewable() && ImGui::Button("Save this recording")) {
            FrameCapSave();
        }
    }
    if (FrameCapSavedDir()) {
        ImGui::TextWrapped("Saved to %s", FrameCapSavedDir());
        if (ImGui::Button("Show saved frames")) {
            OpenFolder(FrameCapSavedDir());
        }
    }
    if (!FrameCapViewable()) {
        frameShown = -1;
        return;
    }
    if (shownGeneration != FrameCapGeneration()) {
        // another recording: from its newest frame, its picture uploaded afresh
        shownGeneration = FrameCapGeneration();
        frameShown = frameUploaded = -1;
    }
    ImGui::Separator();
    if (frameShown < 0 || frameShown >= total) {
        frameShown = total - 1;
        while (frameShown > 0 && !FrameCapFrameAt(frameShown)->shown) {
            frameShown--; // the newest frame that reached the screen
        }
    }
    bool changed = false;
    if (ImGui::ArrowButton("##previous", ImGuiDir_Left) && frameShown > 0) {
        frameShown--;
        changed = true;
    }
    ImGui::SameLine();
    if (ImGui::ArrowButton("##next", ImGuiDir_Right) && frameShown < total - 1) {
        frameShown++;
        changed = true;
    }
    ImGui::SameLine();
    ImGui::SetNextItemWidth(-1);
    changed |= ImGui::SliderInt("##frame", &frameShown, 0, total - 1, "frame %d");
    const FrameCapFrame *f = FrameCapFrameAt(frameShown);
    ImGui::Text("game frame %llu, %d primitives, %s", (unsigned long long)f->gameFrame, f->count,
                f->shown ? "shown" : "never shown");

    // the picture; a click picks the pixel
    if (!frameTexture) {
        glGenTextures(1, &frameTexture);
        glBindTexture(GL_TEXTURE_2D, frameTexture);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    }
    // the picture: the console's 320 columns, and with widescreen the sides (from column `pictureX`)
    bool hasPicture = f->picture && f->shown;
    int pictureW = hasPicture ? f->pictureW : 320, pictureX = hasPicture ? f->pictureX : 0;
    if (frameUploaded != frameShown || changed) {
        static uint8_t black[320 * 240 * 4];
        glBindTexture(GL_TEXTURE_2D, frameTexture);
        glPixelStorei(GL_UNPACK_ALIGNMENT, 4);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, pictureW, 240, 0, GL_RGBA, GL_UNSIGNED_BYTE,
                     hasPicture ? f->picture : black);
        glPixelStorei(GL_UNPACK_ALIGNMENT, 2);
        frameUploaded = frameShown;
        Pick(frameShown);
    }
    // the picture as large as fits, the picked pixel's primitives beside it (in a narrow window below it)
    ImVec2 room = ImGui::GetContentRegionAvail();
    bool beside = room.x > 1000 * ui;
    float zoom = beside ? SDL_min(room.x * 0.5f / pictureW, room.y / 240.0f)
                        : SDL_min(room.x / pictureW, (room.y - 300 * ui) / 240.0f);
    zoom = SDL_max(1.0f, zoom);
    ImVec2 origin = ImGui::GetCursorScreenPos();
    PixelImage(ImVec2(pictureW * zoom, 240 * zoom), ImVec2(0, 0), ImVec2(1, 1));
    bool hovered = ImGui::IsItemHovered();
    if (hovered || testHover) {
        ImVec2 m = ImGui::GetMousePos();
        // the picture's column and row, then the console's
        int px = hovered ? (int)((m.x - origin.x) / zoom) : testHoverX - pictureX;
        int py = hovered ? (int)((m.y - origin.y) / zoom) : testHoverY;
        px = SDL_clamp(px, 0, pictureW - 1);
        py = SDL_clamp(py, 0, 239);
        int x = px + pictureX, y = py;

        if (ImGui::IsItemClicked()) {
            picked = true;
            pickX = x;
            pickY = y;
            Pick(frameShown);
        }
        // the magnifier: the pixels about the pointer, ten times over, the one under it outlined
        const int span = 17;
        const float cell = 10.0f * ui;
        int left = SDL_clamp(px - span / 2, 0, pictureW - span), top = SDL_clamp(py - span / 2, 0, 240 - span);
        if (!hovered) {
            ImGui::SetNextWindowPos(ImVec2(origin.x + (px + 2) * zoom, origin.y + (py + 2) * zoom));
        }
        ImGui::BeginTooltip();
        ImVec2 corner = ImGui::GetCursorScreenPos();
        PixelImage(ImVec2(span * cell, span * cell), ImVec2((float)left / pictureW, top / 240.0f),
                   ImVec2((float)(left + span) / pictureW, (top + span) / 240.0f));
        ImVec2 a(corner.x + (px - left) * cell, corner.y + (py - top) * cell);
        ImGui::GetWindowDrawList()->AddRect(ImVec2(a.x - 1, a.y - 1), ImVec2(a.x + cell + 1, a.y + cell + 1),
                                            IM_COL32(0, 0, 0, 255), 0.0f, 0, 3.0f);
        ImGui::GetWindowDrawList()->AddRect(a, ImVec2(a.x + cell, a.y + cell), IM_COL32(255, 0, 255, 255), 0.0f, 0, 1.5f);
        if (hasPicture) {
            const uint8_t *rgb = &f->picture[(py * pictureW + px) * 4];
            ImGui::Text("pixel %d, %d   colour %d, %d, %d", x, y, rgb[0], rgb[1], rgb[2]);
        } else {
            ImGui::Text("pixel %d, %d", x, y);
        }
        ImGui::EndTooltip();
    }
    ImDrawList *draw = ImGui::GetWindowDrawList();
    draw->PushClipRect(origin, ImVec2(origin.x + pictureW * zoom, origin.y + 240 * zoom), true);
    if (pickHit >= 0 && pickHit < hitCount) {
        const FrameCapPrim *p = &f->prims[hits[pickHit].prim];
        int corners = p->kind == FRAMECAP_FILL ? 0 : p->count;
        // its outline: the quad's corners go 0, 1, 3, 2 around
        static const int quadOrder[4] = {0, 1, 3, 2};
        ImVec2 points[4];
        for (int i = 0; i < corners; i++) {
            const GpuVertex *v = &p->v[corners == 4 ? quadOrder[i] : i];
            points[i] = ImVec2(origin.x + (v->x - f->areaX - pictureX) * zoom, origin.y + (v->y - f->areaY) * zoom);
        }
        if (corners >= 2) {
            draw->AddPolyline(points, corners, IM_COL32(0, 255, 0, 255), ImDrawFlags_Closed, 2.0f);
        }
    }
    if (picked) {
        ImVec2 c(origin.x + (pickX - pictureX + 0.5f) * zoom, origin.y + (pickY + 0.5f) * zoom);
        draw->AddCircle(c, 6.0f * ui, IM_COL32(255, 0, 255, 255), 0, 2.0f);
    }
    draw->PopClipRect();
    if (beside) {
        ImGui::SameLine();
    }
    ImGui::BeginChild("##pixel", ImVec2(0, 0), ImGuiChildFlags_None, ImGuiWindowFlags_HorizontalScrollbar);
    if (picked) {
        // wrapped at the pane's edge (the pane scrolls sideways for the details' long lines)
        ImGui::PushTextWrapPos(ImGui::GetCursorPosX() + ImGui::GetContentRegionAvail().x);
        ImGui::TextWrapped("Pixel %d, %d: %d primitives there (topmost first). Click one for its details.", pickX,
                           pickY, hitCount);
        ImGui::PopTextWrapPos();
    } else {
        ImGui::TextUnformatted("Click the picture to pick a pixel.");
    }
    // the list, above the picked primitive's details (eight lines)
    float listHeight = SDL_max(120 * ui, ImGui::GetContentRegionAvail().y - 8 * ImGui::GetTextLineHeightWithSpacing());
    if (ImGui::BeginTable("hits", 4, ImGuiTableFlags_RowBg | ImGuiTableFlags_ScrollY | ImGuiTableFlags_Borders,
                          ImVec2(-1, listHeight))) {
        ImGui::TableSetupColumn("#", ImGuiTableColumnFlags_WidthFixed, 40 * ui);
        ImGui::TableSetupColumn("built by", ImGuiTableColumnFlags_WidthFixed, 190 * ui);
        ImGui::TableSetupColumn("writes", ImGuiTableColumnFlags_WidthFixed, 60 * ui);
        ImGui::TableSetupColumn("notes");
        ImGui::TableHeadersRow();
        for (int k = hitCount - 1; k >= 0; k--) {
            const FrameCapHit *h = &hits[k];
            const FrameCapPrim *p = &f->prims[h->prim];
            char name[48], label[96], notes[256];
            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            SDL_snprintf(label, sizeof(label), "%u##hit%d", p->serial, k);
            if (ImGui::Selectable(label, pickHit == k, ImGuiSelectableFlags_SpanAllColumns)) {
                pickHit = k;
            }
            ImGui::TableNextColumn();
            ImGui::Text("%s  %s", p->tag[0] ? p->tag : "(untagged)", PrimName(p, name, sizeof(name)));
            ImGui::TableNextColumn();
            ImGui::ColorButton("##c", ImVec4(h->r / 255.0f, h->g / 255.0f, h->b / 255.0f, 1.0f),
                               ImGuiColorEditFlags_NoTooltip, ImVec2(40 * ui, 0));
            ImGui::TableNextColumn();
            Explain(p, h, notes, sizeof(notes));
            ImGui::TextUnformatted(notes);
        }
        ImGui::EndTable();
    }
    if (pickHit >= 0 && pickHit < hitCount) {
        const FrameCapHit *h = &hits[pickHit];
        const FrameCapPrim *p = &f->prims[h->prim];
        char name[48];
        ImGui::Text("#%u %s, %s, packet %02x at %llx, triangle %d", p->serial, PrimName(p, name, sizeof(name)),
                    p->tag[0] ? p->tag : "(untagged)", p->code, (unsigned long long)p->address, h->triangle);
        for (int i = 0; i < (p->kind == FRAMECAP_FILL ? 2 : p->count); i++) {
            const GpuVertex *v = &p->v[i];
            ImGui::Text("  corner %d: packet %d,%d  drawn %.1f,%.1f  depth %.0f  rgb %d,%d,%d  a %d  uv %.0f,%.0f", i,
                        p->ix[i], p->iy[i], v->x - f->offsetX, v->y - f->offsetY, v->w, v->r, v->g, v->b, v->a, v->u,
                        v->v);
        }
        if (p->draw.textured) {
            ImGui::Text("  texture page %d,%d (%s), palette %d,%d; at the pixel uv %.1f,%.1f texel %04x", p->draw.pageX,
                        p->draw.pageY, p->draw.depth == 0 ? "4-bit" : p->draw.depth == 1 ? "8-bit" : "15-bit",
                        p->draw.clutX, p->draw.clutY, h->u, h->v, h->texel & 0xFFFF);
        }
        ImGui::Text("  writes rgb %.0f,%.0f,%.0f", h->r, h->g, h->b);
    }
    ImGui::EndChild();
}

/* WIPEOUT_DEBUG_UI=stats|graphics|game|recorder|files (tests): the tab shown first */
static ImGuiTabItemFlags FirstTab(const char *name)
{
    static int done;
    const char *wanted = SDL_getenv("WIPEOUT_DEBUG_UI");

    if (done >= 5 || !wanted || SDL_strcasecmp(wanted, name)) {
        return 0;
    }
    done++;
    return ImGuiTabItemFlags_SetSelected;
}

/* The console's size follows the game window's: laid out for 720 points across, it is scaled to fit the window
   (ImGui works in the window's points; the backend draws them at the display's density). */
static void FitToWindow(void)
{
    static float applied;
    int w, h;
    float ui;

    SDL_GetWindowSize(window, &w, &h);
    if (w <= 0 || h <= 0) {
        return;
    }
    ui = SDL_clamp(SDL_min(w * 0.92f / 720.0f, h * 0.9f / 560.0f), 0.6f, 3.0f);
    if (SDL_fabsf(ui - applied) < 0.01f) {
        return;
    }
    applied = ui;
    ImGui::GetStyle() = baseStyle;
    ImGui::GetStyle().ScaleAllSizes(ui);
    ImGui::GetIO().FontGlobalScale = ui;
}

// The original Wipeout's tracks (platform/original.c): the player's copy of the original, raced from the RACING
// CLASS menu's WIPEOUT line.
static void SDLCALL OriginalChosen(void *user, const char *const *files, int filter)
{
    (void)user;
    (void)filter;
    if (files && files[0]) {
        SDL_LockMutex(originalLock);
        SDL_free(originalChosen);
        originalChosen = SDL_strdup(files[0]);
        SDL_UnlockMutex(originalLock);
    }
}

static void UseOriginal(const char *path)
{
    SDL_strlcpy(originalEdit, path, sizeof(originalEdit));
    if (PortOriginalOpen(path, originalMessage, sizeof(originalMessage)) > 0 || !path[0]) {
        SDL_strlcpy(portSettings.originalPath, path, sizeof(portSettings.originalPath));
        Changed();
    }
}

static void OriginalWipeout(void)
{
    char *chosen;

    SDL_LockMutex(originalLock);
    chosen = originalChosen;
    originalChosen = nullptr;
    SDL_UnlockMutex(originalLock);
    if (chosen) {
        UseOriginal(chosen);
        SDL_free(chosen);
    }
    ImGui::TextUnformatted("The original Wipeout (PlayStation)");
    ImGui::TextWrapped("Your copy of the original game: its disc image (a .cue with its .bin), or a folder of its "
                       "data (the disc's WIPEOUT folder). Its tracks are then on the RACING CLASS menu, on the "
                       "WIPEOUT line under the classes (arcade races and time trials): left and right go through them.");
    int tracks = PortOriginalTrackCount();
    if (!originalEdit[0] && PortOriginalSource()[0]) {
        SDL_strlcpy(originalEdit, PortOriginalSource(), sizeof(originalEdit));
    }
    if (tracks > 0) {
        ImGui::TextColored(ImVec4(0.5f, 1.0f, 0.5f, 1.0f), "%d tracks from %s", tracks, PortOriginalSource());
    } else {
        ImGui::TextDisabled("None chosen.");
    }
    if (originalMessage[0]) {
        ImGui::TextWrapped("%s", originalMessage);
    }
#ifndef __EMSCRIPTEN__
    static const SDL_DialogFileFilter cue[] = {{"PlayStation disc image (.cue)", "cue"}};
    if (ImGui::Button("Choose its disc image...")) {
        SDL_ShowOpenFileDialog(OriginalChosen, nullptr, window, cue, 1, nullptr, false);
    }
    ImGui::SameLine();
    if (ImGui::Button("Choose its data folder...")) {
        SDL_ShowOpenFolderDialog(OriginalChosen, nullptr, window, nullptr, false);
    }
    ImGui::SameLine();
#endif
    if (ImGui::Button("Forget it")) {
        UseOriginal("");
        originalMessage[0] = 0;
    }
    ImGui::SetNextItemWidth(-ImGui::CalcTextSize("Use this").x - ImGui::GetStyle().ItemSpacing.x -
                            ImGui::GetStyle().FramePadding.x * 2);
    ImGui::InputText("##original", originalEdit, sizeof(originalEdit));
    ImGui::SameLine();
    if (ImGui::Button("Use this")) {
        UseOriginal(originalEdit);
    }
}

static void Console(void)
{
    const ImGuiViewport *view = ImGui::GetMainViewport();

    // the whole window
    ImGui::SetNextWindowPos(view->WorkPos, ImGuiCond_Always);
    ImGui::SetNextWindowSize(view->WorkSize, ImGuiCond_Always);
    // see-through: the game goes on behind it
    ImGui::SetNextWindowBgAlpha(0.8f);
    float width = 720 * ImGui::GetIO().FontGlobalScale;
    if (!ImGui::Begin("Wipeout XL debug console   (~ closes it)", &open,
                      ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize |
                          ImGuiWindowFlags_NoSavedSettings)) {
        ImGui::End();
        return;
    }
    ImGui::PushItemWidth(width * 0.55f);
    if (ImGui::BeginTabBar("tabs")) {
        if (ImGui::BeginTabItem("Stats", nullptr, FirstTab("stats"))) {
            bool showFps = portSettings.showFps != 0;
            if (ImGui::Checkbox("Show frame rate overlay", &showFps)) {
                portSettings.showFps = showFps;
                Changed();
            }
            ImGui::Separator();
            ImGui::Text("Display: %.1f frames a second", presentRate);
            ImGui::Text("Game:    %.1f frames a second (the console's rate is 30)", gameRate);
            ImGui::Text("Video fields: %llu", (unsigned long long)PsxFieldCount());
            float worst = 0;
            for (float t : frameTimes) {
                worst = t > worst ? t : worst;
            }
            char label[64];
            SDL_snprintf(label, sizeof(label), "frame time, worst %.1f ms", worst);
            ImGui::PlotLines("##frametimes", frameTimes, IM_ARRAYSIZE(frameTimes), frameTimeNext, label, 0.0f,
                             SDL_max(40.0f, worst), ImVec2(-1, 80 * ImGui::GetIO().FontGlobalScale));
            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem("Graphics", nullptr, FirstTab("graphics"))) {
            static const char *const distances[] = {"Console (pop-in at the original distance)", "Full (the whole track)"};
            int distance = portSettings.farView ? 1 : 0;
            if (ImGui::Combo("Draw distance", &distance, distances, IM_ARRAYSIZE(distances))) {
                portSettings.farView = distance;
                Changed();
            }
            ImGui::BeginDisabled(!portSettings.farView);
            bool fade = portSettings.fade != 0;
            if (ImGui::Checkbox("Fade distant scenery into the sky", &fade)) {
                portSettings.fade = fade;
                Changed();
            }
            ImGui::EndDisabled();
            ImGui::Separator();
            static const char *const rates[] = {"Uncapped", "Display refresh (VSync)", "Capped", "Console (60 fields a second)"};
            int rate = portSettings.frameRate;
            if (ImGui::Combo("Frame rate", &rate, rates, IM_ARRAYSIZE(rates))) {
                portSettings.frameRate = rate;
                Changed();
            }
            if (portSettings.frameRate == FRAME_RATE_CAPPED) {
                int cap = portSettings.frameCap;
                if (ImGui::DragInt("Pictures a second", &cap, 1.0f, 30, 1000)) {
                    portSettings.frameCap = SDL_clamp(cap, 30, 1000);
                    Changed();
                }
            }
            ImGui::TextDisabled("The game keeps its 30 frames a second; the pictures between move smoothly from one to the next.");
            ImGui::Separator();
            bool wide = portSettings.widescreen != 0;
            if (ImGui::Checkbox("Widescreen", &wide)) {
                portSettings.widescreen = wide;
                Changed();
            }
            ImGui::TextDisabled("In a window wider than 4:3 a race fills it, the HUD at the edges, and the menus' backdrop fills the sides.");
            bool fullscreen = (SDL_GetWindowFlags(window) & SDL_WINDOW_FULLSCREEN) != 0;
            if (ImGui::Checkbox("Full screen (Alt+Enter, F11)", &fullscreen)) {
                SDL_SetWindowFullscreen(window, fullscreen);
            }
            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem("Game", nullptr, FirstTab("game"))) {
            bool skip = portSettings.skipIntro != 0;
            if (ImGui::Checkbox("Skip the intro movie at start-up", &skip)) {
                portSettings.skipIntro = skip;
                Changed();
            }
            ImGui::TextDisabled("The race endings' movies still play.");
            ImGui::Separator();
            bool record = portSettings.recordReplays != 0;
            if (ImGui::Checkbox("Record every session as a replay (from the next start)", &record)) {
                portSettings.recordReplays = record;
                Changed();
            }
            if (PortRecordDir()) {
                ImGui::TextWrapped("Recording this session to %s", PortRecordDir());
                if (ImGui::Button("Show replay folder")) {
                    OpenFolder(PortRecordDir());
                }
            }
            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem("Recorder", nullptr,
                                FirstTab("recorder") | (showRecorder ? ImGuiTabItemFlags_SetSelected : 0))) {
            showRecorder = false;
            RecorderTab();
            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem("Files", nullptr, FirstTab("files"))) {
            ImGui::TextUnformatted("Settings file");
            ImGui::TextWrapped("%s", PortSettingsPath());
            if (ImGui::Button("Show settings folder")) {
                OpenFolder(PortSettingsDefaultSaveDir());
            }
            ImGui::Separator();
            ImGui::TextUnformatted("Memory cards (saves) in use");
            ImGui::TextWrapped("%s", portConfig.savePath ? portConfig.savePath : "");
            if (ImGui::Button("Show saves folder") && portConfig.savePath) {
                OpenFolder(portConfig.savePath);
            }
            ImGui::Spacing();
            ImGui::TextUnformatted("Saves folder (empty: the settings folder)");
            ImGui::SetNextItemWidth(-1);
            ImGui::InputText("##savedir", saveDirEdit, sizeof(saveDirEdit));
            if (ImGui::Button("Use this folder")) {
                SDL_strlcpy(portSettings.saveDir, saveDirEdit, sizeof(portSettings.saveDir));
                Changed();
            }
            ImGui::SameLine();
            if (ImGui::Button("Default")) {
                saveDirEdit[0] = 0;
                portSettings.saveDir[0] = 0;
                Changed();
            }
            const char *configured = portSettings.saveDir[0] ? portSettings.saveDir : PortSettingsDefaultSaveDir();
            if (portConfig.savePath && strncmp(configured, portConfig.savePath, strlen(configured)) != 0) {
                ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.3f, 1.0f), "Takes effect at the next start.");
            }
            ImGui::Separator();
            OriginalWipeout();
            ImGui::EndTabItem();
        }
        ImGui::EndTabBar();
    }
    ImGui::PopItemWidth();
    ImGui::End();
}

extern "C" void DebugUiRender(void)
{
    if (!ready) {
        return;
    }
    Rates();
    if (!open && !portSettings.showFps && !RecorderNoteWanted()) {
        return;
    }
    int width, height;
    SDL_GetWindowSizeInPixels(window, &width, &height);
    FitToWindow();
    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplSDL3_NewFrame();
    ImGui::NewFrame();
    if (portSettings.showFps) {
        FpsOverlay();
    }
    if (RecorderNoteWanted()) {
        RecorderNote();
    }
    if (open) {
        Console();
    }
    ImGui::Render();
    glViewport(0, 0, width, height);
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
}
