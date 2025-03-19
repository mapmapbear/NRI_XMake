#include "NRICompatibility.hlsli"

static const float gridSize = 100.0;

static const float3 pos[4] = {
	float3(-1.0, 0.0, -1.0),
	float3( 1.0, 0.0, -1.0),
	float3( 1.0, 0.0,  1.0),
	float3(-1.0, 0.0,  1.0)
};

static const int indices[6] = {
	0, 1, 2, 2, 3, 0
};

struct PushConstants
{
    float4x4 mvpMat;
    float4 cameraPos;
    float4 origin;
};
NRI_ROOT_CONSTANTS( PushConstants, g_PushConstants, 0, 0 ); 



struct outputVS 
{
    float4 posistionCS : SV_Position;
    float2 uv : TEXCOORD0;
    float2 cameraPos : TEXCOORD1;
};

outputVS main(uint vertexId : SV_VertexID)
{
    outputVS output;
    int idx = indices[vertexId];
	float3 position = pos[idx] * gridSize;

    position.x += g_PushConstants.cameraPos.x;
	position.z += g_PushConstants.cameraPos.z;

    position += g_PushConstants.origin.xyz;

	output.cameraPos = g_PushConstants.cameraPos.xz;

	output.posistionCS = mul(g_PushConstants.mvpMat, float4(position, 1.0));
	output.uv = position.xz;

    return output;
}



