// The debug console: the ~ key (below Esc) opens it over the whole game window (Dear ImGui). It shows the frame rates and
// changes the settings (settings.h), which are saved as they change: the frame-rate overlay, the draw
// distance and its fade, the frame rate, widescreen, the intro movie, full screen, and where the settings and the
// memory cards live. Not in the tests' hidden runs.
#include <SDL3/SDL.h>
#include "port_gl.h"

#include <stdio.h>
#include <string.h>

#include "imgui.h"
#include "backends/imgui_impl_opengl3.h"
#include "backends/imgui_impl_sdl3.h"

extern "C" {
#include "debug_ui.h"
#include "discimage.h"
#include "mobile.h"
#include "touch.h"
#include "hdtex.h"
#include "region.h"
#include "music.h"
#include "saves.h"
#include "original.h"
#include "platform.h"
#include "settings.h"
#include "trace.h"
#include "psx/framecap.h"
#include "psx/disc.h"
#include "psx/gpu.h"
#include "psx/psx_io.h"
}

static SDL_Window *window;
static ImGuiStyle baseStyle; /* the style at a scale of 1 (FitToWindow) */
static bool ready, open;
static char saveDirEdit[1024];
static char originalEdit[1024];     // the original Wipeout's disc image or folder, as typed or chosen
static char originalMessage[300];   // what opening it found
static char musicEdit[1024];        // the music folder, as typed or chosen
static char musicMessage[300];      // what opening it found
static char n64Edit[1024];          // Wipeout 64's cartridge image, as typed or chosen
static char n64Message[300];        // what opening it found
static char packEdit[1024];         // the HD texture pack, as typed or chosen
static char packMessage[300];       // what opening it found
static char discEdit[1024];         // the Wipeout XL disc image, as typed or chosen
static char discMessage[300];       // what checking it found
static SDL_Mutex *pickLock;         // the file dialog may answer from a thread of its own
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
    SDL_strlcpy(discEdit, portSettings.discPath[0] ? portSettings.discPath : (portConfig.discGiven ? portConfig.discGiven : ""),
                sizeof(discEdit));
    pickLock = SDL_CreateMutex();
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

extern "C" void DebugUiSetOpen(int on)
{
    open = ready && on;
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

static int frameShown = -1;
static uint64_t frameShownGame;     // ... its game frame (while a recording grows the oldest frames make way)
static bool followNewest = true;    // while recording: the newest frame, as each comes in
static bool uploaded;               // the picture in frameTexture: of game frame uploadedGame, as uploadedShown
static uint64_t uploadedGame;
static int uploadedShown;
static float pictureZoom;           // the picture's pixels per console pixel; 0: as large as fits
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
    // a recording holding still, or one being made (its frames come in as the game goes on)
    bool live = FrameCapTaking();
    if (!FrameCapViewable() && !(live && total > 0)) {
        frameShown = -1;
        return;
    }
    if (shownGeneration != FrameCapGeneration()) {
        // another recording: from its newest frame, its picture uploaded afresh
        shownGeneration = FrameCapGeneration();
        frameShown = -1;
        uploaded = false;
        followNewest = true;
    }
    ImGui::Separator();
    // while recording, the frames up to the newest shown (the one being drawn is not finished)
    int usable = total;
    while (live && usable > 0 && !FrameCapFrameAt(usable - 1)->shown) {
        usable--;
    }
    if (usable == 0) {
        ImGui::TextDisabled("Waiting for the first frame...");
        return;
    }
    if (frameShown >= 0 && !followNewest) {
        // the same frame as before, wherever it is now (the oldest once it is gone)
        int i = usable - 1;
        while (i >= 0 && FrameCapFrameAt(i)->gameFrame != frameShownGame) {
            i--;
        }
        frameShown = SDL_max(i, 0);
    }
    if (frameShown < 0 || frameShown >= usable || followNewest) {
        frameShown = usable - 1;
        while (frameShown > 0 && !FrameCapFrameAt(frameShown)->shown) {
            frameShown--; // the newest frame that reached the screen
        }
    }
    // , and . step back and forward a frame (held: on and on), as the arrows do
    int step = 0;
    if (ImGui::ArrowButton("##previous", ImGuiDir_Left)) {
        step = -1;
    }
    ImGui::SameLine();
    if (ImGui::ArrowButton("##next", ImGuiDir_Right)) {
        step = 1;
    }
    if (!ImGui::GetIO().WantTextInput) {
        if (ImGui::IsKeyPressed(ImGuiKey_Comma)) {
            step = -1;
        }
        if (ImGui::IsKeyPressed(ImGuiKey_Period)) {
            step = 1;
        }
    }
    bool changed = step != 0;
    frameShown = SDL_clamp(frameShown + step, 0, usable - 1);
    ImGui::SameLine();
    ImGui::SetNextItemWidth(-1);
    char sliderLabel[64];
    SDL_snprintf(sliderLabel, sizeof(sliderLabel), live ? "frame %%d / %d (recording)" : "frame %%d / %d", usable - 1);
    if (ImGui::SliderInt("##frame", &frameShown, 0, usable - 1, sliderLabel)) {
        changed = true;
    }
    if (changed || frameShown >= usable - 1) {
        followNewest = live && frameShown >= usable - 1; // back at the newest: following it again
    }
    const FrameCapFrame *f = FrameCapFrameAt(frameShown);
    frameShownGame = f->gameFrame;

    // the room for the picture (below this line) and the pixel's primitives
    ImVec2 room = ImGui::GetContentRegionAvail();
    room.y -= ImGui::GetFrameHeightWithSpacing();
    bool beside = room.x > 1000 * ui;
    bool hasPicture = f->picture && f->shown;
    int pictureW = hasPicture ? f->pictureW : 320, pictureX = hasPicture ? f->pictureX : 0;
    float paneW = beside ? room.x * 0.5f : room.x, paneH = beside ? room.y : room.y - 300 * ui;
    float fit = SDL_max(0.5f, SDL_min(paneW / pictureW, paneH / 240.0f));

    ImGui::Text("game frame %llu, %d primitives, %s   (, and . step)", (unsigned long long)f->gameFrame, f->count,
                f->shown ? "shown" : "never shown");
    // zoom: steps of whole and half pixels (smaller than the console's too), or as large as fits
    static const float levels[] = {0.5f, 0.75f, 1, 1.5f, 2, 2.5f, 3, 4, 5, 6, 8, 10, 12, 16, 20, 24, 32};
    float current = pictureZoom > 0 ? pictureZoom : fit;
    ImGui::SameLine();
    if (ImGui::Button(" - ##zoomout")) {
        int i = IM_ARRAYSIZE(levels) - 1;
        while (i > 0 && levels[i] >= current * 0.99f) {
            i--;
        }
        pictureZoom = levels[i];
    }
    ImGui::SameLine();
    if (ImGui::Button(" + ##zoomin")) {
        int i = 0;
        while (i < IM_ARRAYSIZE(levels) - 1 && levels[i] <= current * 1.01f) {
            i++;
        }
        pictureZoom = levels[i];
    }
    ImGui::SameLine();
    if (ImGui::Button("Fit##zoom")) {
        pictureZoom = 0;
    }
    ImGui::SameLine();
    ImGui::TextDisabled("%.1fx%s", pictureZoom > 0 ? pictureZoom : fit, pictureZoom > 0 ? "" : " (fits)");

    // the picture; a click picks the pixel
    if (!frameTexture) {
        glGenTextures(1, &frameTexture);
        glBindTexture(GL_TEXTURE_2D, frameTexture);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    }
    // the picture: the console's 320 columns, and with widescreen the sides (from column `pictureX`)
    if (!uploaded || uploadedGame != f->gameFrame || uploadedShown != f->shown) {
        static uint8_t black[320 * 240 * 4];
        glBindTexture(GL_TEXTURE_2D, frameTexture);
        glPixelStorei(GL_UNPACK_ALIGNMENT, 4);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, pictureW, 240, 0, GL_RGBA, GL_UNSIGNED_BYTE,
                     hasPicture ? f->picture : black);
        glPixelStorei(GL_UNPACK_ALIGNMENT, 2);
        uploaded = true;
        uploadedGame = f->gameFrame;
        uploadedShown = f->shown;
        Pick(frameShown);
    }
    // the picture as large as fits or as zoomed (in a pane of its own that scrolls when it is larger), the picked
    // pixel's primitives beside it (in a narrow window below it)
    float zoom = pictureZoom > 0 ? pictureZoom : fit;
    ImVec2 imageSize(pictureW * zoom, 240 * zoom);
    float bar = ImGui::GetStyle().ScrollbarSize;
    bool across = imageSize.x > paneW, down = imageSize.y > paneH;
    ImGui::BeginChild("##picture",
                      ImVec2(across ? paneW : imageSize.x + (down ? bar : 0), down ? paneH : imageSize.y + (across ? bar : 0)),
                      ImGuiChildFlags_None, ImGuiWindowFlags_HorizontalScrollbar);
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
    ImGui::EndChild();
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
#ifdef PORT_MOBILE
    /* a phone: never smaller than the screen's own scale (Android sizes its window in pixels), and it scrolls */
    ui = SDL_max(SDL_min(w * 0.92f / 720.0f, h * 0.9f / 560.0f), SDL_GetDisplayContentScale(SDL_GetDisplayForWindow(window)));
    ui = SDL_clamp(ui, 1.0f, 6.0f);
