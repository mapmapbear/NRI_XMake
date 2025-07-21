// © 2021 NVIDIA Corporation

#include "NRICompatibility.hlsli"
NRI_RESOURCE(cbuffer, CommonConstants, b, 0, 0) {
    float4x4 modelMat;
    float4x4 viewMat;
    float4x4 projectMat;
    float4x4 lightVP[4];
};

struct PushConstants {
    float4x4 modelMat;
    float4 camPos;
    float4 testVec;
    uint4 indexGroup;
};
NRI_ROOT_CONSTANTS(PushConstants, g_PushConstants, 1, 0);

struct inputVS {
#ifdef DEPTH_ONLY
    float3 in_position : POSITION0;
#else
    float3 in_position : POSITION0;
    float2 in_texcoord : TEXCOORD0;
    float3 in_normal   : NORMAL;
    float4 in_tangent  : TANGENT;
    uint instanceID    : SV_InstanceID;
#endif
    uint startInstance : SV_StartInstanceLocation;
};

struct outputVS {
#ifdef DEPTH_ONLY
    float4 position   :  SV_Position;
#else
    float4 position   : SV_Position;
    float2 texCoord   : TEXCOORD0;
    float3 positionWS : TEXCOORD1;
    float4 positionLS : TEXCOORD2;
    float4 positionLS1 : TEXCOORD3;
    nointerpolation uint4  matrialData : TEXCOORD4;
    float3 normalWS   : NORMAL;
    float4 tangentWS  : TANGENT;
    float3 color      : COLOR;
#endif
};

float4x4 inverse(float4x4 m) {
    float n11 = m[0][0], n12 = m[1][0], n13 = m[2][0], n14 = m[3][0];
    float n21 = m[0][1], n22 = m[1][1], n23 = m[2][1], n24 = m[3][1];
    float n31 = m[0][2], n32 = m[1][2], n33 = m[2][2], n34 = m[3][2];
    float n41 = m[0][3], n42 = m[1][3], n43 = m[2][3], n44 = m[3][3];

    float t11 = n23 * n34 * n42 - n24 * n33 * n42 + n24 * n32 * n43 - n22 * n34 * n43 - n23 * n32 * n44 + n22 * n33 * n44;
    float t12 = n14 * n33 * n42 - n13 * n34 * n42 - n14 * n32 * n43 + n12 * n34 * n43 + n13 * n32 * n44 - n12 * n33 * n44;
    float t13 = n13 * n24 * n42 - n14 * n23 * n42 + n14 * n22 * n43 - n12 * n24 * n43 - n13 * n22 * n44 + n12 * n23 * n44;
    float t14 = n14 * n23 * n32 - n13 * n24 * n32 - n14 * n22 * n33 + n12 * n24 * n33 + n13 * n22 * n34 - n12 * n23 * n34;

    float det = n11 * t11 + n21 * t12 + n31 * t13 + n41 * t14;
    float idet = 1.0f / det;

    float4x4 ret;

    ret[0][0] = t11 * idet;
    ret[0][1] = (n24 * n33 * n41 - n23 * n34 * n41 - n24 * n31 * n43 + n21 * n34 * n43 + n23 * n31 * n44 - n21 * n33 * n44) * idet;
    ret[0][2] = (n22 * n34 * n41 - n24 * n32 * n41 + n24 * n31 * n42 - n21 * n34 * n42 - n22 * n31 * n44 + n21 * n32 * n44) * idet;
    ret[0][3] = (n23 * n32 * n41 - n22 * n33 * n41 - n23 * n31 * n42 + n21 * n33 * n42 + n22 * n31 * n43 - n21 * n32 * n43) * idet;

    ret[1][0] = t12 * idet;
    ret[1][1] = (n13 * n34 * n41 - n14 * n33 * n41 + n14 * n31 * n43 - n11 * n34 * n43 - n13 * n31 * n44 + n11 * n33 * n44) * idet;
    ret[1][2] = (n14 * n32 * n41 - n12 * n34 * n41 - n14 * n31 * n42 + n11 * n34 * n42 + n12 * n31 * n44 - n11 * n32 * n44) * idet;
    ret[1][3] = (n12 * n33 * n41 - n13 * n32 * n41 + n13 * n31 * n42 - n11 * n33 * n42 - n12 * n31 * n43 + n11 * n32 * n43) * idet;

    ret[2][0] = t13 * idet;
    ret[2][1] = (n14 * n23 * n41 - n13 * n24 * n41 - n14 * n21 * n43 + n11 * n24 * n43 + n13 * n21 * n44 - n11 * n23 * n44) * idet;
    ret[2][2] = (n12 * n24 * n41 - n14 * n22 * n41 + n14 * n21 * n42 - n11 * n24 * n42 - n12 * n21 * n44 + n11 * n22 * n44) * idet;
    ret[2][3] = (n13 * n22 * n41 - n12 * n23 * n41 - n13 * n21 * n42 + n11 * n23 * n42 + n12 * n21 * n43 - n11 * n22 * n43) * idet;

    ret[3][0] = t14 * idet;
    ret[3][1] = (n13 * n24 * n31 - n14 * n23 * n31 + n14 * n21 * n33 - n11 * n24 * n33 - n13 * n21 * n34 + n11 * n23 * n34) * idet;
    ret[3][2] = (n14 * n22 * n31 - n12 * n24 * n31 - n14 * n21 * n32 + n11 * n24 * n32 + n12 * n21 * n34 - n11 * n22 * n34) * idet;
    ret[3][3] = (n12 * n23 * n31 - n13 * n22 * n31 + n13 * n21 * n32 - n11 * n23 * n32 - n12 * n21 * n33 + n11 * n22 * n33) * idet;

    return ret;
}

