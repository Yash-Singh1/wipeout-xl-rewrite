/* Reuse the standalone GPU setup and stubs. Run with a real OpenGL context. */
#define main HudProbeMain
#include "test_gpu_hud.c"
#undef main

/* Compare hardware Box reduction with a CPU coverage integral. Include clipped
   odd edges, nonzero origins, fractional ratios, all scales and colour channels. */
static void TestBoxCoverage(void)
{
    PictureBuffer output = {0};
    for (int factor = 1; factor <= SDL_min(12, GpuMaxScale()); factor++) {
        SetScale(factor);
        for (int clipped = 0; clipped < 2; clipped++) {
            int w = 5 * factor + clipped, h = 3 * factor + clipped;
            uint8_t *pixels = malloc((size_t)w * h * 4);
            for (int i = 0; i < w * h * 4; i++) pixels[i] = (i * 73 + i / 7 * 19) & 255;
            glBindTexture(GL_TEXTURE_2D, colour);
            glTexSubImage2D(GL_TEXTURE_2D, 0, 7, 9, w, h, GL_RGBA, GL_UNSIGNED_BYTE, pixels);
            for (int target = 1; target <= factor; target++) {
                if (factor % target) continue;
                int ow = (w * target + factor - 1) / factor;
                int oh = (h * target + factor - 1) / factor;
                uint8_t got[128 * 128 * 4];
                BindPictureBuffer(&output, ow, oh);
                portSettings.downsample = target;
                portSettings.downsampleStyle = DOWNSAMPLE_BOX;
                glEnable(GL_SCISSOR_TEST);
                glScissor(0, 0, 1, 1); /* presentation copies must ignore the scene's scissor */
                BlitPicture(0, 7, 9, 7 + w, 9 + h, output.framebuffer, 0, 0, ow, oh);
                glBindFramebuffer(GL_READ_FRAMEBUFFER, output.framebuffer);
                glReadPixels(0, 0, ow, oh, GL_RGBA, GL_UNSIGNED_BYTE, got);
                for (int y = 0; y < oh; y++) for (int x = 0; x < ow; x++) {
                    double step = (double)factor / target;
                    double loX = x * step, loY = y * step;
                    double hiX = fmin(loX + step, w), hiY = fmin(loY + step, h);
                    for (int c = 0; c < 4; c++) {
                        double sum = 0;
                        for (int sy = (int)loY; sy < (int)ceil(hiY); sy++)
                            for (int sx = (int)loX; sx < (int)ceil(hiX); sx++) {
                                double weight = (fmin(sx + 1, hiX) - fmax(sx, loX)) *
                                                (fmin(sy + 1, hiY) - fmax(sy, loY));
                                sum += pixels[(sy * w + sx) * 4 + c] * weight;
                            }
                        int expected = (int)floor(sum / ((hiX - loX) * (hiY - loY)) + 0.5);
                        assert(abs(got[(y * ow + x) * 4 + c] - expected) <= 1);
                    }
                }
            }
            free(pixels);
        }
    }
    glDeleteFramebuffers(1, &output.framebuffer);
    glDeleteTextures(1, &output.texture);
    puts("Box coverage matches CPU averaging within one UNORM level at scales 1..12 and clipped edges: OK");
}

