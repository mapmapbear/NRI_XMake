#include "NRICompatibility.hlsli"


struct PushConstants
{
    float2 gInvScreenSize;
    float gSdrScale;
    float gIsSrgb;
};

NRI_ROOT_CONSTANTS( PushConstants, g_PushConstants, 0, 0 );

float3 FromSrgb( float3 x )
{
    const float4 consts = float4( 1.0f / 12.92f, 1.0f / 1.055f, 0.055f / 1.055f, 1.0f / 0.41666f );
    return lerp( x * consts.x, pow( x * consts.y + consts.z, consts.www ), step( 0.04045f, x ) );
}

struct PS_INPUT
{
    float4 pos : SV_POSITION;
    float2 uv : TEXCOORD0;
    float4 col : COLOR0;
};

NRI_RESOURCE( SamplerState, sampler0, s, 0, 0 );
NRI_RESOURCE( Texture2D<float>, texture0, t, 0, 0 );

float4 main( PS_INPUT input ) : SV_Target
{
    float4 color = input.col;
    color.w *= texture0.Sample( sampler0, input.uv );

    // if( g_PushConstants.gIsSrgb == 0.0 )
    //     color.xyz = FromSrgb( color.xyz );

    color.xyz *= g_PushConstants.gSdrScale;

    return color;
}