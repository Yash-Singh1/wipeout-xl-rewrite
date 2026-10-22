/* No audio device (builds without SDL, such as the unit tests): time follows the video fields. */
#include "internal.h"

bool PsxAudioDeviceOpen(bool queued)
{
    (void)queued;
    return false;
}

void PsxAudioDeviceQueue(const int16_t *samples, uint32_t frames)
{
    (void)samples;
    (void)frames;
}

void PsxAudioDevicePause(bool paused)
{
    (void)paused;
}

void PsxAudioDeviceClose(void) {}
