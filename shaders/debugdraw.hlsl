#include "NRICompatibility.hlsli"

struct PushConstants {
    float4x4 modelMat1;
    float4 camPos;
    float4 testVec;
    uint4 indexGroup;
};
NRI_ROOT_CONSTANTS(PushConstants, g_PushConstants, 1, 0);

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

outputVS vs_main(inputVS input) {
    outputVS output;
    float4x4 testMat = g_PushConstants.modelMat1;
    output.position = mul(testMat, float4(input.in_position.xyz, 1.0));
    output.color = input.in_color;
    return output;
}

float4 ps_main(InputPS input) : SV_TARGET {
    return input.color;
}