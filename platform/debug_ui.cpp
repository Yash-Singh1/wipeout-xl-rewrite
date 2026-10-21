// The debug console: the backtick key opens it over the game (Dear ImGui). It shows the frame rates and
// changes the settings (settings.h), which are saved as they change: the frame-rate overlay, the draw
// distance and its fade, the depth test, the intro movie, full screen, and where the settings and the
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
#include "platform.h"
#include "settings.h"
}

static SDL_Window *window;
static bool ready, open;
static char saveDirEdit[1024];

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
    float scale = SDL_GetWindowDisplayScale(w);
    if (scale > 0.0f) {
        style.ScaleAllSizes(scale);
        io.FontGlobalScale = scale;
    }
    ImGui_ImplSDL3_InitForOpenGL(w, context);
    ImGui_ImplOpenGL3_Init("#version 330 core");
    SDL_strlcpy(saveDirEdit, portSettings.saveDir, sizeof(saveDirEdit));
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

/* WIPEOUT_DEBUG_UI=stats|graphics|game|files (tests): the tab shown first */
static ImGuiTabItemFlags FirstTab(const char *name)
{
    static int done;
    const char *wanted = SDL_getenv("WIPEOUT_DEBUG_UI");

    if (done >= 4 || !wanted || SDL_strcasecmp(wanted, name)) {
        return 0;
    }
    done++;
    return ImGuiTabItemFlags_SetSelected;
}

static void Console(void)
{
    const ImGuiViewport *view = ImGui::GetMainViewport();

    ImGui::SetNextWindowPos(ImVec2(view->WorkPos.x + view->WorkSize.x * 0.5f, view->WorkPos.y + view->WorkSize.y * 0.5f),
                            ImGuiCond_FirstUseEver, ImVec2(0.5f, 0.5f));
    float width = 640 * ImGui::GetIO().FontGlobalScale;
    ImGui::SetNextWindowSizeConstraints(ImVec2(width, 0), ImVec2(width, view->WorkSize.y * 0.9f));
    if (!ImGui::Begin("Wipeout XL debug console   (` closes it)", &open,
                      ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_AlwaysAutoResize)) {
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
            ImGui::BeginDisabled(!portSettings.fade);
            int range[2] = {portSettings.fadeNear, portSettings.fadeFar};
            if (ImGui::DragInt2("Fade from / to", range, 500.0f, 8000, 400000)) {
                portSettings.fadeNear = SDL_clamp(range[0], 8000, 399000);
                portSettings.fadeFar = SDL_clamp(range[1], portSettings.fadeNear + 1000, 400000);
                Changed();
            }
            ImGui::EndDisabled();
            ImGui::EndDisabled();
            ImGui::Separator();
            bool depth = portSettings.depthTest != 0;
            if (ImGui::Checkbox("Per-pixel depth test (experimental)", &depth)) {
                portSettings.depthTest = depth;
                Changed();
            }
            ImGui::TextDisabled("Fixes polygons sorted wrongly, but decals such as sign faces can flicker.");
            bool large = portSettings.largePolygons != 0;
            if (ImGui::Checkbox("Draw near polygons the console drops", &large)) {
                portSettings.largePolygons = large;
                Changed();
            }
            ImGui::TextDisabled("The console skips polygons over 1023x511 pixels: holes in the track close to the camera.");
            ImGui::Separator();
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
    if (!open && !portSettings.showFps) {
        return;
    }
    int width, height;
    SDL_GetWindowSizeInPixels(window, &width, &height);
    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplSDL3_NewFrame();
    ImGui::NewFrame();
    if (portSettings.showFps) {
        FpsOverlay();
    }
    if (open) {
        Console();
    }
    ImGui::Render();
    glViewport(0, 0, width, height);
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
}