static void TestAdaptive(GLuint output, GLuint outputTexture)
{
    portSettings.renderScale = SDL_min(4, GpuMaxScale());
    SetScale(portSettings.renderScale);
    int side = 4 * scale;
    uint8_t *pixels = malloc((size_t)side * side * 4);
    uint8_t result[16 * 16 * 4];

    glBindTexture(GL_TEXTURE_2D, outputTexture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, 16, 16, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
    /* Enlarged 2D pixels are constant inside each console pixel. Adaptive must
       smooth their transitions, while Box retains hard pixel boundaries. */
    for (int y = 0; y < side; y++) {
        for (int x = 0; x < side; x++) {
            int at = (y * side + x) * 4;
            pixels[at] = x >= side / 2 ? 255 : 0;
            pixels[at + 1] = pixels[at + 2] = 0;
            pixels[at + 3] = 255;
        }
    }
    glBindTexture(GL_TEXTURE_2D, colour);
    glTexSubImage2D(GL_TEXTURE_2D, 0, 7 * scale, 9 * scale, side, side, GL_RGBA, GL_UNSIGNED_BYTE, pixels);
    portSettings.downsample = 1;
    portSettings.downsampleStyle = DOWNSAMPLE_BOX;
    BlitPicture(0, 7 * scale, 9 * scale, 11 * scale, 13 * scale, output, 0, 0, 16, 16);
    glBindFramebuffer(GL_READ_FRAMEBUFFER, output);
    glReadPixels(0, 0, 16, 16, GL_RGBA, GL_UNSIGNED_BYTE, result);
    assert(result[(8 * 16 + 7) * 4] == 0 && result[(8 * 16 + 8) * 4] == 255);
    portSettings.downsampleStyle = DOWNSAMPLE_ADAPTIVE;
    BlitPicture(0, 7 * scale, 9 * scale, 11 * scale, 13 * scale, output, 0, 0, 16, 16);
    glBindFramebuffer(GL_READ_FRAMEBUFFER, output);
    glReadPixels(0, 0, 16, 16, GL_RGBA, GL_UNSIGNED_BYTE, result);
    assert(result[(8 * 16 + 7) * 4] > 0 && result[(8 * 16 + 7) * 4] < 128);
    assert(result[(8 * 16 + 8) * 4] > 128 && result[(8 * 16 + 8) * 4] < 255);
    assert(result[(8 * 16) * 4] == 0 && result[(8 * 16 + 15) * 4] == 255);
    uint8_t *recorded = GpuReadPicture(7, 9, 4, 4, 0, 16, 16);
    assert(!memcmp(recorded, result, sizeof(result)));

    /* Fine rendered detail varies within a target pixel. Keep it even though a
       full box reduction would turn this checkerboard grey. */
    for (int factor = 2; factor <= SDL_min(4, GpuMaxScale()); factor++) {
        SetScale(factor);
        side = 4 * factor;
        for (int y = 0; y < side; y++) {
            for (int x = 0; x < side; x++) {
                int at = (y * side + x) * 4;
                pixels[at] = pixels[at + 1] = pixels[at + 2] = (x + y) % 2 ? 255 : 0;
                pixels[at + 3] = 255;
            }
        }
        glBindTexture(GL_TEXTURE_2D, colour);
        glTexSubImage2D(GL_TEXTURE_2D, 0, 7 * factor, 9 * factor, side, side, GL_RGBA, GL_UNSIGNED_BYTE, pixels);
        for (int targetScale = 1; targetScale <= factor; targetScale++) {
            portSettings.downsample = targetScale;
            portSettings.downsampleStyle = DOWNSAMPLE_BOX;
            BlitPicture(0, 7 * factor, 9 * factor, 11 * factor, 13 * factor, output, 0, 0, side, side);
            glBindFramebuffer(GL_READ_FRAMEBUFFER, output);
            glReadPixels(0, 0, side, side, GL_RGBA, GL_UNSIGNED_BYTE, result);
            if (targetScale < factor) {
                assert(result[0] > 0 && result[0] < 255);
            }
            portSettings.downsampleStyle = DOWNSAMPLE_ADAPTIVE;
            BlitPicture(0, 7 * factor, 9 * factor, 11 * factor, 13 * factor, output, 0, 0, side, side);
            glBindFramebuffer(GL_READ_FRAMEBUFFER, output);
            glReadPixels(0, 0, side, side, GL_RGBA, GL_UNSIGNED_BYTE, result);
            assert(!memcmp(result, pixels, (size_t)side * side * 4));

            SetWideMargin(16);
            glBindFramebuffer(GL_READ_FRAMEBUFFER, framebuffer);
            glBindTexture(GL_TEXTURE_2D, wideColour);
            glCopyTexSubImage2D(GL_TEXTURE_2D, 0, 7 * factor, 9 * factor, 7 * factor, 9 * factor, side, side);
            BlitPicture(1, 7 * factor, 9 * factor, 11 * factor, 13 * factor, output, 0, side, side, 0);
            glBindFramebuffer(GL_READ_FRAMEBUFFER, output);
            glReadPixels(0, 0, side, side, GL_RGBA, GL_UNSIGNED_BYTE, result);
            for (int row = 0; row < side; row++) {
                assert(!memcmp(result + row * side * 4, pixels + (side - row - 1) * side * 4, (size_t)side * 4));
            }
            SetWideMargin(0);
            assert(glGetError() == GL_NO_ERROR);
        }
        /* A fine feature at a cell corner must stay bright without a halo. */
        memset(pixels, 0, (size_t)side * side * 4);
        for (int at = 3; at < side * side * 4; at += 4) pixels[at] = 255;
        int point = (factor * side + factor) * 4;
        pixels[point] = pixels[point + 1] = pixels[point + 2] = 255;
        glBindTexture(GL_TEXTURE_2D, colour);
        glTexSubImage2D(GL_TEXTURE_2D, 0, 7 * factor, 9 * factor, side, side, GL_RGBA, GL_UNSIGNED_BYTE, pixels);
        portSettings.downsample = 1;
        BlitPicture(0, 7 * factor, 9 * factor, 11 * factor, 13 * factor, output, 0, 0, side, side);
        glBindFramebuffer(GL_READ_FRAMEBUFFER, output);
        glReadPixels(0, 0, side, side, GL_RGBA, GL_UNSIGNED_BYTE, result);
        assert(!memcmp(result, pixels, (size_t)side * side * 4));
        /* Off bypasses Adaptive too. */
        portSettings.downsample = 0;
        BlitPicture(0, 7 * factor, 9 * factor, 11 * factor, 13 * factor, output, 0, 0, side, side);
        glBindFramebuffer(GL_READ_FRAMEBUFFER, output);
        glReadPixels(0, 0, side, side, GL_RGBA, GL_UNSIGNED_BYTE, result);
        assert(!memcmp(result, pixels, (size_t)side * side * 4));
    }
    free(pixels);
    portSettings.downsampleStyle = DOWNSAMPLE_BOX;
    puts("Adaptive smooths enlarged flat pixels, preserves fine detail, handles target scales, readback and widescreen: OK");
}

int main(void)
{
    assert(SDL_Init(SDL_INIT_VIDEO));
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
    SDL_Window *window = SDL_CreateWindow("downsampling probe", 320, 240, SDL_WINDOW_OPENGL | SDL_WINDOW_HIDDEN);
    assert(window);
    SDL_GLContext context = SDL_GL_CreateContext(window);
    assert(context);
    portSettings.renderScale = 1;
    assert(GpuInit());
    TestBoxCoverage();

    GLuint output, outputTexture;
    glGenFramebuffers(1, &output);
    glGenTextures(1, &outputTexture);
    glBindTexture(GL_TEXTURE_2D, outputTexture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, 8, 8, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
    glBindFramebuffer(GL_FRAMEBUFFER, output);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, outputTexture, 0);
    assert(glCheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE);

    /* Odd and even factors. A single bright corner must contribute even when a
       linear resize would only sample the centre. Also test a nonzero VRAM origin. */
    for (int factor = 1; factor <= SDL_min(4, GpuMaxScale()); factor++) {
        portSettings.renderScale = factor;
        SetScale(factor);
        uint8_t *pixels = calloc((size_t)4 * factor * factor * 4, 1);
        for (int py = 0; py < 2 * factor; py++) {
            for (int px = 0; px < 2 * factor; px++) {
                int at = (py * 2 * factor + px) * 4;
                pixels[at + 3] = 255;
                if (px >= factor) pixels[at + 1] = 200;
                if (py >= factor) pixels[at + 2] = 160;
            }
        }
        pixels[0] = 255;
        glBindTexture(GL_TEXTURE_2D, colour);
        glTexSubImage2D(GL_TEXTURE_2D, 0, 7 * factor, 9 * factor, 2 * factor, 2 * factor,
                        GL_RGBA, GL_UNSIGNED_BYTE, pixels);
        free(pixels);
        portSettings.downsample = 1;
        glEnable(GL_BLEND);
        glEnable(GL_DEPTH_TEST);
        BlitPicture(0, 7 * factor, 9 * factor, 9 * factor, 11 * factor, output, 0, 0, 8, 8);
        assert(glIsEnabled(GL_BLEND) && glIsEnabled(GL_DEPTH_TEST));
        glBindFramebuffer(GL_READ_FRAMEBUFFER, output);
        uint8_t result[8 * 8 * 4];
        glReadPixels(0, 0, 8, 8, GL_RGBA, GL_UNSIGNED_BYTE, result);
        for (int py = 0; py < 8; py++) {
            for (int px = 0; px < 8; px++) {
                int at = (py * 8 + px) * 4;
                int red = px < 4 && py < 4 ? (int)lround(255.0 / (factor * factor)) : 0;
                assert(abs(result[at] - red) <= 1);
                assert(result[at + 1] == (px >= 4 ? 200 : 0));
                assert(result[at + 2] == (py >= 4 ? 160 : 0));
            }
        }
        /* Recorded pictures use the same reduction, with the original row order. */
        uint8_t *recorded = GpuReadPicture(7, 9, 2, 2, 0, 8, 8);
        assert(!memcmp(recorded, result, sizeof(result)));
        assert(glGetError() == GL_NO_ERROR);

        /* Widescreen uses a separate colour texture. The reduction and vertical
           flip for display must agree with the ordinary framebuffer. */
        SetWideMargin(16);
        glBindFramebuffer(GL_READ_FRAMEBUFFER, framebuffer);
        glBindTexture(GL_TEXTURE_2D, wideColour);
        glCopyTexSubImage2D(GL_TEXTURE_2D, 0, 7 * factor, 9 * factor, 7 * factor, 9 * factor,
                           2 * factor, 2 * factor);
        BlitPicture(1, 7 * factor, 9 * factor, 9 * factor, 11 * factor, output, 0, 8, 8, 0);
        glBindFramebuffer(GL_READ_FRAMEBUFFER, output);
        uint8_t flipped[sizeof(result)];
        glReadPixels(0, 0, 8, 8, GL_RGBA, GL_UNSIGNED_BYTE, flipped);
        for (int row = 0; row < 8; row++) {
            assert(!memcmp(flipped + row * 32, result + (7 - row) * 32, 32));
        }
        SetWideMargin(0);

        /* Box resolves non-dividing requests to an integer grid. Include
           3x requesting 2x, and targets above the rendering scale. */
        for (int targetScale = 1; targetScale <= factor + 1; targetScale++) {
            int effective = PortDownsampleTarget(factor, targetScale, DOWNSAMPLE_BOX), side = 2 * effective;
            portSettings.downsample = targetScale;
            BlitPicture(0, 7 * factor, 9 * factor, 9 * factor, 11 * factor, output, 0, 0, side, side);
            glBindFramebuffer(GL_READ_FRAMEBUFFER, output);
            glReadPixels(0, 0, side, side, GL_RGBA, GL_UNSIGNED_BYTE, result);
            for (int py = 0; py < side; py++) {
                for (int px = 0; px < side; px++) {
                    int at = (py * side + px) * 4;
                    double footprint = (double)factor / effective;
                    int red = px == 0 && py == 0 ? (int)lround(255.0 / (footprint * footprint)) : 0;
                    assert(abs(result[at] - red) <= 1);
                    assert(result[at + 1] == (px >= effective ? 200 : 0));
                    assert(result[at + 2] == (py >= effective ? 160 : 0));
                }
            }
            recorded = GpuReadPicture(7, 9, 2, 2, 0, side, side);
            assert(!memcmp(recorded, result, (size_t)side * side * 4));
            assert(glGetError() == GL_NO_ERROR);
        }

        portSettings.downsample = 0;
        BlitPicture(0, 7 * factor, 9 * factor, 9 * factor, 11 * factor, output, 0, 0, 8, 8);
        glBindFramebuffer(GL_READ_FRAMEBUFFER, output);
        glReadPixels(0, 0, 8, 8, GL_RGBA, GL_UNSIGNED_BYTE, result);
        uint8_t reference[sizeof(result)];
        glBindFramebuffer(GL_READ_FRAMEBUFFER, framebuffer);
        glBindFramebuffer(GL_DRAW_FRAMEBUFFER, output);
        glBlitFramebuffer(7 * factor, 9 * factor, 9 * factor, 11 * factor, 0, 0, 8, 8,
                          GL_COLOR_BUFFER_BIT, GL_LINEAR);
        glBindFramebuffer(GL_READ_FRAMEBUFFER, output);
        glReadPixels(0, 0, 8, 8, GL_RGBA, GL_UNSIGNED_BYTE, reference);
        assert(!memcmp(reference, result, sizeof(result)));
        assert(glGetError() == GL_NO_ERROR);
    }
    puts("Downsampling supports target grids, fractional reductions, clamping, sharp pixels, readbacks and off: OK");
    TestAdaptive(output, outputTexture);
    SDL_GL_DestroyContext(context);
    SDL_DestroyWindow(window);
    SDL_Quit();
}
