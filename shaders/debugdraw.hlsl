#include "NRICompatibility.hlsli"

struct PushConstants {
    float4x4 VPMat;
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
    uint instanceID : SV_InstanceID;
};

struct outputVS {
    float4 position : SV_Position;
    float4 color : COLOR;
};

struct InputPS {
    float4 position : SV_Position;
    float4 color : COLOR;
};

struct BoxMesh {
    float4x4 worldMat;
};

outputVS vs_main(inputVS input) {
    outputVS output;
    StructuredBuffer<BoxMesh> BoxMats = ResourceDescriptorHeap[1012];
    float4x4 VPMat = mul(projectMat, viewMat);
    float4x4 worldMat = mul(g_PushConstants.VPMat, BoxMats[g_PushConstants.indexGroup.x + input.instanceID].worldMat);
    output.position = mul(worldMat, float4(input.in_position.xyz, 1.0));
    // float4 posWS = mul(VPMat, float4(input.in_position.xyz, 1.0));
    // posWS = mul(BoxMats[g_PushConstants.indexGroup.x + input.instanceID].worldMat, posWS);
    // output.position = mul(VPMat, posWS);
    output.color = mul(BoxMats[g_PushConstants.indexGroup.x + input.instanceID].worldMat, input.in_color);
    if(g_PushConstants.indexGroup.y > 0.1) {
        float4 posOS = mul(g_PushConstants.VPMat, float4(input.in_position.xyz, 1.0));
        float4 posWS = mul(BoxMats[g_PushConstants.indexGroup.x + input.instanceID].worldMat, posOS);
        output.position = mul(VPMat, posWS);
    }
    return output;
}

float4 ps_main(InputPS input) : SV_TARGET {
    return input.color;
}