/* The ROM's sky model is a cube with twelve columns and two rows. */
#include "../../platform/original.c"
#include <assert.h>

int main(void)
{
    uint8_t model[1488] = {0}, tim[2560] = {0};
    uint8_t *images[12] = {0};
    size_t lengths[12] = {0};
    TimList sky = {images, lengths, 12};
    int column, row, i;

    PutBe16(model + 16, 36);
    PutBe16(model + 32, 24);
    PutLe32(tim, 16); PutLe32(tim + 4, 9); PutLe32(tim + 8, 524);
    PutLe16(tim + 16, 256); PutLe16(tim + 18, 1);
    PutLe32(tim + 532, 12); PutLe16(tim + 540, 28); PutLe16(tim + 542, 36);
    images[0] = tim; lengths[0] = sizeof(tim);
    for (row = 0; row < 3; row++) {
        for (column = 0; column < 12; column++) {
            int face = column / 3, along = -3000 + (column % 3) * 2000;
            int x = face == 0 ? 3000 : face == 1 ? -along : face == 2 ? -3000 : along;
            int z = face == 0 ? along : face == 1 ? 3000 : face == 2 ? -along : -3000;
            uint8_t *v = model + 144 + (row * 12 + column) * 8;
            PutBe16(v, x); PutBe16(v + 2, 1478 - row * 1478); PutBe16(v + 4, z);
        }
    }
    for (column = 0; column < 12; column++) {
        for (row = 0; row < 2; row++) {
            uint8_t *poly = model + 432 + (column * 2 + row) * 44;
            int next = (column + 1) % 12;
            PutBe16(poly, 8);
            PutBe16(poly + 4, (row + 1) * 12 + column);
            PutBe16(poly + 6, (row + 1) * 12 + next);
            PutBe16(poly + 8, row * 12 + column);
            PutBe16(poly + 10, row * 12 + next);
            memset(poly + 18, 127, 8); /* old model's out-of-image UVs */
        }
    }
    assert(Wipeout64Sky(model, sizeof(model), &sky));
    for (column = 0; column < 12; column++) {
        for (row = 0; row < 2; row++) {
            uint8_t *poly = model + 432 + (column * 2 + row) * 44;
            assert(Be16(poly + 12) == (column / 2) * 2 + row);
            for (i = 0; i < 4; i++) {
                assert(poly[18 + i * 2] < 56);
                assert(poly[19 + i * 2] < 36);
                assert(poly[18 + i * 2] / 28 == column % 2);
            }
            assert(poly[19] == 0 && poly[23] == 35);
        }
    }
    assert(!Wipeout64Sky(model, sizeof(model) - 1, &sky));
    PutBe16(model + 436, 36);
    assert(!Wipeout64Sky(model, sizeof(model), &sky));
    puts("guest sky tiles and UVs: OK");
    return 0;
}
