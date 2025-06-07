#include "NRICompatibility.hlsli"

struct PushConstants {
    float4x4 vpMat;
};
NRI_ROOT_CONSTANTS(PushConstants, g_PushConstants, 1, 0);

struct VertexInput
{
    float3 PosL     : POSITION;
	uint instanceID : SV_InstanceID;
};

struct VertexOutput
{
    float4 o_pos	: SV_Position;
	float2 o_uv 	: TEXCOORD0;
};

struct PixelInput 
{
	float4 o_pos	: SV_Position;
	float2 o_uv 	: TEXCOORD0;
};

VertexOutput vs_main(VertexInput input)
{
	VertexOutput outPut;
	StructuredBuffer<float3> posBuffer = ResourceDescriptorHeap[1013];
	float3 posWS = posBuffer[input.instanceID];
	float4x4 worldMat = float4x4(
        1.0, 0.0, 0.0, 0.0,
        0.0, 1.0, 0.0, 0.0,
        0.0, 0.0, 1.0, 0.0,
        // posWS.x, posWS.y, posWS.z, 1.0
		0.0, 0.0, 1.0, 1.0
    );

	worldMat = float4x4(
        1.0, 0.0, 0.0, posWS.x,
        0.0, 1.0, 0.0, posWS.y,
        0.0, 0.0, 1.0, posWS.z,
        0.0, 0.0, 0.0, 1.0
    );
	float4x4 mvpMat = mul(g_PushConstants.vpMat, worldMat);
	//mvpMat = worldMat;//g_PushConstants.vpMat;
	outPut.o_pos = mul(mvpMat, float4(input.PosL.xyz, 1.0));
	float2 uv = input.PosL.xy;
	outPut.o_uv = uv;
	return outPut;
}


float4 ps_main(PixelInput input) : SV_Target
{
	return float4(input.o_uv, 0.0, 1.0);
}
