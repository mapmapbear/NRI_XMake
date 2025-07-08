#include "NRICompatibility.hlsli"

struct PushConstants {
    float4x4 viewMat;
    float4 cameraArgs; // znear, zfar, distCull, cullingEnabled
    float4 frustum[4]; // left, right, top, bottom
    uint4 totalObjectCount;
};
NRI_ROOT_CONSTANTS(PushConstants, g_PushConstants, 1, 0);

NRI_RESOURCE(cbuffer, CommonConstants, b, 0, 1) {
    float4x4 modelMat;
    float4x4 viewMat;
    float4x4 projectMat;
    float4x4 lightVP;
};

struct DrawData {
    uint indexNum;
    uint instanceNum;
    uint baseIndex;
    int baseVertex;
    uint baseInstance;
};

struct CullData {
    float3 center;
    float3 extents;
};

float3 GetAABBPoint(uint cornerIndex, float3 center, float3 extents) {
    float3 p;
    p.x = (cornerIndex & 1) ? center.x + extents.x : center.x - extents.x;
    p.y = (cornerIndex & 2) ? center.y + extents.y : center.y - extents.y;
    p.z = (cornerIndex & 4) ? center.z + extents.z : center.z - extents.z;
    return p;
}

bool isSphereInFrustum(float3 center, float radius) {
    for (int i = 0; i < 4; i++) {
        float4 plane = g_PushConstants.frustum[i];
        float distance = dot(plane.xyz, center);
        if (distance < -radius) {
            return false;
        }
    }
    return true;
}

bool FrustumVisible(uint objectIndex) {
    StructuredBuffer<CullData> sphereCullData = ResourceDescriptorHeap[1015];
    float3 center =  sphereCullData[objectIndex].center;
    float radius = min(sphereCullData[objectIndex].extents.x, min(sphereCullData[objectIndex].extents.y, sphereCullData[objectIndex].extents.z));
    float4 centerVS = mul(g_PushConstants.viewMat, float4(center,radius));
    bool visible = true;

    visible = isSphereInFrustum(centerVS.xyz, radius);
    return visible;
}

float2 ProjectSphere(float x, float z, float r, float ResultScale) {
    float t = sqrt(x*x + z*z - r*r);

    float A = (t*z + r*x);
    float B = (t*z - r*x);
    ResultScale /= (A * B);	// Divide by common denominator instead of dividing twice

    float Min = (t*x - r*z) * B;
    float Max = (t*x + r*z) * A;

    return float2(Min, Max) * ResultScale;
}

// [ Mara & Morgan 2013, "2D Polyhedral Bounds of a Clipped, Perspective-Projected 3D Sphere" ]
float4 SphereToScreenRect(float3 Center, float Radius, float4x4 ViewToClip) {
    float2 ExtentX = ProjectSphere(Center.x, Center.z, Radius, ViewToClip[0][0]) + ViewToClip[2][0];
    float2 ExtentY = ProjectSphere(Center.y, Center.z, Radius, ViewToClip[1][1]) + ViewToClip[2][1];

    return float4(ExtentX.x, ExtentY.x, ExtentX.y, ExtentY.y);
}

bool SphereCullFrustum(uint objectIndex) {
    StructuredBuffer<CullData> sphereCullData = ResourceDescriptorHeap[1015];
    float3 center = sphereCullData[objectIndex].center;
    float radius = max(sphereCullData[objectIndex].extents.x, max(sphereCullData[objectIndex].extents.y, sphereCullData[objectIndex].extents.z));

    float4 centerVS = mul(viewMat, float4(center, 1.0));
    float SphereMinZ = centerVS.z - radius;
    float SphereMaxZ = centerVS.z + radius;

    bool visible = true;
    if (visible) {
        visible = (SphereMinZ * projectMat[2][2] + projectMat[3][2] > 0);
    }

    float4 Rect = SphereToScreenRect(centerVS.xyz, radius, projectMat);

    float3 RectMin = float3(Rect.xy, 0);
    float3 RectMax = float3(Rect.zw, SphereMaxZ);

    if (visible) {
        visible = all(RectMin.xy < 1) && all(RectMax.xy > -1);
    }

    return visible;
}

float Min4(float a, float b, float c, float d) {
    return min(min(a, b), min(c, d));
}

uint ComputeHZBMip(int4 rectPixels, int texelCoverage) {
    int2 rectSize = rectPixels.zw - rectPixels.xy;
    int mipOffset = (int)log2((float)texelCoverage) - 1;
    int2 mipLevelXY = firstbithigh(rectSize);
    int mip = max(max(mipLevelXY.x, mipLevelXY.y) - mipOffset, 0);
    if(any((rectPixels.zw >> mip) - (rectPixels.xy >> mip) >= texelCoverage)) {
        ++mip;
    }
    return mip;
}

