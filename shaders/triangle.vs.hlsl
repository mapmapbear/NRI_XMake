struct ObjectConstants					
{
	float4x4 modelMat;
	float4x4 viewMat;
	float4x4 projectMat;
	float padding[16];
};
ConstantBuffer<ObjectConstants> gObjConstants : register(b0);


struct VertexInput
{
    float3 PosL     : POSITION;
    float2 uv0 		: TEXCOORD0;
};

struct VertexOutput
{
    float4 o_pos	: SV_Position;
	float2 o_uv 	: TEXCOORD0;
};

VertexOutput main(VertexInput input)
{
	VertexOutput outPut;
	float4x4 vpMat = mul(gObjConstants.viewMat, gObjConstants.modelMat);
	float4x4 mvpMat = mul(gObjConstants.projectMat, vpMat);
	outPut.o_pos = mul(mvpMat, float4(input.PosL.xyz, 1.0));
	float2 uv = input.uv0;
	uv.y = 1.0 - uv.y;
	outPut.o_uv = uv;
	return outPut;
}