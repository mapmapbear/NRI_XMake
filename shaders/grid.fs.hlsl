#include "NRICompatibility.hlsli"

static const float gridSize = 100.0;

// size of one cell
static const float gridCellSize = 0.025;

// color of thin lines
static const float4 gridColorThin = float4(0.5, 0.5, 0.5, 1.0);

// color of thick lines (every tenth line)
static const float4 gridColorThick = float4(1.0, 1.0, 1.0, 1.0);

// minimum number of pixels between cell lines before LOD switch should occur. 
static const float gridMinPixelsBetweenCells = 2.0;

float log10(float x) {
    return log(x) / log(10.0);
}

float satf(float x) {
    return clamp(x, 0.0, 1.0);
}

float2 satv(float2 x) {
    return clamp(x, float2(0.0, 0.0), float2(1.0, 1.0));
}

float max2(float2 v) {
    return max(v.x, v.y);
}

float4 gridColor(float2 uv, float2 camPos) {
    float2 dudv = float2(
        length(float2(ddx(uv.x), ddy(uv.x))),
        length(float2(ddx(uv.y), ddy(uv.y)))
    );

    float lodLevel = max(0.0, log10((length(dudv) * gridMinPixelsBetweenCells) / gridCellSize) + 1.0);
    float lodFade = frac(lodLevel);

    // cell sizes for lod0, lod1 and lod2
    float lod0 = gridCellSize * pow(10.0, floor(lodLevel));
    float lod1 = lod0 * 10.0;
    float lod2 = lod1 * 10.0;

    // each anti-aliased line covers up to 4 pixels
    dudv *= 4.0;

    // set grid coordinates to the centers of anti-aliased lines for subsequent alpha calculations
    uv += dudv * 0.5;

    // calculate absolute distances to cell line centers for each lod and pick max X/Y to get coverage alpha value
    float lod0a = max2(float2(1.0, 1.0) - abs(satv(fmod(uv, lod0) / dudv) * 2.0 - float2(1.0, 1.0)));
    float lod1a = max2(float2(1.0, 1.0) - abs(satv(fmod(uv, lod1) / dudv) * 2.0 - float2(1.0, 1.0)));
    float lod2a = max2(float2(1.0, 1.0) - abs(satv(fmod(uv, lod2) / dudv) * 2.0 - float2(1.0, 1.0)));

    uv -= camPos;

    // blend between falloff colors to handle LOD transition
    float4 c = lod2a > 0.0 ? gridColorThick : lod1a > 0.0 ? lerp(gridColorThick, gridColorThin, lodFade) : gridColorThin;

    // calculate opacity falloff based on distance to grid extents
    float opacityFalloff = (1.0 - satf(length(uv) / gridSize));

    // blend between LOD level alphas and scale with opacity falloff
    c.a *= (lod2a > 0.0 ? lod2a : lod1a > 0.0 ? lod1a : (lod0a * (1.0 - lodFade))) * opacityFalloff;

    return c;
}

// struct PushConstants
// {
//     float4x4 mvpMat;
//     float4 cameraPos;
//     float4 origin;
// };
// NRI_ROOT_CONSTANTS( PushConstants, g_PushConstants, 0, 0 ); 
 

struct inputPS
{
    float4 posistionCS : SV_Position;
    float2 uv : TEXCOORD0;
    float2 cameraPos : TEXCOORD1;
};

float4 main(inputPS input) : SV_Target
{
    float2 newUV = input.uv;
    newUV.x = 1.0 - newUV.x;
    newUV.y = 1.0 - newUV.y;
    return gridColor(newUV, input.cameraPos);
}