static const float2 posArray[] = {float2(-3.0, 1.0), float2(1.0, 1.0), float2(1.0, -3.0)};

struct VSOutput {
    float4 position : SV_Position;
    float2 uv : TEXCOORD0;
};

VSOutput vs_main(uint vertexId : SV_VertexID) {    
    VSOutput output;
    output.position = float4(posArray[vertexId], 0.0, 1.0);    output.uv = (output.position.xy + 1.0) * 0.5;
    output.uv.y = 1.0 - output.uv.y;
    return output;
}

struct PSInput {
    float4 position : SV_Position;
    float2 uv : TEXCOORD0;
};

float4 ps_main(PSInput input) : SV_Target {
    return float4(input.uv, 0.0, 1.0);
}








;

