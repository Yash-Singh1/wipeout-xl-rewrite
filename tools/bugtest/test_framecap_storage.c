/* Retired ring slots must not retain geometry beyond the active history.
   JPEG tokens belong to slots and must survive geometry storage reuse. */
#define PORT_VERSION "test"
#define STB_IMAGE_IMPLEMENTATION
#define STBI_ONLY_JPEG
#include "../../platform/psx/framecap.c"
#include <assert.h>
#include <unistd.h>
void RenderTrimMemory(void) {}
void PortLog(const char *format, ...) {}
int PortGameInfo(char *info, int size) { info[0] = 0; return 0; }
int portPal;
uint16_t psxVram[VRAM_HEIGHT][VRAM_WIDTH];
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
    /* A small animated texture must not duplicate all of VRAM every frame. */
    int saved[90];
    for (int i = 0; i < 90; i++) {
        psxVram[0][0] = i;
        FrameCapVramChanged();
        saved[i] = TakeSnapshot();
        assert(saved[i] >= 0);
        if (i) {
            assert(snapshots[saved[i]].pages[0] != snapshots[saved[0]].pages[0]);
            for (size_t page = 1; page < VRAM_PAGES; page++)
                assert(snapshots[saved[i]].pages[page] == snapshots[saved[0]].pages[page]);
        }
    }
    for (int i = 0; i < 90; i++) {
        const uint16_t *pixels = FrameCapVram(saved[i]);
        assert(pixels && pixels[0] == i);
        for (size_t word = 1; word < sizeof(psxVram) / sizeof(uint16_t); word++) assert(!pixels[word]);
    }
    int shared = TakeSnapshot();
    assert(shared == saved[89] && snapshots[shared].users == 2);
    Release(shared);
    for (int i = 0; i < 90; i++) Release(saved[i]);
    assert(lastSnapshot == -1);
    FrameCapClear();
    assert(!snapshotPixels && decodedSnapshot == -1);
    /* Saved images leave RAM but decode identically when browsed again. */
    FrameCapFrame *imageFrame = NewFrame();
    imageFrame->vram = -1;
    imageFrame->shown = 1;
    imageFrame->pictureToken = 12345;
    imageFrame->pictureW = imageFrame->pictureH = 8;
    Job job = {.pixels = malloc(8 * 8 * 4), .w = 8, .h = 8,
               .slot = (int)(imageFrame - frames), .token = imageFrame->pictureToken};
    for (int i = 0; i < 8 * 8 * 4; i++) job.pixels[i] = i;
    Encode(&job);
    int width, height;
    uint8_t expected[8 * 8 * 3];
    memcpy(expected, FrameCapPicture(0, &width, &height), sizeof(expected));
    FrameCapFrame *same = NewFrame();
    same->vram = -1; same->shown = 1; same->pictureToken = 12346;
    same->pictureW = same->pictureH = 8;
    Job duplicate = {.pixels = malloc(8 * 8 * 4), .w = 8, .h = 8,
                     .slot = (int)(same - frames), .token = same->pictureToken};
    for (int i = 0; i < 8 * 8 * 4; i++) duplicate.pixels[i] = i;
    Encode(&duplicate);
    assert(imageFrame->jpeg == same->jpeg && encodedPictures[same - frames]->users == 2);
    char path[] = "/tmp/wipeout-framecap-XXXXXX";
    int fd = mkstemp(path); assert(fd >= 0); close(fd);
    assert(!SavePicture(imageFrame, "/nonexistent/wipeout-missing-dir/picture.jpg"));
    assert(imageFrame->jpeg);
    assert(SavePicture(imageFrame, path));
    assert(!imageFrame->jpeg && imageFrame->jpegSize > 0);
    assert(same->jpeg && !memcmp(expected, FrameCapPicture(1, &width, &height), sizeof(expected)));
    stbi_image_free(decodedPicture); decodedPicture = NULL;
    assert(!memcmp(expected, FrameCapPicture(0, &width, &height), sizeof(expected)));
    assert(width == 8 && height == 8 && SavePicture(imageFrame, path));
    assert(ProtectBackingPictures("/tmp"));
    assert(imageFrame->jpeg); /* loaded before an overlapping re-save */
    assert(SavePicture(imageFrame, path) && !imageFrame->jpeg);
    /* Saving must retain the complete binary inspection data and VRAM files. */
    char directory[] = "/tmp/wipeout-framecap-export-XXXXXX";
    assert(mkdtemp(directory));
    SDL_setenv_unsafe("WIPEOUT_FRAMECAP_DIR", directory, 1);
    psxVram[0][0] = 543;
    FrameCapVramChanged();
    imageFrame->vram = TakeSnapshot();
    current = (int)(imageFrame - frames);
    FrameCapPrim *exported = FrameCapAdd();
    exported->address = 0x12345678;
    Save();
    char exportedPath[256]; size_t exportedSize;
    SDL_snprintf(exportedPath, sizeof(exportedPath), "%s/frames.bin", directory);
    uint8_t *binary = SDL_LoadFile(exportedPath, &exportedSize);
    assert(binary && exportedSize == 12 + 2 * 52 + 244);
    assert(!memcmp(binary, "XLFC", 4) && binary[8] == 2 && binary[52] == 1);
    SDL_free(binary); remove(exportedPath);
    SDL_snprintf(exportedPath, sizeof(exportedPath), "%s/vram_%d.bin", directory, imageFrame->vram);
    uint16_t *vram = SDL_LoadFile(exportedPath, &exportedSize);
    assert(vram && exportedSize == sizeof(psxVram) && vram[0] == 543);
    SDL_free(vram); remove(exportedPath);
    for (int i = 0; i < 2; i++) {
        SDL_snprintf(exportedPath, sizeof(exportedPath), "%s/picture_%03d.jpg", directory, i);
        assert(!frames[(first + i) % FRAMES].jpeg);
        remove(exportedPath);
    }
    SDL_snprintf(exportedPath, sizeof(exportedPath), "%s/info.json", directory);
    remove(exportedPath); rmdir(directory);
    SDL_unsetenv_unsafe("WIPEOUT_FRAMECAP_DIR");
    remove(path);
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
