#include "NRICompatibility.hlsli"
#include "pbrCommon.hlsli"
#include "shadowCommon.hlsl"

struct InputPS {
    float4 position   : SV_Position;
    float2 uv         : TEXCOORD0;
    float3 positionWS : TEXCOORD1;
    float4 positionLS : TEXCOORD2;
    float4 positionLS1 : TEXCOORD3;
    uint4  matrialData : TEXCOORD4;
    float3 normalWS   : NORMAL;
    float4 tangentWS  : TANGENT;
    float3 color      : COLOR;
};

NRI_RESOURCE(cbuffer, CommonConstants, b, 0, 0) {
    float4x4 modelMat;
    float4x4 viewMat;
    float4x4 projectMat;
    float4x4 lightVP[4];
    float4   splitDepth;
};

struct PushConstants {
    float4x4 modelMat;
    float4 camPos;
    float4 testVec;
    float4 baseColor;
    float4 pbrParams;
    uint4 indexGroup;
};
NRI_ROOT_CONSTANTS(PushConstants, g_PushConstants, 1, 0);

struct MaterialBlock {
    uint textureBase;
    uint textureNormal;
    uint textureMetallic;
    uint textureIndex3;
};

struct ObjectIndexBlock {
    uint materialIndex;
};

int GetCascadeIndex(float depth, float4 splitDepth) {
    int cascadeIndex = 0;
    if (depth > splitDepth.x) cascadeIndex = 1;
    if (depth > splitDepth.y) cascadeIndex = 2;
    if (depth > splitDepth.z) cascadeIndex = 3;
    return cascadeIndex;
}

float SampleCascadeShadow(float3 worldPos, float4 splitDepth) {
    float viewDepth = length(g_PushConstants.camPos.xyz - worldPos);
    int cascadeIndex = GetCascadeIndex(viewDepth, splitDepth);

    float4 lightSpacePos = mul(lightVP[cascadeIndex], float4(worldPos, 1.0));
    float3 projCoords = lightSpacePos.xyz / lightSpacePos.w;

    projCoords.x = projCoords.x * 0.5 + 0.5;
    projCoords.y = projCoords.y * -0.5 + 0.5;

    float2 atlasOffset = float2(cascadeIndex % 2, cascadeIndex / 2) * 0.5;
    float2 atlasUV = projCoords.xy * 0.5 + atlasOffset;

    Texture2D<float> shadowMap = ResourceDescriptorHeap[13];
    SamplerComparisonState shadowSampler = SamplerDescriptorHeap[3];

    return shadowSampleCmp(shadowSampler, shadowMap, atlasUV, projCoords.z + 0.005, 0);
}

[earlydepthstencil]
float4 main(InputPS input) : SV_Target {
    float2 newUV = input.uv;
    float4 color = 0.0;
    float3 n = input.normalWS;
    float3 v = normalize(g_PushConstants.camPos.xyz - input.positionWS);
    StructuredBuffer<MaterialBlock> material = ResourceDescriptorHeap[14];
    StructuredBuffer<ObjectIndexBlock> object = ResourceDescriptorHeap[15];
    uint materialIndex = object[input.matrialData.x].materialIndex;
    MaterialBlock materialData = material[materialIndex];

    Texture2D g_AlbedoTexture = ResourceDescriptorHeap[materialData.textureBase];
    Texture2D g_NormalTexture = ResourceDescriptorHeap[materialData.textureNormal];
    Texture2D g_MetallicTexture = ResourceDescriptorHeap[materialData.textureMetallic];
    SamplerState g_Sampler = SamplerDescriptorHeap[0];
    float4 baseColor = g_AlbedoTexture.Sample(g_Sampler, newUV);
    float4 normalTS = g_NormalTexture.Sample(g_Sampler, newUV);
    normalTS.z = 1.0;
    // normalTS.y = -normalTS.y;
    normalTS = normalTS * 2.0 - 1.0;
    normalTS = normalize(normalTS);
    float4 tangent = input.tangentWS;
    float3 normal = n;

    float3 bitangent = cross(normal, tangent.xyz) * tangent.w;
    float3 worldNormal = normalize(normalTS.x * tangent.xyz + normalTS.y * bitangent + normalTS.z * normal);

    float3 ligDir = float3(0.2, 100.0, 0.2);
    ligDir = normalize(ligDir);
    float NdotL = dot(normal, ligDir);
    // return float4(NdotL.xxx, 1.0);

    float metallic = 0.2;//g_PushConstants.pbrParams.x;
    float roughness = 0.7;//g_PushConstants.pbrParams.y;
    uint width = 0;
    uint height = 0;
    uint mip = 0;
    g_MetallicTexture.GetDimensions(0, width, height, mip);
    // if(width > 2) {
    //     float4 materialData = g_MetallicTexture.Sample(g_Sampler, newUV);
    //     roughness = materialData.g;
    //     metallic = materialData.b;
    // }

    float3 projCoords = input.positionLS.xyz / input.positionLS.w;
    projCoords.x = projCoords.x * 0.5 + 0.5;
    projCoords.y = projCoords.y * -0.5 + 0.5;
    float shadow = 1.0;
    //part of PBR
    // color.xyz = DirectionalLight(input.positionWS, worldNormal, v, ligDir, float3(1.0, 1.0, 1.0), baseColor.xyz, metallic, roughness, c_F0);
    float3 dir = -v;
    float3 R = reflect(dir, worldNormal);

    TextureCube<float4> diffuseIBL = ResourceDescriptorHeap[7];
    TextureCube<float4> specularIBL = ResourceDescriptorHeap[8];
    Texture2D<float4> BRDFTex = ResourceDescriptorHeap[9];
    Texture2D<float> shadowMap = ResourceDescriptorHeap[13];

    SamplerState g_SamplerBRDF = SamplerDescriptorHeap[materialData.textureIndex3];
    SamplerComparisonState g_SamplerShadow = SamplerDescriptorHeap[3];

    // shadow = pcf_shadow_poisson_weighted(projCoords, g_SamplerShadow, shadowMap, 0.0001);
    // shadow = uniformPoissonPCF(projCoords, g_SamplerShadow, shadowMap);
    // shadow = pcf_shadow(projCoords, g_SamplerShadow, shadowMap, 0.002, 8);
    shadow = shadowSampleCmp(g_SamplerShadow, shadowMap, projCoords.xy, projCoords.z,  0);

    shadow = SampleCascadeShadow(input.positionWS, splitDepth);

    float4 cascadeColors[4] = {
        float4(1.0, 0.0, 0.0, 1.0), // 红色 - 级联0
        float4(0.0, 1.0, 0.0, 1.0), // 绿色 - 级联1  
        float4(0.0, 0.0, 1.0, 1.0), // 蓝色 - 级联2
        float4(1.0, 1.0, 0.0, 1.0)  // 黄色 - 级联3
    };
    float viewDepth = length(g_PushConstants.camPos.xyz - input.positionWS);
    int cascadeIndex = GetCascadeIndex(viewDepth, splitDepth);
    // return cascadeColors[cascadeIndex] * shadow;

    // shadow = shadow3x3PCF(g_SamplerShadow, shadowMap, projCoords.xy, projCoords.z, 1.0 / 2048.0);
    // return float4(projCoords.zzz, 1.0);
    float4 outPosLS = input.positionLS.xyzz;
    color.xyz += IBL(worldNormal, v, R, baseColor.xyz, metallic, roughness, c_F0, BRDFTex, diffuseIBL, specularIBL, g_Sampler, g_SamplerBRDF);
    //color.xyz *= shadow;
    // color.xyz = input.color.xyz;
    return color;
}