float hash(uint seed) {
    seed = (seed ^ 61u) ^ (seed >> 16u);
    seed *= 9u;
    seed = seed ^ (seed >> 4u);
    seed *= 0x27d4eb2du;
    seed = seed ^ (seed >> 15u);
    return float(seed) * (1.0 / 4294967295.0);
}

float3 hsv2rgb(float3 c) {
    float4 K = float4(1.0, 2.0 / 3.0, 1.0 / 3.0, 3.0);
    float3 p = abs(frac(c.xxx + K.xyz) * 6.0 - K.www);
    return c.z * lerp(K.xxx, clamp(p - K.xxx, 0.0, 1.0), c.y);
}

outputVS main(inputVS input) {
    outputVS output;
    float4x4 testMat = 1.0;
    StructuredBuffer<float4x4> worldMatBuffer = ResourceDescriptorHeap[1006];
    testMat = worldMatBuffer[input.startInstance];
    float4x4 vpMat = mul(viewMat, testMat);
    float4x4 mvpMat = mul(projectMat, vpMat);
#ifdef DEPTH_ONLY
    output.position = mul(mvpMat, float4(input.in_position.xyz, 1.0));
#else
    output.position = mul(mvpMat, float4(input.in_position.xyz, 1.0));
    output.texCoord = input.in_texcoord;
    float4x4 normalMatrix = transpose(inverse(testMat));
    output.normalWS  = normalize(mul(normalMatrix, float4(input.in_normal, 0.0)).xyz);
    output.positionWS = mul(testMat, float4(input.in_position, 1.0)).xyz; 
    output.positionLS = mul(lightVP[g_PushConstants.indexGroup.z], float4(output.positionWS, 1.0));
    output.tangentWS = normalize(mul(normalMatrix, float4(input.in_tangent)));
    output.matrialData = uint4(input.startInstance, 0, 0, 0);
    float h = hash(input.startInstance);
    float s = 0.8;
    float v = 0.95;
    output.color = hsv2rgb(float3(h, s, v));
#endif
    return output;
}
