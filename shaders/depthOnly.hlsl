#include "NRICompatibility.hlsli"
NRI_RESOURCE(cbuffer, CommonConstants, b, 0, 0) {
    float4x4 modelMat;
    float4x4 viewMat;
    float4x4 projectMat;
    float4x4 viewProjMat;
    float4x4 lightVP[4];
    float4   splitDepth;
};

struct PushConstants {
    float4x4 worldMat;
    float4 camPos;
    float4 testVec;
    uint4 indexGroup;
};
NRI_ROOT_CONSTANTS(PushConstants, g_PushConstants, 1, 0);

struct inputVS {
    float3 in_position : POSITION0;
#ifdef ALPHA_TEST
    float2 in_texcoord : TEXCOORD0;
#endif
    uint startInstance : SV_StartInstanceLocation;
};

struct outputVS {
    float4 position : SV_Position;
    float4 testVS : TEXCOORD1;
#ifdef ALPHA_TEST
    float2 texCoord : TEXCOORD0;
#endif
};

struct InputPS {
    float4 position : SV_Position;
    float4 testVS : TEXCOORD1;
#ifdef ALPHA_TEST
    float2 uv : TEXCOORD0;
#endif
};

outputVS vs_main(inputVS input) {
    outputVS output;
    StructuredBuffer<float4x4> worldMatBuffer = ResourceDescriptorHeap[1006];
    float4x4 testMat = worldMatBuffer[input.startInstance];
    uint idx = g_PushConstants.indexGroup.z;
    float4x4 lightMVP = mul(lightVP[idx],testMat);
    // output.testVS = mul(g_PushConstants.worldMat, float4(0.0, 1.0, 1.0, 1.0));
    // float4x4 worldMat = g_PushConstants.worldMat;
    // float4x4 vpMat = mul(viewMat, worldMat);
    // float4x4 mvpMat = mul(projectMat, vpMat);
    // output.position = mul(mvpMat, float4(input.in_position.xyz, 1.0));
    output.position = mul(lightMVP, float4(input.in_position.xyz, 1.0));
#ifdef ALPHA_TEST
    output.texCoord = input.in_texcoord;
#endif
    return output;
}

void ps_main() {
    return ;
}

