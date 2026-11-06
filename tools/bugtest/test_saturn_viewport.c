/* The title's padded 352x240 tilemap must retain both sides when fitted to the port. */
#include "../../platform/saturn.c"
#include <assert.h>
#include <stdlib.h>

int main(void)
{
    Video *video = calloc(1, sizeof(*video));
    uint8_t map[64 * 64 * 2];
    uint16_t pixels[320 * 224];
    const uint32_t cells = VDP2_RAM;

    /* Two blank columns either side of a 40-cell picture, and equal blank rows. */
    memset(map, 0, sizeof(map));
    PutBe16(video->cram + 2, 31);
    PutBe16(video->cram + 4, 31 << 10);
    for (int y = 1; y < 29; y++) {
        for (int x = 2; x < 42; x++) {
            PutBe16(map + (y * 64 + x) * 2, x < 22 ? 3 : 5);
        }
    }
    /* Map character 1 is blank; characters 3 and 5 are the two test colours. */
    for (int i = 0; i < 64 * 64; i++) {
        if (!Be16(map + i * 2)) PutBe16(map + i * 2, 1);
    }
    memset(VideoAt(video, cells, 64), 0, 64);
    memset(VideoAt(video, cells + 64, 64), 1, 64);
    memset(VideoAt(video, cells + 128, 64), 2, 64);
    RenderBackground(video, map, sizeof(map), cells, 0, 0, 352, 240, pixels);
    for (int y = 0; y < 224; y++) {
        for (int x = 0; x < 320; x++) {
            int sx = x * 352 / 320, sy = y * 240 / 224;
            int expected = sx < 16 || sx >= 336 || sy < 8 || sy >= 232 ? 0 : sx < 176 ? 31 : 31 << 10;
            assert(pixels[y * 320 + x] == expected);
        }
    }
    free(video);
    puts("Saturn title retains its left and right borders and both halves of the artwork: OK");
}
