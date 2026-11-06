#include <metal_stdlib>
using namespace metal;
struct Uniforms {
    float targetX, targetY, originX, originY, depthPull;
    int pass, depthPass, left, top, right, bottom, factor, targetScale, adaptive;
    float stepSize, uiScaleX, uiScaleY, uiOffsetX, uiOffsetY;
    float rasterScale;
    int window;
    float surfaceHeight;
};
struct GameIn {
    float3 position [[attribute(0)]];
    float4 colour [[attribute(1)]];
    float2 texel [[attribute(2)]];
    int4 page [[attribute(3)]];
    uint4 window [[attribute(4)]];
    uint4 mode [[attribute(5)]];
    uint4 hd [[attribute(6)]];
    int4 hdTexels [[attribute(7)]];
};
struct GameOut {
    float4 position [[position]];
    float4 vColour;
    float2 vTexel;
    int semiMode [[flat]];
    int hudColourOverlay [[flat]];
    int4 texturePage [[flat]];
    int4 clut [[flat]];
    int4 vWindow [[flat]];
    int4 mode [[flat]];
    uint4 hd [[flat]];
    int4 hdTexels [[flat]];
};
vertex GameOut gameVertex(GameIn a [[stage_in]], constant Uniforms &u [[buffer(1)]])
{
    GameOut o;
    float w = a.position.z != 0.0 ? abs(a.position.z) : 1.0;
    float2 ndc = (a.position.xy - float2(u.originX, u.originY)) / float2(u.targetX, u.targetY) * 2.0 - 1.0;
    float pull = u.depthPass == 1 || (a.mode.w & 1u) == 1u ? 1.0 : u.depthPull;
    float z = a.position.z > 0.0 ? w - pull : (u.depthPass == 1 ? w : 0.0);
    o.position = float4(ndc.x * w, (u.window ? ndc.y : -ndc.y) * w, z, w);
    o.vColour = a.colour;
    o.vTexel = a.texel;
    o.texturePage = int4(a.page.xy, 0, 0);
    o.clut = int4(a.page.zw, 0, 0);
    o.semiMode = int(a.mode.w >> 4u) - 1;
    o.hudColourOverlay = int((a.mode.w >> 1u) & 1u);
    o.vWindow = int4(a.window);
    o.mode = int4(int(a.mode.x), int(a.mode.y), u.pass, int(a.mode.z));
    o.hd = a.hd;
    o.hdTexels = a.hdTexels;
    return o;
}
uint Fetch(texture2d<uint> source, int2 p)
{
    return source.read(uint2(p & 1023)).r;
}
float4 Output(float3 col, float cov, bool blended, GameOut in)
{
    float4 result;
    float4 vColour = in.vColour;
    int4 mode = in.mode;
    int semiMode = in.semiMode;
    int hudColourOverlay = in.hudColourOverlay;

    if (semiMode == 4) blended = false;
    float fade = vColour.a;
    if (mode.z == 3) {
        if (blended || fade < 0.999 || cov < 0.999)
            discard_fragment();
        result = float4(0.0);
        return result;
    }
    if (mode.z == 1 && blended)
        discard_fragment();
    if (mode.z == 2 && !blended)
        discard_fragment();
    col *= fade;
    cov *= fade;
    if (!blended)
        result = float4(col, cov);
    else if (hudColourOverlay != 0 && semiMode == 1) {
        float coverage = clamp(max(col.r, max(col.g, col.b)) * 2.0, 0.0, 1.0);
        result = float4(col, 0.35 * coverage * cov);
    } else if (semiMode == 0)
        result = float4(col * 0.5, cov * 0.5);
    else if (semiMode == 3)
        result = float4(col * 0.25, 0.0);
    else
        result = float4(col, 0.0);

    return result;
}
fragment float4 gameFragment(GameOut in [[stage_in]], constant Uniforms &u [[buffer(1)]], texture2d<uint> vram [[texture(0)]],
                             texture2d<float> glyphs [[texture(1)]], texture2d<float> hdAtlas [[texture(2)]],
                             texture2d<float> portPages [[texture(3)]],
                             texture2d_array<uint> decoded [[texture(4)]],
                             sampler glyphSampler [[sampler(1)]], sampler portSampler [[sampler(3)]])
{
    float4 vColour = in.vColour;
    float2 vTexel = in.vTexel;
    int semiMode = in.semiMode;
    int4 texturePage = in.texturePage, clut = in.clut, vWindow = in.vWindow, mode = in.mode;
    uint4 hd = in.hd;
    int4 hdTexels = in.hdTexels;

    if (semiMode == 4) {
        int2 pixel = int2(floor(in.position.xy / u.rasterScale));
        if (((pixel.x + pixel.y) & 1) != 0) discard_fragment();
    }
    float3 c = vColour.rgb;
    if (mode.x == 0) {
        return Output(c, 1.0, semiMode >= 0, in);
    }
    float2 t = vTexel.xy;
    int portPage = texturePage.x / 64 + 16 * ((texturePage.y / 256) & 1) + (texturePage.y >= 512 ? 32 : 0);
    if (mode.w == 3 && portPage > 0) {
        int k = portPage - 1;
        float2 low = clamp(float2(hdTexels.xy) + 0.5, 0.5, 255.5);
        float2 high = clamp(max(low, float2(hdTexels.zw) - 0.5), 0.5, 255.5);
        float2 at = float2(float(k % 8), float(k / 8)) * 256.0 + clamp(t, low, high);
        float4 g = portPages.sample(portSampler, at / float2(2048.0, 2048.0));
        if (g.a < 0.004)
            discard_fragment();
        return Output(mode.y == 0 ? g.rgb * c : g.rgb, g.a, semiMode >= 0, in);
    }
    if (mode.w == 3) {
        float4 g = glyphs.sample(glyphSampler, t / 256.0);
        if (g.a < 0.004)
            discard_fragment();
        float3 rgb = mode.y == 0 ? min(g.rgb * c * (255.0 / 128.0), float3(g.a)) : g.rgb;
        return Output(rgb, g.a, semiMode >= 0, in);
    }
    int2 uv = int2(floor(t)) & 255;
    uv.x = (uv.x & ~(vWindow.x * 8)) | ((vWindow.z & vWindow.x) * 8);
    uv.y = (uv.y & ~(vWindow.y * 8)) | ((vWindow.w & vWindow.y) * 8);
    if (hd.z > 0u) {
        float2 into = float2(uv - hdTexels.xy) + (t - floor(t));
        if (all(into >= float2(0.0)) && all(into < float2(hdTexels.zw))) {
            int2 at = min(int2(into * float2(hd.zw) / float2(hdTexels.zw)), int2(hd.zw) - 1);
            float4 h = hdAtlas.read(uint2(int2(hd.xy) + at));
            if (h.a < 0.25)
                discard_fragment();
            bool hdSemi = h.a < 0.75;
            float3 hc = mode.y == 0 ? min(h.rgb * c * (255.0 / 128.0), float3(1.0)) : h.rgb;
            return Output(hc, 1.0, semiMode >= 0 && hdSemi, in);
        }
    }
    uint texel;
    if (mode.x == 2) {
        texel = decoded.read(uint2(uv), uint(texturePage.x)).r;
    } else if (mode.w == 0) {
        uint word = Fetch(vram, texturePage.xy + int2(uv.x >> 2, uv.y));
        uint index = (word >> uint((uv.x & 3) * 4)) & 15u;
        texel = Fetch(vram, clut.xy + int2(int(index), 0));
    } else if (mode.w == 1) {
        uint word = Fetch(vram, texturePage.xy + int2(uv.x >> 1, uv.y));
        uint index = (word >> uint((uv.x & 1) * 8)) & 255u;
        texel = Fetch(vram, clut.xy + int2(int(index), 0));
    } else {
        texel = Fetch(vram, texturePage.xy + uv);
    }
    if (texel == 0u)
        discard_fragment();
    bool semi = (texel & 0x8000u) != 0u;
    float3 t8 =
        float3(float(texel & 31u), float((texel >> 5) & 31u), float((texel >> 10) & 31u)) * 8.0 / 255.0;
    if (mode.y == 0) {
        t8 = min(t8 * c * (255.0 / 128.0), float3(1.0));
    }
    return Output(t8, 1.0, semiMode >= 0 && semi, in);
}
struct PictureOut {
    float4 position [[position]];
};
vertex PictureOut pictureVertex(uint id [[vertex_id]], constant Uniforms &u [[buffer(1)]])
{
    float2 p = float2((id << 1) & 2, id & 2);
    float2 ndc = p * 2.0 - 1.0;
    return {float4(ndc.x, u.window ? ndc.y : -ndc.y, 0.0, 1.0)};
}
float2 LogicalPixel(float4 pos, constant Uniforms &u)
{
    return float2(pos.x, u.window ? u.surfaceHeight - pos.y : pos.y);
}
fragment float4 averageFragment(PictureOut in [[stage_in]], constant Uniforms &u [[buffer(1)]],
                                texture2d<float> source [[texture(0)]], sampler linearSampler [[sampler(0)]])
{
    float step = float(u.factor) / float(u.targetScale);
    float2 lo = float2(u.left, u.top) + floor(LogicalPixel(in.position, u)) * step;
    float2 hi = min(lo + step, float2(u.right, u.bottom));
    float4 sum = 0.0;
    float3 squares = 0.0;
    if (u.adaptive == 0 && u.factor % (u.targetScale * 2) == 0 && (u.right - u.left) % 2 == 0 &&
        (u.bottom - u.top) % 2 == 0) {
        float2 size = float2(source.get_width(), source.get_height());
        for (int y = int(lo.y); y < int(hi.y); y += 2)
            for (int x = int(lo.x); x < int(hi.x); x += 2)
                sum += source.sample(linearSampler, (float2(x, y) + 1.0) / size, level(0.0)) * 4.0;
    } else {
        for (int y = int(floor(lo.y)); y < int(ceil(hi.y)); y++)
            for (int x = int(floor(lo.x)); x < int(ceil(hi.x)); x++) {
                float2 cell = float2(x, y);
                float2 covered = max(min(hi, cell + 1.0) - max(lo, cell), float2(0.0));
                float weight = covered.x * covered.y;
                float4 v = source.read(uint2(x, y));
                sum += v * weight;
                if (u.adaptive)
                    squares += v.rgb * v.rgb * weight;
            }
    }
    float area = (hi.x - lo.x) * (hi.y - lo.y);
    float4 colour = sum / area;
    if (u.adaptive) {
        float3 variance = max(squares / area - colour.rgb * colour.rgb, float3(0.0));
        float deviation = sqrt(max(variance.r, max(variance.g, variance.b)));
        colour.a = smoothstep(0.5 / 255.0, 4.0 / 255.0, deviation);
    }
    return colour;
}
fragment float4 detailFragment(PictureOut in [[stage_in]], constant Uniforms &u [[buffer(1)]],
                               texture2d<float> source [[texture(0)]])
{
    int2 at = int2(LogicalPixel(in.position, u)), last = int2(source.get_width(), source.get_height()) - 1;
    float detail = 0.0;
    for (int y = -1; y <= 1; y++)
        for (int x = -1; x <= 1; x++)
            detail = max(detail, source.read(uint2(clamp(at + int2(x, y), int2(0), last))).a);
    return float4(source.read(uint2(at)).rgb, detail);
}
fragment float4 compositeFragment(PictureOut in [[stage_in]], constant Uniforms &u [[buffer(1)]],
                                  texture2d<float> source [[texture(0)]],
                                  texture2d<float> coarse [[texture(1)]],
                                  sampler linearSampler [[sampler(1)]])
{
    float2 p = LogicalPixel(in.position, u), size = float2(coarse.get_width(), coarse.get_height());
    float2 uv = clamp(p / u.stepSize, float2(0.5), size - 0.5) / size;
    float4 low = coarse.sample(linearSampler, uv);
    float3 high = source.read(uint2(int2(u.left, u.top) + int2(p))).rgb;
    return float4(mix(low.rgb, high, low.a), 1.0);
}
struct UiIn {
    float2 position [[attribute(0)]];
    float4 colour [[attribute(1)]];
    float2 uv [[attribute(2)]];
};
struct UiOut {
    float4 position [[position]];
    float4 colour;
    float2 uv;
};
vertex UiOut uiVertex(UiIn a [[stage_in]], constant Uniforms &u [[buffer(1)]])
{
    float2 p = a.position * float2(u.uiScaleX, u.uiScaleY) + float2(u.uiOffsetX, u.uiOffsetY);
    return {float4(p.x, u.window ? p.y : -p.y, 0.0, 1.0), a.colour, a.uv};
}
fragment float4 uiFragment(UiOut in [[stage_in]], texture2d<float> source [[texture(0)]],
                           sampler sam [[sampler(0)]])
{
    float4 c = source.sample(sam, in.uv) * in.colour;
    return float4(c.rgb * c.a, c.a);
}
struct CopyUniforms {
    float fromX, fromY, fromW, fromH, toX, toY, toW, toH, window, height;
};
fragment float4 copyFragment(PictureOut in [[stage_in]], constant CopyUniforms &c [[buffer(2)]],
                             texture2d<float> source [[texture(0)]], sampler sam [[sampler(0)]])
{
    float2 p = float2(in.position.x, c.window ? c.height - in.position.y : in.position.y);
    float2 uv = (float2(c.fromX, c.fromY) +
                 (p - float2(c.toX, c.toY)) / float2(c.toW, c.toH) * float2(c.fromW, c.fromH)) /
                float2(source.get_width(), source.get_height());
    return source.sample(sam, uv, level(0.0));
}
fragment float4 clearFragment(constant float4 &colour [[buffer(2)]])
{
    return colour;
}
struct ClearDepth {
    float4 colour [[color(0)]];
    float depth [[depth(any)]];
};
fragment ClearDepth clearDepthFragment(constant float4 &colour [[buffer(2)]])
{
    return {colour, 1.0};
}
