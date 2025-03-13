#include "NRICompatibility.hlsli"

NRI_RESOURCE( cbuffer, CommonConstants, b, 0, 0 )
{
    float4x4 modelMat;
	float4x4 viewMat;
	float4x4 projectMat;
};


static float3 pos[8] = {
	float3(-1.0,-1.0, 1.0),
	float3( 1.0,-1.0, 1.0),
	float3( 1.0, 1.0, 1.0),
	float3(-1.0, 1.0, 1.0),

	float3(-1.0,-1.0,-1.0),
	float3( 1.0,-1.0,-1.0),
	float3( 1.0, 1.0,-1.0),
	float3(-1.0, 1.0,-1.0)
};

static int indices[36] = {
	0, 1, 2, 2, 3, 0,	// front
	1, 5, 6, 6, 2, 1,	// right 
	7, 6, 5, 5, 4, 7,	// back
	4, 0, 3, 3, 7, 4,	// left
	4, 5, 1, 1, 0, 4,	// bottom
	3, 2, 6, 6, 7, 3	// top
};

struct VSOutput {
    float4 position : SV_POSITION; // Equivalent to gl_Position
    float3 dir : TEXCOORD0;        // Output direction
};

static float2 posArray[3] = {float2(-1.0, -3.0), float2(-1.0, 1.0), float2(3.0, 1.0)};

VSOutput main(uint vertexId : SV_VertexID) {
    VSOutput output;
    int idx = indices[vertexId];
    float3x3 view3x3 = (float3x3)viewMat; // Extract 3x3 rotation matrix from view
    output.position = mul(projectMat, mul(float4(mul(view3x3, pos[idx]), 1.0), 1.0));
	output.position = float4(posArray[vertexId], 0.0, 1.0);
    output.dir = output.position.xyz; 
    return output;
}