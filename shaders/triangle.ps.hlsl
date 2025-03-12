Texture2D mainTex : t0;
SamplerState maintexSampler_s : s0;

struct PixelInput
{
    float4 i_pos	: SV_Position;
	float2 i_uv		: TEXCOORD0;
};

float4 main(PixelInput ps_input): SV_Target0
{
	float2 uv = ps_input.i_uv.xy;
	float4 mainColor = mainTex.Sample(maintexSampler_s, uv);
	return float4(mainColor);
}