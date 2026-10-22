// Touch controls (touch.h). Positions are in the window's points (ImGui's too), within the screen's safe area
// (a phone's notch and rounded corners), sized from its shorter side. Each finger keeps to what it first touched:
// the stick, Start, the console button, or the right-hand buttons, of which it presses every one it is over (a
// thumb between thrust and an air brake presses both). When the game goes between the menus and a race the
// right-hand buttons change; fingers on them stay quiet until lifted, as keys held over do (pad_sdl.c). A tap
// shorter than MIN_PRESS_MS holds its button that long: the game (and a movie, to be skipped) reads the pad only
// between its frames, and a quick tap's touch and lift can come within one of them.
#include <SDL3/SDL.h>
#include <math.h>

#include "imgui.h"

#include "debug_ui.h"
#include "platform.h"
#include "settings.h"
#include "touch.h"

extern "C" {
#include "psx/pad.h"
}

namespace {

enum Kind { NOTHING, STICK, START, CONSOLE, CLUSTER };
enum Symbol { NO_SYMBOL, CROSS, CIRCLE, SQUARE, TRIANGLE };

struct Button {
    float x, y, r;
    const char *label;
    Symbol symbol;
    uint16_t buttons; // in the menus: the pad's buttons
    int action;       // in a race: PAD_ACTION_*, or -1 for buttons
};

struct Finger {
    SDL_FingerID id;
    bool used, quiet;
    bool lifted;    // lifted before MIN_PRESS_MS: still pressing until then
    Uint64 downAt;  // SDL ticks
    Kind kind;
    float x, y, baseX, baseY;
};

struct Layout {
    float u, left, top, right, bottom; // the safe area, its shorter side
    float stickX, stickY, stickR, stickZone, stickTop;
    Button start, console;
    Button cluster[6];
    int clusterCount;
};

const SDL_FingerID MOUSE = (SDL_FingerID)-1;
const int FINGERS = 10;
const Uint64 MIN_PRESS_MS = 100;

SDL_Window *window;
Finger fingers[FINGERS];
bool touched;                          // automatic: the screen was touched since a key or gamepad was used
PadContext layoutContext = PAD_CONTEXT_MENU;

float Size()
{
    return (portSettings.touchSize > 0 ? portSettings.touchSize : 100) / 100.0f;
}

float Opacity()
{
    return (portSettings.touchOpacity > 0 ? portSettings.touchOpacity : 60) / 100.0f;
}

Button Make(float x, float y, float r, const char *label, Symbol symbol, uint16_t buttons, int action)
{
    Button b = {x, y, r, label, symbol, buttons, action};
    return b;
}

Layout MakeLayout(PadContext context)
{
    Layout l;
    SDL_Rect safe;
    int w = 1, h = 1;
    float s = Size(), u;

    SDL_GetWindowSize(window, &w, &h);
    if (!SDL_GetWindowSafeArea(window, &safe) || safe.w <= 0 || safe.h <= 0) {
        safe.x = safe.y = 0;
        safe.w = w;
        safe.h = h;
    }
    l.left = (float)safe.x;
    l.top = (float)safe.y;
    l.right = (float)(safe.x + safe.w);
    l.bottom = (float)(safe.y + safe.h);
    l.u = u = (float)SDL_min(w, h) * s;
    // the stick: its home at the lower left, and wherever a thumb lands on the left (below the top's buttons)
    l.stickR = 0.13f * u;
    l.stickX = l.left + 0.24f * u;
    l.stickY = l.bottom - 0.24f * u;
    l.stickZone = l.left + 0.45f * (l.right - l.left);
    l.stickTop = l.top + 0.16f * u;
    l.console = Make(l.left + 0.07f * u, l.top + 0.07f * u, 0.045f * u, "", NO_SYMBOL, 0, -1);
    l.start = Make((l.left + l.right) / 2, l.top + 0.07f * u, 0.045f * u, context == PAD_CONTEXT_RACE ? "" : "START",
                   NO_SYMBOL, PAD_START, -1);
    float R = l.right, B = l.bottom;
    if (context == PAD_CONTEXT_RACE) {
        l.cluster[0] = Make(R - 0.18f * u, B - 0.19f * u, 0.12f * u, "THRUST", NO_SYMBOL, 0, PAD_ACTION_THRUST);
        l.cluster[1] = Make(R - 0.43f * u, B - 0.12f * u, 0.085f * u, "FIRE", NO_SYMBOL, 0, PAD_ACTION_FIRE);
        l.cluster[2] = Make(R - 0.40f * u, B - 0.36f * u, 0.07f * u, "L BRAKE", NO_SYMBOL, 0, PAD_ACTION_BRAKE_LEFT);
        l.cluster[3] = Make(R - 0.15f * u, B - 0.45f * u, 0.07f * u, "R BRAKE", NO_SYMBOL, 0, PAD_ACTION_BRAKE_RIGHT);
        l.cluster[4] = Make(R - 0.64f * u, B - 0.09f * u, 0.055f * u, "DROP", NO_SYMBOL, 0, PAD_ACTION_DITCH);
        l.cluster[5] = Make(R - 0.62f * u, B - 0.29f * u, 0.055f * u, "VIEW", NO_SYMBOL, 0, PAD_ACTION_VIEW);
        l.clusterCount = 6;
    } else {
        // the face buttons; Cross also presses Start, as Enter does (the title waits for Start)
        float x = R - 0.25f * u, y = B - 0.27f * u, d = 0.125f * u, r = 0.07f * u;

        l.cluster[0] = Make(x, y + d, r, "", CROSS, PAD_CROSS | PAD_START, -1);
        l.cluster[1] = Make(x + d, y, r, "", CIRCLE, PAD_CIRCLE, -1);
        l.cluster[2] = Make(x - d, y, r, "", SQUARE, PAD_SQUARE, -1);
        l.cluster[3] = Make(x, y - d, r, "", TRIANGLE, PAD_TRIANGLE, -1);
        l.clusterCount = 4;
    }
    return l;
}

bool Inside(const Button &b, float x, float y, float slack)
{
    return hypotf(x - b.x, y - b.y) < b.r * slack;
}

// The buttons the fingers hold in a context.
uint16_t Held(PadContext context)
{
    Layout l = MakeLayout(context);
    uint16_t held = 0;

    for (const Finger &f : fingers) {
        if (!f.used || f.quiet) {
            continue;
        }
        switch (f.kind) {
        case STICK: {
            float dx = f.x - f.baseX, dy = f.y - f.baseY, t = 0.35f * l.stickR;

            held |= (dx < -t ? PAD_LEFT : 0) | (dx > t ? PAD_RIGHT : 0) | (dy < -t ? PAD_UP : 0) | (dy > t ? PAD_DOWN : 0);
            break;
        }
        case START:
            held |= PAD_START;
            break;
        case CLUSTER:
            for (int i = 0; i < l.clusterCount; i++) {
                const Button &b = l.cluster[i];

                if (Inside(b, f.x, f.y, 1.2f)) {
                    held |= context == PAD_CONTEXT_RACE && b.action >= 0 ? PsxPadActionButton(b.action) : b.buttons;
                }
            }
            break;
        default:
            break;
        }
    }
    return held;
}

// The pad layer's source (pad_sdl.c): what the fingers hold, in the game's context now.
uint16_t Source(PadContext context)
{
    if (context != layoutContext) {
        for (Finger &f : fingers) {
            f.quiet |= f.used && f.kind != STICK;
        }
        layoutContext = context;
    }
    return TouchShown() ? Held(context) : 0;
}

// A finger on the screen (not one lifted that still presses).
Finger *Find(SDL_FingerID id)
{
    for (Finger &f : fingers) {
        if (f.used && !f.lifted && f.id == id) {
            return &f;
        }
    }
    return nullptr;
}

void Down(SDL_FingerID id, float x, float y)
{
    Layout l = MakeLayout(layoutContext);
    Finger *f = Find(id);

    for (int i = 0; !f && i < FINGERS; i++) {
        if (!fingers[i].used) {
            f = &fingers[i];
        }
    }
    if (!f) {
        return;
    }
    f->id = id;
    f->used = true;
    f->quiet = false;
    f->lifted = false;
    f->downAt = SDL_GetTicks();
    f->x = x;
    f->y = y;
    if (Inside(l.console, x, y, 1.4f)) {
        f->kind = CONSOLE;
        TouchRelease();
        DebugUiSetOpen(1);
        return;
    }
    if (Inside(l.start, x, y, 1.4f)) {
        f->kind = START;
    } else if (x < l.stickZone && y > l.stickTop) {
        // the stick's centre where the thumb landed (its ring kept on the screen)
        f->kind = STICK;
        f->baseX = SDL_clamp(x, l.left + l.stickR, l.stickZone);
        f->baseY = SDL_clamp(y, l.stickTop + l.stickR, l.bottom - l.stickR);
    } else {
        f->kind = CLUSTER;
    }
    PsxPadNoteDevice(PAD_DEVICE_PLAYSTATION); // the game's prompts: the pad's symbols, as drawn here
    PsxPadTouchChanged();
}

void Move(SDL_FingerID id, float x, float y)
{
    Finger *f = Find(id);

    if (f) {
        f->x = x;
        f->y = y;
        PsxPadTouchChanged();
    }
}

void Up(SDL_FingerID id)
{
    Finger *f = Find(id);

    if (f) {
        if (SDL_GetTicks() - f->downAt < MIN_PRESS_MS) {
            f->lifted = true; // (TouchUpdate lets go of it)
            return;
        }
        f->used = false;
        PsxPadTouchChanged();
    }
}

// A finger's position in the window's points.
void Point(float nx, float ny, float *x, float *y)
{
    int w = 1, h = 1;

    SDL_GetWindowSize(window, &w, &h);
    *x = nx * (float)w;
    *y = ny * (float)h;
}

ImU32 Colour(int r, int g, int b, float alpha)
{
    return IM_COL32(r, g, b, (int)SDL_clamp(alpha * 255.0f, 0.0f, 255.0f));
}

void DrawSymbol(ImDrawList *d, const Button &b, float a)
{
    ImVec2 c(b.x, b.y);
    float s = b.r * 0.42f, t = SDL_max(2.0f, b.r * 0.09f);

    switch (b.symbol) {
    case CROSS:
        d->AddLine(ImVec2(c.x - s, c.y - s), ImVec2(c.x + s, c.y + s), Colour(124, 178, 232, a), t);
        d->AddLine(ImVec2(c.x - s, c.y + s), ImVec2(c.x + s, c.y - s), Colour(124, 178, 232, a), t);
        break;
    case CIRCLE:
        d->AddCircle(c, s, Colour(255, 102, 102, a), 32, t);
        break;
    case SQUARE:
        d->AddRect(ImVec2(c.x - s * 0.85f, c.y - s * 0.85f), ImVec2(c.x + s * 0.85f, c.y + s * 0.85f), Colour(255, 105, 248, a),
                   0.0f, 0, t);
        break;
    case TRIANGLE:
        d->AddTriangle(ImVec2(c.x, c.y - s), ImVec2(c.x - s * 1.05f, c.y + s * 0.75f), ImVec2(c.x + s * 1.05f, c.y + s * 0.75f),
                       Colour(64, 226, 160, a), t);
        break;
    default:
        break;
    }
}

void DrawButton(ImDrawList *d, const Button &b, bool pressed, float a, float textSize)
{
    d->AddCircleFilled(ImVec2(b.x, b.y), b.r, pressed ? Colour(255, 255, 255, 0.45f * a) : Colour(0, 0, 0, 0.35f * a), 48);
    d->AddCircle(ImVec2(b.x, b.y), b.r, Colour(255, 255, 255, 0.7f * a), 48, SDL_max(1.5f, b.r * 0.04f));
    DrawSymbol(d, b, a);
    if (b.label[0]) {
        float size = SDL_min(textSize, b.r * 2.0f / (0.62f * (float)SDL_strlen(b.label)));
        ImVec2 extent = ImGui::GetFont()->CalcTextSizeA(size, FLT_MAX, 0.0f, b.label);

        d->AddText(ImGui::GetFont(), size, ImVec2(b.x - extent.x / 2, b.y - extent.y / 2), Colour(255, 255, 255, 0.9f * a),
                   b.label);
    }
}

bool Pressing(const Button &b, Kind kind)
{
    for (const Finger &f : fingers) {
        if (f.used && !f.quiet && f.kind == kind && Inside(b, f.x, f.y, kind == CLUSTER ? 1.2f : 1.4f)) {
            return true;
        }
    }
    return false;
}

} // namespace