bool HZBCull2(uint objectIndex) {
    StructuredBuffer<CullData> sphereCullData = ResourceDescriptorHeap[1015];
    Texture2D<float> HizBuffer = ResourceDescriptorHeap[1022];
    SamplerState HizSampler = ResourceDescriptorHeap[5];
    static const uint hzbTexelCoverage = 4;
    float3 center = sphereCullData[objectIndex].center;
    float3 extents = sphereCullData[objectIndex].extents;

    float3 corners[8];
    corners[0] = center + float3(-extents.x, -extents.y, -extents.z);
    corners[1] = center + float3(extents.x, -extents.y, -extents.z);
    corners[2] = center + float3(extents.x,  extents.y, -extents.z);
    corners[3] = center + float3(-extents.x,  extents.y, -extents.z);
    corners[4] = center + float3(-extents.x, -extents.y,  extents.z);
    corners[5] = center + float3(extents.x, -extents.y,  extents.z);
    corners[6] = center + float3(extents.x,  extents.y,  extents.z);
    corners[7] = center + float3(-extents.x,  extents.y,  extents.z);

    float4x4 VPMat = mul(projectMat, viewMat);
    // VPMat = g_PushConstants.viewMat;
    float3 minXY = float3(2.0, 2.0, 2.0);
    float3 maxXY = float3(-2.0, -2.0, -2.0);
    float minDepth = 1.0;

    for (int i = 0; i < 8; ++i) {
        float4 clipPos = mul(VPMat, float4(corners[i], 1.0));
        float3 ndc = clipPos.xyz / clipPos.w;
        ndc = clamp(ndc, -1.0, 1.0);
        ndc.xy = ndc.xy * float2(0.5, -0.5) + float2(0.5, 0.5);
        minXY = saturate(min(minXY, ndc));
        maxXY = saturate(max(maxXY, ndc));
        minDepth = saturate(min(minDepth, ndc.z));
    }
    const int2 RTSize = int2(1920.0 / 2, 1080.0 / 2);
    const int MaxMipLevel = 9;
    float4 boxUVs = float4(minXY.xy, maxXY.xy);
    int2 size = (maxXY.xy - minXY.xy) * RTSize.xy;
    float mip = ceil(log2(max(size.x, size.y)));
    mip = clamp(mip, 0, MaxMipLevel);

    float  level_lower = max(mip - 1, 0);
    float2 scale = exp2(-level_lower);
    float2 a = floor(boxUVs.xy*scale);
    float2 b = ceil(boxUVs.zw*scale);
    float2 dims = b - a;

    if (dims.x <= 2 && dims.y <= 2)
    mip = level_lower;

    float4 depth;
    depth.x = HizBuffer.SampleLevel(HizSampler, boxUVs.xy, mip).r;
    depth.y = HizBuffer.SampleLevel(HizSampler, boxUVs.zy, mip).r;
    depth.z = HizBuffer.SampleLevel(HizSampler, boxUVs.xw, mip).r;
    depth.w = HizBuffer.SampleLevel(HizSampler, boxUVs.zw, mip).r;

    //find the max depth
    float HiZDepth = max(max(max(depth.x, depth.y), depth.z), depth.w);
    return minDepth <= HiZDepth;
}

#if 1
groupshared uint s_DrawCount[32];

[numthreads(32, 1, 1)]
void main(uint3 DTid : SV_DispatchThreadID, uint3 Gid : SV_GroupID, uint3 GTid : SV_GroupThreadID) {
    RWStructuredBuffer<DrawData> visibleObjects = ResourceDescriptorHeap[1017];
    RWStructuredBuffer<uint> visibleObjectCounter = ResourceDescriptorHeap[1018];
    RWStructuredBuffer<uint> visibleObjectFlags = ResourceDescriptorHeap[1019];
    StructuredBuffer<DrawData> allObjects = ResourceDescriptorHeap[1016];

#ifdef HIZ_CULL_PRE_PASS
    uint objectIndex = DTid.x;
    if (objectIndex >= g_PushConstants.totalObjectCount.x) {
        return;
    }

    if(g_PushConstants.totalObjectCount.y > 0) {
        visibleObjectCounter[0] = g_PushConstants.totalObjectCount.x;
        visibleObjects[objectIndex] = allObjects[objectIndex];
        return;
    }

    if (DTid.x == 0) {
        // s_DrawCount[Gid.x] = 0;
        visibleObjectCounter[0] = 0;
    }

    visibleObjects[objectIndex] = (DrawData)0;

    DeviceMemoryBarrierWithGroupSync();

    bool visible = SphereCullFrustum(objectIndex);
    visible = visible && HZBCull2(objectIndex);
    if (visible) {
        visibleObjectFlags[objectIndex] = 1;
    }
    else {
        visibleObjectFlags[objectIndex] = 0;
    }

    DeviceMemoryBarrierWithGroupSync();

    if (visibleObjectFlags[objectIndex] == 1) {
        uint writeIndex = 0;
        InterlockedAdd(visibleObjectCounter[0], 1, writeIndex);
        if (writeIndex < g_PushConstants.totalObjectCount.x) {
            visibleObjects[writeIndex] = allObjects[objectIndex];
        }
    }
#endif

#ifdef HIZ_CULL_POST_PASS
    uint objectIndex = DTid.x;
    if (objectIndex >= g_PushConstants.totalObjectCount.x) {
        return;
    }
    if(visibleObjectCounter[0] == 0) {
        return ;
    }

    bool wasVisibleInPrePass = (visibleObjectFlags[objectIndex] == 1);
    bool newlyVisible = false;

    if (!wasVisibleInPrePass) {
        newlyVisible = HZBCull2(objectIndex);
        if (newlyVisible) {
            visibleObjectFlags[objectIndex] = 1;
        }
    }

    DeviceMemoryBarrierWithGroupSync();

    if (newlyVisible) {
        uint writeIndex = 0;
        InterlockedAdd(visibleObjectCounter[0], 1, writeIndex);
        if (writeIndex < g_PushConstants.totalObjectCount.x) {
            StructuredBuffer<DrawData> allObjects = ResourceDescriptorHeap[1016];
            visibleObjects[writeIndex] = allObjects[objectIndex];
        }
    }
#endif
}

