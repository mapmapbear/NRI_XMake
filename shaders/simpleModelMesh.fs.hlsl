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
    float3 tangentWS  : TANGENT;
    float3 color      : COLOR;
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
    // return float4(newUV, 0.0, 1.0);
    //baseColor *= g_PushConstants.baseColor;
    float4 normalTS = g_NormalTexture.Sample(g_Sampler, newUV);

    normalTS = normalTS * 2.0 - 1.0;
    normalTS = normalize(normalTS);
    float3 tangent = input.tangentWS;
    float3 normal = n;
    float3 bitangent = cross(normal, tangent);
    float3 worldNormal = normalize(normalTS.x * tangent + normalTS.y * bitangent + normalTS.z * normal);
    float3 ligDir = float3(0.2, 100.0, 0.2);
    ligDir = normalize(ligDir);
    float NdotL = dot(normal, ligDir);
    return float4(baseColor.xyz, 1.0);

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
    // color.xyz = DirectionalLight(input.positionWS, worldNormal, v, ligDir, float3(1.0, 0.0, 1.0), baseColor.xyz, metallic, roughness, c_F0);
    float3 dir = -v;
    float3 R = reflect(dir, worldNormal);

    TextureCube<float4> diffuseIBL = ResourceDescriptorHeap[g_PushConstants.indexGroup.w];
    TextureCube<float4> specularIBL = ResourceDescriptorHeap[g_PushConstants.indexGroup.w + 1];
    Texture2D<float4> BRDFTex = ResourceDescriptorHeap[g_PushConstants.indexGroup.w + 2];
    Texture2D<float> shadowMap = ResourceDescriptorHeap[g_PushConstants.indexGroup.w + 3];

    SamplerState g_SamplerBRDF = SamplerDescriptorHeap[2];
    SamplerComparisonState g_SamplerShadow = SamplerDescriptorHeap[3];
    // shadow = hard_shadow(projCoords, g_SamplerShadow, shadowMap, -0.002);
    // shadow = pcf_shadow(projCoords, g_SamplerShadow, shadowMap, 0.002, 9);
    shadow = pcf_shadow_poisson(projCoords, g_SamplerShadow, shadowMap);
    float4 outPosLS = input.positionLS.xyzz;
    baseColor.xyz *= shadow; //clamp(shadow, 0.4, 1.0);
    // baseColor = float4(projCoords.xy, 0.0, 1.0);
    return baseColor;
    color.xyz += IBL(worldNormal, v, R, baseColor.xyz, metallic, roughness, c_F0, BRDFTex, diffuseIBL, specularIBL, g_Sampler, g_SamplerBRDF);
    return color;
}
