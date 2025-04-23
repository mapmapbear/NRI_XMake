#include "NRICompatibility.hlsli"
#include "pbrCommon.hlsli"

struct InputPS
{
    float4 position : SV_Position;
    float2 uv : TEXCOORD0;
    float3 positionWS : TEXCOORD1;
    float3 normalWS : NORMAL;
    float3 tangentWS : TANGENT;
};

struct PushConstants
{
    float4 camPos;
    uint4 indexGroup;
};
NRI_ROOT_CONSTANTS( PushConstants, g_PushConstants, 1, 0 );

[earlydepthstencil]
float4 main(InputPS input) : SV_Target
{
    float2 newUV = input.uv;
    newUV.y = 1.0 - newUV.y;
    float4 color = 0.0;
    float3 n = normalize(input.normalWS);
	float3 v = normalize(g_PushConstants.camPos.xyz - input.positionWS);
    Texture2D g_AlbedoTexture = ResourceDescriptorHeap[g_PushConstants.indexGroup.x];
    Texture2D g_NormalTexture = ResourceDescriptorHeap[g_PushConstants.indexGroup.y];
    Texture2D g_MetallicTexture = ResourceDescriptorHeap[g_PushConstants.indexGroup.z];
    SamplerState g_Sampler = SamplerDescriptorHeap[0];
    float4 baseColor = g_AlbedoTexture.Sample(g_Sampler, newUV);
    float4 normalTS = g_NormalTexture.Sample(g_Sampler, newUV);
    normalTS = normalTS * 2.0 - 1.0;
    normalTS = normalize(normalTS);
    float3 tangent = input.tangentWS;
    float3 normal = input.normalWS;
    float3 bitangent = cross(normal, tangent);
    float3 worldNormal = normalize( normalTS.x * tangent + normalTS.y * bitangent + normalTS.z * normal );
    const float3 ligDir = float3(0.0, 1.0, 0.0);
    float NdotL = max(dot(worldNormal, ligDir), 0.0);
    color.xyz += NdotL * 0.5;

    float roughness = 0.001;
    float metallic = 0.0;

    uint width = 0;
    uint height = 0;
    uint mip = 0;
    g_MetallicTexture.GetDimensions(0, width, height, mip);
    if(width > 2)
    {
        float4 materialData = g_MetallicTexture.Sample(g_Sampler, newUV);
        roughness = materialData.g;
        metallic = materialData.b;
    }


    //part of PBR
    color.xyz = DirectionalLight(input.positionWS, worldNormal, v, ligDir, float3(1.0, 1.0, 1.0), baseColor.xyz, metallic, roughness, c_F0);
    float3 dir = -v;;
    float3 R = reflect(dir, worldNormal);
    TextureCube<float4> diffuseIBL = ResourceDescriptorHeap[22];
    TextureCube<float4> specularIBL = ResourceDescriptorHeap[23];
    Texture2D<float4> BRDFTex = ResourceDescriptorHeap[24];
    color.xyz += IBL(worldNormal, v, R, baseColor.xyz, metallic, roughness, c_F0, BRDFTex, diffuseIBL, specularIBL, g_Sampler);
    return color;
}