#else
    ui = SDL_clamp(SDL_min(w * 0.92f / 720.0f, h * 0.9f / 560.0f), 0.6f, 3.0f);
#endif
    if (SDL_fabsf(ui - applied) < 0.01f) {
        return;
    }
    applied = ui;
    ImGui::GetStyle() = baseStyle;
    ImGui::GetStyle().ScaleAllSizes(ui);
    ImGui::GetIO().FontGlobalScale = ui;
}

// ---- choosing files: the system's file dialog, or where the system has none the console's own browser ----

struct Picker {
    char *chosen;     // the answer, until taken
    bool folder;      // a folder is wanted, else a file of the kind:
    int kind;         // PICK_*
    bool browse;      // the system had no dialog: the console's browser
    char start[1024]; // the folder it starts in
};
enum { PICK_DISC, PICK_ZIP, PICK_N64, PICK_CARD }; // a disc image (.cue), a texture pack (.zip), a cartridge
                                                    // image, a memory card or save
static Picker discPicker, originalPicker, packPicker = {nullptr, false, PICK_ZIP, false, ""},
    n64Picker = {nullptr, false, PICK_N64, false, ""}, musicPicker, cardPicker = {nullptr, false, PICK_CARD, false, ""};
static Picker *browsing;            // the picker the console's browser is open for
static char browseDir[1024];
static char **browseNames;          // its entries: disc images first, then folders (with a trailing /)
static int browseCount;

static void SDLCALL PickerAnswer(void *user, const char *const *files, int filter)
{
    Picker *picker = (Picker *)user;

    (void)filter;
    SDL_LockMutex(pickLock);
    if (!files) {
        picker->browse = true; // no dialog on this system (or it failed): the console's browser instead
    } else if (files[0]) {
        SDL_free(picker->chosen);
        picker->chosen = SDL_strdup(files[0]);
    }
    SDL_UnlockMutex(pickLock);
}

// The folder of a path (the path itself when it is a folder), for where a picker starts.
static void StartFolder(const char *path, char *out, size_t size)
{
    SDL_PathInfo info;

    if (path && path[0] && SDL_GetPathInfo(path, &info)) {
        SDL_strlcpy(out, path, size);
        if (info.type != SDL_PATHTYPE_DIRECTORY) {
            char *slash = SDL_strrchr(out, '/');
#ifdef _WIN32
            char *back = SDL_strrchr(out, '\\');
            if (back && (!slash || back > slash)) {
                slash = back;
            }
#endif
            if (slash) {
                slash[1] = 0;
            }
        }
        return;
    }
    const char *home = PortImportFolder() ? PortImportFolder() : SDL_GetUserFolder(SDL_FOLDER_HOME);
    SDL_strlcpy(out, home ? home : "/", size);
}

