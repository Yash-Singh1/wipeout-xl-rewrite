/* Both recorder codecs must preserve dimensions, 4:4:4 chroma and quality-95
   quantization. The accelerated encoder may round DCT coefficients differently. */
#define main StorageRegressionMain
#include "test_framecap_storage.c"
#undef main
static void Tables(const uint8_t *jpeg, size_t size, uint8_t tables[2][64])
{
    int found = 0, colour = 0;
    for (size_t at = 2; at + 4 < size;) {
        assert(jpeg[at] == 255);
        int marker = jpeg[at + 1];
        size_t length = (jpeg[at + 2] << 8) | jpeg[at + 3];
        assert(length >= 2 && at + 2 + length <= size);
        if (marker == 0xda) break;
        if (marker == 0xdb) {
            for (size_t q = at + 4; q < at + 2 + length; q += 65) {
                assert(q + 65 <= at + 2 + length && jpeg[q] < 2);
                memcpy(tables[jpeg[q]], jpeg + q + 1, 64);
                found |= 1 << jpeg[q];
            }
        }
        if (marker == 0xc0) {
            assert(jpeg[at + 4] == 8 && jpeg[at + 9] == 3);
            for (int c = 0; c < 3; c++) assert(jpeg[at + 11 + c * 3] == 0x11);
            colour = 1;
        }
        at += 2 + length;
    }
    assert(found == 3 && colour);
}
int main(void)
{
    enum { W = 319, H = 241 };
    uint8_t source[W * H * 4];
    for (int y = 0; y < H; y++) for (int x = 0; x < W; x++) {
        uint8_t *p = source + (y * W + x) * 4;
        p[0] = x * 7 + y; p[1] = y * 3 - x;
        p[2] = (x % 13 < 3 || y % 17 < 2) ? 255 : 12; p[3] = 255;
    }
    Output reference = {0};
    assert(stbi_write_jpg_to_func(Write, &reference, W, H, 4, source, PICTURE_QUALITY));
    FrameCapFrame *frame = NewFrame(); frame->vram = -1;
    frame->pictureToken = 7;
    Job job = {.w = W, .h = H, .slot = (int)(frame - frames), .token = 7};
    job.pixels = malloc(sizeof(source)); memcpy(job.pixels, source, sizeof(source));
    Encode(&job); assert(frame->jpeg);
    uint8_t expected[2][64], actual[2][64];
    Tables(reference.data, reference.size, expected); Tables(frame->jpeg, frame->jpegSize, actual);
    assert(!memcmp(expected, actual, sizeof(expected)));
    int w, h, channels;
    uint8_t *old = stbi_load_from_memory(reference.data, (int)reference.size, &w, &h, &channels, 3);
    assert(old && w == W && h == H);
    uint8_t *decoded = stbi_load_from_memory(frame->jpeg, (int)frame->jpegSize, &w, &h, &channels, 3);
    assert(decoded && w == W && h == H);
    double oldError = 0, error = 0;
    for (int i = 0; i < W * H; i++) for (int c = 0; c < 3; c++) {
        oldError += abs(old[i * 3 + c] - source[i * 4 + c]);
        error += abs(decoded[i * 3 + c] - source[i * 4 + c]);
    }
    assert(error / (W * H * 3) <= oldError / (W * H * 3) + 0.3);
    stbi_image_free(old); stbi_image_free(decoded); free(reference.data); FrameCapClear();
    puts("recorder JPEG retains quality-95 quantization, 4:4:4 chroma, dimensions and reconstruction quality: OK");
}
