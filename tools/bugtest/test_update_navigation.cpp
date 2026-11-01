/* Drive the real alert through Dear ImGui's SDL backend with a virtual controller. */
#include "../../platform/debug_ui.cpp"
#include "imgui_internal.h"
#include <assert.h>

PortConfig portConfig;
PortSettings portSettings;
static bool notice;
static int opened, saved;
const char *PortUpdateAvailable(void) { return notice ? "v0.0.7" : nullptr; }
void PortUpdateDismiss(void) { notice = false; }
void PortUpdateOpenRelease(void) { assert(notice); opened++; }
int PortSettingsSave(void) { saved++; return 1; }
static SDL_Joystick *stick;

static void Frames(int count = 4)
{
    while (count--) {
        SDL_UpdateJoysticks();
        SDL_Event event;
        while (SDL_PollEvent(&event)) ImGui_ImplSDL3_ProcessEvent(&event);
        UpdateNoticeNavigation();
        ImGui_ImplSDL3_NewFrame();
        ImGui::GetIO().DeltaTime = 1.0f / 60;
        ImGui::NewFrame();
        UpdateNotice();
        ImGui::Render();
    }
}
static void Button(SDL_GamepadButton button)
{
    assert(SDL_SetJoystickVirtualButton(stick, button, true)); Frames();
    assert(SDL_SetJoystickVirtualButton(stick, button, false)); Frames();
}
static void Show(void)
{
    notice = true; Frames();
    assert(ImGui::GetIO().ConfigFlags & ImGuiConfigFlags_NavEnableGamepad);
}
static void Focused(const char *label)
{
    ImGuiContext &ctx = *ImGui::GetCurrentContext();
    assert(ctx.NavWindow && ctx.NavId == ctx.NavWindow->GetID(label));
}
int main(void)
{
    SDL_SetHint(SDL_HINT_VIDEO_DRIVER, "dummy");
    SDL_SetHint(SDL_HINT_JOYSTICK_ALLOW_BACKGROUND_EVENTS, "1");
    assert(SDL_Init(SDL_INIT_VIDEO | SDL_INIT_GAMEPAD));
    SDL_Window *testWindow = SDL_CreateWindow("update navigation test", 960, 720, SDL_WINDOW_HIDDEN);
    assert(testWindow);
    SDL_VirtualJoystickDesc desc;
    SDL_INIT_INTERFACE(&desc);
    desc.type = SDL_JOYSTICK_TYPE_GAMEPAD;
    desc.naxes = SDL_GAMEPAD_AXIS_COUNT;
    desc.nbuttons = SDL_GAMEPAD_BUTTON_COUNT;
    desc.button_mask = desc.axis_mask = ~0u;
    desc.name = "update alert test pad";
    SDL_JoystickID id = SDL_AttachVirtualJoystick(&desc);
    assert(id);
    stick = SDL_OpenJoystick(id);
    SDL_Gamepad *pad = SDL_OpenGamepad(id);
    assert(stick && pad);
    ImGui::CreateContext();
    ImGuiIO &io = ImGui::GetIO();
    io.IniFilename = nullptr;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    unsigned char *pixels;
    int width, height;
    io.Fonts->GetTexDataAsRGBA32(&pixels, &width, &height);
    assert(ImGui_ImplSDL3_InitForOther(testWindow));
    ImGui_ImplSDL3_SetGamepadMode(ImGui_ImplSDL3_GamepadMode_Manual, &pad, 1);

    Show(); Focused("Later");
    Button(SDL_GAMEPAD_BUTTON_DPAD_UP); Focused("Check for updates automatically");
    Button(SDL_GAMEPAD_BUTTON_SOUTH);
    assert(notice && portSettings.noUpdateCheck && saved == 1);
    Button(SDL_GAMEPAD_BUTTON_DPAD_DOWN); Focused("Later");
    Button(SDL_GAMEPAD_BUTTON_DPAD_RIGHT);
    Focused("View release");
    Button(SDL_GAMEPAD_BUTTON_SOUTH); assert(!notice && opened == 1);
    assert(!(io.ConfigFlags & ImGuiConfigFlags_NavEnableGamepad));

    Show(); Button(SDL_GAMEPAD_BUTTON_EAST); assert(!notice && opened == 1);
    Show(); Focused("Later");
    Button(SDL_GAMEPAD_BUTTON_SOUTH); assert(!notice && opened == 1);
    ImGui_ImplSDL3_Shutdown();
    ImGui::DestroyContext();
    SDL_CloseGamepad(pad);
    SDL_CloseJoystick(stick);
    assert(SDL_DetachVirtualJoystick(id));
    SDL_DestroyWindow(testWindow);
    SDL_Quit();
    puts("update alert: D-pad navigation, confirm, cancel, saved checkbox and safe default: OK");
    return 0;
}