static void PickerOpen(Picker *picker, bool folder, const char *current)
{
    static const SDL_DialogFileFilter cue[] = {
        {"PlayStation disc image (.cue, .bin, or an archive of one)", PORT_DISC_EXTENSIONS}};
    static const SDL_DialogFileFilter zip[] = {{"Texture pack (.zip)", "zip"}};
    static const SDL_DialogFileFilter n64[] = {{"Nintendo 64 cartridge image (.z64, .n64, .v64)", "z64;n64;v64"}};
    static const SDL_DialogFileFilter card[] = {
        {"Memory card or save", "mcd;mcr;srm;mc;gme;mem;vgs;vmp;mcs;mcb;psx;ddf;vm1;ps;bin;sav;card"}, {"All files", "*"}};

    picker->folder = folder;
    StartFolder(current, picker->start, sizeof(picker->start));
#if defined(__EMSCRIPTEN__) || defined(SDL_PLATFORM_IOS) /* (no file dialog: the console's own browser) */
    (void)cue;
    (void)zip;
    (void)n64;
    (void)card;
    picker->browse = true;
#else
    if (SDL_getenv("WIPEOUT_CONSOLE_BROWSER")) {
        picker->browse = true; /* the console's browser, not the system's dialog */
        return;
    }
    if (folder) {
        SDL_ShowOpenFolderDialog(PickerAnswer, picker, window, picker->start, false);
    } else {
        if (picker->kind == PICK_CARD) {
            SDL_ShowOpenFileDialog(PickerAnswer, picker, window, card, 2, picker->start, false);
        } else {
            SDL_ShowOpenFileDialog(PickerAnswer, picker, window,
                                   picker->kind == PICK_ZIP ? zip : picker->kind == PICK_N64 ? n64 : cue, 1,
                                   picker->start, false);
        }
    }
#endif
}

// The picker's answer, once (SDL_free it), or nullptr.
static char *PickerTake(Picker *picker)
{
    char *chosen;

    SDL_LockMutex(pickLock);
    chosen = picker->chosen;
    picker->chosen = nullptr;
    if (picker->browse && !browsing) {
        picker->browse = false;
        browsing = picker;
        browseDir[0] = 0;
    }
    SDL_UnlockMutex(pickLock);
    return chosen;
}

static int CompareNames(const void *a, const void *b)
{
    const char *x = *(const char *const *)a, *y = *(const char *const *)b;
    bool xd = x[strlen(x) - 1] == '/', yd = y[strlen(y) - 1] == '/';

    return xd != yd ? (xd ? 1 : -1) : SDL_strcasecmp(x, y); /* the disc images first, then the folders */
}

// Whether a file's name is of the kind a picker wants.
static bool Wanted(int kind, const char *name)
{
    size_t length = strlen(name);
    const char *extension = length > 4 ? name + length - 4 : name;

    switch (kind) {
    case PICK_ZIP:
        return !SDL_strcasecmp(extension, ".zip");
    case PICK_N64:
        return !SDL_strcasecmp(extension, ".z64") || !SDL_strcasecmp(extension, ".n64") ||
               !SDL_strcasecmp(extension, ".v64");
    case PICK_CARD:
        return true;
    default:
        return PortIsDiscName(name);
    }
}

// One entry of the folder (not what is inside its folders): folders, and disc images unless a folder is wanted.
static SDL_EnumerationResult SDLCALL BrowseEntry(void *user, const char *dir, const char *name)
{
    static int capacity;
    char path[2048], named[1100];
    SDL_PathInfo info;
    size_t length = strlen(name);

    (void)user;
    if (name[0] == '.') {
        return SDL_ENUM_CONTINUE;
    }
    SDL_snprintf(path, sizeof(path), "%s%s", dir, name);
    if (!SDL_GetPathInfo(path, &info)) {
        return SDL_ENUM_CONTINUE;
    }
    if (info.type == SDL_PATHTYPE_DIRECTORY) {
        SDL_snprintf(named, sizeof(named), "%s/", name);
    } else if (!browsing->folder && Wanted(browsing->kind, name)) {
        SDL_strlcpy(named, name, sizeof(named));
    } else {
        return SDL_ENUM_CONTINUE;
    }
    if (browseCount == capacity || !browseNames) {
        capacity = capacity ? capacity * 2 : 64;
        browseNames = (char **)SDL_realloc(browseNames, (size_t)capacity * sizeof(char *));
    }
    browseNames[browseCount++] = SDL_strdup(named);
    return SDL_ENUM_CONTINUE;
}

static void BrowseRead(const char *dir)
{
    int i;

    for (i = 0; i < browseCount; i++) {
        SDL_free(browseNames[i]);
    }
    browseCount = 0;
    SDL_strlcpy(browseDir, dir, sizeof(browseDir));
    SDL_EnumerateDirectory(dir, BrowseEntry, nullptr);
    if (browseCount) {
        SDL_qsort(browseNames, (size_t)browseCount, sizeof(char *), CompareNames);
    }
}

// The console's own browser: folders to go into, and the disc images (or, for a folder, "Choose this folder").
static void Browser(void)
{
    char path[2048];
    int i;

    if (!browsing) {
        return;
    }
    if (!browseDir[0]) {
        SDL_strlcpy(path, browsing->start, sizeof(path));
        size_t length = strlen(path);
        while (length > 1 && (path[length - 1] == '/' || path[length - 1] == '\\')) {
            path[--length] = 0;
        }
        BrowseRead(path);
        ImGui::OpenPopup("Choose a file");
    }
    ImGui::SetNextWindowSize(ImVec2(ImGui::GetMainViewport()->WorkSize.x * 0.8f,
                                    ImGui::GetMainViewport()->WorkSize.y * 0.8f), ImGuiCond_Appearing);
    if (!ImGui::BeginPopupModal("Choose a file", nullptr, ImGuiWindowFlags_NoSavedSettings)) {
        browsing = nullptr;
        return;
    }
    ImGui::TextWrapped("%s", browseDir);
    if (ImGui::Button("Up")) {
        SDL_strlcpy(path, browseDir, sizeof(path));
        char *slash = SDL_strrchr(path, '/');
        if (slash && slash != path) {
            *slash = 0;
        } else {
            SDL_strlcpy(path, "/", sizeof(path));
        }
        BrowseRead(path);
    }
    if (browsing->folder) {
        ImGui::SameLine();
        if (ImGui::Button("Choose this folder")) {
            SDL_LockMutex(pickLock);
            browsing->chosen = SDL_strdup(browseDir);
            SDL_UnlockMutex(pickLock);
            browsing = nullptr;
            ImGui::CloseCurrentPopup();
        }
    }
    ImGui::SameLine();
    if (ImGui::Button("Cancel")) {
        browsing = nullptr;
        ImGui::CloseCurrentPopup();
    }
    ImGui::BeginChild("##entries", ImVec2(0, 0), ImGuiChildFlags_Borders);
    for (i = 0; browsing && i < browseCount; i++) {
        const char *name = browseNames[i];
        bool folder = name[strlen(name) - 1] == '/';

        if (ImGui::Selectable(name)) {
            SDL_snprintf(path, sizeof(path), "%s%s%s", browseDir, strcmp(browseDir, "/") ? "/" : "", name);
            if (folder) {
                path[strlen(path) - 1] = 0;
                BrowseRead(path);
                break;
            }
            SDL_LockMutex(pickLock);
            browsing->chosen = SDL_strdup(path);
            SDL_UnlockMutex(pickLock);
            browsing = nullptr;
            ImGui::CloseCurrentPopup();
        }
    }
    ImGui::EndChild();
    ImGui::EndPopup();
}

