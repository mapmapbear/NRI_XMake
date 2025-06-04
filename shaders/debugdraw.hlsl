#include "NRICompatibility.hlsli"

struct PushConstants {
    float4x4 modelMat1;
    float4 camPos;
    float4 testVec;
    uint4 indexGroup;
};
NRI_ROOT_CONSTANTS(PushConstants, g_PushConstants, 1, 0);

NRI_RESOURCE(cbuffer, CommonConstants, b, 0, 0) {
    float4x4 modelMat;
    float4x4 viewMat;
    float4x4 projectMat;
    float4x4 lightVP;
};

struct inputVS {
    float3 in_position : POSITION;
    float4 in_color : COLOR;
};

struct outputVS {
    float4 position : SV_Position;
    float4 color : COLOR;
};

struct InputPS {
    float4 position : SV_Position;
    float4 color : COLOR;
};

struct BoxMesh
{
    float4x4 worldMat;
};

outputVS vs_main(inputVS input) {
    outputVS output;
    StructuredBuffer<BoxMesh> BoxMats = ResourceDescriptorHeap[0];
    float4x4 testMat = mul(BoxMats[0].worldMat, g_PushConstants.modelMat1);
    output.position = mul(testMat, float4(input.in_position.xyz, 1.0));
    output.color = input.in_color;
    return output;
}

float4 ps_main(InputPS input) : SV_TARGET {
    return input.color;
}