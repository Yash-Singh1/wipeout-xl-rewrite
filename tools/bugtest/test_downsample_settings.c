/* Target scales survive the settings file and environment override. */
#include <SDL3/SDL.h>
#include <assert.h>
#include <unistd.h>
static char directory[128];
static char *TestPrefPath(const char *org, const char *app)
{
    (void)org;
    (void)app;
    return SDL_strdup(directory);
}
#define SDL_GetPrefPath TestPrefPath
#include "../../platform/settings.c"
#undef SDL_GetPrefPath
PortConfig portConfig;

int main(void)
{
    char temp[] = "/tmp/wipeout-downsample-test-XXXXXX";
    assert(mkdtemp(temp));
    SDL_snprintf(directory, sizeof(directory), "%s/", temp);
    SDL_UnsetEnvironmentVariable(SDL_GetEnvironment(), "WIPEOUT_DOWNSAMPLE");
    SDL_UnsetEnvironmentVariable(SDL_GetEnvironment(), "WIPEOUT_DOWNSAMPLE_STYLE");
    assert(portSettings.downsample == 0 && portSettings.downsampleStyle == DOWNSAMPLE_BOX);
    PortSettingsLoad();
    Set("downsample", "1"); /* the old checkbox setting */
    assert(portSettings.downsample == 1);
    Set("downsample", "3");
    Set("downsample_style", "1");
    assert(PortSettingsSave());
    portSettings.downsample = portSettings.downsampleStyle = 0;
    PortSettingsLoad();
    assert(portSettings.downsample == 3 && portSettings.downsampleStyle == DOWNSAMPLE_ADAPTIVE);
    SDL_SetEnvironmentVariable(SDL_GetEnvironment(), "WIPEOUT_DOWNSAMPLE", "2", true);
    PortSettingsLoad();
    assert(portSettings.downsample == 2);
    SDL_SetEnvironmentVariable(SDL_GetEnvironment(), "WIPEOUT_DOWNSAMPLE", "0", true);
    PortSettingsLoad();
    assert(portSettings.downsample == 0);
    SDL_SetEnvironmentVariable(SDL_GetEnvironment(), "WIPEOUT_DOWNSAMPLE_STYLE", "0", true);
    PortSettingsLoad();
    assert(portSettings.downsampleStyle == DOWNSAMPLE_BOX);
    Set("downsample_style", "99"); assert(portSettings.downsampleStyle == DOWNSAMPLE_ADAPTIVE);
    Set("downsample_style", "-1"); assert(portSettings.downsampleStyle == DOWNSAMPLE_BOX);
    Set("downsample", "-4"); assert(portSettings.downsample == 0);
    Set("downsample", "999"); assert(portSettings.downsample == 16);
    unlink(path);
    rmdir(temp);
    puts("Downsampling target persists, legacy native target works, environment overrides and bounds hold: OK");
    return 0;
}