// ---- touch controls (touch.cpp) ----

// Help text in the disabled colour, wrapped to the window.
static void DisabledWrapped(const char *text)
{
    ImGui::PushStyleColor(ImGuiCol_Text, ImGui::GetStyle().Colors[ImGuiCol_TextDisabled]);
    ImGui::TextWrapped("%s", text);
    ImGui::PopStyleColor();
}

static void TouchControls(void)
{
    static const char *const modes[] = {"Automatic", "On", "Off"};
    int mode = SDL_clamp(portSettings.touchControls, 0, 2);
    int size = portSettings.touchSize > 0 ? portSettings.touchSize : 100;
    int opacity = portSettings.touchOpacity > 0 ? portSettings.touchOpacity : 60;

    ImGui::TextUnformatted("Touch controls");
    ImGui::SetNextItemWidth(ImGui::GetFontSize() * 10);
    if (ImGui::Combo("##touch", &mode, modes, IM_ARRAYSIZE(modes))) {
        portSettings.touchControls = mode;
        Changed();
    }
#ifdef PORT_MOBILE
    DisabledWrapped("Automatic: shown until a gamepad or keyboard is used, and again at the next touch.");
#else
    DisabledWrapped("Automatic: shown once the screen is touched, until a key or gamepad is used. While they are "
                        "shown the mouse works them too.");
#endif
    DisabledWrapped("Left: a stick wherever the thumb lands (steering, the menus' cursor). Right: thrust, fire, the "
                        "air brakes, drop and view in a race, the four buttons in the menus. Top: pause, and this console.");
    ImGui::BeginDisabled(mode == TOUCH_OFF);
    ImGui::SetNextItemWidth(ImGui::GetFontSize() * 14);
    if (ImGui::SliderInt("Size##touch", &size, 50, 200, "%d%%")) {
        portSettings.touchSize = size;
    }
    if (ImGui::IsItemDeactivatedAfterEdit()) {
        Changed();
    }
    ImGui::SetNextItemWidth(ImGui::GetFontSize() * 14);
    if (ImGui::SliderInt("Opacity##touch", &opacity, 10, 100, "%d%%")) {
        portSettings.touchOpacity = opacity;
    }
    if (ImGui::IsItemDeactivatedAfterEdit()) {
        Changed();
    }
    ImGui::EndDisabled();
}

// ---- the Wipeout XL disc ----

// Whether a disc image is Wipeout XL's (USA) or WipEout 2097's (Europe, the same game, region.c): its program and
// data folder; message says what is wrong.
static bool CheckDisc(const char *path, char *message, size_t size)
{
    char error[256];
    int32_t lba;
    uint32_t length;
    Disc *disc = DiscOpen(path, error, sizeof(error));

    if (!disc) {
        SDL_snprintf(message, size, "%s", error);
        return false;
    }
    bool ok = (DiscFindFile(disc, "\\SCUS_943.51", &lba, &length) || DiscFindFile(disc, "\\SLES_003.27", &lba, &length)) &&
              DiscFindFile(disc, "\\WIPEOUT2\\TEXTURES\\MENUTIMS.CMP", &lba, &length);
    DiscClose(disc);
    if (!ok) {
        SDL_snprintf(message, size, "That is not Wipeout XL (USA) or WipEout 2097 (Europe): its program "
                                    "(SCUS_943.51, SLES_003.27) and WIPEOUT2 folder's menus are not on it.");
    }
    return ok;
}

// An archive of a disc image: unpacked on a thread of its own (discimage.c), then used as a disc (for Wipeout XL
// or the original Wipeout, unpackFor).
static SDL_Thread *unpacking;
static SDL_AtomicInt unpacked; // 1 done, 2 failed
static char unpackArchive[1024], unpackCue[1200], unpackMessage[300];
enum { UNPACK_XL, UNPACK_ORIGINAL };
static int unpackFor;

static int SDLCALL Unpack(void *)
{
    char local[1024];

    // (Android's picker: a document, copied into the game's folder first; the copy is the disc from then on)
    if (!PortImportFile(unpackArchive, local, sizeof(local), unpackMessage, sizeof(unpackMessage))) {
        SDL_SetAtomicInt(&unpacked, 2);
        return 0;
    }
    SDL_strlcpy(unpackArchive, local, sizeof(unpackArchive));
    SDL_SetAtomicInt(&unpacked, PortDiscImage(unpackArchive, unpackCue, sizeof(unpackCue), unpackMessage,
                                              sizeof(unpackMessage)) ? 1 : 2);
    return 0;
}

// Starts unpacking an archive (unless one is being unpacked); whether it is one.
static bool StartUnpack(const char *path, int what, char *message)
{
    if (!PortIsArchive(path) && SDL_strncmp(path, "content://", 10)) {
        return false;
    }
    if (!unpacking) {
        SDL_strlcpy(unpackArchive, path, sizeof(unpackArchive));
        unpackFor = what;
        SDL_SetAtomicInt(&unpacked, 0);
        unpacking = SDL_CreateThread(Unpack, "unpack", nullptr);
        message[0] = 0;
    }
    return true;
}

static void UseDisc(const char *path)
{
    char cue[1200];

    SDL_strlcpy(discEdit, path, sizeof(discEdit));
    if (StartUnpack(path, UNPACK_XL, discMessage)) {
        return;
    }
    if (PortDiscImage(path, cue, sizeof(cue), discMessage, sizeof(discMessage)) &&
        CheckDisc(cue, discMessage, sizeof(discMessage))) {
        SDL_strlcpy(portSettings.discPath, path, sizeof(portSettings.discPath));
        Changed();
        discMessage[0] = 0;
    }
}

static void OpenOriginal(const char *path);

