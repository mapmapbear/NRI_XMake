#include "NRICompatibility.hlsli"

struct PushConstants
{
    uint texDepth;
    uint texRotation;
    uint texOut;
    uint smpl;
    float zNear;
    float zFar;
    float radius;
    float attScale;
    float distScale;
};
NRI_ROOT_CONSTANTS(PushConstants, g_PushConstants, 1, 0);

[numthreads(16, 16, 1)]
void main(uint3 DTid : SV_DispatchThreadID)
{
    Texture2D<float> depthTexture = ResourceDescriptorHeap[g_PushConstants.texDepth];
    Texture2D<float3> rotationTexture = ResourceDescriptorHeap[g_PushConstants.texRotation];
    RWTexture2D<float4> outTexture = ResourceDescriptorHeap[g_PushConstants.texOut];
    SamplerState samplerState = SamplerDescriptorHeap[g_PushConstants.smpl];

    uint2 size;
    depthTexture.GetDimensions(size.x, size.y);

    uint2 xy = DTid.xy;
    float2 uv = float2(xy) + 0.5 / float2(size);

    if (xy.x >= size.x || xy.y >= size.y)
        return;

    // 反转深度线性化
    float depth = depthTexture.SampleLevel(samplerState, uv, 0).x;
    float Z = (g_PushConstants.zNear * g_PushConstants.zFar) / (g_PushConstants.zFar + depth * (g_PushConstants.zNear - g_PushConstants.zFar));

    float3 plane = rotationTexture.SampleLevel(samplerState, float2(xy) / 4.0, 0).xyz - 1.0;

    static const float3 offsets[8] = {
        float3(-0.5, -0.5, -0.5),
        float3( 0.5, -0.5, -0.5),
        float3(-0.5,  0.5, -0.5),
        float3( 0.5,  0.5, -0.5),
        float3(-0.5, -0.5,  0.5),
        float3( 0.5, -0.5,  0.5),
        float3(-0.5,  0.5,  0.5),
        float3( 0.5,  0.5,  0.5)
    };

    float att = 0.0;

    // SSAO采样循环
    [unroll]
    for (int i = 0; i < 8; i++)
    {
        float3 rSample = reflect(offsets[i], plane);
        // 反转深度线性化
        float zSample = (g_PushConstants.zNear * g_PushConstants.zFar) / (g_PushConstants.zFar + depthTexture.SampleLevel(samplerState, uv + g_PushConstants.radius * rSample.xy / Z, 0).x * (g_PushConstants.zNear - g_PushConstants.zFar));
        float dist = max(zSample - Z, 0.0) / g_PushConstants.distScale;
        float occl = 15.0 * max(dist * (2.0 - dist), 0.0);
        att += 1.0 / (1.0 + occl * occl);
    }

    // 后处理并写入结果
    att = clamp(att * att / 64.0 + 0.45, 0.0, 1.0) * g_PushConstants.attScale;
    outTexture[xy] = float4(att, att, att, 1.0);
}