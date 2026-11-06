/* Retired ring slots must not retain geometry beyond the active history.
   JPEG tokens belong to slots and must survive geometry storage reuse. */
#define PORT_VERSION "test"
#include "../../platform/psx/framecap.c"
#include <assert.h>
int portPal;
uint16_t psxVram[VRAM_HEIGHT][VRAM_WIDTH];
uint64_t PortGameFrames(void) { return 0; }
PortConfig portConfig;
PortSettings portSettings;
int PortLinkDownsample(void) { return -1; }
int PortLinkDownsampleStyle(void) { return -1; }
int GpuScale(void) { return 12; }
int GpuShownMargin(int x, int w) { return 32; }
int GpuReadPictureDone(int reader, int wait, void (*use)(const uint8_t *, int, int, void *), void *user) { return 0; }
int GpuReadPictureStart(int reader, int x, int y, int w, int h, int margin, int width, int height) { return 1; }
int PortShownFrames(void) { return 0; }
int main(void)
{
    SDL_setenv_unsafe("WIPEOUT_FRAMECAP", "1", 1);
    FrameCapDisplayEnabled(0);
    FrameCapTable(0, 0, 0, 0, 320, 240);
    assert(!count && !FrameCapAdd());
    FrameCapDisplayEnabled(1);
    FrameCapTable(0, 0, 0, 0, 320, 240);
    assert(count == 1 && FrameCapAdd());
    FrameCapDisplayEnabled(0);
    assert(!FrameCapAdd());
    FrameCapDisplayEnabled(1);
    FrameCapTable(0, 256, 0, 256, 320, 240);
    assert(count == 2 && FrameCapFrameAt(1)->count == 0);
    assert(!FrameCapPictureReady(0)); /* Unshown draws cannot enter the picture viewer. */
    FrameCapClear();
    for (int n = 0; n < FRAMES * 3; n++) {
        FrameCapFrame *f = NewFrame();
        f->vram = -1;
        current = (int)(f - frames);
        FrameCapPrim *p = FrameCapAdd();
        assert(p && p->serial == 0);
        p->address = (uint64_t)n;
        int allocated = 0;
        for (int i = 0; i < FRAMES; i++) allocated += frames[i].prims != NULL;
        assert(allocated == count && count <= Capacity());
    }
    /* Shrinking the history releases the excess, including inactive slots. */
    portSettings.recordRate = 30;
    FrameCapFrame *f = NewFrame(); f->vram = -1;
    assert(count == 90);
    int allocated = 0;
    for (int i = 0; i < FRAMES; i++) allocated += frames[i].prims != NULL;
    assert(allocated == 90);
    portSettings.downsample = 1;
    portSettings.downsampleStyle = DOWNSAMPLE_BOX;
    TakePicture(f, 0, 0, 320, 240);
    assert(f->pictureW == 384 && f->pictureH == GPU_LINES && f->pictureScale == 1);
    portSettings.downsampleStyle = DOWNSAMPLE_ADAPTIVE;
    TakePicture(f, 0, 0, 320, 240);
    assert(f->pictureW == 4608 && f->pictureH == GPU_LINES * 12 && f->pictureScale == 12);
    portSettings.downsampleStyle = DOWNSAMPLE_BOX;
    portSettings.downsample = 0;
    TakePicture(f, 0, 0, 320, 240);
    assert(f->pictureScale == 12);
    current = (int)(f - frames);
    f->shown = 0;
    DropUnfinished();
    assert(count == 89);
    FrameCapClear();
    assert(!count);
    for (int i = 0; i < FRAMES; i++) assert(!frames[i].prims);
    puts("recording reuses active history storage and retains effective image resolution: OK");
}
