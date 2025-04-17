#include "NRICompatibility.hlsli"

struct InputPS
{
    float4 position : SV_Position;
    float2 uv : TEXCOORD0;
    float3 posWS : TEXCOORD1;
    float3 normal : NORMAL;
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
};
NRI_ROOT_CONSTANTS( PushConstants, g_PushConstants, 1, 0 );


float4 main(InputPS input) : SV_Target
{
    float2 newUV = input.uv;
    newUV.y = 1.0 - newUV.y;
    float4 color = 0.0;
    float3 n = normalize(input.normal);
	float3 v = normalize(g_PushConstants.camPos.xyz - input.posWS);
    color = float4(n, 1.0);
    return color;
}
