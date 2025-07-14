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
    SamplerComparisonState g_SamplerShadow = SamplerDescriptorHeap[materialData.textureIndex3 + 1];

    shadow = pcf_shadow_poisson_weighted(projCoords, g_SamplerShadow, shadowMap, 0.0001);
    float4 outPosLS = input.positionLS.xyzz;
    // baseColor.xyz *= shadow; //clamp(shadow, 0.4, 1.0);
    // baseColor = float4(projCoords.xy, 0.0, 1.0);
    // return baseColor;
    color.xyz += IBL(worldNormal, v, R, baseColor.xyz, metallic, roughness, c_F0, BRDFTex, diffuseIBL, specularIBL, g_Sampler, g_SamplerBRDF);
    color.xyz *= shadow;
    return color;
}