extern "C" void TouchInit(SDL_Window *w)
{
    window = w;
#ifdef PORT_MOBILE
    touched = true; // a phone or tablet: shown from the start
#endif
    PsxPadSetTouchSource(Source);
}

extern "C" int TouchShown(void)
{
    if (!window || DebugUiOpen()) {
        return 0;
    }
    return portSettings.touchControls == TOUCH_ON || (portSettings.touchControls == TOUCH_AUTO && touched);
}

extern "C" void TouchUpdate(void)
{
    bool changed = false;

    for (Finger &f : fingers) {
        if (f.used && f.lifted && SDL_GetTicks() - f.downAt >= MIN_PRESS_MS) {
            f.used = false;
            changed = true;
        }
    }
    if (changed) {
        PsxPadTouchChanged();
    }
}

extern "C" void TouchRelease(void)
{
    for (Finger &f : fingers) {
        f.used = false;
    }
    PsxPadTouchChanged();
}

extern "C" int TouchEvent(const SDL_Event *e)
{
    float x, y;

    if (!window) {
        return 0;
    }
    switch (e->type) {
    case SDL_EVENT_KEY_DOWN:
    case SDL_EVENT_GAMEPAD_BUTTON_DOWN:
        if (e->type == SDL_EVENT_GAMEPAD_BUTTON_DOWN || !e->key.repeat) {
            touched = false; // automatic: back to the keys or the gamepad
        }
        return 0;
    case SDL_EVENT_WINDOW_FOCUS_LOST:
        TouchRelease();
        return 0;
    case SDL_EVENT_FINGER_DOWN:
        if (DebugUiOpen()) {
            return 0; // (the console takes it, as the mouse)
        }
        touched = true;
        if (!TouchShown()) {
            return 0;
        }
        Point(e->tfinger.x, e->tfinger.y, &x, &y);
        Down(e->tfinger.fingerID, x, y);
        return 1;
    case SDL_EVENT_FINGER_MOTION:
        if (!Find(e->tfinger.fingerID)) {
            return 0;
        }
        Point(e->tfinger.x, e->tfinger.y, &x, &y);
        Move(e->tfinger.fingerID, x, y);
        return 1;
    case SDL_EVENT_FINGER_UP:
    case SDL_EVENT_FINGER_CANCELED:
        if (!Find(e->tfinger.fingerID)) {
            return 0;
        }
        Up(e->tfinger.fingerID);
        return 1;
    // the mouse, while the controls are shown (a computer with a touch screen and a mouse, or trying them out)
    case SDL_EVENT_MOUSE_BUTTON_DOWN:
        if (e->button.which == SDL_TOUCH_MOUSEID || e->button.button != SDL_BUTTON_LEFT || !TouchShown()) {
            return 0;
        }
        Down(MOUSE, e->button.x, e->button.y);
        return 1;
    case SDL_EVENT_MOUSE_MOTION:
        if (e->motion.which == SDL_TOUCH_MOUSEID || !Find(MOUSE)) {
            return 0;
        }
        Move(MOUSE, e->motion.x, e->motion.y);
        return 1;
    case SDL_EVENT_MOUSE_BUTTON_UP:
        if (e->button.which == SDL_TOUCH_MOUSEID || e->button.button != SDL_BUTTON_LEFT || !Find(MOUSE)) {
            return 0;
        }
        Up(MOUSE);
        return 1;
    default:
        return 0;
    }
}

