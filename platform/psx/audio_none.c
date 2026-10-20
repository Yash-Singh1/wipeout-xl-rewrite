/* No audio device (builds without SDL, such as the unit tests): time follows the video fields. */
#include "internal.h"

bool PsxAudioDeviceOpen(void)
{
    return false;
}

void PsxAudioDeviceClose(void) {}
