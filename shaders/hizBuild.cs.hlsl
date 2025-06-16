#include "NRICompatibility.hlsli"

struct PushConstants {
    float2 DimensionsInv;
    uint texDepth;
    uint texHiZ;
    uint sampleIndex;
};
NRI_ROOT_CONSTANTS(PushConstants, g_PushConstants, 1, 0);

[numthreads(16, 16, 1)]
void main(uint3 threadID : SV_DispatchThreadID) {
    Texture2D<float> depthTexture = ResourceDescriptorHeap[g_PushConstants.texDepth];
    RWTexture2D<float> Hiz = ResourceDescriptorHeap[g_PushConstants.texHiZ];
    SamplerState sPointClamp = SamplerDescriptorHeap[g_PushConstants.sampleIndex];
    float2 uv = ((float2)threadID.xy + 0.5f) * g_PushConstants.DimensionsInv;
    float4 depths = depthTexture.Gather(sPointClamp, uv);
    float minDepth = min(min(min(depths.x, depths.y), depths.z), depths.w);
    Hiz[threadID.xy] = minDepth;
}