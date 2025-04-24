#include "NRICompatibility.hlsli"

struct PSInput {
	float4 position : SV_Position; // Equivalent to gl_Position
	float3 dir : TEXCOORD0; // Output direction
};

NRI_RESOURCE(Texture2D, g_DiffuseTexture, t, 0, 0);
NRI_RESOURCE(SamplerState, g_Sampler, s, 0, 0);

float4 main(PSInput input) : SV_Target {
	float3 cube_normal = 0.0;
	float2 uv_interp = input.dir.xy;
	uv_interp.y = 1.0 - uv_interp.y;
	float4 color = g_DiffuseTexture.Sample(g_Sampler, uv_interp);
	// color = pow(color, 2.2);
	return color;
}