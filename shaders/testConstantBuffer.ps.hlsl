cbuffer ColorBuffer : register(b0)
{
    float4 Color;
};

// 像素着色器输入结构
struct PS_INPUT
{
    float4 Position : SV_POSITION;
};

// 像素着色器
float4 main(PS_INPUT input) : SV_TARGET
{
    return Color;
}