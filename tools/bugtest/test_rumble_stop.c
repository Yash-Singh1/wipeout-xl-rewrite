/* Freezing must silence both controllers and discard effects that could restart. */
#include "../../platform/psx/pad_sdl.c"
#include <assert.h>
#include <stdio.h>

static struct Motors { Uint16 low, high; int calls; } motors[2];
static bool SDLCALL MotorOutput(void *userdata, Uint16 low, Uint16 high)
{
    struct Motors *m = userdata;
    m->low = low; m->high = high; m->calls++;
    return true;
}
int PortWindowFocused(void) { return 1; }
int PortLinkRole(void) { return LINK_NONE; }
int PortLinkTakeRumble(int kind, int *level) { return 0; }
void PortPadSecondRumble(int kind, int level) { assert(0); }

int main(void)
{
    assert(SDL_Init(SDL_INIT_JOYSTICK));
    SDL_JoystickID ids[2];
    for (int player = 0; player < 2; player++) {
        SDL_VirtualJoystickDesc desc;
        SDL_INIT_INTERFACE(&desc);
        desc.type = SDL_JOYSTICK_TYPE_GAMEPAD;
        desc.nbuttons = 16;
        desc.userdata = &motors[player];
        desc.Rumble = MotorOutput;
        ids[player] = SDL_AttachVirtualJoystick(&desc);
        assert(ids[player]);
        joysticks[player] = SDL_OpenJoystick(ids[player]);
        assert(joysticks[player]);
    }
    routed = 1; firstDevice = ids[0]; secondDevice = ids[1];
    for (int player = 0; player < 2; player++) {
        for (int kind = 0; kind < PAD_RUMBLE_KINDS; kind++) PsxPadRumble(player, kind, 20);
        assert(motors[player].low && motors[player].high);
    }
    PsxPadStopRumble();
    for (int player = 0; player < 2; player++) {
        assert(motors[player].calls && !motors[player].low && !motors[player].high);
        for (int kind = 0; kind < PAD_RUMBLE_KINDS; kind++) {
            assert(!rumbles[player].level[kind] && !rumbles[player].until[kind]);
        }
    }
    rumble_tick(); /* Resuming without a new effect must remain silent. */
    assert(!motors[0].low && !motors[0].high && !motors[1].low && !motors[1].high);
    PsxPadRumble(0, PAD_RUMBLE_SCRAPE, 10);
    assert(!motors[0].low && motors[0].high && !motors[1].high);
    PsxPadStopRumble();
    for (int player = 0; player < 2; player++) {
        SDL_CloseJoystick(joysticks[player]);
        SDL_DetachVirtualJoystick(ids[player]);
    }
    SDL_Quit();
    puts("Both controllers stop all rumble effects and resume only on fresh requests: OK");
}
