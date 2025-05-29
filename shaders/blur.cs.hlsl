#include "NRICompatibility.hlsli"

static const bool kIsHorizontal = true;

struct PushConstants {
    uint texDepth;
    uint texIn;
    uint texOut;
    uint smpl;
    float depthThreshold;
};
NRI_ROOT_CONSTANTS(PushConstants, g_PushConstants, 1, 0);

static const int kFilterSize = 17;

static const float gaussWeights[kFilterSize] = {
    0.00001525878906f,
    0.0002441406250f,
    0.001831054688f,
    0.008544921875f,
    0.02777099609f,
    0.06665039063f,
    0.1221923828f,
    0.1745605469f,
    0.1963806152f,
    0.1745605469f,
    0.1221923828f,
    0.06665039063f,
    0.02777099609f,
    0.008544921875f,
    0.001831054688f,
    0.0002441406250f,
    0.00001525878906f
};

[numthreads(16, 16, 1)]
void main(uint2 DTid : SV_DispatchThreadID) {
    Texture2D<float> depthTexture = ResourceDescriptorHeap[g_PushConstants.texDepth];
    Texture2D<float4> colorTexture = ResourceDescriptorHeap[g_PushConstants.texIn];
    RWTexture2D<float4> outTexture = ResourceDescriptorHeap[g_PushConstants.texOut];
    SamplerState samplerState = SamplerDescriptorHeap[g_PushConstants.smpl];

    uint width, height;
    colorTexture.GetDimensions(width, height);
    const float2 size = float2(width, height);
    const float2 xy_float = float2(DTid.xy);
    if (DTid.x >= (uint)size.x || DTid.y >= (uint)size.y) {
        return;
    }

    const float2 texCoord = (float2(DTid.xy) + float2(0.5f, 0.5f)) / size;
    const float texScaler = 1.0f / (kIsHorizontal ? size.x : size.y);

    float3 c = float3(0.0f, 0.0f, 0.0f);

    float3 fragColor = colorTexture.SampleLevel(samplerState, texCoord, 0).rgb;
    float  fragDepth = depthTexture.SampleLevel(samplerState, texCoord, 0).r;

    for (int i = 0; i < kFilterSize; ++i) {
        float offset = float(i - kFilterSize / 2);
        float2 uv_offset = texCoord + texScaler * (kIsHorizontal ? float2(offset, 0.0f) : float2(0.0f, offset));
        float3 color = colorTexture.SampleLevel(samplerState, uv_offset, 0).rgb;
        float  depth = depthTexture.SampleLevel(samplerState, uv_offset, 0).r;

        float weight = clamp(abs(depth - fragDepth) * g_PushConstants.depthThreshold, 0.0f, 1.0f);
        c += lerp(color, fragColor, weight) * gaussWeights[i];
    }

    outTexture[DTid.xy] = float4(c, 1.0f);
}