// The unpacking's end: the archive is the disc from then on (its image is used as unpacked).
static void Unpacked(void)
{
    int state = SDL_GetAtomicInt(&unpacked);

    if (!unpacking || !state) {
        return;
    }
    SDL_WaitThread(unpacking, nullptr);
    unpacking = nullptr;
    if (unpackFor == UNPACK_ORIGINAL) {
        if (state == 2) {
            SDL_strlcpy(originalMessage, unpackMessage, sizeof(originalMessage));
        } else {
            OpenOriginal(unpackArchive); /* (unpacked now: at once) */
        }
    } else if (state == 2) {
        SDL_strlcpy(discMessage, unpackMessage, sizeof(discMessage));
    } else if (CheckDisc(unpackCue, discMessage, sizeof(discMessage))) {
        SDL_strlcpy(portSettings.discPath, unpackArchive, sizeof(portSettings.discPath));
        Changed();
        discMessage[0] = 0;
    }
}

// The unpacking's progress, when it is for this (UNPACK_*).
static void UnpackProgress(int what)
{
    if (unpacking && unpackFor == what) {
        char label[64];
        SDL_snprintf(label, sizeof(label), "Unpacking %d%%", PortArchiveProgress() / 10);
        ImGui::ProgressBar(PortArchiveProgress() / 1000.0f, ImVec2(-1, 0), label);
    }
}

static void XlDisc(void)
{
    static bool tested;
    char *chosen;

    if (!tested && SDL_getenv("WIPEOUT_TEST_BROWSE")) {
        PickerOpen(&discPicker, false, discEdit); /* tests: the picker, at once */
    }
    if (!tested && SDL_getenv("WIPEOUT_TEST_RESTART")) {
        /* tests: this disc, and the game again with it */
        char path[1024];

        SDL_strlcpy(path, SDL_getenv("WIPEOUT_TEST_RESTART"), sizeof(path));
        SDL_UnsetEnvironmentVariable(SDL_GetEnvironment(), "WIPEOUT_TEST_RESTART");
        UseDisc(path);
        PortRestart(path);
    }
    if (!tested && SDL_getenv("WIPEOUT_TEST_USE_DISC")) {
        UseDisc(SDL_getenv("WIPEOUT_TEST_USE_DISC")); /* tests: this disc, as if chosen */
    }
    tested = true;
    Unpacked();
    chosen = PickerTake(&discPicker);

    if (chosen) {
        UseDisc(chosen);
        SDL_free(chosen);
    }
    ImGui::TextUnformatted("Wipeout XL disc image (PlayStation)");
    ImGui::TextWrapped("Wipeout XL (USA), or WipEout 2097 (Europe): the same game, with its own title screen, logo and "
                       "movies. A .cue with its .bin tracks, a .bin without its .cue, or an archive of them (.7z, "
                       ".zip, .rar, .tar, .gz and the like: unpacked once, into the settings folder).");
    ImGui::TextWrapped("In use: %s (%s)", portConfig.discGiven ? portConfig.discGiven : "",
                       PortRegionEurope() ? "WipEout 2097, Europe" : "Wipeout XL, USA");
    UnpackProgress(UNPACK_XL);
    if (ImGui::Button("Browse...##disc")) {
        PickerOpen(&discPicker, false, discEdit);
    }
    ImGui::SameLine();
    ImGui::SetNextItemWidth(-ImGui::CalcTextSize("Use this").x - ImGui::GetStyle().ItemSpacing.x -
                            ImGui::GetStyle().FramePadding.x * 2);
    ImGui::InputText("##disc", discEdit, sizeof(discEdit));
    ImGui::SameLine();
    if (ImGui::Button("Use this##disc")) {
        UseDisc(discEdit);
    }
    if (discMessage[0]) {
        ImGui::TextColored(ImVec4(1.0f, 0.5f, 0.4f, 1.0f), "%s", discMessage);
    }
    if (portSettings.discPath[0] && portConfig.discGiven && strcmp(portSettings.discPath, portConfig.discGiven) != 0) {
        ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.3f, 1.0f), "The game starts with %s from now on.",
                           portSettings.discPath);
#ifdef PORT_MOBILE
        ImGui::TextWrapped("Close the game and open it again to play it."); /* (an app cannot start itself again) */
#else
        if (ImGui::Button("Restart with it now")) {
            if (!PortRestart(portSettings.discPath)) {
                SDL_snprintf(discMessage, sizeof(discMessage), "The game could not be started again: %s", SDL_GetError());
            }
        }
#endif
    }
}

// The original Wipeout's tracks (platform/original.c): the player's copy of the original, raced from the RACING
// CLASS menu's WIPEOUT line.

static void OpenOriginal(const char *path)
{
    if (PortOriginalOpen(path, originalMessage, sizeof(originalMessage)) > 0 || !path[0]) {
        SDL_strlcpy(portSettings.originalPath, path, sizeof(portSettings.originalPath));
        Changed();
    }
}

static void UseOriginal(const char *path)
{
    SDL_strlcpy(originalEdit, path, sizeof(originalEdit));
    if (!StartUnpack(path, UNPACK_ORIGINAL, originalMessage)) {
        OpenOriginal(path);
    }
}

