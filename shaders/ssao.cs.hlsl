#include "NRICompatibility.hlsli"

struct PushConstants {
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

float linearize_depth(float d,float zNear,float zFar) {
    return zNear * zFar / (zFar + d * (zNear - zFar));
}

Texture2D<float4> texture0 : register(t0); 
Texture2D<float4> texture1 : register(t1); 
Texture2D<float4> texture2 : register(t2); 

RWTexture2D<float4> storageTexture : register(u1);

SamplerState sampler0 : register(s0);

[numthreads(16, 16, 1)]
void main(uint3 DTid : SV_DispatchThreadID) {
    Texture2D<float> depthTexture = ResourceDescriptorHeap[g_PushConstants.texDepth];
    Texture2D<float3> rotationTexture = ResourceDescriptorHeap[g_PushConstants.texRotation];
    RWTexture2D<float4> outTexture = ResourceDescriptorHeap[g_PushConstants.texOut];
    SamplerState samplerState = SamplerDescriptorHeap[g_PushConstants.smpl];
    uint2 size;
    depthTexture.GetDimensions(size.x, size.y);

    uint2 xy = DTid.xy;
    float2 uv = float2(xy) / float2(size);

    if (xy.x >= size.x || xy.y >= size.y)
    return;

    float depth = depthTexture.SampleLevel(samplerState, uv, 0).x;
    outTexture[xy] = depth;
    return ;
    float Z = linearize_depth(1.0 - depth, g_PushConstants.zNear, g_PushConstants.zFar);

    float3 plane = rotationTexture.Sample(samplerState, float2(xy) / 4.0).xyz - 1.0;

    static const float3 offsets[8] = {
        float3(-0.5, -0.5, -0.5),
        float3(0.5, -0.5, -0.5),
        float3(-0.5,  0.5, -0.5),
        float3(0.5,  0.5, -0.5),
        float3(-0.5, -0.5,  0.5),
        float3(0.5, -0.5,  0.5),
        float3(-0.5,  0.5,  0.5),
        float3(0.5,  0.5,  0.5)
    };

    float att = 0.0;

    [unroll]
    for (int i = 0; i < 8; i++) {
        float3 rSample = reflect(offsets[i], plane);
        float newDepth = depthTexture.SampleLevel(samplerState, uv + g_PushConstants.radius * rSample.xy / Z, 0).x;
        float zSample = linearize_depth(1.0 - newDepth, g_PushConstants.zNear, g_PushConstants.zFar);
        float dist = max(zSample - Z, 0.0) / g_PushConstants.distScale;
        float occl = 15.0 * max(dist * (2.0 - dist), 0.0);
        att += 1.0 / (1.0 + occl * occl);
    }

    att = clamp(att * att / 64.0 + 0.45, 0.0, 1.0) * g_PushConstants.attScale;
    outTexture[xy] = float4(att, att, att, 1.0);
}