#else 
groupshared uint s_DrawCount;
groupshared uint s_GroupBaseIndex;

[numthreads(32, 1, 1)]
void main(uint3 DTid : SV_DispatchThreadID, uint3 Gid : SV_GroupID, uint GI : SV_GroupIndex) {
    RWStructuredBuffer<DrawData> visibleObjects = ResourceDescriptorHeap[1017];
    RWStructuredBuffer<uint> visibleObjectCounter = ResourceDescriptorHeap[1018];
    RWStructuredBuffer<uint> visibleObjectCounter = ResourceDescriptorHeap[1019];
    StructuredBuffer<DrawData> allObjects = ResourceDescriptorHeap[1016];

    uint objectIndex = DTid.x;

    // 初始化
    if (GI == 0) {
        s_DrawCount = 0;
        if (Gid.x == 0) {
            visibleObjectCounter[0] = 0;
        }
    }

    GroupMemoryBarrierWithGroupSync();

    if (objectIndex >= g_PushConstants.totalObjectCount) {
        return;
    }

    // 第一阶段：组内计数
    bool visible = HZBCull2(objectIndex);
    uint localIndex = 0;
    if (visible) {
        InterlockedAdd(s_DrawCount, 1, localIndex);
    }

    GroupMemoryBarrierWithGroupSync();

    // 第二阶段：获取全局基础索引
    if (GI == 0 && s_DrawCount > 0) {
        InterlockedAdd(visibleObjectCounter[0], s_DrawCount, s_GroupBaseIndex);
    }

    GroupMemoryBarrierWithGroupSync();

    // 第三阶段：写入最终结果
    if (visible) {
        uint finalIndex = s_GroupBaseIndex + localIndex;
        if (finalIndex < g_PushConstants.totalObjectCount) {
            visibleObjects[finalIndex] = allObjects[objectIndex];
        }
    }
}
#endif

// [numthreads(32, 1, 1)]
// void main(uint3 DTid : SV_DispatchThreadID, uint3 Gid : SV_GroupID, uint3 GTid : SV_GroupThreadID) {
//     RWStructuredBuffer<DrawData> visibleObjects = ResourceDescriptorHeap[1017];
//     RWStructuredBuffer<uint> visibleObjectCounter = ResourceDescriptorHeap[1018];
//     RWStructuredBuffer<uint> visibleObjectFlags = ResourceDescriptorHeap[1019];

//     uint objectIndex = DTid.x;
//     if (objectIndex >= g_PushConstants.totalObjectCount) {
//         return;
//     }

//     if (DTid.x == 0) {
//         visibleObjectCounter[0] = 0;
//     }

//     visibleObjects[objectIndex] = (DrawData)0;

//     DeviceMemoryBarrierWithGroupSync();

//     // bool visible = FrustumVisible(objectIndex);
//     // visible = visible && Hiz_Culling(objectIndex);
//     // bool visible = Hiz_Culling(objectIndex);
//     // bool visible = SphereCullFrustum(objectIndex);
//     // bool visible = HizCull(objectIndex);
//     bool visible = HZBCull2(objectIndex);
//     if(visible) {
//         visibleObjectFlags[objectIndex] = 1;
//     }

//     DeviceMemoryBarrierWithGroupSync();

//     StructuredBuffer<DrawData> allObjects = ResourceDescriptorHeap[1016];

//     if (visibleObjectFlags[objectIndex] == 1) {
//         uint writeIndex = 0;
//         InterlockedAdd(visibleObjectCounter[0], 1, writeIndex);
//         if (writeIndex < g_PushConstants.totalObjectCount) {
//             visibleObjects[writeIndex] = allObjects[objectIndex];
//         }
//     }
// }

