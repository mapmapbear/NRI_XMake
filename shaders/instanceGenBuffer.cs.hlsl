#include "NRICompatibility.hlsli"


// 只读缓冲区，存储位置和初始角度
StructuredBuffer<float4> Positions : register(t0); // 绑定到 SRV 槽位 t0

// 只写缓冲区，存储变换矩阵
RWStructuredBuffer<float4x4> Matrices : register(u0); // 绑定到 UAV 槽位 u0

struct PushConstants
{
    float4 time;
};
NRI_ROOT_CONSTANTS( PushConstants, g_PushConstants, 0, 0 ); 


// 平移函数
float4x4 translate(float4x4 m, float3 v) {
    float4x4 Result = m;
    Result[3] = m[0] * v.x + m[1] * v.y + m[2] * v.z + m[3];
    return Result;
}


float4x4 translate1(float4x4 m, float3 v)
{
    float4x4 Result = m;
    Result[3].x = m[3].x + v.x;
    Result[3].y = m[3].y + v.y;
    Result[3].z = m[3].z + v.z;
    return Result;
}

// 旋转函数（基于轴和角度）
float4x4 rotate(float4x4 m, float angle, float3 v) {
    float a = angle;
    float c = cos(a);
    float s = sin(a); 

    float3 axis = normalize(v);
    float3 temp = (1.0f - c) * axis;

    float4x4 r;
    r[0][0] = c + temp.x * axis.x;
    r[0][1] = temp.x * axis.y + s * axis.z;
    r[0][2] = temp.x * axis.z - s * axis.y;
    r[0][3] = 0.0f;

    r[1][0] = temp.y * axis.x - s * axis.z;
    r[1][1] = c + temp.y * axis.y;
    r[1][2] = temp.y * axis.z + s * axis.x;
    r[1][3] = 0.0f;

    r[2][0] = temp.z * axis.x + s * axis.y;
    r[2][1] = temp.z * axis.y - s * axis.x;
    r[2][2] = c + temp.z * axis.z;
    r[2][3] = 0.0f;

    r[3] = float4(0.0f, 0.0f, 0.0f, 1.0f);

    float4x4 res;
    res[0] = m[0] * r[0][0] + m[1] * r[0][1] + m[2] * r[0][2];
    res[1] = m[0] * r[1][0] + m[1] * r[1][1] + m[2] * r[1][2];
    res[2] = m[0] * r[2][0] + m[1] * r[2][1] + m[2] * r[2][2];
    res[3] = m[3];
    return res;
}


[numthreads(32, 1, 1)]
void main(uint3 DTid : SV_DispatchThreadID) {
    uint idx = DTid.x;

    // 从 Positions 缓冲区读取中心位置和初始角度
    float4 center = Positions[idx];

    // 计算模型矩阵：先平移到 center.xyz，再绕 (1,1,1) 轴旋转
    // float4x4 model = rotate(translate(float4x4(1.0f, 0.0f, 0.0f, 0.0f,
    //                                           0.0f, 1.0f, 0.0f, 0.0f,
    //                                           0.0f, 0.0f, 1.0f, 0.0f,
    //                                           0.0f, 0.0f, 0.0f, 1.0f), 
    //                                  float3(center.x, 0.2, center.z)), 
    //                        g_PushConstants.time.x + center.w, 
    //                        float3(1.0f, 1.0f, 1.0f));
    float4x4 model = translate1(float4x4(1.0f, 0.0f, 0.0f, 0.0f,
                        0.0f, 1.0f, 0.0f, 0.0f,
                        0.0f, 0.0f, 1.0f, 0.0f,
                        0.0f, 0.0f, 0.0f, 1.0f), 
                float3(center.x, 0.2, center.z));

    // 将结果写入 Matrices 缓冲区
    Matrices[idx] = model;
}