extern "C" void TouchDraw(void)
{
    if (!TouchShown()) {
        return;
    }
    Layout l = MakeLayout(layoutContext);
    ImDrawList *d = ImGui::GetForegroundDrawList();
    float a = Opacity(), text = 0.045f * l.u;
    const Finger *stick = nullptr;

    for (const Finger &f : fingers) {
        if (f.used && !f.quiet && f.kind == STICK) {
            stick = &f;
        }
    }
    // the stick: its ring where the thumb landed (else at home), its knob under the thumb
    {
        float bx = stick ? stick->baseX : l.stickX, by = stick ? stick->baseY : l.stickY;
        float kx = bx, ky = by;

        if (stick) {
            float dx = stick->x - bx, dy = stick->y - by, n = hypotf(dx, dy);

            if (n > l.stickR) {
                dx *= l.stickR / n;
                dy *= l.stickR / n;
            }
            kx += dx;
            ky += dy;
        }
        d->AddCircleFilled(ImVec2(bx, by), l.stickR, Colour(0, 0, 0, 0.25f * a), 48);
        d->AddCircle(ImVec2(bx, by), l.stickR, Colour(255, 255, 255, 0.55f * a), 48, SDL_max(1.5f, l.stickR * 0.03f));
        d->AddCircleFilled(ImVec2(kx, ky), 0.45f * l.stickR, stick ? Colour(255, 255, 255, 0.5f * a) : Colour(255, 255, 255, 0.3f * a),
                           40);
    }
    for (int i = 0; i < l.clusterCount; i++) {
        DrawButton(d, l.cluster[i], Pressing(l.cluster[i], CLUSTER), a, text);
    }
    DrawButton(d, l.start, Pressing(l.start, START), a, text * 0.7f);
    if (!l.start.label[0]) {
        // a race: Start pauses (two bars)
        float s = l.start.r * 0.38f;
        d->AddRectFilled(ImVec2(l.start.x - s, l.start.y - s), ImVec2(l.start.x - s * 0.35f, l.start.y + s),
                         Colour(255, 255, 255, 0.9f * a));
        d->AddRectFilled(ImVec2(l.start.x + s * 0.35f, l.start.y - s), ImVec2(l.start.x + s, l.start.y + s),
                         Colour(255, 255, 255, 0.9f * a));
    }
    // the console: three lines
    DrawButton(d, l.console, false, a, text);
    for (int i = -1; i <= 1; i++) {
        float s = l.console.r * 0.45f, y = l.console.y + (float)i * l.console.r * 0.32f;

        d->AddLine(ImVec2(l.console.x - s, y), ImVec2(l.console.x + s, y), Colour(255, 255, 255, 0.9f * a),
                   SDL_max(1.5f, l.console.r * 0.1f));
    }
}
