/* Session recordings must retain stick values, including changes without a button press. */
#include <unistd.h>
#include "../../platform/trace.c"
#include "../../platform/controls.c"
#include <assert.h>
PortSettings portSettings;
static int liveSteering = -1;
int PsxPadSteering(int port) { return liveSteering; }
int main(void)
{
    char path[] = "/tmp/wipeout-steering-record-XXXXXX";
    int fd = mkstemp(path);
    unsigned char packet[8] = {0, 0x41, 0xff, 0xff};
    assert(fd >= 0);
    recordPads = fdopen(fd, "w");
    padReads = 0; RecordPad(packet);
    padReads = 1; liveSteering = 140; RecordPad(packet);
    padReads = 2; liveSteering = 200; RecordPad(packet);
    padReads = 3; liveSteering = -1; RecordPad(packet);
    fclose(recordPads); recordPads = NULL;
    LoadPads(path);
    assert(padStepCount == 4);
    padReads = 0; assert(PortGamepadSteering() == -1);
    padReads = 1; assert(PortGamepadSteering() == 140);
    padReads = 2; assert(PortGamepadSteering() == 200);
    padReads = 3; assert(PortGamepadSteering() == -1);
    file = tmpfile(); /* console comparison runs retain digital inputs */
    padReads = 2; assert(PortGamepadSteering() == -1 && !PortAnalogSteeringEnabled());
    fclose(file); file = NULL;
    unlink(path);
    SDL_free(padSteps);
    puts("session recordings preserve stick input and accept old digital entries: OK");
    return 0;
}