static void OriginalWipeout(void)
{
    char *chosen;

    Unpacked();
    chosen = PickerTake(&originalPicker);

    if (chosen) {
        UseOriginal(chosen);
        SDL_free(chosen);
    }
    ImGui::TextUnformatted("The original Wipeout (PlayStation)");
    ImGui::TextWrapped("Your copy of the original game: its disc image (a .cue with its .bin, or an archive of it), "
                       "or a folder of its data (the disc's WIPEOUT folder). Its tracks are then on the RACING CLASS menu, on the "
                       "WIPEOUT line under the classes (arcade races and time trials): left and right go through them.");
    int tracks = PortOriginalFoundCount();
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
    UnpackProgress(UNPACK_ORIGINAL);
#ifndef __EMSCRIPTEN__
    if (ImGui::Button("Browse for its disc image...")) {
        PickerOpen(&originalPicker, false, originalEdit);
    }
    ImGui::SameLine();
    if (ImGui::Button("... or its data folder")) {
        PickerOpen(&originalPicker, true, originalEdit);
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

// Wipeout 64's tracks (platform/original.c): the player's cartridge image, its tracks on the same WIPEOUT line.

static void UseWipeout64(const char *path)
{
    SDL_strlcpy(n64Edit, path, sizeof(n64Edit));
    if (PortWipeout64Open(path, n64Message, sizeof(n64Message)) > 0 || !path[0]) {
        SDL_strlcpy(portSettings.wipeout64Path, path, sizeof(portSettings.wipeout64Path));
        Changed();
    }
}

static void Wipeout64(void)
{
    char *chosen = PickerTake(&n64Picker);

    if (chosen) {
        UseWipeout64(chosen);
        SDL_free(chosen);
    }
    ImGui::TextUnformatted("Wipeout 64 (Nintendo 64)");
    ImGui::TextWrapped("Your copy of Wipeout 64: its cartridge image (.z64, .n64 or .v64). Its seven tracks are then on "
                       "the RACING CLASS menu's WIPEOUT line too, after the original's, at Rapier class.");
    int tracks = PortWipeout64TrackCount();
    if (!n64Edit[0] && PortWipeout64Source()[0]) {
        SDL_strlcpy(n64Edit, PortWipeout64Source(), sizeof(n64Edit));
    }
    if (tracks > 0) {
        ImGui::TextColored(ImVec4(0.5f, 1.0f, 0.5f, 1.0f), "%d tracks from %s", tracks, PortWipeout64Source());
    } else {
        ImGui::TextDisabled("None chosen.");
    }
    if (n64Message[0] && tracks == 0) {
        ImGui::TextWrapped("%s", n64Message);
    }
#ifndef __EMSCRIPTEN__
    if (ImGui::Button("Browse for its cartridge image...")) {
        PickerOpen(&n64Picker, false, n64Edit);
    }
    ImGui::SameLine();
#endif
    if (ImGui::Button("Forget it##n64")) {
        UseWipeout64("");
        n64Message[0] = 0;
    }
    ImGui::SetNextItemWidth(-ImGui::CalcTextSize("Use this").x - ImGui::GetStyle().ItemSpacing.x -
                            ImGui::GetStyle().FramePadding.x * 2);
    ImGui::InputText("##n64", n64Edit, sizeof(n64Edit));
    ImGui::SameLine();
    if (ImGui::Button("Use this##n64")) {
        UseWipeout64(n64Edit);
    }
}

// Music (music.c): the player's own folder of music in place of the disc's soundtrack.

static void UseMusic(const char *path)
{
    SDL_strlcpy(musicEdit, path, sizeof(musicEdit));
    if (PortMusicOpen(path, musicMessage, sizeof(musicMessage)) > 0 || !path[0]) {
        SDL_strlcpy(portSettings.musicFolder, path, sizeof(portSettings.musicFolder));
        if (path[0]) {
            portSettings.customMusic = 1;
        }
        Changed();
        PortMusicSettingsChanged();
    }
}

static void Music(void)
{
    char *chosen = PickerTake(&musicPicker);

    if (chosen) {
        UseMusic(chosen);
        SDL_free(chosen);
    }
    if (!musicEdit[0] && PortMusicFolder()[0]) {
        SDL_strlcpy(musicEdit, PortMusicFolder(), sizeof(musicEdit));
    }
    ImGui::TextUnformatted("Music");
    bool own = portSettings.customMusic != 0;
    if (ImGui::Checkbox("Play my own music instead of the game's", &own)) {
        portSettings.customMusic = own;
        Changed();
        PortMusicSettingsChanged();
    }
    ImGui::SameLine();
    bool shuffle = portSettings.musicShuffle != 0;
    if (ImGui::Checkbox("Shuffle", &shuffle)) {
        portSettings.musicShuffle = shuffle;
        Changed();
        PortMusicSettingsChanged();
    }
    ImGui::TextWrapped("A folder of music files (MP3, FLAC, Ogg Vorbis or WAV; the folders in it too). Each time the "
                       "game would start a piece of its music (the menus', a race's) the next song plays instead, "
                       "and when a song ends the one after it. The game's music volume and pause still apply.");
    if (PortMusicCount() > 0) {
        ImGui::TextColored(ImVec4(0.5f, 1.0f, 0.5f, 1.0f), "%d songs from %s", PortMusicCount(), PortMusicFolder());
        const char *playing = PortMusicPlaying();
        if (playing[0]) {
            ImGui::Text("Playing: %s", playing);
            ImGui::SameLine();
            if (ImGui::SmallButton("Next song")) {
                PortMusicSkip();
            }
        }
    } else {
        ImGui::TextDisabled("No folder chosen.");
    }
    if (musicMessage[0] && PortMusicCount() == 0) {
        ImGui::TextColored(ImVec4(1.0f, 0.5f, 0.4f, 1.0f), "%s", musicMessage);
    }
#ifndef __EMSCRIPTEN__
    if (ImGui::Button("Choose the music folder...")) {
        PickerOpen(&musicPicker, true, musicEdit);
    }
    ImGui::SameLine();
#endif
    if (ImGui::Button("Forget it##music")) {
        UseMusic("");
        musicMessage[0] = 0;
    }
    ImGui::SetNextItemWidth(-ImGui::CalcTextSize("Use this").x - ImGui::GetStyle().ItemSpacing.x -
                            ImGui::GetStyle().FramePadding.x * 2);
    ImGui::InputText("##music", musicEdit, sizeof(musicEdit));
    ImGui::SameLine();
    if (ImGui::Button("Use this##music")) {
        UseMusic(musicEdit);
    }
}

// Saves (saves.c, game/src/portsave.c): the save in use, the game's cards, and other emulators' saves to import.

static void SavesTab(void)
{
    static bool scanned;
    static char message[300];
    static char extra[1024];
    char *chosen = PickerTake(&cardPicker);

    if (chosen) {
        SDL_strlcpy(extra, chosen, sizeof(extra));
        SDL_free(chosen);
        scanned = false;
    }
    if (!scanned) {
        if (!extra[0] && SDL_getenv("WIPEOUT_SAVES_FROM")) {
            SDL_strlcpy(extra, SDL_getenv("WIPEOUT_SAVES_FROM"), sizeof(extra)); /* (tests: a folder of cards) */
        }
        PortSavesScan(extra);
        scanned = true;
        if (SDL_getenv("WIPEOUT_TEST_IMPORT")) {
            /* tests: the found save of that name, imported at once */
            for (int i = 0; i < PortSavesCount(); i++) {
                if (!strcmp(PortSavesFound()[i].name, SDL_getenv("WIPEOUT_TEST_IMPORT"))) {
                    PortSavesImport(i, message, sizeof(message));
                    break;
                }
            }
            SDL_UnsetEnvironmentVariable(SDL_GetEnvironment(), "WIPEOUT_TEST_IMPORT");
        }
    }
    bool autosave = !portSettings.noAutoSave;
    if (ImGui::Checkbox("The game saves itself", &autosave)) {
        portSettings.noAutoSave = !autosave;
        Changed();
    }
    ImGui::TextWrapped("The save in use is loaded at start-up, and whenever your records, options or progress change it "
                       "is written again (made, as AUTOSAVE, if there is none). The game's LOAD/SAVE screen still "
                       "changes the save in use: load one, or save under a new name.");
    if (portSettings.saveSlot[0]) {
        ImGui::Text("In use: %.8s on memory card %c", portSettings.saveSlot + 2, portSettings.saveSlot[0] == '0' ? '1' : '2');
    } else {
        ImGui::TextDisabled("No save in use yet.");
    }
    uint64_t when;
    int last = PortSavesLastWrite(&when);
    if (last) {
        ImGui::SameLine();
        if (last > 0) {
            ImGui::TextDisabled("  (saved %llu s ago)", (unsigned long long)((SDL_GetTicks() - when) / 1000));
        } else {
            ImGui::TextColored(ImVec4(1.0f, 0.5f, 0.4f, 1.0f), "  (could not save: is the memory card full?)");
        }
    }
    ImGui::Separator();
    // the game's own cards
    for (int port = 0; port < 2; port++) {
        char names[15][21];
        int sizes[15], count = PsxCardList(port, names, sizes), shown = 0;

        ImGui::Text("Memory card %d", port + 1);
        for (int i = 0; i < count; i++) {
            if (strncmp(names[i], "BASCUS-94351", 12)) {
                continue;
            }
            const char *name = names[i] + 12;
            bool inUse = portSettings.saveSlot[0] == '0' + port && !strncmp(portSettings.saveSlot + 2, name, 8);
            ImGui::BulletText("%s%s", name, inUse ? "  (in use)" : "");
            if (!inUse) {
                char label[48];
                ImGui::SameLine();
                SDL_snprintf(label, sizeof(label), "Use this one##%d%d", port, i);
                if (ImGui::SmallButton(label)) {
                    PortSavesUse(port, name);
                    SDL_snprintf(message, sizeof(message), "%s: the game loads it (in the menus).", name);
                }
            }
            shown++;
        }
        if (!shown) {
            ImGui::TextDisabled("   no saves of Wipeout XL");
        }
    }
    ImGui::Separator();
    // other emulators'
    ImGui::TextUnformatted("Saves in other emulators");
    ImGui::TextWrapped("Wipeout XL saves (and WipEout 2097's) found in DuckStation's, RetroArch's, OpenEmu's, Mednafen's, "
                       "ePSXe's and PCSX's memory cards, or in a card or save file you choose (.mcd, .mcr, .srm, .gme, "
                       ".mem, .vgs, .vmp, .mcs ...). Import copies one onto memory card 1 and loads it.");
    if (ImGui::Button("Look again")) {
        scanned = false;
    }
#ifndef __EMSCRIPTEN__
    ImGui::SameLine();
    if (ImGui::Button("Choose a memory card or save file...")) {
        PickerOpen(&cardPicker, false, extra);
    }
#endif
    if (message[0]) {
        ImGui::TextWrapped("%s", message);
    }
    const FoundSave *saves = PortSavesFound();
    int count = PortSavesCount();
    if (!count) {
        ImGui::TextDisabled("None found.");
    } else if (ImGui::BeginTable("found", 4, ImGuiTableFlags_RowBg | ImGuiTableFlags_Borders | ImGuiTableFlags_ScrollY,
                                 ImVec2(-1, 260 * ImGui::GetIO().FontGlobalScale))) {
        ImGui::TableSetupColumn("save", ImGuiTableColumnFlags_WidthFixed, 160 * ImGui::GetIO().FontGlobalScale);
        ImGui::TableSetupColumn("from", ImGuiTableColumnFlags_WidthFixed, 200 * ImGui::GetIO().FontGlobalScale);
        ImGui::TableSetupColumn("file");
        ImGui::TableSetupColumn("", ImGuiTableColumnFlags_WidthFixed, 80 * ImGui::GetIO().FontGlobalScale);
        ImGui::TableHeadersRow();
        for (int i = 0; i < count; i++) {
            const FoundSave *s = &saves[i];
            char label[32];

            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            ImGui::Text("%s (%s)", s->name, s->region);
            ImGui::TableNextColumn();
            ImGui::Text("%s, %s", s->program, s->format);
            ImGui::TableNextColumn();
            ImGui::TextWrapped("%s", s->file);
            ImGui::TableNextColumn();
            SDL_snprintf(label, sizeof(label), "Import##%d", i);
            if (ImGui::SmallButton(label)) {
                PortSavesImport(i, message, sizeof(message));
            }
        }
        ImGui::EndTable();
    }
}

// The resolution: the framebuffer's scale (gpu_gl.c), the window's or the player's.
static void Resolution(void)
{
    char auto_[64], labels[16][48];
    const char *items[17];
    int count = GpuMaxScale(), choice = SDL_min(portSettings.renderScale, count), i;

    SDL_snprintf(auto_, sizeof(auto_), "Auto: as the window (%dx, %dx%d)", GpuWindowScale(), 320 * GpuWindowScale(),
                 240 * GpuWindowScale());
    items[0] = auto_;
    for (i = 1; i <= count && i <= 16; i++) {
        SDL_snprintf(labels[i - 1], sizeof(labels[i - 1]), "%dx  %dx%d%s", i, 320 * i, 240 * i,
                     i == 1 ? "  (the console's)" : "");
        items[i] = labels[i - 1];
    }
    if (ImGui::Combo("Resolution", &choice, items, SDL_min(count, 16) + 1)) {
        portSettings.renderScale = choice;
        Changed();
    }
    ImGui::TextDisabled("Drawn at %dx (%dx%d) now. Above the window's it is smoothed down to fit; each step up takes "
                        "more video memory.", GpuScale(), 320 * GpuScale(), 240 * GpuScale());
}

static void UsePack(const char *path)
{
    SDL_strlcpy(packEdit, path, sizeof(packEdit));
    if (HdTexOpen(path, packMessage, sizeof(packMessage)) > 0 || !path[0]) {
        SDL_strlcpy(portSettings.hdPackPath, path, sizeof(portSettings.hdPackPath));
        if (path[0]) {
            portSettings.hdTextures = 1;
        }
        Changed();
    }
}

// HD textures (hdtex.c): the player's own texture pack, made for DuckStation.
static void HdTextures(void)
{
    char *chosen = PickerTake(&packPicker);

    if (chosen) {
        UsePack(chosen);
        SDL_free(chosen);
    }
    if (!packEdit[0] && portSettings.hdPackPath[0]) {
        SDL_strlcpy(packEdit, portSettings.hdPackPath, sizeof(packEdit));
    }
    bool on = portSettings.hdTextures != 0;
    if (ImGui::Checkbox("HD textures (from your texture pack)", &on)) {
        portSettings.hdTextures = on;
        Changed();
    }
    ImGui::TextWrapped("Made for NR74W's WipEout 2097 / XL texture pack (github.com/NR74W/WipEout-Mods): the menus, "
                       "fonts and HUD redrawn at 16 times the detail. The game does not include it: download it from "
                       "there and choose its .zip, or the folder it is in. Other packs made for DuckStation's texture "
                       "replacement (for SCUS-94351) work as well.");
    if (ImGui::Button("Open the pack's page")) {
        SDL_OpenURL("https://github.com/NR74W/WipEout-Mods");
    }
    if (HdTexCount() > 0) {
        ImGui::TextColored(ImVec4(0.5f, 1.0f, 0.5f, 1.0f), "%d replacement textures from %s", HdTexCount(),
                           HdTexSource());
        if (HdTexAbout()[0]) {
            ImGui::Indent();
            ImGui::TextDisabled("%s", HdTexAbout());
            ImGui::Unindent();
        }
    } else if (portSettings.hdPackPath[0] && !portSettings.hdTextures) {
        ImGui::TextDisabled("%s (opened when HD textures are on)", portSettings.hdPackPath);
    } else {
        ImGui::TextDisabled("No pack chosen.");
    }
    if (packMessage[0] && HdTexCount() == 0) {
        ImGui::TextColored(ImVec4(1.0f, 0.5f, 0.4f, 1.0f), "%s", packMessage);
    }
#ifndef __EMSCRIPTEN__
    if (ImGui::Button("Choose the pack's .zip...")) {
        PickerOpen(&packPicker, false, packEdit);
    }
    ImGui::SameLine();
    if (ImGui::Button("... or its folder")) {
        PickerOpen(&packPicker, true, packEdit);
    }
    ImGui::SameLine();
#endif
    if (ImGui::Button("Forget it##pack")) {
        UsePack("");
        packMessage[0] = 0;
    }
    ImGui::SetNextItemWidth(-ImGui::CalcTextSize("Use this").x - ImGui::GetStyle().ItemSpacing.x -
                            ImGui::GetStyle().FramePadding.x * 2);
    ImGui::InputText("##pack", packEdit, sizeof(packEdit));
    ImGui::SameLine();
    if (ImGui::Button("Use this##pack")) {
        UsePack(packEdit);
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
            Resolution();
            ImGui::Separator();
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
            bool depthBuffer = !portSettings.consoleDepth;
            if (ImGui::Checkbox("Depth buffer", &depthBuffer)) {
                portSettings.consoleDepth = !depthBuffer;
                Changed();
            }
            ImGui::TextDisabled("Off: the console's sorting, where a far polygon can be drawn over a nearer one.");
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
            ImGui::Separator();
            HdTextures();
            Browser(); /* (a picker's, where the system has no file dialog) */
            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem("Game", nullptr, FirstTab("game"))) {
            TouchControls();
            ImGui::Separator();
            bool skip = portSettings.skipIntro != 0;
            if (ImGui::Checkbox("Skip the intro movie at start-up", &skip)) {
                portSettings.skipIntro = skip;
                Changed();
            }
            bool logos = portSettings.skipLogos != 0;
            if (ImGui::Checkbox("Skip the logos at start-up (straight to the title screen)", &logos)) {
                portSettings.skipLogos = logos;
                Changed();
            }
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
            ImGui::Separator();
            Music();
            Browser(); /* (a picker's, where the system has no file dialog) */
            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem("Recorder", nullptr,
                                FirstTab("recorder") | (showRecorder ? ImGuiTabItemFlags_SetSelected : 0))) {
            showRecorder = false;
            RecorderTab();
            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem("Saves", nullptr, FirstTab("saves"))) {
            SavesTab();
            Browser(); /* (a picker's, where the system has no file dialog) */
            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem("Files", nullptr, FirstTab("files"))) {
#ifndef __EMSCRIPTEN__
            XlDisc(); /* (the web page chooses the disc) */
            ImGui::Separator();
#endif
            Browser(); /* (a picker's, where the system has no file dialog) */
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
            ImGui::Separator();
            Wipeout64();
            ImGui::EndTabItem();
        }
        ImGui::EndTabBar();
    }
    ImGui::PopItemWidth();
    ImGui::End();
}

extern "C" void DebugUiSplash(const char *text, float fraction)
{
    if (!ready) {
        return;
    }
    int width, height;
    SDL_GetWindowSizeInPixels(window, &width, &height);
    FitToWindow();
    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplSDL3_NewFrame();
    ImGui::NewFrame();
    const ImGuiViewport *view = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(ImVec2(view->Pos.x + view->Size.x / 2, view->Pos.y + view->Size.y / 2), ImGuiCond_Always,
                            ImVec2(0.5f, 0.5f));
    ImGui::SetNextWindowSize(ImVec2(view->Size.x * 0.7f, 0), ImGuiCond_Always);
    ImGui::Begin("##splash", nullptr, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove |
                                          ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoBackground);
    ImGui::TextWrapped("%s", text);
    if (fraction >= 0) {
        ImGui::ProgressBar(SDL_clamp(fraction, 0.0f, 1.0f), ImVec2(-1, 0));
    }
    ImGui::End();
    ImGui::Render();
    GpuClearWindow(0.0f, 0.0f, 0.18f);
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
    SDL_GL_SwapWindow(window);
}

extern "C" void DebugUiRender(void)
{
    if (!ready) {
        return;
    }
    Rates();
    if (!open && !portSettings.showFps && !RecorderNoteWanted() && !TouchShown()) {
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
    TouchDraw();
    ImGui::Render();
    glViewport(0, 0, width, height);
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
}
