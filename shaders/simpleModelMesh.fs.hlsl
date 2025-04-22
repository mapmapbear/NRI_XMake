#include "NRICompatibility.hlsli"

struct InputPS
{
    float4 position : SV_Position;
    float2 uv : TEXCOORD0;
    float3 positionWS : TEXCOORD1;
    float3 normalWS : NORMAL;
    float3 tangentWS : TANGENT;
    float4x4 tbnMat : TEXCOORD2;
};

// NRI_RESOURCE(Texture2D, g_AlbedoTexture, t, 0, 1);
// NRI_RESOURCE(Texture2D, g_NormalTexture, t, 1, 1);
// NRI_RESOURCE(Texture2D, g_MRTexture, t, 2, 1);
// NRI_RESOURCE(Texture2D, g_AOTexture, t, 3, 1);
// NRI_RESOURCE(Texture2D, g_EmissiveTexture, t, 4, 1);
// NRI_RESOURCE(SamplerState, g_Sampler, s, 0, 1 );


struct PushConstants
{
    float4 camPos;
    uint2 indexGroup;
};
NRI_ROOT_CONSTANTS( PushConstants, g_PushConstants, 1, 0 );

const float3 ligDir = normalize(float3(0, -1, 0));
// [earlydepthstencil]
float4 main(InputPS input) : SV_Target
{
    float2 newUV = input.uv;
    newUV.y = 1.0 - newUV.y;
    float4 color = 0.0;
    float3 n = normalize(input.normalWS);
	float3 v = normalize(g_PushConstants.camPos.xyz - input.positionWS);
    Texture2D g_AlbedoTexture = ResourceDescriptorHeap[g_PushConstants.indexGroup.x];
    Texture2D g_NormalTexture = ResourceDescriptorHeap[g_PushConstants.indexGroup.y];
    SamplerState g_Sampler = SamplerDescriptorHeap[0];
    color = g_AlbedoTexture.Sample(g_Sampler, newUV) * 0.5;
    float4 normalTS = g_NormalTexture.Sample(g_Sampler, newUV);
    normalTS = normalTS * 2.0 - 1.0;
    normalTS = normalize(normalTS);
    float3 tangent = input.tangentWS;
    float3 normal = input.normalWS;
    float3 bitangent = cross(normal, tangent);
    float3 worldNormal = normalize( normalTS.x * tangent + normalTS.y * bitangent + normalTS.z * normal );
    // color.xyz = worldNormal.xyz;
    float NdotL = max(dot(worldNormal, ligDir), 0.0);
    color.xyz = color.xyz * NdotL;
    return color;
}
